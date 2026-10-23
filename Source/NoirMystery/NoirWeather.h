#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoirWeather.generated.h"

class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class ULocalFogVolumeComponent;
class UMaterialInterface;
class UMaterialParameterCollection;

/**
 * The night outside: fog in layers, and rain. Silent Hill's trick is that fog is never one flat
 * wall, so this stacks several, all drifting on the same wind:
 *  - the level's height fog, thick and lying low, with a second, denser layer along the ground;
 *  - fog banks, local fog volumes that roll past the detective and slowly breathe;
 *  - wisps, soft fog cards near the camera that make the fog visibly move;
 *  - rain, streaks lit by the torch and the moon, and the wet sheen it leaves (a parameter
 *    collection the ground, stone and prop materials read).
 * Indoors (in any NoirRoom not marked outdoors) the fog thins and the rain fades, blended across
 * the doorway. Materials and the parameter collection come from Scripts/build_weather.py.
 */
UCLASS()
class NOIRMYSTERY_API ANoirWeather : public AActor
{
	GENERATED_BODY()

public:
	ANoirWeather();

	// ─── Fog ───

	/** Height fog density outside and in. The fog is one for the whole level, so indoors it only
	 *  thins so far: through a door the grounds must still be a wall of grey. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float OutdoorFogDensity = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float IndoorFogDensity = 0.07f;

	/** How fast the height fog thins with height (higher: it lies lower). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float FogHeightFalloff = 0.35f;

	/** The second layer: dense along the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float GroundFogDensity = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float GroundFogFalloff = 2.2f;

	/** Colour the fog takes from the night sky, before the moon and torch light it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	FLinearColor FogColor = FLinearColor(0.0035f, 0.0042f, 0.0055f);

	/** Volumetric fog: how much it swallows light outside and in, and how far it is computed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float OutdoorExtinction = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float IndoorExtinction = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float VolumetricDistance = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	FColor FogAlbedo = FColor(150, 154, 160);

	/** How strongly the moon lights the fog: shafts through the trees, without the whole night
	 *  glowing milky. Set on the level's directional light at the start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float MoonFogScatter = 0.9f;

	/** The fog's own faint light, outdoors: where no moon reaches (under the trees) it would
	 *  otherwise read as plain darkness; with it, things stand dark against grey. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	FLinearColor FogGlow = FLinearColor(0.065f, 0.07f, 0.085f);

	/** Share of the glow kept indoors (it would grey out the lamplit rooms; none leaves the
	 *  doorway black). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float IndoorGlow = 0.35f;

	/** Wind the fog and rain drift on, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	FVector2D Wind = FVector2D(140.f, 60.f);

	// ─── Fog banks ───

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog Banks")
	int32 FogBanks = 16;

	/** Banks are kept within this distance of the detective. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog Banks")
	float BankRange = 5500.f;

	/** Bank radii, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog Banks")
	FVector2D BankRadius = FVector2D(1200.f, 3200.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog Banks")
	float BankDensity = 0.9f;

	// ─── Wisps ───

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisps")
	int32 Wisps = 90;

	/** Wisps live this near and far from the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisps")
	FVector2D WispRange = FVector2D(200.f, 2600.f);

	/** Wisp card sizes, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisps")
	FVector2D WispSize = FVector2D(500.f, 1400.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisps")
	TObjectPtr<UMaterialInterface> WispMaterial;

	// ─── Rain ───

	/** 0 dry, 1 a downpour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0", ClampMax = "1"))
	float Rain = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	int32 Drops = 3500;

	/** Rain falls in a cylinder this wide and tall around the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	float RainRadius = 1300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	float RainHeight = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	float FallSpeed = 950.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	TObjectPtr<UMaterialInterface> RainMaterial;

	/** Weather parameters the materials read: Wetness (0..1), Rain (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	TObjectPtr<UMaterialParameterCollection> Parameters;

	/** How wet things are when play starts; it creeps toward Rain while it rains. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0", ClampMax = "1"))
	float StartWetness = 0.75f;

	/** 0 outside, 1 indoors (blended across doorways), for playtests. */
	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetIndoors() const { return Indoors; }

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetRain(float NewRain) { Rain = FMath::Clamp(NewRain, 0.f, 1.f); }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FBank
	{
		FVector P;
		float Radius;
		float Phase;
		float Rate;
	};
	struct FWisp
	{
		FVector P;
		float Size;
		float Spin;
		float Phase;
	};
	struct FDrop
	{
		FVector P;
		float Speed;
	};

	/** Where the camera is (the detective's view), and which way it faces. */
	bool GetView(FVector& OutLocation, FRotator& OutRotation) const;
	bool IsIndoorsAt(const FVector& P, float Margin = 0.f) const;
	/** Distance across the ground from a point to the nearest building (0 inside one). */
	float HouseClearance(const FVector& P) const;
	/** The ground under a point (a downward trace), or the point's own height if nothing is hit. */
	float GroundAt(const FVector& P) const;
	FVector RandomAround(const FVector& Centre, float MinRange, float MaxRange) const;
	void PlaceBank(FBank& Bank, const FVector& Centre, bool bUpwind);
	void UpdateFog(float DeltaSeconds);
	void UpdateBanks(const FVector& View, float DeltaSeconds);
	void UpdateWisps(const FVector& View, float DeltaSeconds);
	void UpdateRain(const FVector& View, const FRotator& ViewRotation, float DeltaSeconds);

	UPROPERTY(Transient)
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULocalFogVolumeComponent>> BankVolumes;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> WispCards;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> RainStreaks;

	/** The house and other indoor rooms, as boxes (world space), and one box around them all. */
	TArray<FBox> IndoorBoxes;
	FBox IndoorBounds = FBox(ForceInit);
	/** Raindrops hidden last frame (scale 0), so they are not rewritten every frame. */
	TBitArray<> DropHidden;
	TArray<FBank> BankState;
	TArray<FWisp> WispState;
	TArray<FDrop> DropState;
	TArray<FTransform> WispTransforms;
	TArray<FTransform> DropTransforms;
	float Indoors = 0.f;
	/** In the Mind Place (1) or not (0), eased: the weather all but stops there. */
	float InMind = 0.f;
	TWeakObjectPtr<class ANoirMindPlace> MindPlace;
	float Wetness = 0.f;
	float Time = 0.f;
};
