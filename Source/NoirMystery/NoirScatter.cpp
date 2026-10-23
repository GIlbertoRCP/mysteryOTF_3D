#include "NoirScatter.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "NoirCaseDirector.h"

ANoirScatter::ANoirScatter()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ANoirScatter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build(FString());
}

void ANoirScatter::Rebuild()
{
	Build(FString());
}

void ANoirScatter::BeginPlay()
{
	Super::BeginPlay();
	// The instanced meshes are transient (rebuilt, never saved), so play always builds them here.
	bool bVaries = false;
	for (const FNoirScatterSet& Set : Sets) bVaries |= Set.CaseKeep < 1.f;
	FString Seed;
	if (bVaries)
	{
		for (TActorIterator<ANoirCaseDirector> It(GetWorld()); It; ++It)
		{
			Seed = It->GetPlayedSeed().IsEmpty() ? It->Seed : It->GetPlayedSeed();
		}
		if (Seed.IsEmpty()) Seed = TEXT("default");
	}
	Build(Seed);
}

void ANoirScatter::Build(const FString& CaseSeed)
{
	for (UHierarchicalInstancedStaticMeshComponent* Part : Parts)
	{
		if (Part) Part->DestroyComponent();
	}
	Parts.Reset();

	for (int32 i = 0; i < Sets.Num(); ++i)
	{
		const FNoirScatterSet& Set = Sets[i];
		if (!Set.Mesh || Set.Instances.IsEmpty()) continue;

		UHierarchicalInstancedStaticMeshComponent* Part = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Part->SetMobility(EComponentMobility::Static);
		Part->SetupAttachment(RootComponent);
		Part->SetStaticMesh(Set.Mesh);
		Part->SetCollisionEnabled(Set.bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Part->SetCollisionProfileName(Set.bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		Part->SetCastShadow(Set.bCastShadow);
		if (Set.CullDistance > 0.f) Part->SetCullDistances(FMath::RoundToInt(Set.CullDistance * 0.8f), FMath::RoundToInt(Set.CullDistance));
		Part->RegisterComponent();
		AddInstanceComponent(Part);

		TArray<FTransform> Keep;
		if (Set.CaseKeep < 1.f && !CaseSeed.IsEmpty())
		{
			// The same seed always thins the same way; each set draws its own sequence.
			FRandomStream Stream(static_cast<int32>(GetTypeHash(CaseSeed) ^ (0x9E3779B9u * static_cast<uint32>(i + 1))));
			for (const FTransform& T : Set.Instances)
			{
				if (Stream.FRand() < Set.CaseKeep) Keep.Add(T);
			}
		}
		else
		{
			Keep = Set.Instances;
		}
		Part->AddInstances(Keep, false, false);
		Parts.Add(Part);
	}
}

int32 ANoirScatter::CountInstances() const
{
	int32 N = 0;
	for (const UHierarchicalInstancedStaticMeshComponent* Part : Parts)
	{
		if (Part) N += Part->GetInstanceCount();
	}
	return N;
}
