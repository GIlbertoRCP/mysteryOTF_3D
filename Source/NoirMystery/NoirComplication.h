#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoirComplication.generated.h"

class ULightComponent;

/**
 * Makes a case's complication physical in the level, not just a line in the notes:
 *  - blackout: the house is still without power. Every light tagged "Mains" is dead; the
 *    candles (actors tagged "Candle", hidden otherwise) are lit and gutter.
 *  - storm: lightning through the windows (lights tagged "Lightning"), the lamps dipping
 *    with each strike.
 * Tampered evidence is shown on the decoy itself (see NoirEvidenceModels).
 *
 * Spawned by the case director when the case has a complication.
 */
UCLASS()
class NOIRMYSTERY_API ANoirComplication : public AActor
{
	GENERATED_BODY()

public:
	ANoirComplication();

	void Start(const FString& InKey);

	UFUNCTION(BlueprintPure, Category = "Complication")
	FString GetKey() const { return Key; }

	/** Fires a lightning strike now (storms only); for playtests. */
	UFUNCTION(BlueprintCallable, Category = "Complication")
	void Strike();

	/** Seconds between strikes. */
	UPROPERTY(EditAnywhere, Category = "Complication")
	FVector2D StrikeInterval = FVector2D(9.f, 22.f);

	/** Peak intensity of a strike, in candela, for each window light. */
	UPROPERTY(EditAnywhere, Category = "Complication")
	float StrikeIntensity = 150.f;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FLit
	{
		TWeakObjectPtr<ULightComponent> Light;
		float Base = 0.f;
		float Phase = 0.f;
	};

	static void Gather(UWorld* World, FName Tag, TArray<FLit>& Out);
	static void Set(const FLit& Lit, float Intensity);

	FString Key;
	TArray<FLit> Mains;
	TArray<FLit> Candles;
	TArray<FLit> Lightning;
	/** The window panes, which flash with each strike. */
	TArray<TWeakObjectPtr<class UMaterialInstanceDynamic>> Panes;
	float Time = 0.f;
	float NextStrike = 0.f;
	/** Time since the current strike began, or negative between strikes. */
	float StrikeAge = -1.f;
};
