#include "NoirWeather.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "NoirMindPlace.h"
#include "NoirRoom.h"

ANoirWeather::ANoirWeather()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));

	auto MakeCards = [this, Plane](const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(RootComponent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetStaticMesh(Plane);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		// They follow the camera: never cull them by their (stale) bounds.
		C->bNeverDistanceCull = true;
		C->SetBoundsScale(1000.f);
		return C;
	};
	WispCards = MakeCards(TEXT("Wisps"));
	RainStreaks = MakeCards(TEXT("Rain"));
}

bool ANoirWeather::GetView(FVector& OutLocation, FRotator& OutRotation) const
{
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		OutLocation = Cam->GetCameraLocation();
		OutRotation = Cam->GetCameraRotation();
		return true;
	}
	return false;
}

bool ANoirWeather::IsIndoorsAt(const FVector& P, float Margin) const
{
	if (!IndoorBounds.IsValid || !IndoorBounds.ExpandBy(Margin).IsInside(P)) return false;
	for (const FBox& B : IndoorBoxes)
	{
		if (B.ExpandBy(Margin).IsInside(P)) return true;
	}
	return false;
}

float ANoirWeather::GroundAt(const FVector& P) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NoirWeatherGround), false);
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0)) Params.AddIgnoredActor(Pawn);
	const FVector Top(P.X, P.Y, P.Z + 3000.f), Bottom(P.X, P.Y, P.Z - 6000.f);
	return GetWorld()->LineTraceSingleByChannel(Hit, Top, Bottom, ECC_WorldStatic, Params) ? Hit.ImpactPoint.Z : P.Z;
}

FVector ANoirWeather::RandomAround(const FVector& Centre, float MinRange, float MaxRange) const
{
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	// Uniform over the ring's area, not crowded at its middle.
	const float R = FMath::Sqrt(FMath::FRandRange(MinRange * MinRange, MaxRange * MaxRange));
	return Centre + FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 0.f);
}

void ANoirWeather::BeginPlay()
{
	Super::BeginPlay();
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		HeightFog = It->GetComponent();
	}
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		It->GetLightComponent()->SetVolumetricScatteringIntensity(MoonFogScatter);
	}
	for (TActorIterator<ANoirRoom> It(GetWorld()); It; ++It)
	{
		if (It->bOutdoors || !It->Bounds) continue;
		// The world box around the room's (possibly turned) bounds, from its eight corners.
		const FTransform T = It->Bounds->GetComponentTransform();
		FBox Box(ForceInit);
		const FVector E = It->Bounds->GetUnscaledBoxExtent();
		for (int32 c = 0; c < 8; ++c)
		{
			Box += T.TransformPosition(FVector((c & 1) ? E.X : -E.X, (c & 2) ? E.Y : -E.Y, (c & 4) ? E.Z : -E.Z));
		}
		IndoorBoxes.Add(Box);
		IndoorBounds += Box;
	}

	FVector View;
	FRotator ViewRot;
	if (!GetView(View, ViewRot)) View = GetActorLocation();

	// Fog banks: local fog volumes, scattered around the detective to start with.
	for (int32 i = 0; i < FogBanks; ++i)
	{
		ULocalFogVolumeComponent* V = NewObject<ULocalFogVolumeComponent>(this, NAME_None, RF_Transient);
		V->SetupAttachment(RootComponent);
		V->SetUsingAbsoluteLocation(true);
		V->SetUsingAbsoluteScale(true);
		V->RegisterComponent();
		BankVolumes.Add(V);
		FBank& B = BankState.AddDefaulted_GetRef();
		PlaceBank(B, View, false);
	}

	WispCards->SetMaterial(0, WispMaterial);
	WispState.SetNum(Wisps);
	WispTransforms.SetNum(Wisps);
	for (int32 i = 0; i < Wisps; ++i)
	{
		FWisp& W = WispState[i];
		W.P = RandomAround(View, WispRange.X, WispRange.Y);
		W.P.Z = GroundAt(W.P) + FMath::FRandRange(40.f, 380.f);
		W.Size = FMath::FRandRange(WispSize.X, WispSize.Y);
		W.Spin = FMath::FRandRange(-12.f, 12.f);
		W.Phase = FMath::FRandRange(0.f, 2.f * PI);
		WispTransforms[i] = FTransform(W.P);
	}
	WispCards->AddInstances(WispTransforms, false);

	RainStreaks->SetMaterial(0, RainMaterial);
	DropState.SetNum(Drops);
	DropTransforms.SetNum(Drops);
	for (int32 i = 0; i < Drops; ++i)
	{
		DropState[i].P = RandomAround(View, 0.f, RainRadius) + FVector(0, 0, FMath::FRandRange(-RainHeight * 0.4f, RainHeight * 0.6f));
		DropState[i].Speed = FallSpeed * FMath::FRandRange(0.85f, 1.15f);
		DropTransforms[i] = FTransform(DropState[i].P);
	}
	RainStreaks->AddInstances(DropTransforms, false);
	DropHidden.Init(false, Drops);

	Wetness = StartWetness;
	Indoors = IsIndoorsAt(View) ? 1.f : 0.f;
	UE_LOG(LogTemp, Display, TEXT("Weather: %d fog banks, %d wisps, %d raindrops, %d indoor rooms, height fog %s"),
		BankVolumes.Num(), Wisps, Drops, IndoorBoxes.Num(), HeightFog ? TEXT("found") : TEXT("missing"));
}

