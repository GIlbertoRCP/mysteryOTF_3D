#include "NoirSuspect.h"

#include "TimerManager.h"

#include "NoirSave.h"

#include "NoirCaseContent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "NoirCharacter.h"
#include "NoirCaseDirector.h"
#include "NoirEvidence.h"
#include "NoirEvidenceModels.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "NoirSuspect"

ANoirSuspect::ANoirSuspect()
{
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(30.f, 88.f);
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = Capsule;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// A rough standing figure, 1.75 m: body from the floor to the shoulders, head above.
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetRelativeLocation(FVector(0.f, 0.f, -15.f));
	Body->SetRelativeScale3D(FVector(0.42f, 0.32f, 1.46f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Capsule);
	Head->SetStaticMesh(Sphere.Object);
	Head->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	Head->SetRelativeScale3D(FVector(0.2f, 0.18f, 0.24f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Figure = CreateDefaultSubobject<USceneComponent>(TEXT("Figure"));
	Figure->SetupAttachment(Capsule);
	Figure->SetRelativeLocation(FVector(0.f, 0.f, -88.f)); // the floor
	HeadPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HeadPivot"));
	HeadPivot->SetupAttachment(Figure);
	HeadPivot->SetRelativeLocation(FVector(0.f, 0.f, 156.f));

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	DefaultReaction = NoirTr(TEXT("I don't see what that has to do with me."), TEXT("No veo qué tiene eso que ver conmigo."));
}

FText ANoirSuspect::GetInteractPrompt_Implementation() const
{
	return FText::Format(NoirTr(TEXT("Question {0}"), TEXT("Interrogar a {0}")), DisplayName);
}

void ANoirSuspect::Interact_Implementation(APawn* InstigatorPawn)
{
	if (ANoirCharacter* Detective = Cast<ANoirCharacter>(InstigatorPawn))
	{
		Detective->BeginInterview(this);
	}
}

TArray<int32> ANoirSuspect::GetAvailableTopics() const
{
	TArray<int32> Available;
	for (int32 i = 0; i < Topics.Num(); ++i)
	{
		if (!Topics[i].bRequiresCracked || bCracked)
		{
			Available.Add(i);
		}
	}
	return Available;
}

FText ANoirSuspect::Ask(int32 TopicIndex, ANoirCharacter* Detective)
{
	if (!Topics.IsValidIndex(TopicIndex))
	{
		return FText::GetEmpty();
	}
	AskedTopics.Add(TopicIndex);
	return Topics[TopicIndex].Answer;
}

bool ANoirSuspect::HasStatements() const
{
	return Director.IsValid() && Topics.ContainsByPredicate([](const FNoirTopic& T) { return !T.StatementId.IsNone(); });
}

TArray<int32> ANoirSuspect::GetChallengeableTopics() const
{
	TArray<int32> Out;
	for (int32 i = 0; i < Topics.Num(); ++i)
	{
		if (!Topics[i].StatementId.IsNone() && AskedTopics.Contains(i)) Out.Add(i);
	}
	return Out;
}

FText ANoirSuspect::Challenge(int32 Topic, const FNoirClue& Clue, ANoirCharacter* Detective)
{
	ANoirCaseDirector* Case = Director.Get();
	if (!Case || !Topics.IsValidIndex(Topic) || Topics[Topic].StatementId.IsNone())
	{
		return Present(Clue, Detective);
	}
	const FName SuspectId(*CaseSuspectId), ClueId(*Clue.Id);
	if (Case->EvaluateContradiction(SuspectId, Topics[Topic].StatementId, ClueId))
	{
		if (Detective && !bCracked)
		{
			Detective->ShowMessage(FText::Format(NoirTr(TEXT("Contradiction: {0}"), TEXT("Contradicción: {0}")), Topics[Topic].Question));
		}
		return Present(Clue, Detective);
	}
	bool bAlready = false;
	Challenges.Add(FString::FromInt(Topic) + TEXT("|") + Clue.Id, &bAlready);
	if (!bAlready && !bCracked)
	{
		Stress = FMath::Min(Stress + 0.1f, 0.9f);
		Composure = FMath::Clamp(1.f - Stress, 0.f, 1.f);
	}
	if (Case->EvaluateContradiction(SuspectId, FName(TEXT("any")), ClueId))
	{
		// The right clue, aimed at the wrong thing they said: close.
		return NoirTr(TEXT("You're twisting my words. That has nothing to do with what I told you."),
			TEXT("Está tergiversando mis palabras. Eso no tiene nada que ver con lo que le dije."));
	}
	return Case->Confront(CaseSuspectId, Clue.Id).Line;
}

FNoirSuspectSave ANoirSuspect::SaveState() const
{
	FNoirSuspectSave S;
	S.Name = GetName();
	S.Stress = Stress;
	S.bCracked = bCracked;
	S.AskedTopics = AskedTopics.Array();
	S.ShownClues = ShownClues.Array();
	for (const int32 i : FollowUpTopics)
	{
		if (!Topics.IsValidIndex(i)) continue;
		S.FollowUpQuestions.Add(Topics[i].Question.ToString());
		S.FollowUpAnswers.Add(Topics[i].Answer.ToString());
	}
	return S;
}

void ANoirSuspect::LoadState(const FNoirSuspectSave& Saved)
{
	Stress = Saved.Stress;
	bCracked = Saved.bCracked;
	for (int32 k = 0; k < Saved.FollowUpQuestions.Num() && k < Saved.FollowUpAnswers.Num(); ++k)
	{
		FollowUpTopics.Add(Topics.Num());
		FNoirTopic& T = Topics.AddDefaulted_GetRef();
		T.Question = FText::FromString(Saved.FollowUpQuestions[k]);
		T.Answer = FText::FromString(Saved.FollowUpAnswers[k]);
	}
	AskedTopics = TSet<int32>(Saved.AskedTopics);
	ShownClues = TSet<FString>(Saved.ShownClues);
}

void ANoirSuspect::BindToCase(ANoirCaseDirector* InDirector, const FString& InCaseSuspectId)
{
	Director = InDirector;
	CaseSuspectId = InCaseSuspectId;
}

FText ANoirSuspect::Present(const FNoirClue& Clue, ANoirCharacter* Detective)
{
	bool bAlreadyShown = false;
	ShownClues.Add(Clue.Id, &bAlreadyShown);

	if (ANoirCaseDirector* Case = Director.Get())
	{
		// Generated case: the right clue makes them crack and opens a follow-up question;
		// anything else is brushed off and only rattles them a little.
		const FNoirConfrontResult Result = Case->Confront(CaseSuspectId, Clue.Id);
		if (Result.bCracked)
		{
			bCracked = true;
			Stress = 1.f;
			Composure = 0.f;
			if (!Result.FollowUpQuestion.IsEmpty())
			{
				FNoirTopic* Existing = Topics.FindByPredicate(
					[&Result](const FNoirTopic& T) { return T.Question.EqualTo(Result.FollowUpQuestion); });
				if (!Existing) FollowUpTopics.Add(Topics.Num());
				FNoirTopic& FollowUp = Existing ? *Existing : Topics.AddDefaulted_GetRef();
				FollowUp.Question = Result.FollowUpQuestion;
				FollowUp.Answer = Result.FollowUpAnswer;
			}
			if (Detective && !bAlreadyShown)
			{
				Detective->AddClue(FText::Format(NoirTr(TEXT("{0}, shown {1}: \"{2}\""), TEXT("{0}, ante {1}: «{2}»")), DisplayName, Clue.Title, Result.Line));
			}
		}
		else if (!bAlreadyShown && !bCracked)
		{
			Stress = FMath::Min(Stress + 0.15f, 0.9f);
			Composure = FMath::Clamp(1.0f - Stress, 0.0f, 1.0f);
		}
		return Result.Line;
	}

	const FNoirEvidenceReaction* Reaction = Reactions.FindByPredicate(
		[&Clue](const FNoirEvidenceReaction& R) { return R.Evidence && R.Evidence == Clue.Actor.Get(); });
	if (!Reaction)
	{
		return DefaultReaction;
	}

	if (!bAlreadyShown && !bCracked)
	{
		Stress = FMath::Clamp(Stress + Reaction->Stress, 0.f, 1.f);
		Composure = FMath::Clamp(1.0f - Stress, 0.0f, 1.0f);
		if (Stress >= 1.f)
		{
			bCracked = true;
			if (Detective && !CrackedClue.IsEmpty())
			{
				Detective->AddClue(CrackedClue);
			}
			return FText::Format(LOCTEXT("Cracks", "{0}\n{1}"), Reaction->Response, CrackedLine);
		}
	}
	return Reaction->Response;
}

bool ANoirSuspect::EvaluateContradiction(FName StatementId, FName EvidenceId)
{
	if (ANoirCaseDirector* Case = Director.Get())
	{
		return Case->EvaluateContradiction(FName(*CaseSuspectId), StatementId, EvidenceId);
	}

	const FString EvStr = EvidenceId.ToString();
	for (const FNoirEvidenceReaction& R : Reactions)
	{
		if (R.Evidence && (R.Evidence->GetName().Contains(EvStr) || R.Evidence->GetActorLabel().Contains(EvStr)))
		{
			return R.Stress > 0.4f;
		}
	}
	return false;
}


UStaticMeshComponent* ANoirSuspect::Part(USceneComponent* Parent, const TCHAR* Shape, const FVector& At, const FVector& SizeCm,
	UMaterialInterface* Material, const FRotator& Rotation)
{
	UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
	M->SetupAttachment(Parent);
	M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape)));
	M->SetRelativeLocationAndRotation(At, Rotation);
	M->SetRelativeScale3D(SizeCm / 100.f);
	M->SetMaterial(0, Material);
	M->OverrideMaterials = { Material };
	M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M->RegisterComponent();
	DressParts.Add(M);
	return M;
}

