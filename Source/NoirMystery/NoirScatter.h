#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoirScatter.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

/** One mesh scattered over the grounds, and where each copy stands. */
USTRUCT(BlueprintType)
struct FNoirScatterSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	TArray<FTransform> Instances;

	/** Trees and rocks block the detective; ferns and grass do not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	bool bCollision = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	bool bCastShadow = true;

	/** Beyond this (cm) the copies fade out; 0 never. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	float CullDistance = 0.f;

	/** Below 1, each case keeps only this share of the copies, picked by the case's seed: the same
	 *  woods, but the fallen branches and the leaf piles lie differently from case to case. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0", ClampMax = "1"))
	float CaseKeep = 1.f;
};

/**
 * Instanced dressing for outdoor areas: the woods, rocks, undergrowth and debris. A build script
 * decides where everything stands (Scripts/build_grounds.py); this turns each set into one
 * hierarchical instanced mesh, so thousands of trees cost a handful of draw calls.
 */
UCLASS()
class NOIRMYSTERY_API ANoirScatter : public AActor
{
	GENERATED_BODY()

public:
	ANoirScatter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	TArray<FNoirScatterSet> Sets;

	/** Rebuilds the instanced meshes from Sets (the build script calls it after filling them). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Scatter")
	void Rebuild();

	/** How many copies are standing now, for playtests. */
	UFUNCTION(BlueprintPure, Category = "Scatter")
	int32 CountInstances() const;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	/** Builds the components; with a case seed, the CaseKeep sets are thinned by it. */
	void Build(const FString& CaseSeed);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Parts;
};
