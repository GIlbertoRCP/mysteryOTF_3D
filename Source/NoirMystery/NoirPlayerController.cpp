#include "NoirPlayerController.h"

#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "NoirCaseContent.h"
#include "NoirCaseDirector.h"
#include "NoirCharacter.h"
#include "NoirGameMode.h"
#include "NoirMenu.h"
#include "NoirMap.h"
#include "NoirCaseNotebook.h"
#include "NoirPuzzle.h"
#include "NoirSave.h"

namespace
{
	ANoirCaseDirector* FindDirector(const UObject* Context)
	{
		return Cast<ANoirCaseDirector>(UGameplayStatics::GetActorOfClass(Context, ANoirCaseDirector::StaticClass()));
	}

	bool InMenuMap(const UWorld* World)
	{
		return World && Cast<ANoirMenuGameMode>(World->GetAuthGameMode()) != nullptr;
	}

	/** -LogInput: log keys reaching the player and the input state (diagnosing input problems). */
	bool ShouldLogInput()
	{
		static const bool b = FParse::Param(FCommandLine::Get(), TEXT("LogInput"));
		return b;
	}
}

void ANoirPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	if (ShouldLogInput())
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			UE_LOG(LogTemp, Display, TEXT("[Input] %s state: %s"), *GetWorld()->GetMapName(), *DescribeInput());
		}), 3.f, true);
	}
	if (!InMenuMap(GetWorld()) && FParse::Param(FCommandLine::Get(), TEXT("SimKeys")))
	{
		// -SimKeys: press W for two seconds, then Tab, through Slate as a real keyboard would.
		auto Key = [](const FKey& K, bool bDown)
		{
			const FKeyEvent Event(K, FSlateApplication::Get().GetModifierKeys(), 0, false, 0, 0);
			bDown ? FSlateApplication::Get().ProcessKeyDownEvent(Event) : FSlateApplication::Get().ProcessKeyUpEvent(Event);
		};
		FTimerHandle A, B, C, D;
		GetWorldTimerManager().SetTimer(A, FTimerDelegate::CreateWeakLambda(this, [Key]() { Key(EKeys::W, true); }), 6.f, false);
		GetWorldTimerManager().SetTimer(B, FTimerDelegate::CreateWeakLambda(this, [Key]() { Key(EKeys::W, false); }), 8.f, false);
		GetWorldTimerManager().SetTimer(C, FTimerDelegate::CreateWeakLambda(this, [Key]() { Key(EKeys::Tab, true); }), 9.f, false);
		GetWorldTimerManager().SetTimer(D, FTimerDelegate::CreateWeakLambda(this, [Key]() { Key(EKeys::Tab, false); }), 9.2f, false);
	}
	if (!InMenuMap(GetWorld()))
	{
		// The menu's UI-only input mode lives on the game viewport and survives travel into
		// the case level: hand input back to the game, or the detective can't move.
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
		return;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("MenuCamera")))
		{
			MenuCamera = *It;
			MenuCameraHome = It->GetActorTransform();
			SetViewTarget(*It);
			break;
		}
	}
	ShowMainMenu();
	// -AutoCase=SEED: start that case from the title by itself (for testing the real game flow).
	FString AutoSeed;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoCase="), AutoSeed))
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, AutoSeed]()
		{
			StartCase(AutoSeed, TEXT("Detective"), false, AutoSeed.Contains(TEXT("express")) ? TEXT("express") : TEXT("gothic"));
		}), 3.f, false);
	}
}

bool ANoirPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Key == EKeys::N && Params.Event == IE_Pressed)
	{
		ToggleCaseNotebook();
		return true;
	}
	const bool bHandled = Super::InputKey(Params);
	if (ShouldLogInput() && !Params.Key.IsMouseButton() && Params.Event == IE_Pressed)
	{
		UE_LOG(LogTemp, Display, TEXT("[Input] key %s (handled %d)"), *Params.Key.ToString(), bHandled);
	}
	return bHandled;
}

