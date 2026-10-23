#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ANoirCharacter;

/**
 * The map (M): an inked floor plan of the location on parchment, like the cartographer's map on
 * the mood board. Each room carries the name the case gives it (the crime scene marked), every
 * piece of furniture shows whether it has been searched, the suspects stand where they are in
 * their signature colours, and an arrow shows the detective. A snapshot taken when it opens.
 */
class SNoirMap : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNoirMap) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ANoirCharacter>, Detective)
		SLATE_ARGUMENT(TFunction<void()>, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Shows the grounds (every named area) or just the house. */
	void SetGrounds(bool bOn);
	bool IsShowingGrounds() const { return bGrounds; }

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	struct FRoomMark { TArray<FVector2D> Corners; FVector2D Centre; FString Label; bool bScene = false; bool bCast = true; bool bOutdoors = false; bool bOutbuilding = false; };
	struct FThingMark { FVector2D At; bool bSearched = false; };
	struct FPersonMark { FVector2D At; FLinearColor Color; FString Name; FVector2D LabelAt; };

	/** Reads the level into marks for the current view (house or grounds). */
	void Gather(ANoirCharacter* Detective);
	FVector2D ToMap(const FVector& World) const;

	TArray<FRoomMark> Rooms;
	TArray<FThingMark> Furniture;
	TArray<FPersonMark> Suspects;
	FVector2D You = FVector2D::ZeroVector;
	FVector2D YouFacing = FVector2D(1, 0);
	FText Place;

	/** The grounds (every named area), or just the house; G switches where there are grounds. */
	bool bGrounds = false;
	bool bHasGrounds = false;
	TWeakObjectPtr<ANoirCharacter> Detective;
	TSharedPtr<class SNoirSketch> Sketch;
	/** The house's outline on the grounds view (rooms and hall together), for its one label. */
	FVector2D HouseLo = FVector2D(FLT_MAX), HouseHi = FVector2D(-FLT_MAX);

	/** Whether a sketch point is on the page. */
	bool OnPage(const FVector2D& P) const;

	/** World XY bounds and the scale onto the sketch (X and Y may differ for long, thin places). */
	FVector2D WorldMin, WorldMax, Scale, Offset;
	TFunction<void()> OnClose;
};