void ANoirSuspect::BeginPlay()
{
	Super::BeginPlay();
	// Suspects placed by hand (the look-dev room) dress themselves; a case director dresses the
	// ones it spawns right after spawning them, so wait a tick to see whether that happens.
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (bDressed) return;
		FNoirLook Look;
		const FString NameStr = DisplayName.ToString();
		Look.bFemale = NameStr.Contains(TEXT("Margaret")) || NameStr.Contains(TEXT("Lady")) || NameStr.Contains(TEXT("Miss")) || NameStr.Contains(TEXT("Mrs"));
		Look.Height = 1.0f;
		Look.Coat = SignatureColor.GetLuminance() > 0.05f ? SignatureColor : FLinearColor(0.04f, 0.035f, 0.03f);
		Look.Accent = SignatureColor;
		Look.Seed = static_cast<int32>(GetTypeHash(NameStr.IsEmpty() ? GetName() : NameStr));
		Dress(Look);
	}));
}

void ANoirSuspect::Dress(const FNoirLook& Look)
{
	// Dressing again replaces the figure (never two coats on one suspect).
	for (UStaticMeshComponent* Old : DressParts)
	{
		if (Old) Old->DestroyComponent();
	}
	DressParts.Reset();
	// Skin and hair from the suspect's name, the coat from what a witness could describe.
	static const FLinearColor Skins[] = {FLinearColor(0.42f, 0.28f, 0.2f), FLinearColor(0.32f, 0.2f, 0.13f), FLinearColor(0.2f, 0.12f, 0.07f),
		FLinearColor(0.48f, 0.34f, 0.26f)};
	static const FLinearColor Hairs[] = {FLinearColor(0.02f, 0.015f, 0.01f), FLinearColor(0.08f, 0.04f, 0.02f), FLinearColor(0.2f, 0.12f, 0.05f),
		FLinearColor(0.25f, 0.24f, 0.22f)};
	const uint32 H = static_cast<uint32>(Look.Seed);
	auto Mat = [this](const FLinearColor& C, float Rough) { return NoirEvidenceModels::MakeMaterial(this, C, Rough); };

	UMaterialInterface* WoolParent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_Suspect_Margaret.MI_Suspect_Margaret"));
	auto WoolMat = [this, WoolParent, &Mat](const FLinearColor& C, float Rough) -> UMaterialInterface* {
		if (WoolParent)
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(WoolParent, this);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), C);
			Mid->SetScalarParameterValue(TEXT("Roughness"), 1.f); // the texture carries the roughness
			return Mid;
		}
		return Mat(C, Rough);
	};

	UMaterialInterface* LeatherParent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_Leather_Green.MI_Leather_Green"));
	auto LeatherMat = [this, LeatherParent, &Mat](const FLinearColor& C, float Rough) -> UMaterialInterface* {
		if (LeatherParent)
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(LeatherParent, this);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), C);
			Mid->SetScalarParameterValue(TEXT("Roughness"), 1.f); // the texture carries the roughness
			return Mid;
		}
		return Mat(C, Rough);
	};

	UMaterialInterface* Coat = WoolMat(Look.Coat, 0.6f);
	UMaterialInterface* CoatDark = WoolMat(Look.Coat * 0.55f, 0.6f);
	UMaterialInterface* Trousers = WoolMat(FLinearColor(0.04f, 0.035f, 0.03f), 0.7f);

	UMaterialInterface* SkinParent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_Skin_Placeholder.MI_Skin_Placeholder"));
	UMaterialInstanceDynamic* SkinMid = SkinParent ? UMaterialInstanceDynamic::Create(SkinParent, this) : nullptr;
	if (SkinMid)
	{
		SkinMid->SetVectorParameterValue(TEXT("BaseColor"), Skins[H % 4]);
		SkinMid->SetScalarParameterValue(TEXT("Roughness"), 1.f); // the texture carries the roughness
	}
	UMaterialInterface* Skin = SkinMid ? SkinMid : Mat(Skins[H % 4], 0.5f);
	UMaterialInterface* Hair = Mat(Hairs[(H / 4) % 4], 0.6f);
	UMaterialInterface* Accent = WoolMat(Look.Accent * 0.8f, 0.5f);
	UMaterialInterface* Shoe = LeatherMat(FLinearColor(0.12f, 0.09f, 0.07f), 0.35f);
	UMaterialInterface* Felt = WoolMat(FLinearColor(0.05f, 0.045f, 0.04f), 0.8f);

	// The plain placeholder makes way.
	if (IsValid(Body)) Body->SetVisibility(false);
	if (IsValid(Head)) Head->SetVisibility(false);

	const bool F = Look.bFemale;
	for (const float Side : {-1.f, 1.f})
	{
		Part(Figure, TEXT("Cube"), FVector(3, Side * (F ? 8 : 10), F ? 3.5f : 4), F ? FVector(22, 9, 7) : FVector(28, 11, 8), Shoe);
		Part(Figure, TEXT("Cylinder"), FVector(0, Side * (F ? 8 : 10), F ? 20 : 46), F ? FVector(10, 10, 36) : FVector(14, 14, 84),
			F ? Mat(Skins[H % 4] * 0.55f, 0.5f) : Trousers);
		// Arms hang a little out from the coat; hands at the cuffs.
		Part(Figure, TEXT("Cylinder"), FVector(0, Side * (F ? 21 : 23.5f), 108), FVector(9, 9.5f, 54), Coat, FRotator(0, 0, Side * 5.f));
		Part(Figure, TEXT("Sphere"), FVector(2, Side * (F ? 25 : 28), 80), FVector(9, 8, 11), Skin);
	}
	// The coat: long and flared below the belt, then the chest and the shoulders.
	Body = Part(Figure, TEXT("Cylinder"), F ? FVector(0, 0, 62) : FVector(0, 0, 72), F ? FVector(48, 36, 80) : FVector(44, 32, 62), Coat);
	Part(Figure, TEXT("Cylinder"), FVector(0, 0, 122), F ? FVector(36, 26, 42) : FVector(40, 28, 42), Coat);
	Part(Figure, TEXT("Cylinder"), FVector(0, 0, 104), F ? FVector(40, 29, 4) : FVector(45, 33, 4), CoatDark);
	Part(Figure, TEXT("Cube"), FVector(F ? 13.5f : 14.5f, 0, 118), FVector(2, 5, 30), CoatDark);
	Part(Figure, TEXT("Sphere"), FVector(0, 0, 141), F ? FVector(40, 26, 9) : FVector(46, 29, 10), Coat);
	Part(Figure, TEXT("Cylinder"), FVector(0, 0, 150), FVector(10, 10, 10), Skin);
	Part(Figure, TEXT("Cylinder"), FVector(0, 0, 146.5f), FVector(22, 22, 7), Accent);

	// The head turns on its own.
	Head = Part(HeadPivot, TEXT("Sphere"), FVector(1, 0, 11), FVector(19, 17, 23), Skin);
	Part(HeadPivot, TEXT("Sphere"), FVector(9.5f, 0, 9), FVector(4, 3.5f, 5), Skin); // the nose, so you can tell which way they face
	for (const float Side : {-1.f, 1.f}) Part(HeadPivot, TEXT("Sphere"), FVector(0, Side * 9, 11), FVector(3, 3, 5), Skin);
	if (F)
	{
		Part(HeadPivot, TEXT("Sphere"), FVector(-3, 0, 13), FVector(21, 19, 22), Hair);
		Part(HeadPivot, TEXT("Sphere"), FVector(-11, 0, 8), FVector(9, 9, 9), Hair);
		Part(HeadPivot, TEXT("Sphere"), FVector(0, 0, 18), FVector(22, 21, 13), Accent); // a cloche
	}
	else
	{
		Part(HeadPivot, TEXT("Sphere"), FVector(-2, 0, 15), FVector(19.5f, 17.5f, 16), Hair);
		Part(HeadPivot, TEXT("Cylinder"), FVector(0, 0, 21), FVector(34, 32, 1.5f), Felt); // a fedora
		Part(HeadPivot, TEXT("Cylinder"), FVector(0, 0, 26), FVector(20, 18, 11), Felt);
		Part(HeadPivot, TEXT("Cylinder"), FVector(0, 0, 22.5f), FVector(20.6f, 18.6f, 2.5f), Accent);
	}

	FigureScale = FVector(Look.Height * (F ? 0.96f : 1.f));
	Figure->SetRelativeScale3D(FigureScale);
	Phase = (H % 1000) * 0.0063f;
	bDressed = true;
	SetActorTickEnabled(true);
}