float ANoirWeather::HouseClearance(const FVector& P) const
{
	// Each building on its own: the house and the outbuildings are far apart, and the fog lies
	// between them.
	float Best = BIG_NUMBER;
	for (const FBox& B : IndoorBoxes)
	{
		const FBox Flat(FVector(B.Min.X, B.Min.Y, P.Z), FVector(B.Max.X, B.Max.Y, P.Z));
		Best = FMath::Min(Best, FMath::Sqrt(Flat.ComputeSquaredDistanceToPoint(P)));
	}
	return Best;
}

void ANoirWeather::PlaceBank(FBank& Bank, const FVector& Centre, bool bUpwind)
{
	// New banks arrive from upwind, at the edge of the range; at the start they are anywhere.
	// Never inside the house: a few tries at a spot clear of it.
	Bank.Radius = FMath::FRandRange(BankRadius.X, BankRadius.Y);
	FVector P;
	for (int32 Try = 0; Try < 8; ++Try)
	{
		if (bUpwind && !Wind.IsNearlyZero())
		{
			const FVector2D Up = -Wind.GetSafeNormal();
			const FVector2D Side(-Up.Y, Up.X);
			const FVector2D At = FVector2D(Centre) + Up * BankRange * 0.95f + Side * FMath::FRandRange(-BankRange, BankRange) * 0.8f;
			P = FVector(At.X, At.Y, Centre.Z);
		}
		else
		{
			P = RandomAround(Centre, 0.f, BankRange);
		}
		if (HouseClearance(P) > Bank.Radius * 0.6f) break;
	}
	// Centred a little below the ground, so the sphere's top is a low, wide drift.
	P.Z = GroundAt(P) - Bank.Radius * 0.35f;
	Bank.P = P;
	Bank.Phase = FMath::FRandRange(0.f, 2.f * PI);
	Bank.Rate = FMath::FRandRange(0.05f, 0.14f);
}

void ANoirWeather::UpdateFog(float DeltaSeconds)
{
	if (!HeightFog) return;
	const float Out = 1.f - Indoors;
	// A slow swell in the whole fog, so the far silhouettes come and go.
	const float Swell = 1.f + 0.18f * FMath::Sin(Time * 0.07f) + 0.08f * FMath::Sin(Time * 0.23f + 1.3f);
	HeightFog->SetFogDensity(FMath::Lerp(FMath::Lerp(IndoorFogDensity, OutdoorFogDensity * Swell, Out), 0.004f, InMind));
	HeightFog->SetFogHeightFalloff(FogHeightFalloff);
	FExponentialHeightFogData Second;
	Second.FogDensity = GroundFogDensity * FMath::Lerp(0.5f, 1.f, Out) * Swell * (1.f - InMind);
	Second.FogHeightFalloff = GroundFogFalloff;
	Second.FogHeightOffset = 0.f;
	HeightFog->SetSecondFogData(Second);
	HeightFog->SetFogInscatteringColor(FogColor);
	HeightFog->SetVolumetricFog(true);
	HeightFog->SetVolumetricFogExtinctionScale(FMath::Lerp(FMath::Lerp(IndoorExtinction, OutdoorExtinction, Out), 0.4f, InMind));
	HeightFog->SetVolumetricFogDistance(VolumetricDistance);
	HeightFog->SetVolumetricFogAlbedo(FogAlbedo);
	HeightFog->SetVolumetricFogScatteringDistribution(0.55f);
	HeightFog->SetVolumetricFogEmissive(FogGlow * (Out + IndoorGlow * Indoors) * (1.f - InMind));
	HeightFog->SetStartDistance(0.f);
}

