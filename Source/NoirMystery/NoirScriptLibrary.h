#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NoirScriptLibrary.generated.h"

class UMaterialExpression;
class UMaterialParameterCollection;

/** Small helpers for the build scripts, for what the editor's Python cannot reach on its own. */
UCLASS()
class NOIRMYSTERY_API UNoirScriptLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Gives every parameter of a collection a valid, unique id (parameters made from Python have
	 *  none, so material nodes cannot find them). Returns how many it fixed. */
	UFUNCTION(BlueprintCallable, Category = "Noir|Scripts")
	static int32 FixCollectionIds(UMaterialParameterCollection* Collection);

	/** Points a material's collection-parameter node at one parameter, by name, id and all. */
	UFUNCTION(BlueprintCallable, Category = "Noir|Scripts")
	static bool BindCollectionParameter(UMaterialExpression* Node, UMaterialParameterCollection* Collection, FName Parameter);
};
