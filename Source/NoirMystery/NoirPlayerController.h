#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NoirPlayerController.generated.h"

class SNoirMenu;
class SNoirPuzzle;
class SNoirMap;
struct FNoirClue;

/**
 * Owns the menus: the main menu (in the menu map) and the pause menu (Esc during a case).
 * Starting, restarting and leaving cases goes through here.
 */
UCLASS()
class NOIRMYSTERY_API ANoirPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** The level cases are played in, and the menu level. */
	static constexpr const TCHAR* CaseMap = TEXT("/Game/Maps/L_Manor");
	static constexpr const TCHAR* TrainMap = TEXT("/Game/Maps/L_Train");
	static constexpr const TCHAR* MenuMap = TEXT("/Game/Maps/L_Menu");

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ShowMainMenu();

	/** After the dossier: how the accusation went, and a new case or this one again. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ShowCaseClosed(int32 Correct, int32 Parts);

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void CloseMenu();

	UFUNCTION(BlueprintPure, Category = "Menu")
	bool IsMenuOpen() const { return Menu.IsValid(); }

	/** Opens the case level with a seed (empty: a random one). */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void StartCase(const FString& Seed, const FString& Difficulty, bool bSpanish, const FString& Theme = TEXT("gothic"));

	/** Picks up the saved case where it was left. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ContinueCase();

	/** The same case again, from the beginning. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void RestartCase();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void QuitToMainMenu();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void QuitGame();

	/** Opens a page of the open menu by name ("main", "case", "settings"); for playtests. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ShowMenuPage(const FString& Page);

	/** The map of the location (M); M or Esc closes it. */
	UFUNCTION(BlueprintCallable, Category = "Map")
	void ToggleMap();

	UFUNCTION(BlueprintPure, Category = "Map")
	bool IsMapOpen() const { return Map.IsValid(); }

	/** Opens the puzzle on a clue over the game (the detective keeps inspecting behind it). */
	void OpenPuzzle(const FNoirClue& Clue);

	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void ClosePuzzle();

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsPuzzleOpen() const { return Puzzle.IsValid(); }

	/** For playtests: solve the open puzzle (or skip it where it takes more than one choice). */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void SolvePuzzleForTest();

	/** Where input is going right now (viewport, focus, pause, move input), for playtests. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	FString DescribeInput() const;

	/** The case being played: title and seed, for the pause menu. */
	FText GetCaseTitle() const;
	FString GetCaseSeed() const;
	FString GetCaseTheme() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void OpenMenu(bool bPause, bool bClosed = false, const FText& Verdict = FText());

	TSharedPtr<SNoirMenu> Menu;
	TSharedPtr<SNoirPuzzle> Puzzle;
	TSharedPtr<SNoirMap> Map;
	TSharedPtr<class SWidget> MenuContainer;
	/** In the menu map: the camera the title screen is seen through, drifting slowly. */
	TWeakObjectPtr<AActor> MenuCamera;
	FTransform MenuCameraHome;
	float MenuTime = 0.f;
	/** The case's language, restored when the pause menu closes. */
	bool bCaseSpanish = false;
};
