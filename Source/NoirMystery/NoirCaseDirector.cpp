#include "NoirCaseDirector.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "MysteryCore/Blueprint.h"
#include "MysteryCore/Case.h"
#include "MysteryCore/Content.h"
#include "MysteryCore/Staging.h"
#include "NoirCase.h"
#include "NoirCaseContent.h"
#include "NoirComplication.h"
#include "NoirCharacter.h"
#include "NoirEvidence.h"
#include "NoirEvidenceModels.h"
#include "NoirMindPlace.h"
#include "NoirRoom.h"
#include "NoirSave.h"
#include "NoirSearchable.h"
#include "NoirSuspect.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "NoirCaseDirector"

struct ANoirCaseDirector::FState
{
	const mystery::Content* Content = nullptr;
	mystery::Lang Lang = mystery::Lang::En;
	mystery::Blueprint Bp;
	mystery::PublicMystery Case;
	mystery::Staging Staging;
};

namespace
{
	/** A clue's puzzle from the generated case; Reveal is the full text it gives up when solved. */
	FNoirPuzzle MakePuzzle(const mystery::PublicEvidence& E)
	{
		FNoirPuzzle P;
		if (!E.puzzle) return P;
		P.Type = FName(NoirStr(E.puzzle->type));
		P.Answer = NoirStr(E.puzzle->answer);
		P.Key = NoirStr(E.puzzle->key);
		P.Shift = E.puzzle->shift;
		for (const std::string& Piece : E.puzzle->pieces) P.Pieces.Add(NoirStr(Piece));
		P.Reveal = NoirText(E.details);
		return P;
	}

	/** What the clue says until its puzzle is worked out: what it is, not what it shows. */
	FText PuzzleTeaser(const FName& Type)
	{
		if (Type == TEXT("hand")) return NoirTr(TEXT("Smudged prints on the cloth. Which hand left them is still to be worked out."), TEXT("Huellas borrosas en la tela. Falta averiguar qué mano las dejó."));
		if (Type == TEXT("sole")) return NoirTr(TEXT("A plaster cast of a shoeprint, not yet matched to a sole."), TEXT("Un molde de yeso de una pisada, aún sin comparar con ninguna suela."));
		if (Type == TEXT("uv")) return NoirTr(TEXT("There is something on it the eye can't see. A UV lamp would show it."), TEXT("Tiene algo que el ojo no ve. Una lámpara ultravioleta lo mostraría."));
		if (Type == TEXT("blood")) return NoirTr(TEXT("A blood sample, not yet typed."), TEXT("Una muestra de sangre, aún sin tipar."));
		if (Type == TEXT("torn")) return NoirTr(TEXT("Torn into scraps. It needs putting back together."), TEXT("Roto en trozos. Hay que recomponerlo."));
		if (Type == TEXT("cipher")) return NoirTr(TEXT("The entries are written in a letter-shift cipher."), TEXT("Las entradas están escritas en un cifrado por desplazamiento."));
		return FText();
	}

	/** What the detective calls each kind of furniture, in the case's language. */
	FText FurnitureName(const FName& Kind, const FText& Fallback, const FString& Theme)
	{
		struct FFurnitureName { const TCHAR* Kind; const TCHAR* En; const TCHAR* Es; };
		// On the train the same kinds of hiding place are different furniture.
		static const FFurnitureName Train[] = {
			{TEXT("desk"), TEXT("the writing table"), TEXT("la mesa de escritura")},
			{TEXT("wastebasket"), TEXT("the wastebasket"), TEXT("la papelera")},
			{TEXT("window"), TEXT("the window"), TEXT("la ventanilla")},
			{TEXT("cupboard"), TEXT("the wardrobe"), TEXT("el ropero")},
			{TEXT("rug"), TEXT("the carpet runner"), TEXT("la alfombra del pasillo")},
			{TEXT("bookshelf"), TEXT("the luggage rack"), TEXT("el portaequipajes")},
			{TEXT("coatrack"), TEXT("the coat hooks"), TEXT("el colgador")},
			{TEXT("sofa"), TEXT("the banquette"), TEXT("el banco tapizado")},
		};
		if (Theme == TEXT("express"))
		{
			for (const FFurnitureName& N : Train)
			{
				if (Kind == FName(N.Kind)) return NoirTr(N.En, N.Es);
			}
		}
		static const FFurnitureName Names[] = {
			{TEXT("desk"), TEXT("the desk"), TEXT("el escritorio")},
			{TEXT("wastebasket"), TEXT("the wastebasket"), TEXT("la papelera")},
			{TEXT("window"), TEXT("the window"), TEXT("la ventana")},
			{TEXT("cupboard"), TEXT("the cupboard"), TEXT("el armario")},
			{TEXT("rug"), TEXT("the rug"), TEXT("la alfombra")},
			{TEXT("bookshelf"), TEXT("the bookshelf"), TEXT("la estantería")},
			{TEXT("coatrack"), TEXT("the coat rack"), TEXT("el perchero")},
			{TEXT("sofa"), TEXT("the sofa"), TEXT("el sofá")},
		};
		for (const FFurnitureName& N : Names)
		{
			if (Kind == FName(N.Kind)) return NoirTr(N.En, N.Es);
		}
		return Fallback;
	}
}