void ANoirPlayerController::ToggleMapView()
{
	if (Map) Map->SetGrounds(!Map->IsShowingGrounds());
}

void ANoirPlayerController::ToggleMap()
{
	if (Map)
	{
		if (GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Map.ToSharedRef());
		Map.Reset();
		if (ANoirCharacter* Detective = Cast<ANoirCharacter>(GetPawn())) Detective->RestoreInputMode();
		return;
	}
	ANoirCharacter* Detective = Cast<ANoirCharacter>(GetPawn());
	if (!Detective || Menu || Puzzle || !GEngine || !GEngine->GameViewport) return;
	if (CaseNotebook) ToggleCaseNotebook();
	SAssignNew(Map, SNoirMap).Detective(Detective).OnClose([this]() { ToggleMap(); });
	GEngine->GameViewport->AddViewportWidgetContent(Map.ToSharedRef(), 40);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Map);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void ANoirPlayerController::ToggleCaseNotebook()
{
	if (CaseNotebook)
	{
		if (GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(CaseNotebook.ToSharedRef());
		CaseNotebook.Reset();
		if (ANoirCharacter* Detective = Cast<ANoirCharacter>(GetPawn())) Detective->RestoreInputMode();
		return;
	}
	ANoirCharacter* Detective = Cast<ANoirCharacter>(GetPawn());
	if (!Detective || Menu || Puzzle || !GEngine || !GEngine->GameViewport) return;
	if (Map) ToggleMap();
	SAssignNew(CaseNotebook, SNoirCaseNotebook).Detective(Detective).Owner(this).OnClose([this]() { ToggleCaseNotebook(); });
	GEngine->GameViewport->AddViewportWidgetContent(CaseNotebook.ToSharedRef(), 40);
	bShowMouseCursor = true;
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(CaseNotebook);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	FSlateApplication::Get().SetKeyboardFocus(CaseNotebook, EFocusCause::SetDirectly);
}

void ANoirPlayerController::OpenPuzzle(const FNoirClue& Clue)
{
	if (Puzzle || Menu || !GEngine || !GEngine->GameViewport || !Clue.Puzzle.IsOpen())
	{
		return;
	}
	const FString Id = Clue.Id;
	SAssignNew(Puzzle, SNoirPuzzle)
		.Clue(Clue)
		.OnSolved([this, Id]()
		{
			if (ANoirCharacter* Detective = Cast<ANoirCharacter>(GetPawn())) Detective->SolvePuzzle(Id);
		})
		.OnClose([this]() { ClosePuzzle(); });
	GEngine->GameViewport->AddViewportWidgetContent(Puzzle.ToSharedRef(), 40);
	bShowMouseCursor = true;
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Puzzle);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void ANoirPlayerController::ClosePuzzle()
{
	if (!Puzzle) return;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Puzzle.ToSharedRef());
	}
	Puzzle.Reset();
	if (ANoirCharacter* Detective = Cast<ANoirCharacter>(GetPawn()))
	{
		Detective->RestoreInputMode();
	}
}

void ANoirPlayerController::SolvePuzzleForTest()
{
	if (Puzzle) Puzzle->SolveForTest();
}

void ANoirPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Puzzle && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Puzzle.ToSharedRef());
	}
	Puzzle.Reset();
	if (Map && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Map.ToSharedRef());
	}
	Map.Reset();
	if (CaseNotebook && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(CaseNotebook.ToSharedRef());
	}
	CaseNotebook.Reset();
	if (MenuContainer && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MenuContainer.ToSharedRef());
	}
	Menu.Reset();
	MenuContainer.Reset();
	Super::EndPlay(Reason);
}

void ANoirPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (AActor* Camera = MenuCamera.Get())
	{
		// A slow, breathing drift, as if hand-held by someone very patient.
		MenuTime += DeltaSeconds;
		const float T = MenuTime;
		const FVector Offset(FMath::Sin(T * 0.13f) * 10.f, FMath::Sin(T * 0.09f + 1.f) * 14.f, FMath::Sin(T * 0.11f + 2.f) * 4.f);
		const FRotator Turn(FMath::Sin(T * 0.08f) * 0.6f, FMath::Sin(T * 0.06f + 0.5f) * 1.4f, 0.f);
		Camera->SetActorLocationAndRotation(MenuCameraHome.GetLocation() + Offset,
			(MenuCameraHome.Rotator() + Turn).Quaternion());
	}
}

void ANoirPlayerController::OpenMenu(bool bPause, bool bClosed, const FText& Verdict)
{
	if (Menu || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	if (CaseNotebook)
	{
		ToggleCaseNotebook();
	}
	if (Map)
	{
		ToggleMap();
	}
	if (bPause || bClosed)
	{
		SetPause(true);
	}
	bCaseSpanish = NoirIsSpanish();
	MenuContainer = SAssignNew(Menu, SNoirMenu).bPause(bPause).bClosed(bClosed).Verdict(Verdict).Owner(this);
	GEngine->GameViewport->AddViewportWidgetContent(MenuContainer.ToSharedRef(), 50);
	bShowMouseCursor = true;
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Menu);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	Menu->FocusFirst();
}

void ANoirPlayerController::ShowMainMenu()
{
	OpenMenu(false);
}

void ANoirPlayerController::ShowCaseClosed(int32 Correct, int32 Parts)
{
	FText Verdict = Correct == Parts
		? FText::Format(NoirTr(TEXT("Solved: {0} of {1}. Every part of it."), TEXT("Resuelto: {0} de {1}. Todo.")), Correct, Parts)
		: FText::Format(NoirTr(TEXT("{0} of {1} right. The rest of the truth is in the dossier."), TEXT("{0} de {1} correctas. El resto de la verdad está en el expediente.")), Correct, Parts);
	const ANoirCaseDirector* Director = FindDirector(this);
	const int32 HintsUsed = Director ? Director->GetHintsUsed() : 0;
	const FText HintLine = HintsUsed == 0 ? NoirTr(TEXT("Without a single hint."), TEXT("Sin una sola pista."))
		: HintsUsed == 1 ? NoirTr(TEXT("With one hint."), TEXT("Con una pista."))
		: FText::Format(NoirTr(TEXT("With {0} hints."), TEXT("Con {0} pistas.")), HintsUsed);
	Verdict = FText::Format(INVTEXT("{0} {1}"), Verdict, HintLine);
	UNoirSaveGame::Delete(); // the case is closed: nothing to continue
	OpenMenu(true, true, Verdict);
}

void ANoirPlayerController::TogglePauseMenu()
{
	if (InMenuMap(GetWorld()))
	{
		return;
	}
	if (Menu)
	{
		CloseMenu();
	}
	else
	{
		// Pausing is a safe moment to keep the case: quitting from here loses nothing.
		if (ANoirCaseDirector* Director = FindDirector(this)) Director->SaveCase();
		OpenMenu(true);
	}
}

void ANoirPlayerController::CloseMenu()
{
	if (!Menu || InMenuMap(GetWorld()))
	{
		return;
	}
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MenuContainer.ToSharedRef());
	}
	Menu.Reset();
	MenuContainer.Reset();
	// Settings may have switched the menus' language; the case keeps its own.
	NoirSetSpanish(bCaseSpanish);
	SetPause(false);
	if (ANoirCharacter* Detective = Cast<ANoirCharacter>(GetPawn()))
	{
		Detective->RestoreInputMode();
	}
	else
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
	}
}

void ANoirPlayerController::ShowMenuPage(const FString& Page)
{
	if (Menu)
	{
		Menu->ShowPage(Page);
	}
}

