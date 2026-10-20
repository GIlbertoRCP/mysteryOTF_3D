#pragma once

#include "CoreMinimal.h"
#include "NoirClue.h"
#include "Widgets/SCompoundWidget.h"
#include "NoirSketch.h"

class SVerticalBox;

/**
 * The web game's evidence puzzles, worked on in the inspector: flip the hand onto the prints,
 * match the cast to a sole, sweep the UV lamp, type the blood, order the torn scraps, turn the
 * cipher dial. Solving calls OnSolved once and shows what the clue really says; "Just show me"
 * skips it. Esc (or Close) leaves; an unsolved puzzle can be picked up again later.
 */
class SNoirPuzzle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNoirPuzzle) {}
		SLATE_ARGUMENT(FNoirClue, Clue)
		SLATE_ARGUMENT(TFunction<void()>, OnSolved)
		SLATE_ARGUMENT(TFunction<void()>, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

	/** For playtests: solve it the honest way where that is a single choice, else skip. */
	void SolveForTest();

private:
	TSharedRef<SWidget> MakeHand();
	TSharedRef<SWidget> MakeSole();
	TSharedRef<SWidget> MakeUv();
	TSharedRef<SWidget> MakeBlood();
	TSharedRef<SWidget> MakeTorn();
	TSharedRef<SWidget> MakeCipher();

	void Solve();
	void Wrong(const FText& Why);
	void RebuildTorn();

	FNoirClue Clue;
	TFunction<void()> OnSolved;
	TFunction<void()> OnClose;
	bool bSolved = false;
	FText Status;

	// hand
	bool bRightHand = false;
	// uv
	FVector2D Lamp = FVector2D(-1000.f);
	FVector2D Spot;
	int32 Dwell = 0;
	bool bFound = false;
	// blood
	bool bAddedA = false, bAddedB = false;
	// torn
	TArray<int32> Shuffled, Placed;
	TSharedPtr<SVerticalBox> PlacedBox;
	// cipher
	int32 Dial = 0;
	FString Encoded;
};