ANoirCaseDirector::ANoirCaseDirector()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANoirCaseDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	NoirSetSpanish(false); // the interface goes back to English outside a Spanish case
	Super::EndPlay(Reason);
}

void ANoirCaseDirector::BeginPlay()
{
	Super::BeginPlay();
	ReadOverrides();
	NoirSetSpanish(Language == ENoirCaseLanguage::Spanish);
	if (!BuildCase())
	{
		return;
	}
	Stage();
	if (State->Case.modifier)
	{
		Complication = GetWorld()->SpawnActor<ANoirComplication>();
		Complication->Start(NoirStr(State->Case.modifier->key));
	}
	GetWorldTimerManager().SetTimer(BriefTimer, this, &ANoirCaseDirector::BriefDetective, 0.25f, true, 0.1f);
}

void ANoirCaseDirector::ReadOverrides()
{
	PlayedSeed = Seed;
	FString Value;
	if (FParse::Value(FCommandLine::Get(), TEXT("CaseSeed="), Value))
	{
		PlayedSeed = Value;
	}
	if (const AGameModeBase* Mode = GetWorld()->GetAuthGameMode())
	{
		const FString Option = UGameplayStatics::ParseOption(Mode->OptionsString, TEXT("Seed"));
		if (!Option.IsEmpty()) PlayedSeed = Option;
		const FString Lang = UGameplayStatics::ParseOption(Mode->OptionsString, TEXT("Lang"));
		if (Lang == TEXT("es")) Language = ENoirCaseLanguage::Spanish;
		if (Lang == TEXT("en")) Language = ENoirCaseLanguage::English;
		const FString Level = UGameplayStatics::ParseOption(Mode->OptionsString, TEXT("Difficulty"));
		if (Level == TEXT("Novice") || Level == TEXT("Detective") || Level == TEXT("Mastermind")) Difficulty = Level;
		bResume = UGameplayStatics::HasOption(Mode->OptionsString, TEXT("Continue")) && UNoirSaveGame::Exists();
	}
	if (PlayedSeed.IsEmpty())
	{
		// Same shape as the web game's random seeds.
		static const TCHAR* Words[] = {TEXT("RAVEN"), TEXT("ASH"), TEXT("MOTH"), TEXT("IVORY"), TEXT("EMBER"), TEXT("FOG"),
			TEXT("VELVET"), TEXT("COBALT"), TEXT("THORN"), TEXT("SILK"), TEXT("MARIGOLD"), TEXT("NEON")};
		PlayedSeed = FString::Printf(TEXT("%s-%d"), Words[FMath::RandRange(0, UE_ARRAY_COUNT(Words) - 1)], FMath::RandRange(1000, 9999));
	}
}

bool ANoirCaseDirector::BuildCase()
{
	State = MakeShared<FState>();
	State->Content = NoirLoadCaseContent();
	if (!State->Content)
	{
		return false;
	}
	State->Lang = Language == ENoirCaseLanguage::Spanish ? mystery::Lang::Es : mystery::Lang::En;
	State->Bp = mystery::BuildBlueprint(*State->Content, NoirUtf8(Difficulty), NoirUtf8(Theme), NoirUtf8(PlayedSeed));
	State->Case = mystery::Localize(*State->Content, State->Bp, State->Lang);
	UE_LOG(LogNoirCase, Display, TEXT("Case \"%s\" (seed %s, %s, %s)"), *NoirStr(State->Case.title), *PlayedSeed, *Theme, *Difficulty);
	return true;
}