void ANoirPlayerController::StartCase(const FString& Seed, const FString& Difficulty, bool bSpanish, const FString& Theme)
{
	// Seeds travel in the map URL, so keep them to characters that survive it.
	FString Clean;
	for (const TCHAR C : Seed.TrimStartAndEnd())
	{
		if (FChar::IsAlnum(C) || C == TEXT('-') || C == TEXT('_')) Clean.AppendChar(C);
		else if (C == TEXT(' ')) Clean.AppendChar(TEXT('-'));
	}
	FString Options = FString::Printf(TEXT("Difficulty=%s?Lang=%s"), *Difficulty, bSpanish ? TEXT("es") : TEXT("en"));
	if (!Clean.IsEmpty())
	{
		Options += TEXT("?Seed=") + Clean;
	}
	SetPause(false);
	// Each location is its own level; its director knows its theme.
	UGameplayStatics::OpenLevel(this, FName(Theme == TEXT("express") ? TrainMap : CaseMap), true, Options);
}

void ANoirPlayerController::ContinueCase()
{
	const UNoirSaveGame* Save = UNoirSaveGame::Load();
	if (!Save) return;
	const FString Options = FString::Printf(TEXT("Seed=%s?Difficulty=%s?Lang=%s?Continue=1"), *Save->Seed, *Save->Difficulty,
		Save->bSpanish ? TEXT("es") : TEXT("en"));
	SetPause(false);
	UGameplayStatics::OpenLevel(this, FName(Save->Theme == TEXT("express") ? TrainMap : CaseMap), true, Options);
}

void ANoirPlayerController::RestartCase()
{
	if (const ANoirCaseDirector* Director = FindDirector(this))
	{
		UNoirSaveGame::Delete(); // from the beginning: the old progress goes
		StartCase(Director->GetPlayedSeed(), Director->Difficulty, Director->Language == ENoirCaseLanguage::Spanish, Director->Theme);
	}
}

void ANoirPlayerController::QuitToMainMenu()
{
	SetPause(false);
	UGameplayStatics::OpenLevel(this, FName(MenuMap));
}

void ANoirPlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ANoirPlayerController::TakeHint()
{
	ANoirCaseDirector* Director = FindDirector(this);
	const FText Hint = Director ? Director->TakeHint() : FText();
	if (Hint.IsEmpty())
	{
		return;
	}
	CloseMenu();
	if (ANoirCharacter* Detective = Cast<ANoirCharacter>(GetPawn()))
	{
		Detective->ShowMessage(Hint);
	}
}

int32 ANoirPlayerController::GetHintsLeft() const
{
	const ANoirCaseDirector* Director = FindDirector(this);
	return Director ? Director->GetHintsLeft() : 0;
}

FText ANoirPlayerController::GetCaseTitle() const
{
	const ANoirCaseDirector* Director = FindDirector(this);
	return Director ? Director->GetCaseTitle() : FText();
}

FString ANoirPlayerController::DescribeInput() const
{
	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport.Get() : nullptr;
	const TSharedPtr<SWidget> Focus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	return FString::Printf(TEXT("ignore input %d, mouse capture %d, focus %s, paused %d, pawn %s at %s, move ignored %d, cursor %d"),
		Viewport ? Viewport->IgnoreInput() : -1,
		Viewport ? static_cast<int32>(Viewport->GetMouseCaptureMode()) : -1,
		Focus ? *Focus->GetTypeAsString() : TEXT("none"),
		IsPaused(), GetPawn() ? *GetPawn()->GetName() : TEXT("none"), GetPawn() ? *GetPawn()->GetActorLocation().ToCompactString() : TEXT("-"),
		IsMoveInputIgnored(), bShowMouseCursor);
}

FString ANoirPlayerController::GetCaseTheme() const
{
	const ANoirCaseDirector* Director = FindDirector(this);
	return Director ? Director->Theme : FString(TEXT("gothic"));
}

FString ANoirPlayerController::GetCaseSeed() const
{
	const ANoirCaseDirector* Director = FindDirector(this);
	return Director ? Director->GetPlayedSeed() : FString();
}
