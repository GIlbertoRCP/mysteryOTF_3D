#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ANoirPlayerController;
class SButton;
class SEditableTextBox;
class SWidgetSwitcher;

/**
 * The game's menus, in Slate: the title screen (New case, Settings, Quit) and the pause menu
 * (Resume, Settings, Restart, Abandon, Quit). Text follows the menu language, which switches live.
 * Mouse, keyboard (arrows, Enter, Esc) and gamepad all work.
 */
class SNoirMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNoirMenu) : _bPause(false) {}
		SLATE_ARGUMENT(bool, bPause)
		/** After the dossier: the verdict and what to do next. Implies bPause. */
		SLATE_ARGUMENT(bool, bClosed)
		SLATE_ARGUMENT(FText, Verdict)
		SLATE_ARGUMENT(TWeakObjectPtr<ANoirPlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** "main" (the first page), "case" (New case), "settings" or "controls". */
	void ShowPage(const FString& Page);
	void FocusFirst();

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

private:
	TSharedRef<SWidget> MakeMainPage();
	TSharedRef<SWidget> MakeCasePage();
	TSharedRef<SWidget> MakeSettingsPage();
	/** Every action, its key and its gamepad button (from Settings). */
	TSharedRef<SWidget> MakeControlsPage();

	/** A text button: cream, amber when hovered or focused. */
	TSharedRef<SWidget> MakeButton(TAttribute<FText> Label, TFunction<void()> OnClick, TSharedPtr<SButton>* OutButton = nullptr, float Size = 22.f);
	/** Label on the left, "<  value  >" on the right; Step(-1 or +1) changes the value. */
	TSharedRef<SWidget> MakeChoice(TAttribute<FText> Label, TAttribute<FText> Value, TFunction<void(int32)> Step, TSharedPtr<SButton>* OutButton = nullptr);
	TSharedRef<SWidget> MakeHeading(TAttribute<FText> Text);

	void Back();
	void Begin();

	bool bPause = false;
	bool bClosed = false;
	FText Verdict;
	TWeakObjectPtr<ANoirPlayerController> Owner;
	TSharedPtr<SWidgetSwitcher> Pages;
	TSharedPtr<class SHorizontalBox> Fade;
	TSharedPtr<SEditableTextBox> SeedBox;
	TSharedPtr<SButton> FirstOnPage[4];
	int32 Page = 0;

	// New case choices.
	int32 Difficulty = 1;
	bool bTrain = false;

	// Display choices, read from the game user settings.
	TArray<FIntPoint> Resolutions;
};
