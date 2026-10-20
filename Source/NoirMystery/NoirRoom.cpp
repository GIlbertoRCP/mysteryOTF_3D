#include "NoirRoom.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"

ANoirRoom::ANoirRoom()
{
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetBoxExtent(FVector(300.f, 250.f, 165.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->ShapeColor = FColor(200, 160, 90);
	RootComponent = Bounds;
}

bool ANoirRoom::Contains(const FVector& WorldLocation) const
{
	const FVector Local = Bounds->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

ANoirStandPoint::ANoirStandPoint()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	UArrowComponent* Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(RootComponent);
		Arrow->ArrowColor = FColor(120, 200, 180);
	}
#endif
}
