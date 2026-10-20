#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoirMindPlace.generated.h"

class ANoirCharacter;
class ANoirEvidence;
class UBoxComponent;

/**
 * The detective's Mind Place (Alan Wake 2's): a room outside the case's world, the same in every
 * case, entered from anywhere with a key press. The case board lives here, and every physical clue
 * examined so far is laid out on the evidence table to be picked up again. Time in the case's
 * world stands still while the detective is inside.
 *
 * Built into each case level, away from the playable map; one per level.
 */
UCLASS()
class NOIRMYSTERY_API ANoirMindPlace : public AActor
{
	GENERATED_BODY()

public:
	ANoirMindPlace();

	/** Where the detective appears on entering (relative to this actor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mind Place", meta = (MakeEditWidget = true))
	FTransform Entry;

	/** The evidence table's top: centre, and the area items are laid out over (X by Y, cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mind Place", meta = (MakeEditWidget = true))
	FVector TableCentre = FVector(0.f, 200.f, 80.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mind Place")
	FVector2D TableSize = FVector2D(70.f, 220.f);

	/** The case board on the Mind Place's wall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mind Place")
	TObjectPtr<class ANoirCase> Board;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Bounds;

	UFUNCTION(BlueprintPure, Category = "Mind Place")
	bool Contains(const FVector& WorldLocation) const;
	FTransform GetEntryTransform() const { return Entry * GetActorTransform(); }

	/** Called as the detective arrives and leaves: freezes the case's clock, refreshes the table. */
	void OnEnter(ANoirCharacter* Detective);
	void OnLeave(ANoirCharacter* Detective);

	/** The Mind Place in this level, if it has one. */
	static ANoirMindPlace* Find(const UObject* WorldContext);

private:
	void LayOutTable(ANoirCharacter* Detective);

	/** Copies of examined evidence, by case evidence id (or source actor name). */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<ANoirEvidence>> TableItems;
};
