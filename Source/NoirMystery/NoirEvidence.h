#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoirClue.h"
#include "NoirInteractable.h"
#include "NoirEvidence.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** One piece of a placeholder evidence model, e.g. the watch's case, dial or a hand. */
USTRUCT(BlueprintType)
struct FNoirEvidencePart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	TObjectPtr<UMaterialInterface> Material;

	/** Relative to the actor; the actor's +X is the object's front. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	FTransform Transform;
};

/** Something only noticed by turning the object to face you: an engraving, a seal, a monogram. */
USTRUCT(BlueprintType)
struct FNoirInspectDetail
{
	GENERATED_BODY()

	/** Direction, in the mesh's local space, the detail faces (out of the surface it sits on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	FVector LocalDirection = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect", meta = (MultiLine = true))
	FText Text;

	/** How squarely the detail must face the viewer (cosine of the angle) to be noticed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect", meta = (ClampMin = 0, ClampMax = 1))
	float FacingThreshold = 0.85f;
};

/**
 * A physical clue that can be picked up and turned over. Placed in the level where it rests
 * once found; if bHiddenUntilFound it stays invisible until a searchable reveals it.
 *
 * The model's local +X is its "front": inspection starts with +X facing the viewer.
 * The model is either Mesh, or (for placeholders built from basic shapes) the Parts list,
 * which is turned into components when the actor is constructed.
 */
UCLASS()
class NOIRMYSTERY_API ANoirEvidence : public AActor, public INoirInteractable
{
	GENERATED_BODY()

public:
	ANoirEvidence();

	virtual FText GetInteractPrompt_Implementation() const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

	/** Makes hidden evidence visible and interactable. */
	void Reveal();

	/**
	 * Marks the first undiscovered detail that faces ToViewer (unit vector, world space).
	 * Returns true and the detail's text if one was newly discovered.
	 */
	bool DiscoverFacing(const FVector& ToViewer, FText& OutText);

	int32 NumDetailsFound() const { return FoundDetails.Num(); }
	int32 NumDetails() const { return Details.Num(); }
	float GetInspectRadius() const;

	/** "the pocket watch" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FText DisplayName;

	/** First impression, shown when inspection starts and recorded as a clue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	TArray<FNoirInspectDetail> Details;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	bool bHiddenUntilFound = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	TArray<FNoirEvidencePart> Parts;

	/** For generated cases: the case's evidence id, and the card it adds to the notes. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Evidence|Case")
	FString CaseEvidenceId;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Evidence|Case")
	FText CardTitle;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Evidence|Case")
	FText CardText;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Evidence|Case")
	int32 CardMinute = INDEX_NONE;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Evidence|Case")
	FText CardTime;

	/** The clue this item gives once examined. */
	/** The puzzle on this item, if any (its solved state lives with the detective's clue). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Evidence|Case")
	FNoirPuzzle Puzzle;

	FNoirClue MakeClue();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	TSet<int32> FoundDetails;
};