void ANoirSuspect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	// Breathing, and a slow shift of weight from foot to foot.
	Figure->SetRelativeScale3D(FigureScale * FVector(1.f, 1.f, 1.f + 0.006f * FMath::Sin(Time * 1.6f + Phase)));
	float Yaw = Figure->GetRelativeRotation().Yaw;

	const APawn* Detective = UGameplayStatics::GetPlayerPawn(this, 0);
	const ANoirCharacter* Noir = Cast<ANoirCharacter>(Detective);
	float TargetYaw = 0.f, HeadYaw = 0.f;
	if (Detective)
	{
		const FVector To = GetActorTransform().InverseTransformPosition(Detective->GetActorLocation());
		const float Toward = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
		// In an interview they turn to face the detective; otherwise only the head follows, up close.
		if (Noir && Noir->GetInterviewee() == this) TargetYaw = Toward;
		if (To.Size2D() < 450.f) HeadYaw = FMath::Clamp(FMath::FindDeltaAngleDegrees(TargetYaw, Toward), -65.f, 65.f);
	}
	Yaw = FMath::FInterpTo(Yaw, TargetYaw, DeltaSeconds, 2.5f);
	Figure->SetRelativeRotation(FRotator(0.f, Yaw, 0.8f * FMath::Sin(Time * 0.5f + Phase)));
	const float Current = HeadPivot->GetRelativeRotation().Yaw;
	HeadPivot->SetRelativeRotation(FRotator(0.f, FMath::FInterpTo(Current, HeadYaw, DeltaSeconds, 3.f), 0.f));
}

FVector ANoirSuspect::GetFaceLocation() const
{
	return Head->GetComponentLocation();
}

#undef LOCTEXT_NAMESPACE