void ANoirCaseDirector::Stage()
{
	FState& S = *State;
	UWorld* World = GetWorld();

	// ─── Read the level: rooms, the furniture and stand points inside each ───
	for (TActorIterator<ANoirRoom> It(World); It; ++It) Rooms.Add(*It);
	Rooms.Sort([](const ANoirRoom& A, const ANoirRoom& B) { return A.RoomId.LexicalLess(B.RoomId); });

	TArray<ANoirSearchable*> AllFurniture;
	for (TActorIterator<ANoirSearchable> It(World); It; ++It) AllFurniture.Add(*It);
	AllFurniture.Sort([](const ANoirSearchable& A, const ANoirSearchable& B) { return A.GetName() < B.GetName(); });
	TArray<ANoirStandPoint*> AllStands;
	for (TActorIterator<ANoirStandPoint> It(World); It; ++It) AllStands.Add(*It);
	AllStands.Sort([](const ANoirStandPoint& A, const ANoirStandPoint& B) { return A.GetName() < B.GetName(); });

	TMap<FString, ANoirSearchable*> FurnitureById;
	TMap<FString, ANoirRoom*> RoomById;
	TMap<FString, TArray<ANoirStandPoint*>> StandsByRoom;
	mystery::StageLayout Layout;
	for (ANoirRoom* Room : Rooms)
	{
		mystery::StageRoom StageRoom;
		StageRoom.id = NoirUtf8(Room->RoomId.ToString());
		StageRoom.canBeScene = Room->bCanBeScene;
		for (const FString& Label : Room->Suits) StageRoom.suits.push_back(NoirUtf8(Label));
		RoomById.Add(Room->RoomId.ToString(), Room);
		for (ANoirSearchable* F : AllFurniture)
		{
			if (!Room->Contains(F->GetActorLocation())) continue;
			StageRoom.furniture.push_back({NoirUtf8(F->GetName()), NoirUtf8(F->FurnitureKind.ToString())});
			FurnitureById.Add(F->GetName(), F);
			// The case decides what is hidden where.
			F->Items.Reset();
			F->SearchText = FText::GetEmpty();
			F->ClueText = FText::GetEmpty();
			F->DisplayName = FurnitureName(F->FurnitureKind, F->DisplayName, Theme);
		}
		for (ANoirStandPoint* Stand : AllStands)
		{
			if (Room->Contains(Stand->GetActorLocation())) StandsByRoom.FindOrAdd(Room->RoomId.ToString()).Add(Stand);
		}
		StageRoom.standPoints = StandsByRoom.FindOrAdd(Room->RoomId.ToString()).Num();
		Layout.rooms.push_back(StageRoom);
	}
	S.Staging = mystery::StageCase(*S.Content, S.Bp, Layout);
	for (const std::string& Problem : S.Staging.problems)
	{
		UE_LOG(LogNoirCase, Warning, TEXT("Staging: %s"), *NoirStr(Problem));
	}

	// ─── Rooms take the case's names ───
	for (ANoirRoom* Room : Rooms) Room->Label = FText::GetEmpty();
	for (const auto& [CaseRoom, RoomId] : S.Staging.roomFor)
	{
		const std::string PlanId = "room-" + std::to_string(CaseRoom);
		for (const mystery::PublicPlanRoom& PlanRoom : S.Case.plan.rooms)
		{
			if (PlanRoom.id == PlanId) RoomById.FindRef(NoirStr(RoomId))->Label = NoirText(PlanRoom.label);
		}
	}

	// ─── Searchable spots ───
	for (const mystery::SpotPlacement& Spot : S.Staging.spots)
	{
		const auto Result = mystery::SearchSpot(*S.Content, S.Bp, Spot.spotId, S.Lang);
		if (ANoirSearchable* F = FurnitureById.FindRef(NoirStr(Spot.furnitureId)); F && Result)
		{
			F->SearchText = NoirText(Result->text);
		}
	}

	// ─── The body ───
	ANoirRoom* SceneRoom = RoomById.FindRef(NoirStr(S.Staging.sceneRoomId));
	if (SceneRoom) SceneRoom->bPlaysScene = true;
	const FTransform Body = SceneRoom ? SceneRoom->BodyPoint * SceneRoom->GetActorTransform() : GetActorTransform();
	{
		UMaterialInterface* Sheet = NoirEvidenceModels::MakeMaterial(this, FLinearColor(0.32f, 0.3f, 0.26f), 0.95f);
		auto Prop = [&](const TCHAR* Mesh, const FVector& Offset, const FVector& Scale, const FRotator& Rotation)
		{
			const FTransform T(Rotation, Offset, Scale);
			AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), T * Body);
			A->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			A->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Mesh));
			A->GetStaticMeshComponent()->SetMaterial(0, Sheet);
			A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		};
		// A sheet over the victim: torso and legs along X, head at +X.
		Prop(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0, 0, 14), FVector(0.42f, 0.55f, 1.5f), FRotator(90, 0, 0));
		Prop(TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(90, 0, 12), FVector(0.26f, 0.24f, 0.22f), FRotator::ZeroRotator);
	}

	// ─── Evidence: physical items in furniture or by the body, the rest in the case file ───
	int32 BodyItems = 0;
	for (const mystery::PublicEvidence& E : S.Case.evidence)
	{
		const mystery::EvidencePlacement* Place = S.Staging.FindEvidence(E.id);
		FNoirClue Clue;
		Clue.Id = NoirStr(E.id);
		Clue.Title = NoirText(E.title);
		Clue.Text = NoirText(E.details);
		Clue.Puzzle = MakePuzzle(E);
		// Until it's worked out, a clue with a puzzle says what it is, not what it shows.
		mystery::PublicEvidence Shown = E;
		if (Clue.Puzzle.IsOpen())
		{
			Clue.Text = FText::Format(LOCTEXT("Unsolved", "{0} {1}"), NoirText(E.summary), PuzzleTeaser(Clue.Puzzle.Type));
			Shown.details = NoirUtf8(PuzzleTeaser(Clue.Puzzle.Type).ToString());
		}
		if (E.event && E.event->min >= 0) // weapons carry -1: found, not timed
		{
			Clue.Minute = E.event->min;
			Clue.TimeLabel = NoirText(mystery::FormatTime(E.event->min, S.Lang));
		}
		if (!Place || Place->where == mystery::EvidencePlacement::Where::CaseFile)
		{
			if (Place && Place->late) LateLead = Clue;
			else CaseFile.Add(Clue);
			continue;
		}

		FNoirEvidenceSpec Spec = NoirEvidenceModels::Build(this, *S.Content, S.Bp, Shown, S.Lang);
		FTransform Spawn;
		ANoirSearchable* Hidden = nullptr;
		if (Place->where == mystery::EvidencePlacement::Where::Body)
		{
			// Beside the body, a little apart from each other.
			Spawn = FTransform(FRotator(90.f, 25.f * BodyItems, 0.f), FVector(40.f + 25.f * BodyItems, 45.f, 3.f)) * Body;
			++BodyItems;
		}
		else if ((Hidden = FurnitureById.FindRef(NoirStr(Place->furnitureId))) != nullptr)
		{
			// Lies on top of the furniture once found.
			FVector Origin, Extent;
			Hidden->GetActorBounds(true, Origin, Extent);
			Spawn = FTransform(FRotator(90.f, 0.f, 0.f), Origin + FVector(0, 0, Extent.Z + 3.f));
		}
		ANoirEvidence* Item = World->SpawnActorDeferred<ANoirEvidence>(ANoirEvidence::StaticClass(), Spawn);
		Item->DisplayName = Spec.DisplayName;
		Item->Description = Spec.Description;
		Item->Details = Spec.Details;
		Item->Parts = Spec.Parts;
		Item->bHiddenUntilFound = Hidden != nullptr;
		Item->CaseEvidenceId = Clue.Id;
		Item->CardTitle = Clue.Title;
		Item->CardText = Clue.Text;
		Item->CardMinute = Clue.Minute;
		Item->CardTime = Clue.TimeLabel;
		Item->Puzzle = Clue.Puzzle;
		Item->FinishSpawning(Spawn);
		if (Hidden)
		{
			Hidden->Items.Add(Item);
		}
	}

	// ─── Suspects, where their alibis put them ───
	for (size_t i = 0; i < S.Case.suspects.size(); ++i)
	{
		const mystery::PublicSuspect& PS = S.Case.suspects[i];
		const mystery::SuspectPlacement& Place = S.Staging.suspects[i];
		ANoirRoom* Room = RoomById.FindRef(NoirStr(Place.roomId));
		const TArray<ANoirStandPoint*>& Stands = StandsByRoom.FindOrAdd(NoirStr(Place.roomId));
		FTransform Where;
		if (Stands.IsValidIndex(Place.standIdx))
		{
			Where = Stands[Place.standIdx]->GetActorTransform();
		}
		else if (Room)
		{
			// More suspects than stand points: a loose ring in the middle of the room.
			const float Angle = Place.standIdx * 2.1f;
			Where = FTransform(FRotator(0, FMath::RadiansToDegrees(Angle) + 180.f, 0),
				Room->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * 120.f - FVector(0, 0, Room->Bounds->GetScaledBoxExtent().Z));
		}
		Where.AddToTranslation(FVector(0, 0, 90.f));
		Where.SetScale3D(FVector::OneVector);
		ANoirSuspect* Suspect = World->SpawnActor<ANoirSuspect>(ANoirSuspect::StaticClass(), Where);
		Suspect->DisplayName = NoirText(PS.name);
		Suspect->Occupation = NoirText(PS.role);
		Suspect->Greeting = FText::GetEmpty();
		Suspect->Topics.Reset();
		for (const mystery::QA& Qa : PS.interview)
		{
			FNoirTopic& Topic = Suspect->Topics.AddDefaulted_GetRef();
			Topic.Question = NoirText(Qa.q);
			Topic.Answer = NoirText(Qa.a);
		}
		Suspect->SignatureColor = FLinearColor(FColor::FromHex(NoirStr(PS.color)));
		// Dressed as a witness would describe them: height and coat colour.
		FNoirLook Look;
		Look.bFemale = PS.look == "f";
		const auto Trait = [&PS](const char* Dim) { const auto It = PS.traitKeys.find(Dim); return It != PS.traitKeys.end() ? It->second : std::string(); };
		const std::string Height = Trait("height"), Coat = Trait("coat");
		Look.Height = Height == "tall" ? 1.06f : Height == "short" ? 0.93f : 1.f;
		Look.Coat = Coat == "black" ? FLinearColor(0.012f, 0.012f, 0.014f)
			: Coat == "grey" ? FLinearColor(0.09f, 0.09f, 0.085f)
			: Coat == "green" ? FLinearColor(0.02f, 0.05f, 0.03f)
			: Coat == "burgundy" ? FLinearColor(0.13f, 0.015f, 0.025f)
			: Coat == "camel" ? FLinearColor(0.3f, 0.19f, 0.09f)
			: FLinearColor(0.04f, 0.035f, 0.03f);
		Look.Accent = Suspect->SignatureColor;
		Look.Seed = static_cast<int32>(GetTypeHash(NoirStr(PS.name)));
		Suspect->Dress(Look);
		Suspect->BindToCase(this, NoirStr(PS.id));
		Suspects.Add(Suspect);
	}

	// ─── The case board: options, solution, and the true story ───
	if (Board)
	{
		const mystery::Solution& Sol = S.Bp.solution;
		Board->Title = NoirText(S.Case.title);
		Board->VictimName = NoirText(S.Case.victim.name);
		Board->Suspects.Reset();
		for (ANoirSuspect* Suspect : Suspects) Board->Suspects.Add(Suspect);
		Board->Killer = S.Bp.killerIdx;
		Board->bHasAccomplice = S.Bp.accompliceIdx.has_value();
		Board->Accomplice = S.Bp.accompliceIdx.value_or(0);
		Board->WrongAccusationLine = NoirTr(TEXT("The room goes quiet. Somewhere in the house, the real killer breathes easier."),
			TEXT("La sala se queda en silencio. En algún lugar de la casa, el verdadero asesino respira más tranquilo."));
		auto Options = [](const std::vector<mystery::Option>& From, const std::string& Answer, TArray<FText>& To, int32& AnswerIdx)
		{
			To.Reset();
			for (const mystery::Option& O : From)
			{
				if (O.id == Answer) AnswerIdx = To.Num();
				To.Add(NoirText(O.label));
			}
		};
		Options(S.Case.motiveOptions, Sol.motiveId, Board->Motives, Board->Motive);
		Options(S.Case.weaponOptions, Sol.weaponId, Board->Weapons, Board->Weapon);
		// Whose alibi is a lie: answered by pointing at the record (or receipt) that breaks it.
		Board->Alibis.Reset();
		for (const mystery::PublicEvidence& E : S.Case.evidence)
		{
			if (E.kind != "record" && E.kind != "receipt") continue;
			if (E.id == Sol.alibiEvidenceId) Board->BrokenAlibi = Board->Alibis.Num();
			Board->Alibis.Add(NoirText(E.title));
		}
		Board->RevealLines.Reset();
		Board->RevealLines.Add(NoirText(mystery::ExplainSolution(*S.Content, S.Bp, S.Lang)));
		Board->Timeline.Reset();
		for (const mystery::StoryEntry& Beat : mystery::StoryFor(*S.Content, S.Bp, S.Lang))
		{
			Board->RevealLines.Add(NoirText(Beat.text));
			FNoirTimelineEntry& Entry = Board->Timeline.AddDefaulted_GetRef();
			Entry.Time = NoirText(mystery::FormatTime(Beat.min, S.Lang));
			Entry.Event = NoirText(Beat.text);
		}
	}

	Briefing = NoirText(S.Case.title);
	UE_LOG(LogNoirCase, Display, TEXT("%s"), *DescribeStaging());
}

