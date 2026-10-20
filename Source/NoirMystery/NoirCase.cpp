#include "NoirCase.h"

#include "NoirCaseContent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NoirClue.h"
#include "NoirEvidenceModels.h"
#include "NoirCharacter.h"
#include "NoirSuspect.h"

#define LOCTEXT_NAMESPACE "NoirCase"

ANoirCase::ANoirCase()
{
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetBoxExtent(FVector(10.f, 120.f, 60.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	Bounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = Bounds;

	// Standing back from the board, looking at it: the view while pinning and tying cards.
	BoardView = CreateDefaultSubobject<UCameraComponent>(TEXT("BoardView"));
	BoardView->SetupAttachment(Bounds);
	BoardView->SetRelativeLocationAndRotation(FVector(210.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));
	BoardView->SetFieldOfView(70.f);

	WrongAccusationLine = NoirTr(TEXT("The room goes quiet. Somewhere in the house, the real killer breathes easier."),
		TEXT("La sala se queda en silencio. En algún lugar de la casa, el verdadero asesino respira más tranquilo."));
}

FText ANoirCase::GetInteractPrompt_Implementation() const
{
	return NoirTr(TEXT("Study the case board and make an accusation"), TEXT("Estudiar el tablero del caso y acusar"));
}

void ANoirCase::Interact_Implementation(APawn* InstigatorPawn)
{
	if (ANoirCharacter* Detective = Cast<ANoirCharacter>(InstigatorPawn))
	{
		Detective->BeginBoard(this);
	}
}

FText ANoirCase::GetQuestion(ENoirAccusationPart Part) const
{
	switch (Part)
	{
	case ENoirAccusationPart::Killer: return FText::Format(NoirTr(TEXT("Who killed {0}?"), TEXT("¿Quién mató a {0}?")), VictimName);
	case ENoirAccusationPart::Motive: return NoirTr(TEXT("Why?"), TEXT("¿Por qué?"));
	case ENoirAccusationPart::Weapon: return NoirTr(TEXT("With what?"), TEXT("¿Con qué?"));
	case ENoirAccusationPart::BrokenAlibi: return NoirTr(TEXT("Whose alibi is a lie?"), TEXT("¿Qué coartada es falsa?"));
	case ENoirAccusationPart::Accomplice: return NoirTr(TEXT("Who covered for them?"), TEXT("¿Quién le encubrió?"));
	default: return FText::GetEmpty();
	}
}

TArray<FText> ANoirCase::GetOptions(ENoirAccusationPart Part) const
{
	switch (Part)
	{
	case ENoirAccusationPart::Killer:
	case ENoirAccusationPart::Accomplice:
	{
		TArray<FText> Names;
		for (const ANoirSuspect* Suspect : Suspects)
		{
			Names.Add(Suspect ? Suspect->DisplayName : FText::GetEmpty());
		}
		return Names;
	}
	case ENoirAccusationPart::Motive: return Motives;
	case ENoirAccusationPart::Weapon: return Weapons;
	case ENoirAccusationPart::BrokenAlibi: return Alibis;
	default: return {};
	}
}

int32 ANoirCase::GetSolution(ENoirAccusationPart Part) const
{
	switch (Part)
	{
	case ENoirAccusationPart::Killer: return Killer;
	case ENoirAccusationPart::Motive: return Motive;
	case ENoirAccusationPart::Weapon: return Weapon;
	case ENoirAccusationPart::BrokenAlibi: return BrokenAlibi;
	case ENoirAccusationPart::Accomplice: return bHasAccomplice ? Accomplice : INDEX_NONE;
	default: return INDEX_NONE;
	}
}

TArray<FText> ANoirCase::GetRevealLines(const TArray<int32>& Picks) const
{
	const int32 Accused = Picks.IsValidIndex(0) ? Picks[0] : INDEX_NONE;
	if (Accused == Killer)
	{
		return RevealLines;
	}
	const FText Name = Suspects.IsValidIndex(Accused) && Suspects[Accused] ? Suspects[Accused]->DisplayName : FText::GetEmpty();
	return {
		FText::Format(NoirTr(TEXT("You name {0}. They hold your gaze, and say nothing."),
			TEXT("Señalas a {0}. Te sostiene la mirada y no dice nada.")), Name),
		WrongAccusationLine,
	};
}

// ─── The physical board ───

namespace
{
	UStaticMesh* BasicShape(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}

	/** Breaks a card title into short lines for the text on the paper (TextRender's <br>). */
	FString WrapForCard(const FString& Text, int32 Chars, int32 MaxLines)
	{
		TArray<FString> Words;
		Text.ParseIntoArrayWS(Words);
		TArray<FString> Lines;
		FString Line;
		for (const FString& Word : Words)
		{
			if (!Line.IsEmpty() && Line.Len() + 1 + Word.Len() > Chars)
			{
				Lines.Add(Line);
				Line.Reset();
			}
			Line += Line.IsEmpty() ? Word : TEXT(" ") + Word;
		}
		if (!Line.IsEmpty()) Lines.Add(Line);
		if (Lines.Num() > MaxLines)
		{
			Lines.SetNum(MaxLines);
			Lines.Last() += TEXT("...");
		}
		return FString::Join(Lines, TEXT("<br>"));
	}

	UStaticMeshComponent* NewMesh(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Material)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Mesh);
		C->SetMaterial(0, Material);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetupAttachment(Parent);
		C->RegisterComponent();
		return C;
	}
}

