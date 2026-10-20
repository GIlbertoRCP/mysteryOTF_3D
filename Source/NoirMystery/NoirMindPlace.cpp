#include "NoirMindPlace.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "NoirCase.h"
#include "Kismet/GameplayStatics.h"
#include "NoirCaseDirector.h"
#include "NoirCharacter.h"
#include "NoirEvidence.h"

ANoirMindPlace::ANoirMindPlace()
{
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetBoxExtent(FVector(450.f, 350.f, 170.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->ShapeColor = FColor(220, 170, 80);
	RootComponent = Bounds;
}

ANoirMindPlace* ANoirMindPlace::Find(const UObject* WorldContext)
{
	return Cast<ANoirMindPlace>(UGameplayStatics::GetActorOfClass(WorldContext, ANoirMindPlace::StaticClass()));
}

bool ANoirMindPlace::Contains(const FVector& WorldLocation) const
{
	const FVector Local = Bounds->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

void ANoirMindPlace::OnEnter(ANoirCharacter* Detective)
{
	ANoirCaseDirector* Director = Cast<ANoirCaseDirector>(UGameplayStatics::GetActorOfClass(this, ANoirCaseDirector::StaticClass()));
	if (Director)
	{
		Director->SetCaseClockPaused(true);
	}
	LayOutTable(Detective);
	// The board is up to date the moment you walk in, not only once you step up to it.
	if (Board)
	{
		Board->SyncCards(Detective->GetKnownClues());
	}
	if (Director)
	{
		Director->SaveCase();
	}
}

void ANoirMindPlace::OnLeave(ANoirCharacter* Detective)
{
	if (ANoirCaseDirector* Director = Cast<ANoirCaseDirector>(UGameplayStatics::GetActorOfClass(this, ANoirCaseDirector::StaticClass())))
	{
		Director->SetCaseClockPaused(false);
	}
}

void ANoirMindPlace::LayOutTable(ANoirCharacter* Detective)
{
	ANoirCaseDirector* Director = Cast<ANoirCaseDirector>(UGameplayStatics::GetActorOfClass(this, ANoirCaseDirector::StaticClass()));
	// Every physical clue examined so far gets a copy on the table, in the order found.
	for (const FNoirClue& Clue : Detective->GetKnownClues())
	{
		const ANoirEvidence* Source = Clue.Actor.Get();
		if (TableItems.Contains(Clue.Id))
		{
			continue;
		}
		if (!Source)
		{
			// A lab result with something to work out: the cast, the slide, the swatch.
			if (!Clue.Puzzle.Reveal.IsEmpty() && Director)
			{
				if (ANoirEvidence* Lab = Director->MakeLabItem(Clue, FTransform::Identity))
				{
					TableItems.Add(Clue.Id, Lab);
				}
			}
			continue;
		}
		if (Contains(Source->GetActorLocation()))
		{
			continue;
		}
		ANoirEvidence* Copy = GetWorld()->SpawnActorDeferred<ANoirEvidence>(ANoirEvidence::StaticClass(), FTransform::Identity);
		Copy->DisplayName = Source->DisplayName;
		Copy->Description = Source->Description;
		Copy->Details = Source->Details;
		Copy->Parts = Source->Parts;
		Copy->CaseEvidenceId = Clue.Id;
		Copy->CardTitle = Source->CardTitle.IsEmpty() ? Clue.Title : Source->CardTitle;
		Copy->CardText = Source->CardText.IsEmpty() ? Clue.Text : Source->CardText;
		Copy->CardMinute = Source->CardMinute;
		Copy->CardTime = Source->CardTime;
		Copy->Puzzle = Source->Puzzle;
		Copy->bHiddenUntilFound = false;
		Copy->FinishSpawning(FTransform::Identity);
		Copy->Mesh->SetStaticMesh(Source->Mesh->GetStaticMesh());
		TableItems.Add(Clue.Id, Copy);
	}

	// Laid out in rows across the table top, face up.
	const int32 PerRow = FMath::Max(1, FMath::FloorToInt(TableSize.Y / 40.f));
	int32 Index = 0;
	for (const TPair<FString, TObjectPtr<ANoirEvidence>>& Pair : TableItems)
	{
		ANoirEvidence* Item = Pair.Value;
		if (!Item || Detective->GetInspected() == Item)
		{
			++Index;
			continue;
		}
		const int32 Row = Index / PerRow, Col = Index % PerRow;
		const float Y = -TableSize.Y * 0.5f + TableSize.Y * (Col + 0.5f) / PerRow;
		const float X = Row % 2 == 0 ? -TableSize.X * 0.22f : TableSize.X * 0.22f;
		const FVector Local = TableCentre + FVector(X, Y, 3.f + 2.f * (Row / 2));
		Item->SetActorLocationAndRotation(GetActorTransform().TransformPosition(Local),
			(GetActorRotation() + FRotator(90.f, 0.f, 0.f)).Quaternion());
		++Index;
	}
}
