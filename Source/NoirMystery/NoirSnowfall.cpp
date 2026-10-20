#include "NoirSnowfall.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "NoirEvidenceModels.h"

ANoirSnowfall::ANoirSnowfall()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Flakes"));
	RootComponent = Mesh;
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
}

FVector ANoirSnowfall::RandomSpot(float CentreX, bool bAnyHeight) const
{
	// Denser near the train, where the windows look out.
	const float Side = FMath::RandBool() ? 1.f : -1.f;
	const float T = FMath::Square(FMath::FRand());
	const float Y = Side * FMath::Lerp(InnerHalfWidth, OuterHalfWidth, T);
	const float Z = bAnyHeight ? FMath::FRandRange(Heights.X, Heights.Y) : Heights.Y;
	return FVector(CentreX + FMath::FRandRange(-HalfLength, HalfLength), Y, Z);
}

void ANoirSnowfall::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetMaterial(0, NoirEvidenceModels::MakeMaterial(this, FLinearColor(0.8f, 0.85f, 0.9f), 0.9f, 0.f,
		FLinearColor(0.35f, 0.4f, 0.45f)));
	State.SetNum(Flakes);
	Transforms.SetNum(Flakes);
	for (int32 i = 0; i < Flakes; ++i)
	{
		State[i] = {RandomSpot(0.f, true), FMath::FRandRange(55.f, 110.f), FMath::FRandRange(0.f, 6.28f)};
		Transforms[i] = FTransform(FQuat::Identity, State[i].P, FVector(FMath::FRandRange(0.025f, 0.045f)));
	}
	Mesh->AddInstances(Transforms, false);
	UE_LOG(LogTemp, Display, TEXT("Snowfall: %d flakes"), Mesh->GetInstanceCount());
}

void ANoirSnowfall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	// Keep the snow around the player along the train.
	float CentreX = 0.f;
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		CentreX = GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation()).X;
	}

	const float Wind = bBlizzard ? 420.f : 35.f;
	const float Fall = bBlizzard ? 1.8f : 1.f;
	for (int32 i = 0; i < State.Num(); ++i)
	{
		FFlake& F = State[i];
		F.P.Z -= F.Speed * Fall * DeltaSeconds;
		F.P.X += (Wind + FMath::Sin(Time * 0.7f + F.Phase) * 25.f) * DeltaSeconds;
		F.P.Y += FMath::Cos(Time * 1.1f + F.Phase) * 12.f * DeltaSeconds;
		// Landed, blown past, or left behind by the player: start again somewhere above.
		if (F.P.Z < Heights.X || FMath::Abs(F.P.X - CentreX) > HalfLength || FMath::Abs(F.P.Y) < InnerHalfWidth)
		{
			const bool bAnyHeight = FMath::Abs(F.P.X - CentreX) > HalfLength;
			F.P = RandomSpot(CentreX, bAnyHeight);
			if (bBlizzard && !bAnyHeight) F.P.X -= HalfLength * 0.5f; // the wind brings it in from upwind
		}
		Transforms[i].SetLocation(F.P);
	}
	Mesh->BatchUpdateInstancesTransforms(0, Transforms, false, true, true);
}