void ANoirCase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateBoardView();
}

void ANoirCase::BeginPlay()
{
	Super::BeginPlay();
	UpdateBoardView(); // the level may have set BoardSize after construction
}

void ANoirCase::UpdateBoardView()
{
	// Far enough back that the board fits a 16:9 view at this field of view, with a margin.
	const float HalfTan = FMath::Tan(FMath::DegreesToRadians(BoardView->FieldOfView * 0.5f));
	const float Distance = 1.08f * FMath::Max(BoardSize.X / (2.f * HalfTan), BoardSize.Y / (2.f * HalfTan * 9.f / 16.f));
	BoardView->SetRelativeLocationAndRotation(FVector(Bounds->GetUnscaledBoxExtent().X + Distance, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));
}

FVector2D ANoirCase::CardSize() const
{
	const float W = FMath::Min(34.f, BoardSize.X / 7.f);
	return FVector2D(W, W * 0.68f);
}

FVector ANoirCase::BoardToLocal(const FVector2D& Position, float Lift) const
{
	return FVector(Bounds->GetUnscaledBoxExtent().X + Lift, Position.X, Position.Y);
}

bool ANoirCase::RayToBoard(const FVector& Origin, const FVector& Direction, FVector2D& OutPosition) const
{
	const FTransform& T = Bounds->GetComponentTransform();
	const FVector O = T.InverseTransformPosition(Origin);
	const FVector D = T.InverseTransformVectorNoScale(Direction);
	const float Face = Bounds->GetUnscaledBoxExtent().X;
	if (FMath::IsNearlyZero(D.X))
	{
		return false;
	}
	const float Along = (Face - O.X) / D.X;
	if (Along <= 0.f)
	{
		return false;
	}
	const FVector Hit = O + D * Along;
	OutPosition = FVector2D(Hit.Y, Hit.Z);
	return true;
}

FString ANoirCase::CardAt(const FVector2D& Position) const
{
	const FVector2D Half = CardSize() * 0.5f;
	for (int32 i = Cards.Num() - 1; i >= 0; --i) // later cards sit on top
	{
		const FVector2D D = Position - Cards[i].Position;
		if (FMath::Abs(D.X) <= Half.X && FMath::Abs(D.Y) <= Half.Y)
		{
			return Cards[i].Id;
		}
	}
	return FString();
}

const FNoirBoardCard* ANoirCase::FindCard(const FString& Id) const
{
	return Cards.FindByPredicate([&Id](const FNoirBoardCard& C) { return C.Id == Id; });
}

