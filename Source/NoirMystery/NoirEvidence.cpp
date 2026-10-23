#include "NoirEvidence.h"

#include "NoirCaseContent.h"
#include "Components/StaticMeshComponent.h"
#include "NoirCharacter.h"

#define LOCTEXT_NAMESPACE "NoirEvidence"

namespace
{
	void SetupEvidenceMesh(UStaticMeshComponent* Component)
	{
		Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->SetCollisionResponseToAllChannels(ECR_Ignore);
		Component->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Component->SetMobility(EComponentMobility::Movable);
	}
}

ANoirEvidence::ANoirEvidence()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetupEvidenceMesh(Mesh);
	RootComponent = Mesh;

	DisplayName = NoirTr(TEXT("the object"), TEXT("el objeto"));
}

void ANoirEvidence::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Construction-script components are destroyed and rebuilt whenever the actor is reconstructed.
	for (const FNoirEvidencePart& Part : Parts)
	{
		if (!Part.Mesh)
		{
			continue;
		}
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		SetupEvidenceMesh(Piece);
		Piece->SetStaticMesh(Part.Mesh);
		if (Part.Material)
		{
			Piece->SetMaterial(0, Part.Material);
			Piece->OverrideMaterials = { Part.Material };
		}
		Piece->SetupAttachment(RootComponent);
		Piece->SetRelativeTransform(Part.Transform);
		Piece->RegisterComponent();
	}
}

void ANoirEvidence::BeginPlay()
{
	Super::BeginPlay();
	if (bHiddenUntilFound)
	{
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
	}
}

void ANoirEvidence::Reveal()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}

FText ANoirEvidence::GetInteractPrompt_Implementation() const
{
	return FText::Format(NoirTr(TEXT("Examine {0}"), TEXT("Examinar {0}")), DisplayName);
}

void ANoirEvidence::Interact_Implementation(APawn* InstigatorPawn)
{
	if (ANoirCharacter* Detective = Cast<ANoirCharacter>(InstigatorPawn))
	{
		Detective->BeginInspect(this);
	}
}

bool ANoirEvidence::DiscoverFacing(const FVector& ToViewer, FText& OutText)
{
	const FQuat Rotation = GetActorQuat();
	for (int32 i = 0; i < Details.Num(); ++i)
	{
		if (FoundDetails.Contains(i))
		{
			continue;
		}
		const FNoirInspectDetail& Detail = Details[i];
		const FVector Facing = Rotation.RotateVector(Detail.LocalDirection.GetSafeNormal());
		if (FVector::DotProduct(Facing, ToViewer) >= Detail.FacingThreshold)
		{
			FoundDetails.Add(i);
			OutText = Detail.Text;
			return true;
		}
	}
	return false;
}

FNoirClue ANoirEvidence::MakeClue()
{
	FNoirClue Clue;
	Clue.Id = CaseEvidenceId.IsEmpty() ? GetName() : CaseEvidenceId;
	Clue.Title = CardTitle.IsEmpty() ? DisplayName : CardTitle;
	Clue.Text = CardText.IsEmpty() ? Description : CardText;
	Clue.Actor = this;
	Clue.Minute = CardMinute;
	Clue.TimeLabel = CardTime;
	Clue.Puzzle = Puzzle;
	return Clue;
}

float ANoirEvidence::GetInspectRadius() const
{
	const FBox Local = CalculateComponentsBoundingBoxInLocalSpace(true);
	return Local.IsValid ? Local.GetExtent().Size() * GetActorScale3D().GetMax() : 5.f;
}

#undef LOCTEXT_NAMESPACE