void ANoirCaseDirector::BriefDetective()
{
	ANoirCharacter* Detective = Cast<ANoirCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Detective || !State)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(BriefTimer);
	bBriefed = true;
	if (bResume)
	{
		ResumeCase();
		return;
	}
	Detective->AddClue(NoirText(State->Case.synopsis));
	if (State->Case.modifier)
	{
		Detective->AddClue(FText::Format(LOCTEXT("Modifier", "{0}: {1}"), NoirText(State->Case.modifier->label), NoirText(State->Case.modifier->note)));
	}
	for (const FNoirClue& Clue : CaseFile)
	{
		Detective->LearnClue(Clue);
	}
	Detective->ShowMessage(FText::Format(NoirTr(TEXT("{0}   (Tab: Mind Place    N: notes)"),
		TEXT("{0}   (Tab: palacio mental    N: notas)")), Briefing));
	if (LateLead)
	{
		GetWorldTimerManager().SetTimer(LateLeadTimer, this, &ANoirCaseDirector::DeliverLateLead, LateLeadSeconds, false);
	}
}

ANoirEvidence* ANoirCaseDirector::MakeLabItem(const FNoirClue& Clue, const FTransform& Where)
{
	if (!State || !State->Content) return nullptr;
	const FState& S = *State;
	for (const mystery::PublicEvidence& E : S.Case.evidence)
	{
		if (NoirStr(E.id) != Clue.Id) continue;
		mystery::PublicEvidence Shown = E;
		if (Clue.Puzzle.IsOpen()) Shown.details = NoirUtf8(PuzzleTeaser(Clue.Puzzle.Type).ToString());
		const FNoirEvidenceSpec Spec = NoirEvidenceModels::Build(this, *S.Content, S.Bp, Shown, S.Lang);
		ANoirEvidence* Item = GetWorld()->SpawnActorDeferred<ANoirEvidence>(ANoirEvidence::StaticClass(), Where);
		Item->DisplayName = Spec.DisplayName;
		Item->Description = Spec.Description;
		Item->Details = Spec.Details;
		Item->Parts = Spec.Parts;
		Item->bHiddenUntilFound = false;
		Item->CaseEvidenceId = Clue.Id;
		Item->CardTitle = Clue.Title;
		Item->CardText = Clue.Text;
		Item->CardMinute = Clue.Minute;
		Item->CardTime = Clue.TimeLabel;
		Item->Puzzle = Clue.Puzzle;
		Item->FinishSpawning(Where);
		return Item;
	}
	return nullptr;
}

