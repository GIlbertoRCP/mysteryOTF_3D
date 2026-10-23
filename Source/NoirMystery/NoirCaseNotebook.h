#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ANoirCharacter;
class ANoirPlayerController;
class SBox;

/**
 * The Detective's Case Overview (N).
 * Clean, focused summary of the investigation:
 * - Case briefing and victim profile
 * - The formal accusation objectives (The Killer, Motive, Weapon, Broken Alibi)
 * - Current investigation progress
 * - Detective intuition hints
 * Styled in authentic 1940s film-noir aesthetic (obsidian archival paper, aged brass hairlines,
 * red ink rubber stamp, cream and amber typography). Detailed 3D clues and suspect evidence remain in the Mind Place (Tab).
 */
class SNoirCaseNotebook : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNoirCaseNotebook) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ANoirCharacter>, Detective)
		SLATE_ARGUMENT(TWeakObjectPtr<ANoirPlayerController>, Owner)
		SLATE_ARGUMENT(TFunction<void()>, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

	void Refresh();

private:
	TSharedRef<SWidget> MakeOverviewContent();

	TWeakObjectPtr<ANoirCharacter> Detective;
	TWeakObjectPtr<ANoirPlayerController> Owner;
	TFunction<void()> OnClose;

	TSharedPtr<SBox> ContentContainer;
};
