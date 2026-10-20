#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoirInteractable.h"
#include "NoirSearchable.generated.h"

class UBoxComponent;
class ANoirEvidence;

/**
 * A searchable spot placed over a piece of furniture. Searching it once reveals its clue
 * (or nothing); afterwards it reports that it has been searched.
 *
 * The box only blocks the Visibility channel, so it catches the interaction trace without
 * changing how the player collides with the furniture underneath.
 */
UCLASS()
class NOIRMYSTERY_API ANoirSearchable : public AActor, public INoirInteractable
{
	GENERATED_BODY()

public:
	ANoirSearchable();

	virtual FText GetInteractPrompt_Implementation() const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

	bool IsSearched() const { return bSearched; }

	/** Searched in a saved case: open, its items out, nothing picked up. */
	void RestoreSearched();

	/** What the player calls it: "the desk", "the bookcase". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
	FText DisplayName;

	/** The clue found here. Empty means there is nothing to find. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search", meta = (MultiLine = true))
	FText ClueText;

	/** Evidence hidden here: revealed when searched, the first brought up for inspection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
	TArray<TObjectPtr<ANoirEvidence>> Items;

	/** What searching shows, for generated cases ("In the desk drawer you find..."). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search", meta = (MultiLine = true))
	FText SearchText;

	/** What kind of furniture this is, so a generated case can hide its spots in matching pieces:
	 *  desk, wastebasket, window, cupboard, rug, bookshelf, coatrack, sofa (or anything else). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
	FName FurnitureKind;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Bounds;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Search")
	bool bSearched = false;
};
