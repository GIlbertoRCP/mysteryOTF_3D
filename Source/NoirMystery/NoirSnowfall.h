#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoirSnowfall.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Snow falling outside a long, narrow interior (the train): a few thousand tiny flakes in two
 * bands either side of the actor's X axis, kept around the player along X so the windows always
 * have snow behind them. Nothing falls inside the band |Y| < InnerHalfWidth.
 *
 * A storm (the case's complication) turns it into a blizzard: more wind, faster, slanting.
 */
UCLASS()
class NOIRMYSTERY_API ANoirSnowfall : public AActor
{
	GENERATED_BODY()

public:
	ANoirSnowfall();

	UPROPERTY(EditAnywhere, Category = "Snow")
	int32 Flakes = 4000;

	/** Half the length of snow kept around the player, along X. */
	UPROPERTY(EditAnywhere, Category = "Snow")
	float HalfLength = 1400.f;

	/** No snow closer to the axis than this (the carriage and its walls). */
	UPROPERTY(EditAnywhere, Category = "Snow")
	float InnerHalfWidth = 185.f;

	UPROPERTY(EditAnywhere, Category = "Snow")
	float OuterHalfWidth = 1500.f;

	/** Ground and top of the fall, relative to the actor. */
	UPROPERTY(EditAnywhere, Category = "Snow")
	FVector2D Heights = FVector2D(-130.f, 650.f);

	UFUNCTION(BlueprintCallable, Category = "Snow")
	void SetBlizzard(bool bOn) { bBlizzard = bOn; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FFlake
	{
		FVector P;
		float Speed;
		float Phase;
	};

	FVector RandomSpot(float CentreX, bool bAnyHeight) const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Mesh;

	TArray<FFlake> State;
	TArray<FTransform> Transforms;
	bool bBlizzard = false;
	float Time = 0.f;
};
