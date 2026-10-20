#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoirRoom.generated.h"

class UBoxComponent;

/**
 * A hand-built room the case director can cast as any room of a generated case. Furniture
 * (searchables) and stand points inside its bounds belong to it.
 */
UCLASS()
class NOIRMYSTERY_API ANoirRoom : public AActor
{
	GENERATED_BODY()

public:
	ANoirRoom();

	UFUNCTION(BlueprintPure, Category = "Room")
	bool Contains(const FVector& WorldLocation) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	FName RoomId;

	/** Has space for the body; the crime scene is cast from these rooms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	bool bCanBeScene = false;

	/** Case places this room is dressed for, by English label ("Dining Car", "Luggage Van");
	 *  the director casts it as one of them when it can. Empty: plays anything. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	TArray<FString> Suits;

	/** Where the body lies if this room plays the crime scene (relative to the room). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room", meta = (MakeEditWidget = true))
	FTransform BodyPoint;

	/** The name the case gives this room ("Billiard Room"); empty if the case does not use it. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Room")
	FText Label;

	/** Cast as the crime scene in this case (the body lies here). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Room")
	bool bPlaysScene = false;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Bounds;
};

/** Where a suspect stands, facing along the arrow. */
UCLASS()
class NOIRMYSTERY_API ANoirStandPoint : public AActor
{
	GENERATED_BODY()

public:
	ANoirStandPoint();
};