FNoirBoardCard* ANoirCase::FindCardMutable(const FString& Id)
{
	return Cards.FindByPredicate([&Id](const FNoirBoardCard& C) { return C.Id == Id; });
}

TArray<FString> ANoirCase::GetCardIds() const
{
	TArray<FString> Ids;
	for (const FNoirBoardCard& C : Cards) Ids.Add(C.Id);
	return Ids;
}

void ANoirCase::SyncCards(const TArray<FNoirClue>& Clues)
{
	// Photographs of everyone involved, then a card per clue.
	for (const ANoirSuspect* Suspect : Suspects)
	{
		if (!Suspect) continue;
		const FString Id = TEXT("suspect:") + Suspect->GetName();
		if (FindCard(Id)) continue;
		FNoirBoardCard Card;
		Card.Id = Id;
		Card.Title = Suspect->DisplayName;
		Card.Text = Suspect->Occupation;
		Card.bPortrait = true;
		Card.Material = NoirEvidenceModels::MakeMaterial(this, Suspect->SignatureColor * 0.12f + FLinearColor(0.02f, 0.02f, 0.02f), 0.4f);
		AddCard(MoveTemp(Card));
	}
	for (const FNoirClue& Clue : Clues)
	{
		if (FNoirBoardCard* Existing = FindCardMutable(Clue.Id))
		{
			Existing->Text = Clue.Text; // a worked-out puzzle says more now
			continue;
		}
		FNoirBoardCard Card;
		Card.Id = Clue.Id;
		Card.Title = Clue.Title;
		Card.Text = Clue.Text;
		Card.Minute = Clue.Minute;
		Card.TimeLabel = Clue.TimeLabel;
		// Kept a little darker than white paper: under the board lamps it would clip and wash out the ink.
		Card.Material = NoirEvidenceModels::MakeMaterial(this, FLinearColor(0.3f, 0.26f, 0.19f), 0.85f);
		AddCard(MoveTemp(Card));
	}
	LayOut();
}

void ANoirCase::AddCard(FNoirBoardCard Card)
{
	static UStaticMesh* Cube = BasicShape(TEXT("Cube"));
	static UStaticMesh* Sphere = BasicShape(TEXT("Sphere"));
	if (!PinMaterial)
	{
		PinMaterial = NoirEvidenceModels::MakeMaterial(this, FLinearColor(0.35f, 0.02f, 0.02f), 0.3f);
	}
	Card.Paper = NewMesh(this, Bounds, Cube, Card.Material);
	Card.Pin = NewMesh(this, Bounds, Sphere, PinMaterial);
	Card.Label = NewObject<UTextRenderComponent>(this);
	Card.Label->SetupAttachment(Bounds);
	Card.Label->SetHorizontalAlignment(EHTA_Center);
	Card.Label->SetVerticalAlignment(EVRTA_TextCenter);
	Card.Label->SetWorldSize(2.5f);
	// Light ink on dark photographs, dark ink on paper and on pale photographs.
	FLinearColor Paper;
	Card.Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BaseColor")), Paper);
	const bool bLightInk = Card.bPortrait && Paper.GetLuminance() < 0.06f;
	Card.Label->SetTextRenderColor(bLightInk ? FColor(225, 205, 170) : FColor(2, 1, 0)); // unlit ink is exposed with the scene: near-black reads as ink
	if (!bLightInk)
	{
		// Ink doesn't catch the lamp light the way the paper does: unlit, it stays dark on the hottest card.
		static UMaterialInterface* UnlitInk = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/UnlitText.UnlitText"));
		if (UnlitInk) Card.Label->SetTextMaterial(UnlitInk);
	}
	FString Text = Card.bPortrait
		? WrapForCard(Card.Title.ToString(), 14, 2) // the name on the photo; the role shows on hover
		: WrapForCard(Card.Title.ToString(), 17, 4);
	if (!Card.TimeLabel.IsEmpty())
	{
		Text = Card.TimeLabel.ToString() + TEXT("<br>") + Text;
	}
	Card.Label->SetText(FText::FromString(Text));
	Card.Label->RegisterComponent();
	Cards.Add(MoveTemp(Card));
}

