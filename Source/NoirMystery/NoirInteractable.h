#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NoirInteractable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UNoirInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Anything the detective can look at and use: furniture to search, suspects, evidence. */
class NOIRMYSTERY_API INoirInteractable
{
	GENERATED_BODY()

public:
	/** Short verb phrase shown under the crosshair, e.g. "Search the desk". Empty hides the prompt. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractPrompt() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(APawn* InstigatorPawn);
};
