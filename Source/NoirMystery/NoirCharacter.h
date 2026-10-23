#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NoirClue.h"
#include "NoirCharacter.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UPointLightComponent;
class ANoirEvidence;
class ANoirSuspect;
class ANoirCase;

/** Where the detective is in making an accusation. */
UENUM()
enum class ENoirAccusePhase : uint8
{
	None,
	Choosing,  // answering who / why / with what / whose alibi, then confirming
	Reveal,    // letterboxed, camera on the accused, the story plays out
	Dossier,   // the verdict and the true timeline
};
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** One line of the interview menu as the HUD shows it. */
struct FNoirInterviewOption
{
	FString Label;
	bool bDim = false;  // already asked
};

/** First-person detective: slow walk, hand-held torch, and look-at interaction. */
UCLASS()
class NOIRMYSTERY_API ANoirCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ANoirCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** The interactable under the crosshair, if any. */
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	/** Shows a line of text on screen for a few seconds (search results and the like). */
	void ShowMessage(const FText& Message);
	const FText& GetMessage() const { return Message; }
	float GetMessageAge() const;

	/** Records a clue in the detective's notes; ignores duplicates. */
	void AddClue(const FText& Clue);
	const TArray<FText>& GetClues() const { return Clues; }

	/** Brings the evidence up in front of the camera to be turned over; E puts it back. */
	void BeginInspect(ANoirEvidence* Evidence);
	UFUNCTION(BlueprintCallable, Category = "Inspect")
	void EndInspect();
	ANoirEvidence* GetInspected() const { return Inspected.Get(); }
	bool IsInspecting() const { return Inspected.IsValid(); }

	/** Turns the inspected object in view space: yaw about the view's up axis, pitch about its right. */
	UFUNCTION(BlueprintCallable, Category = "Inspect")
	void TurnInspected(float YawDegrees, float PitchDegrees);

	/** Records a clue (examined item or case-file page) in the notes; false if already known. */
	bool LearnClue(const FNoirClue& Clue);
	/** Everything known so far, in the order learned; what can be presented to suspects. */
	const TArray<FNoirClue>& GetKnownClues() const { return KnownClues; }

	/** Faces the suspect and opens the interview menu; E leaves. */
	void BeginInterview(ANoirSuspect* Suspect);
	UFUNCTION(BlueprintCallable, Category = "Interview")
	void EndInterview();
	ANoirSuspect* GetInterviewee() const { return Interviewee.Get(); }
	bool IsInterviewing() const { return Interviewee.IsValid(); }

	/** Picks menu option 1-9 (0 goes back from the evidence list). Bound to the number keys. */
	UFUNCTION(BlueprintCallable, Category = "Interview")
	void Choose(int32 Option);

	/** Switches the menu between questions and the evidence to present. Bound to P. */
	UFUNCTION(BlueprintCallable, Category = "Interview")
	void TogglePresent();

	bool IsPresenting() const { return bPresenting; }
	/** In Present: the statement being challenged (a topic index), or INDEX_NONE while choosing it. */
	int32 GetChallengedTopic() const { return ChallengedTopic; }
	/** In Present: still picking which statement to challenge (generated-case suspects). */
	bool IsChoosingStatement() const;
	TArray<FNoirInterviewOption> GetInterviewOptions() const;
	const FText& GetDialogueLine() const { return DialogueLine; }

	/** For playtests: what the suspect last said, and the ids of everything known, in order. */
	UFUNCTION(BlueprintPure, Category = "Interview")
	FString GetDialogueText() const { return DialogueLine.ToString(); }
	UFUNCTION(BlueprintPure, Category = "Interview")
	TArray<FString> GetKnownClueIds() const
	{
		TArray<FString> Ids;
		for (const FNoirClue& C : KnownClues) Ids.Add(C.Id);
		return Ids;
	}

	/** Opens the accusation at the case board. */
	void BeginAccusation(ANoirCase* InCase);

	/** Steps up to the case board: the view moves to it, the mouse moves, pins and ties cards. */
	void BeginBoard(ANoirCase* Board);
	UFUNCTION(BlueprintCallable, Category = "Board")
	void EndBoard();

	/** Into the Mind Place from anywhere in the case, and back to the same spot (Tab). */
	UFUNCTION(BlueprintCallable, Category = "Mind Place")
	void ToggleMindPlace();
	bool IsInMindPlace() const { return bInMindPlace; }

	/** Cursor and input mode for what the detective is doing (after a menu closes). */
	void RestoreInputMode();

	/** Where to put the detective back in a saved case (out of the Mind Place, if in it). */
	FTransform GetResumeTransform(FRotator& OutLooking) const;
	bool IsTorchOn() const;
	UFUNCTION(BlueprintCallable, Category = "Torch")
	void SetTorchOn(bool bOn);
	const TArray<FText>& GetNotes() const { return Clues; }
	/** A saved case's knowledge, put back as it was (no notes or messages added). */
	void RestoreKnowledge(const TArray<FNoirClue>& Known, const TArray<FText>& Notes);

	/** The known clue the inspected item stands for, if it still has a puzzle to work out. */
	const FNoirClue* GetInspectedPuzzle() const;

	/** Opens the inspected item's puzzle (R). */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void WorkOnInspected();

	/** Marks a clue's puzzle solved: the clue now says everything, in the notes and on the board. */
	void SolvePuzzle(const FString& ClueId);
	bool IsOnBoard() const { return OnBoard.IsValid(); }
	ANoirCase* GetBoard() const { return OnBoard.Get(); }
	const FString& GetHoveredCard() const { return HoveredCard; }
	const FString& GetPickedCard() const { return PickedCard; }
	UFUNCTION(BlueprintCallable, Category = "Accusation")
	void EndAccusation();
	ANoirCase* GetCase() const { return Case.Get(); }
	ENoirAccusePhase GetAccusePhase() const { return AccusePhase; }
	bool IsAccusing() const { return AccusePhase != ENoirAccusePhase::None; }
	/** 0-3: the part being chosen; 4: confirming. */
	int32 GetAccuseStep() const { return AccuseStep; }
	const TArray<int32>& GetAccusePicks() const { return AccusePicks; }
	FText GetCurrentRevealLine() const;
	/** 0..1 ease for the letterbox bars. */
	float GetLetterbox() const;

	/** Seconds each reveal line stays on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accusation")
	float RevealLineSeconds = 4.5f;

	/** Field of view the camera narrows to while the story is told. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accusation")
	float RevealFOV = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float InteractRange = 220.f;

	/** How quickly the torch catches up with the view; lower feels heavier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Torch")
	float TorchFollowSpeed = 14.f;

	// ─── Walking and running ───

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 220.f;

	/** Running (Shift held, or the left stick clicked): a hurried jog, not a sprint across a field. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed = 480.f;

	/** Seconds of running from full breath, and seconds to get it all back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SprintSeconds = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float RecoverSeconds = 5.f;

	/** Degrees the view widens at a full run. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SprintFOVKick = 7.f;

	UFUNCTION(BlueprintPure, Category = "Movement")
	bool IsSprinting() const { return bSprinting; }

	/** Breath, 0 (winded) to 1. */
	UFUNCTION(BlueprintPure, Category = "Movement")
	float GetStamina() const { return Stamina; }

	/** For playtests: the board view's zoom (1 = whole board) and centre, as the wheel and WASD set them. */
	UFUNCTION(BlueprintCallable, Category = "Board")
	void SetBoardView(float Zoom, FVector2D Focus) { BoardZoom = FMath::Clamp(Zoom, 1.f, 3.2f); BoardFocus = Focus; }

	/** For playtests: as if Shift were held (or let go). */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetSprintHeld(bool bHeld) { bSprintHeld = bHeld; }

	/** Narrower view while inspecting: larger object, less distortion, shallower focus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inspect")
	float InspectFOV = 40.f;

	/** Radians of turn per unit of look input. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inspect")
	float InspectTurnRate = 0.035f;

protected:
	virtual void BeginPlay() override;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Interact();
	void Zoom(const FInputActionValue& Value);
	void UpdateInspect(float DeltaSeconds);
	void UpdateInterview(float DeltaSeconds);
	void UpdateAccusation(float DeltaSeconds);
	void ChooseAccusation(int32 Option);
	void AimAt(const FVector& Target, float DeltaSeconds, float Speed);
	/** Moves the accused to a clear spot in sight for the reveal if a wall is in the way. */
	void BringIntoView(ANoirSuspect* Accused);
	void ToggleTorch();
	void UpdateFocus();
	void CreateInput();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	/** Not attached to the camera: it lags behind the view so the torch feels hand-held. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpotLightComponent> Torch;

	/** Small warm fill that picks out the object while inspecting. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> InspectLight;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InputMapping;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TorchAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ZoomAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PresentAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ClickAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TieAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AccuseAction;

	void BoardClickStarted();
	void BoardClickCompleted();
	void BoardTie();
	void BoardAccuse();
	void UpdateBoard(float DeltaSeconds);
	bool CursorOnBoard(FVector2D& OutPosition) const;

	TWeakObjectPtr<ANoirCase> OnBoard;
	FString HoveredCard;
	FString DraggedCard;
	FString PickedCard;
	FVector2D DragOffset = FVector2D::ZeroVector;
	bool bTorchBeforeBoard = true;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MindAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> WorkAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MapAction;
	void OpenMap();

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CaseNotebookAction;
	void OpenCaseNotebook();

	// Running: Shift is held, the left stick's click toggles (until the stick is let go).
	UPROPERTY()
	TObjectPtr<UInputAction> SprintAction;
	UPROPERTY()
	TObjectPtr<UInputAction> SprintToggleAction;
	void SprintPressed() { bSprintHeld = true; }
	void SprintReleased() { bSprintHeld = false; }
	void SprintToggle() { bSprintToggled = !bSprintToggled; }
	/** Speed, breath, and the view and torch moving with the stride; returns the torch's sway. */
	FTransform UpdateGait(float DeltaSeconds);
	bool bSprintHeld = false;
	bool bSprintToggled = false;
	bool bSprinting = false;
	/** Out of breath: no running until some of it is back. */
	bool bWinded = false;
	float Stamina = 1.f;
	float SinceSprint = 0.f;
	/** Walk-to-run blend, and stood-still-to-walking blend, both eased. */
	float SprintBlend = 0.f;
	float MoveBlend = 0.f;
	/** Advances with distance walked: one step per half turn. */
	float GaitPhase = 0.f;
	float Lean = 0.f;
	float GaitTime = 0.f;

	// The board view: zoom (1 = whole board) and where it is centred, wanted and eased.
	float BoardZoom = 1.f, BoardZoomNow = 1.f;
	FVector2D BoardFocus = FVector2D::ZeroVector, BoardFocusNow = FVector2D::ZeroVector;
	void Pause();
	void MindPlaceTravel();
	bool bInMindPlace = false;
	bool bMindTravelling = false;
	bool bTorchBeforeMind = true;
	FVector MindReturnLocation = FVector::ZeroVector;
	FRotator MindReturnRotation = FRotator::ZeroRotator;
	FTimerHandle MindFadeTimer;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> ChoiceActions;  // index = digit key

	TWeakObjectPtr<AActor> FocusedActor;
	FText Message;
	float MessageTime = -1000.f;

	TWeakObjectPtr<ANoirEvidence> Inspected;
	FTransform InspectOrigin;   // where the evidence rests in the world
	FQuat InspectRotation;      // object rotation relative to the camera
	float InspectDistance = 30.f;
	float InspectMinDistance = 12.f;
	float InspectMaxDistance = 60.f;
	float DefaultFOV = 80.f;

	UPROPERTY()
	TArray<FText> Clues;

	UPROPERTY()
	TArray<FNoirClue> KnownClues;
	int32 PresentPage = 0;

	TWeakObjectPtr<ANoirSuspect> Interviewee;

	TWeakObjectPtr<ANoirCase> Case;
	ENoirAccusePhase AccusePhase = ENoirAccusePhase::None;
	int32 AccuseStep = 0;
	TArray<int32> AccusePicks;
	TArray<FText> RevealLines;
	float RevealTime = 0.f;
	TWeakObjectPtr<ANoirSuspect> RevealAccused; // moved into view for the reveal
	FTransform RevealOrigin;                     // where they stood before
	bool bPresenting = false;
	int32 ChallengedTopic = INDEX_NONE;
	FText DialogueLine;
};