void ANoirCase::LayOut()
{
	const FVector2D Size = CardSize();
	const float HalfW = BoardSize.X * 0.5f, HalfH = BoardSize.Y * 0.5f;
	auto Spread = [&](TArray<FNoirBoardCard*>& Row, float Y)
	{
		const float Span = BoardSize.X - Size.X - 4.f;
		for (int32 i = 0; i < Row.Num(); ++i)
		{
			const float T = Row.Num() == 1 ? 0.5f : static_cast<float>(i) / (Row.Num() - 1);
			Row[i]->Position = FVector2D(-Span * 0.5f + Span * T, Y);
		}
	};

	// Top row: the suspects' photographs.
	TArray<FNoirBoardCard*> Portraits, Timed, Loose;
	for (FNoirBoardCard& C : Cards)
	{
		if (C.bMoved) continue;
		(C.bPortrait ? Portraits : C.Minute != INDEX_NONE ? Timed : Loose).Add(&C);
	}
	const float TopY = HalfH - Size.Y * 0.5f - 2.f;
	Spread(Portraits, TopY);

	// Bottom row: everything with a time, in order, along a cord.
	Timed.StableSort([](const FNoirBoardCard& A, const FNoirBoardCard& B) { return A.Minute < B.Minute; });
	const float TimelineY = -HalfH + Size.Y * 0.5f + 2.f;
	Spread(Timed, TimelineY);

	// The rest fill the rows between, left to right.
	const int32 Columns = FMath::Max(1, FMath::FloorToInt(BoardSize.X / (Size.X + 5.f)));
	const float StepX = BoardSize.X / Columns;
	for (int32 i = 0; i < Loose.Num(); ++i)
	{
		const int32 Row = i / Columns, Col = i % Columns;
		Loose[i]->Position = FVector2D(-HalfW + StepX * (Col + 0.5f), TopY - (Size.Y + 6.f) * (Row + 1));
	}

	for (FNoirBoardCard& C : Cards) PlaceCard(C);
	for (FNoirBoardString& S : Strings) PlaceString(S);

	if (!TimelineCord && !Timed.IsEmpty())
	{
		static UStaticMesh* Cylinder = BasicShape(TEXT("Cylinder"));
		TimelineCord = NewMesh(this, Bounds, Cylinder, NoirEvidenceModels::MakeMaterial(this, FLinearColor(0.08f, 0.05f, 0.03f), 0.9f));
	}
	if (TimelineCord)
	{
		const FVector Mid = BoardToLocal(FVector2D(0.f, TimelineY + Size.Y * 0.5f + 1.5f), 0.6f);
		TimelineCord->SetRelativeLocationAndRotation(Mid, FRotator(0.f, 0.f, 90.f));
		TimelineCord->SetRelativeScale3D(FVector(0.004f, 0.004f, (BoardSize.X - 6.f) / 100.f));
	}
}

void ANoirCase::PlaceCard(FNoirBoardCard& Card)
{
	const FVector2D Size = CardSize();
	const bool bHovered = Card.Id == HoveredId;
	const float Lift = bHovered ? 1.2f : 0.f; // the hovered card comes off the board a little
	Card.Paper->SetRelativeLocation(BoardToLocal(Card.Position, 0.3f + Lift));
	Card.Paper->SetRelativeScale3D(FVector(0.004f, Size.X / 100.f, Size.Y / 100.f) * (bHovered ? 1.06f : 1.f));
	Card.Label->SetRelativeLocation(BoardToLocal(Card.Position - FVector2D(0.f, 1.f), 0.55f + Lift));
	Card.Pin->SetRelativeLocation(BoardToLocal(Card.Position + FVector2D(0.f, Size.Y * 0.5f - 2.f), 0.9f + Lift));
	Card.Pin->SetRelativeScale3D(FVector(0.014f));
	const float Glow = Card.Id == PickedId ? 0.25f : 0.f;
	Card.Material->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(Glow, Glow * 0.55f, Glow * 0.2f));
}