void ANoirWeather::UpdateBanks(const FVector& View, float DeltaSeconds)
{
	const FVector Drift(Wind.X * DeltaSeconds, Wind.Y * DeltaSeconds, 0.f);
	const float Base = ULocalFogVolumeComponent::GetBaseVolumeSize();
	for (int32 i = 0; i < BankState.Num(); ++i)
	{
		FBank& B = BankState[i];
		B.P += Drift;
		if (FVector::Dist2D(B.P, View) > BankRange) PlaceBank(B, View, true);
		ULocalFogVolumeComponent* V = BankVolumes[i];
		if (!V) continue;
		V->SetWorldLocation(B.P);
		V->SetWorldScale3D(FVector(B.Radius / Base));
		// Each bank breathes on its own slow cycle, and thins as it rolls over the house (the
		// fog outside must stay put whether the detective is in or out, or doors look onto nothing).
		const float Breath = 0.55f + 0.45f * FMath::Sin(Time * B.Rate + B.Phase);
		const float Clear = FMath::Clamp((HouseClearance(B.P) - B.Radius * 0.3f) / (B.Radius * 0.5f), 0.f, 1.f);
		const float Density = BankDensity * Breath * Clear * (1.f - InMind);
		V->SetRadialFogExtinction(Density * 0.35f);
		V->SetHeightFogExtinction(Density);
		V->SetHeightFogFalloff(450.f);
		V->SetHeightFogOffset(0.f);
		V->SetFogAlbedo(FLinearColor(0.85f, 0.87f, 0.9f));
		V->SetFogPhaseG(0.45f);
	}
}

void ANoirWeather::UpdateWisps(const FVector& View, float DeltaSeconds)
{
	if (!WispMaterial || WispState.IsEmpty()) return;
	const FVector Drift(Wind.X * DeltaSeconds, Wind.Y * DeltaSeconds, 0.f);
	const float Out = 1.f - Indoors;
	for (int32 i = 0; i < WispState.Num(); ++i)
	{
		FWisp& W = WispState[i];
		W.P += Drift + FVector(0, 0, FMath::Sin(Time * 0.3f + W.Phase) * 6.f * DeltaSeconds);
		const float D = FVector::Dist2D(W.P, View);
		if (D > WispRange.Y || D < WispRange.X * 0.5f || IsIndoorsAt(W.P, 100.f))
		{
			// Back in somewhere around the camera; mostly upwind, so the fog keeps streaming past.
			W.P = RandomAround(View, WispRange.X, WispRange.Y);
			if (!Wind.IsNearlyZero() && FMath::FRand() < 0.6f)
			{
				const FVector2D Up = -Wind.GetSafeNormal();
				W.P = View + FVector(Up.X, Up.Y, 0) * WispRange.Y * 0.9f + FVector(-Up.Y, Up.X, 0) * FMath::FRandRange(-WispRange.Y, WispRange.Y) * 0.7f;
			}
			W.P.Z = GroundAt(W.P) + FMath::FRandRange(40.f, 380.f);
		}
		// Upright cards turned to face the camera, each tilted a little in its own plane.
		const FVector ToCam = (View - W.P).GetSafeNormal2D();
		const FRotator Face = FRotationMatrix::MakeFromZX(ToCam, FVector::UpVector).Rotator();
		const FQuat Q = Face.Quaternion() * FQuat(FVector::UpVector, FMath::DegreesToRadians(W.Spin));
		// Indoors they shrink away rather than pop.
		const float S = W.Size / 100.f * FMath::Max(Out * (1.f - InMind), 0.001f);
		WispTransforms[i] = FTransform(Q, W.P, FVector(S * 0.4f, S, 1.f)); // local X is up: low and wide
	}
	WispCards->BatchUpdateInstancesTransforms(0, WispTransforms, true, true, true);
}