bool ANoirCaseDirector::FindClue(const FString& Id, FNoirClue& Out) const
{
	if (const FNoirClue* C = CaseFile.FindByPredicate([&Id](const FNoirClue& K) { return K.Id == Id; }))
	{
		Out = *C;
		return true;
	}
	if (LateLead && LateLead->Id == Id)
	{
		Out = *LateLead;
		return true;
	}
	const ANoirMindPlace* Mind = ANoirMindPlace::Find(this);
	for (TActorIterator<ANoirEvidence> It(GetWorld()); It; ++It)
	{
		if (It->CaseEvidenceId != Id || (Mind && Mind->Contains(It->GetActorLocation()))) continue;
		Out = It->MakeClue();
		return true;
	}
	return false;
}

void ANoirCaseDirector::SaveCase()
{
	ANoirCharacter* Detective = Cast<ANoirCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Detective || !State || !bBriefed) // nothing to keep until the case has begun
	{
		return;
	}
	UNoirSaveGame* Save = NewObject<UNoirSaveGame>();
	Save->Seed = PlayedSeed;
	Save->Theme = Theme;
	Save->Difficulty = Difficulty;
	Save->bSpanish = Language == ENoirCaseLanguage::Spanish;
	Save->CaseTitle = Briefing.ToString();
	Save->Where = Detective->GetResumeTransform(Save->Looking);
	Save->bTorchOn = Detective->IsTorchOn();
	for (const FNoirClue& C : Detective->GetKnownClues())
	{
		Save->KnownClues.Add(C.Id);
		if (!C.Puzzle.IsOpen() && !C.Puzzle.Reveal.IsEmpty()) Save->SolvedPuzzles.Add(C.Id);
	}
	for (const FText& Note : Detective->GetNotes()) Save->Notes.Add(Note.ToString());
	const FTimerManager& Timers = GetWorldTimerManager();
	Save->LateLeadIn = Timers.TimerExists(LateLeadTimer) ? Timers.GetTimerRemaining(LateLeadTimer)
		: (LateLead && !bLateLeadDelivered ? LateLeadSeconds : -1.f);
	for (TActorIterator<ANoirSearchable> It(GetWorld()); It; ++It)
	{
		if (It->IsSearched()) Save->Searched.Add(It->GetName());
	}
	for (const ANoirSuspect* Suspect : Suspects)
	{
		if (Suspect) Save->Suspects.Add(Suspect->SaveState());
	}
	if (Board)
	{
		Board->GetArrangement(Save->MovedCards, Save->MovedTo, Save->StringsA, Save->StringsB);
	}
	Save->Write();
	UE_LOG(LogNoirCase, Display, TEXT("Saved case %s: at %s, %d clues (%d puzzles solved), %d notes, %d searched, %d suspects, %d moved cards, %d strings, late lead in %.0fs"),
		*Save->Seed, *Save->Where.GetLocation().ToCompactString(), Save->KnownClues.Num(), Save->SolvedPuzzles.Num(), Save->Notes.Num(),
		Save->Searched.Num(), Save->Suspects.Num(), Save->MovedCards.Num(), Save->StringsA.Num(), Save->LateLeadIn);
}