void ANoirCase::PlaceString(FNoirBoardString& String)
{
	const FNoirBoardCard* A = FindCard(String.A);
	const FNoirBoardCard* B = FindCard(String.B);
	if (!A || !B || !String.Cord) return;
	const float PinY = CardSize().Y * 0.5f - 2.f;
	const FVector From = BoardToLocal(A->Position + FVector2D(0.f, PinY), 1.0f);
	const FVector To = BoardToLocal(B->Position + FVector2D(0.f, PinY), 1.0f);
	const FVector Dir = To - From;
	String.Cord->SetRelativeLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromZ(Dir.GetSafeNormal()).Rotator());
	String.Cord->SetRelativeScale3D(FVector(0.0035f, 0.0035f, Dir.Size() / 100.f));
}

void ANoirCase::MoveCard(const FString& Id, FVector2D Position)
{
	FNoirBoardCard* Card = FindCardMutable(Id);
	if (!Card) return;
	const FVector2D Half = CardSize() * 0.5f;
	Card->Position = FVector2D(
		FMath::Clamp(Position.X, -BoardSize.X * 0.5f + Half.X, BoardSize.X * 0.5f - Half.X),
		FMath::Clamp(Position.Y, -BoardSize.Y * 0.5f + Half.Y, BoardSize.Y * 0.5f - Half.Y));
	Card->bMoved = true;
	PlaceCard(*Card);
	for (FNoirBoardString& S : Strings)
	{
		if (S.A == Id || S.B == Id) PlaceString(S);
	}
}

void ANoirCase::GetArrangement(TArray<FString>& Moved, TArray<FVector2D>& To, TArray<FString>& A, TArray<FString>& B) const
{
	for (const FNoirBoardCard& C : Cards)
	{
		if (!C.bMoved) continue;
		Moved.Add(C.Id);
		To.Add(C.Position);
	}
	for (const FNoirBoardString& S : Strings)
	{
		A.Add(S.A);
		B.Add(S.B);
	}
}

void ANoirCase::Arrange(const TArray<FString>& Moved, const TArray<FVector2D>& To, const TArray<FString>& A, const TArray<FString>& B)
{
	for (int32 i = 0; i < Moved.Num() && i < To.Num(); ++i)
	{
		MoveCard(Moved[i], To[i]);
	}
	for (int32 i = 0; i < A.Num() && i < B.Num(); ++i)
	{
		const bool bTied = Strings.ContainsByPredicate([&](const FNoirBoardString& S)
		{
			return (S.A == A[i] && S.B == B[i]) || (S.A == B[i] && S.B == A[i]);
		});
		if (!bTied) ToggleString(A[i], B[i]);
	}
}

void ANoirCase::ToggleString(const FString& A, const FString& B)
{
	if (A == B || !FindCard(A) || !FindCard(B)) return;
	const int32 Existing = Strings.IndexOfByPredicate([&](const FNoirBoardString& S)
	{
		return (S.A == A && S.B == B) || (S.A == B && S.B == A);
	});
	if (Existing != INDEX_NONE)
	{
		Strings[Existing].Cord->DestroyComponent();
		Strings.RemoveAt(Existing);
		return;
	}
	static UStaticMesh* Cylinder = BasicShape(TEXT("Cylinder"));
	FNoirBoardString& S = Strings.AddDefaulted_GetRef();
	S.A = A;
	S.B = B;
	S.Cord = NewMesh(this, Bounds, Cylinder, NoirEvidenceModels::MakeMaterial(this, FLinearColor(0.4f, 0.015f, 0.01f), 0.6f));
	PlaceString(S);
}

void ANoirCase::SetHighlight(const FString& Hovered, const FString& Picked)
{
	if (Hovered == HoveredId && Picked == PickedId) return;
	HoveredId = Hovered;
	PickedId = Picked;
	for (FNoirBoardCard& C : Cards) PlaceCard(C);
}

#undef LOCTEXT_NAMESPACE