void ANoirWeather::UpdateRain(const FVector& View, const FRotator& ViewRotation, float DeltaSeconds)
{
	if (!RainMaterial || DropState.IsEmpty()) return;
	const float Out = 1.f - Indoors;
	const int32 Live = InMind > 0.5f ? 0 : FMath::RoundToInt(DropState.Num() * Rain * FMath::Max(Out, 0.15f));
	// Rain slants with the wind (stronger than the fog feels it).
	const FVector Fall = FVector(Wind.X * 2.f, Wind.Y * 2.f, 0.f);
	for (int32 i = 0; i < DropState.Num(); ++i)
	{
		FDrop& D = DropState[i];
		const FVector Vel = FVector(Fall.X, Fall.Y, -D.Speed);
		D.P += Vel * DeltaSeconds;
		const bool bGone = D.P.Z < View.Z - RainHeight * 0.4f || FVector::Dist2D(D.P, View) > RainRadius;
		if (bGone)
		{
			D.P = RandomAround(View, 0.f, RainRadius);
			D.P.Z = View.Z + RainHeight * FMath::FRandRange(0.4f, 0.6f);
		}
		if (i >= Live || IsIndoorsAt(D.P, 60.f))
		{
			if (!DropHidden[i]) DropTransforms[i] = FTransform(FQuat::Identity, D.P, FVector(0.f));
			DropHidden[i] = true;
			continue;
		}
		DropHidden[i] = false;
		// A thin streak along the fall, turned about it to face the camera.
		const FVector Axis = Vel.GetSafeNormal();
		const FVector ToCam = (View - D.P);
		const FVector Side = FVector::CrossProduct(Axis, ToCam).GetSafeNormal();
		const FVector Normal = FVector::CrossProduct(Side, Axis);
		const FMatrix M(Side, Axis, Normal, FVector::ZeroVector); // plane X across, Y along, Z out
		const float Length = D.Speed * 0.045f; // motion-blurred length, cm
		DropTransforms[i] = FTransform(M.ToQuat(), D.P, FVector(0.012f, Length / 100.f, 1.f));
	}
	RainStreaks->BatchUpdateInstancesTransforms(0, DropTransforms, true, true, true);
}

void ANoirWeather::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	FVector View;
	FRotator ViewRot;
	if (!GetView(View, ViewRot)) return;

	// Indoors blends in over the doorway, about a metre either side of the line.
	if (!MindPlace.IsValid()) MindPlace = ANoirMindPlace::Find(this);
	const bool bMind = MindPlace.IsValid() && MindPlace->Contains(View);
	InMind = bMind ? 1.f : FMath::FInterpTo(InMind, 0.f, DeltaSeconds, 3.f); // in at once, out with a breath
	const float Target = bMind || IsIndoorsAt(View) ? 1.f : 0.f;
	Indoors = bMind ? 1.f : FMath::FInterpTo(Indoors, Target, DeltaSeconds, 2.5f);

	// Things get wetter while it rains, and dry very slowly when it stops.
	Wetness = FMath::FInterpTo(Wetness, Rain > 0.05f ? FMath::Max(Rain, StartWetness) : 0.f, DeltaSeconds, Rain > 0.05f ? 0.02f : 0.004f);
	if (Parameters)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, Parameters, TEXT("Wetness"), Wetness);
		UKismetMaterialLibrary::SetScalarParameterValue(this, Parameters, TEXT("Rain"), Rain * (1.f - Indoors));
	}

	UpdateFog(DeltaSeconds);
	UpdateBanks(View, DeltaSeconds);
	UpdateWisps(View, DeltaSeconds);
	UpdateRain(View, ViewRot, DeltaSeconds);
}