void ANoirCaseDirector::ResumeCase()
{
	ANoirCharacter* Detective = Cast<ANoirCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const UNoirSaveGame* Save = UNoirSaveGame::Load();
	if (!Detective || !Save || Save->Seed != PlayedSeed)
	{
		bResume = false;
		BriefDetective();
		return;
	}
	Detective->TeleportTo(Save->Where.GetLocation(), Save->Where.Rotator());
	if (APlayerController* PC = Cast<APlayerController>(Detective->GetController())) PC->SetControlRotation(Save->Looking);
	Detective->SetTorchOn(Save->bTorchOn);

	TArray<FNoirClue> Known;
	for (const FString& Id : Save->KnownClues)
	{
		FNoirClue Clue;
		if (!FindClue(Id, Clue)) continue;
		if (Save->SolvedPuzzles.Contains(Id) && Clue.Puzzle.IsOpen())
		{
			Clue.Text = Clue.Puzzle.Reveal;
			Clue.Puzzle.Type = NAME_None;
		}
		Known.Add(Clue);
	}
	TArray<FText> Notes;
	for (const FString& Note : Save->Notes) Notes.Add(FText::FromString(Note));
	Detective->RestoreKnowledge(Known, Notes);

	for (TActorIterator<ANoirSearchable> It(GetWorld()); It; ++It)
	{
		if (Save->Searched.Contains(It->GetName())) It->RestoreSearched();
	}
	for (ANoirSuspect* Suspect : Suspects)
	{
		if (!Suspect) continue;
		const FNoirSuspectSave* S = Save->Suspects.FindByPredicate([Suspect](const FNoirSuspectSave& X) { return X.Name == Suspect->GetName(); });
		if (S) Suspect->LoadState(*S);
	}
	if (Board)
	{
		Board->SyncCards(Known);
		Board->Arrange(Save->MovedCards, Save->MovedTo, Save->StringsA, Save->StringsB);
	}
	bLateLeadDelivered = Save->LateLeadIn < 0.f;
	if (LateLead && Save->LateLeadIn >= 0.f)
	{
		GetWorldTimerManager().SetTimer(LateLeadTimer, this, &ANoirCaseDirector::DeliverLateLead, FMath::Max(Save->LateLeadIn, 1.f), false);
	}
	int32 Searched = 0, Cracked = 0;
	for (TActorIterator<ANoirSearchable> It(GetWorld()); It; ++It) Searched += It->IsSearched();
	int32 Solved = 0;
	for (const FNoirClue& C : Known) Solved += !C.Puzzle.IsOpen() && !C.Puzzle.Reveal.IsEmpty();
	for (const FNoirSuspectSave& S : Save->Suspects) Cracked += S.bCracked;
	UE_LOG(LogNoirCase, Display, TEXT("Resumed case %s: at %s, %d clues (%d puzzles solved), %d notes, %d searched, %d suspects cracked"),
		*Save->Seed, *Detective->GetActorLocation().ToCompactString(), Detective->GetKnownClues().Num(), Solved, Detective->GetNotes().Num(),
		Searched, Cracked);
	Detective->ShowMessage(FText::Format(NoirTr(TEXT("{0}   (case resumed)"), TEXT("{0}   (caso retomado)")), Briefing));
}

