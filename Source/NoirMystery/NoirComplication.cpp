#include "NoirComplication.h"

#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "NoirSnowfall.h"

namespace
{
	/** A strike's brightness over time: a flash, a flicker, then the long one fading. */
	float StrikeEnvelope(float Age)
	{
		if (Age < 0.f) return 0.f;
		if (Age < 0.07f) return 1.f;
		if (Age < 0.16f) return 0.08f;
		if (Age < 0.24f) return 0.7f;
		if (Age < 0.36f) return 0.05f;
		if (Age < 1.1f) return FMath::Lerp(1.f, 0.f, (Age - 0.36f) / 0.74f);
		return 0.f;
	}
}

ANoirComplication::ANoirComplication()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANoirComplication::Gather(UWorld* World, FName Tag, TArray<FLit>& Out)
{
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->ActorHasTag(Tag)) continue;
		TInlineComponentArray<ULightComponent*> Lights(*It);
		for (ULightComponent* Light : Lights)
		{
			FLit& Lit = Out.AddDefaulted_GetRef();
			Lit.Light = Light;
			Lit.Base = Light->Intensity;
			Lit.Phase = FMath::FRandRange(0.f, 100.f);
		}
	}
}

void ANoirComplication::Set(const FLit& Lit, float Intensity)
{
	if (ULightComponent* Light = Lit.Light.Get())
	{
		Light->SetIntensity(Intensity);
	}
}

void ANoirComplication::Start(const FString& InKey)
{
	Key = InKey;
	UWorld* World = GetWorld();
	Gather(World, TEXT("Mains"), Mains);
	if (Key == TEXT("blackout"))
	{
		// The power never came back: the electric lights are dead and candles were brought out.
		for (const FLit& Lit : Mains)
		{
			if (ULightComponent* Light = Lit.Light.Get()) Light->SetVisibility(false);
		}
		int32 Shades = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("Candle"))) It->SetActorHiddenInGame(false);
			if (!It->ActorHasTag(TEXT("Mains"))) continue;
			// The lamps' shades stop glowing too.
			TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
			for (UStaticMeshComponent* Mesh : Meshes)
			{
				for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
				{
					if (UMaterialInstanceDynamic* Dark = Mesh->CreateDynamicMaterialInstance(i))
					{
						Dark->SetVectorParameterValue(TEXT("Emissive"), FLinearColor::Black);
						++Shades;
					}
				}
			}
		}
		Gather(World, TEXT("Candle"), Candles);
		SetActorTickEnabled(true);
		UE_LOG(LogTemp, Display, TEXT("Complication blackout: %d mains lights out, %d shades dark, %d candles lit"), Mains.Num(), Shades, Candles.Num());
	}
	else if (Key == TEXT("storm"))
	{
		Gather(World, TEXT("Lightning"), Lightning);
		// Where it snows (the train), the storm is a blizzard.
		for (TActorIterator<ANoirSnowfall> It(World); It; ++It)
		{
			It->SetBlizzard(true);
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->ActorHasTag(TEXT("Lightning"))) continue;
			TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
			for (UStaticMeshComponent* Mesh : Meshes)
			{
				if (UMaterialInstanceDynamic* Pane = Mesh->CreateDynamicMaterialInstance(0)) Panes.Add(Pane);
			}
		}
		for (const FLit& Lit : Lightning)
		{
			if (ULightComponent* Light = Lit.Light.Get())
			{
				Light->SetIntensity(0.f);
				Light->SetVisibility(true);
			}
		}
		NextStrike = FMath::FRandRange(3.f, 6.f);
		SetActorTickEnabled(true);
	}
}

void ANoirComplication::Strike()
{
	if (Key == TEXT("storm"))
	{
		StrikeAge = 0.f;
	}
}

void ANoirComplication::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	// Candle flames: two slow waves and a fast one, different for each flame.
	for (const FLit& Lit : Candles)
	{
		const float T = Time + Lit.Phase;
		const float Gutter = 0.84f + 0.08f * FMath::Sin(T * 2.3f) + 0.05f * FMath::Sin(T * 5.7f + 1.3f) + 0.03f * FMath::Sin(T * 17.f);
		Set(Lit, Lit.Base * Gutter);
	}

	if (Key == TEXT("storm"))
	{
		NextStrike -= DeltaSeconds;
		if (NextStrike <= 0.f)
		{
			Strike();
			NextStrike = FMath::FRandRange(StrikeInterval.X, StrikeInterval.Y);
		}
		if (StrikeAge >= 0.f)
		{
			StrikeAge += DeltaSeconds;
			const float Flash = StrikeEnvelope(StrikeAge);
			for (const FLit& Lit : Lightning)
			{
				Set(Lit, StrikeIntensity * Flash);
			}
			const FLinearColor Night(0.004f, 0.012f, 0.012f), Sky(2.4f, 3.0f, 3.4f);
			for (const TWeakObjectPtr<UMaterialInstanceDynamic>& Pane : Panes)
			{
				if (Pane.IsValid()) Pane->SetVectorParameterValue(TEXT("Emissive"), Night + Sky * Flash);
			}
			// The lamps dip as the strike hits the lines.
			const float Dip = StrikeAge < 0.5f ? 0.45f + 0.55f * FMath::Abs(FMath::Sin(StrikeAge * 40.f)) : 1.f;
			for (const FLit& Lit : Mains)
			{
				Set(Lit, Lit.Base * Dip);
			}
			if (StrikeAge > 1.2f)
			{
				StrikeAge = -1.f;
			}
		}
	}
}