void ANoirCaseDirector::SetCaseClockPaused(bool bPaused)
{
	FTimerManager& Timers = GetWorldTimerManager();
	if (!Timers.TimerExists(LateLeadTimer))
	{
		return;
	}
	if (bPaused)
	{
		Timers.PauseTimer(LateLeadTimer);
	}
	else
	{
		Timers.UnPauseTimer(LateLeadTimer);
	}
}

void ANoirCaseDirector::DeliverLateLead()
{
	ANoirCharacter* Detective = Cast<ANoirCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	bLateLeadDelivered = true;
	if (Detective && LateLead && Detective->LearnClue(*LateLead))
	{
		Detective->ShowMessage(FText::Format(NoirTr(TEXT("A new lead: {0}"), TEXT("Nueva pista: {0}")), LateLead->Title));
	}
}

FNoirConfrontResult ANoirCaseDirector::Confront(const FString& SuspectId, const FString& EvidenceId) const
{
	FNoirConfrontResult Result;
	if (!State)
	{
		return Result;
	}
	const auto Outcome = mystery::Confront(*State->Content, State->Bp, NoirUtf8(SuspectId), NoirUtf8(EvidenceId), State->Lang);
	if (!Outcome)
	{
		return Result;
	}
	Result.bCracked = Outcome->cracked;
	Result.Line = NoirText(Outcome->line);
	if (Outcome->followUp)
	{
		Result.FollowUpQuestion = NoirText(Outcome->followUp->q);
		Result.FollowUpAnswer = NoirText(Outcome->followUp->a);
	}
	return Result;
}

FText ANoirCaseDirector::RoomLabelAt(const FVector& Location) const
{
	for (const ANoirRoom* Room : Rooms)
	{
		if (Room && Room->Contains(Location)) return Room->Label;
	}
	return FText::GetEmpty();
}

FString ANoirCaseDirector::DescribeStaging() const
{
	if (!State)
	{
		return TEXT("(no case)");
	}
	const FState& S = *State;
	FString Out = FString::Printf(TEXT("Staging for seed %s:"), *PlayedSeed);
	for (const auto& [CaseRoom, RoomId] : S.Staging.roomFor)
	{
		Out += FString::Printf(TEXT("\n  case room %d -> %s"), CaseRoom, *NoirStr(RoomId));
	}
	for (const mystery::SpotPlacement& Spot : S.Staging.spots)
	{
		Out += FString::Printf(TEXT("\n  %s -> %s"), *NoirStr(Spot.spotId), *NoirStr(Spot.furnitureId));
	}
	for (const mystery::EvidencePlacement& P : S.Staging.evidence)
	{
		const TCHAR* Where = P.where == mystery::EvidencePlacement::Where::Body ? TEXT("body")
			: P.where == mystery::EvidencePlacement::Where::Furniture ? TEXT("furniture") : TEXT("case file");
		Out += FString::Printf(TEXT("\n  %s -> %s %s%s"), *NoirStr(P.evidenceId), Where, *NoirStr(P.furnitureId), P.late ? TEXT(" (late)") : TEXT(""));
	}
	for (const mystery::SuspectPlacement& P : S.Staging.suspects)
	{
		Out += FString::Printf(TEXT("\n  %s -> %s #%d"), *NoirStr(P.suspectId), *NoirStr(P.roomId), P.standIdx);
	}
	return Out;
}

#undef LOCTEXT_NAMESPACE
