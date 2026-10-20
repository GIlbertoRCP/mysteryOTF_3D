#include "NoirCharacter.h"

#include "NoirPlayerController.h"
#include "NoirSettings.h"

#include "NoirCaseContent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "NoirCase.h"
#include "NoirEvidence.h"
#include "NoirInteractable.h"
#include "NoirMindPlace.h"
#include "NoirSuspect.h"

#define LOCTEXT_NAMESPACE "NoirCharacter"

ANoirCharacter::ANoirCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(80.f);

	Torch = CreateDefaultSubobject<USpotLightComponent>(TEXT("Torch"));
	Torch->SetupAttachment(GetCapsuleComponent());
	Torch->SetUsingAbsoluteLocation(true);
	Torch->SetUsingAbsoluteRotation(true);
	Torch->SetIntensityUnits(ELightUnits::Candelas);
	Torch->SetIntensity(12.f);
	Torch->SetUseTemperature(true);
	Torch->SetTemperature(4300.f);
	Torch->SetInnerConeAngle(7.f);
	Torch->SetOuterConeAngle(16.f);
	Torch->SetAttenuationRadius(1500.f);
	Torch->SetSourceRadius(2.f);
	Torch->SetVolumetricScatteringIntensity(1.f);

	InspectLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("InspectLight"));
	InspectLight->SetupAttachment(Camera);
	InspectLight->SetRelativeLocation(FVector(0.f, 25.f, 20.f));  // above and right, like a lamp at the shoulder
	InspectLight->SetIntensityUnits(ELightUnits::Candelas);
	InspectLight->SetIntensity(1.5f);
	InspectLight->SetUseTemperature(true);
	InspectLight->SetTemperature(2800.f);
	InspectLight->SetAttenuationRadius(150.f);
	InspectLight->SetSourceRadius(5.f);
	InspectLight->SetVisibility(false);

	// Depth of field is only switched on while inspecting.
	Camera->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
	Camera->PostProcessSettings.DepthOfFieldFstop = 2.f;
	Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
	Camera->PostProcessSettings.DepthOfFieldFocalDistance = 0.f;  // 0 disables
	Camera->PostProcessSettings.VignetteIntensity = 1.1f;  // switched on for the reveal

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = 220.f;  // a detective walks; this is not a shooter
	Movement->BrakingDecelerationWalking = 1200.f;
	Movement->GroundFriction = 6.f;

	bUseControllerRotationYaw = true;
}

void ANoirCharacter::BeginPlay()
{
	Super::BeginPlay();
	Torch->SetWorldLocationAndRotation(Camera->GetComponentLocation(), Camera->GetComponentRotation());
}

void ANoirCharacter::CreateInput()
{
	// Built in code rather than as assets so the bindings live next to the handlers.
	// Move them to InputAction/MappingContext assets once rebinding or a settings menu is needed.
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};
	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	InteractAction = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	TorchAction = MakeAction(TEXT("IA_Torch"), EInputActionValueType::Boolean);
	ZoomAction = MakeAction(TEXT("IA_Zoom"), EInputActionValueType::Axis1D);
	PresentAction = MakeAction(TEXT("IA_Present"), EInputActionValueType::Boolean);
	ClickAction = MakeAction(TEXT("IA_BoardClick"), EInputActionValueType::Boolean);
	TieAction = MakeAction(TEXT("IA_BoardTie"), EInputActionValueType::Boolean);
	AccuseAction = MakeAction(TEXT("IA_BoardAccuse"), EInputActionValueType::Boolean);
	MindAction = MakeAction(TEXT("IA_MindPlace"), EInputActionValueType::Boolean);
	PauseAction = MakeAction(TEXT("IA_Pause"), EInputActionValueType::Boolean);
	WorkAction = MakeAction(TEXT("IA_WorkOnIt"), EInputActionValueType::Boolean);
	MapAction = MakeAction(TEXT("IA_Map"), EInputActionValueType::Boolean);
	ChoiceActions.Reset();
	for (int32 Digit = 0; Digit <= 9; ++Digit)
	{
		ChoiceActions.Add(MakeAction(*FString::Printf(TEXT("IA_Choice%d"), Digit), EInputActionValueType::Boolean));
	}

	InputMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Detective"));
	auto Map = [this](const UInputAction* Action, const FKey& Key, bool bSwizzle = false, bool bNegate = false)
	{
		FEnhancedActionKeyMapping& Mapping = InputMapping->MapKey(Action, Key);
		if (bSwizzle)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(InputMapping));
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(InputMapping));
		}
	};
	// Move: X is right, Y is forward.
	Map(MoveAction, EKeys::W, true);
	Map(MoveAction, EKeys::S, true, true);
	Map(MoveAction, EKeys::D);
	Map(MoveAction, EKeys::A, false, true);
	Map(MoveAction, EKeys::Gamepad_Left2D);
	Map(LookAction, EKeys::Mouse2D);
	Map(LookAction, EKeys::Gamepad_Right2D);
	Map(InteractAction, EKeys::E);
	Map(InteractAction, EKeys::Gamepad_FaceButton_Bottom);
	Map(TorchAction, EKeys::F);
	Map(TorchAction, EKeys::Gamepad_FaceButton_Top);
	Map(ZoomAction, EKeys::MouseWheelAxis);
	Map(PresentAction, EKeys::P);
	Map(ClickAction, EKeys::LeftMouseButton);
	Map(TieAction, EKeys::RightMouseButton);
	Map(AccuseAction, EKeys::A); // only acts at the board, where A does not walk
	Map(MindAction, EKeys::Tab);
	Map(MindAction, EKeys::Gamepad_Special_Left);
	// Esc (in the editor, Esc stops play-in-editor; F10 pauses there).
	Map(PauseAction, EKeys::Escape);
	Map(PauseAction, EKeys::F10);
	Map(PauseAction, EKeys::Gamepad_Special_Right);
	Map(WorkAction, EKeys::R);
	Map(WorkAction, EKeys::Gamepad_FaceButton_Left);
	Map(MapAction, EKeys::M);
	Map(MapAction, EKeys::Gamepad_DPad_Up);
	const FKey DigitKeys[] = {EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
	for (int32 Digit = 0; Digit <= 9; ++Digit)
	{
		Map(ChoiceActions[Digit], DigitKeys[Digit]);
	}
}

void ANoirCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	CreateInput();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Input->AddMappingContext(InputMapping, 0);
		}
	}

	UEnhancedInputComponent* Enhanced = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ANoirCharacter::Move);
	Enhanced->BindAction(LookAction, ETriggerEvent::Triggered, this, &ANoirCharacter::Look);
	Enhanced->BindAction(InteractAction, ETriggerEvent::Started, this, &ANoirCharacter::Interact);
	Enhanced->BindAction(TorchAction, ETriggerEvent::Started, this, &ANoirCharacter::ToggleTorch);
	Enhanced->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ANoirCharacter::Zoom);
	Enhanced->BindAction(PresentAction, ETriggerEvent::Started, this, &ANoirCharacter::TogglePresent);
	Enhanced->BindAction(ClickAction, ETriggerEvent::Started, this, &ANoirCharacter::BoardClickStarted);
	Enhanced->BindAction(ClickAction, ETriggerEvent::Completed, this, &ANoirCharacter::BoardClickCompleted);
	Enhanced->BindAction(TieAction, ETriggerEvent::Started, this, &ANoirCharacter::BoardTie);
	Enhanced->BindAction(AccuseAction, ETriggerEvent::Started, this, &ANoirCharacter::BoardAccuse);
	Enhanced->BindAction(MindAction, ETriggerEvent::Started, this, &ANoirCharacter::ToggleMindPlace);
	Enhanced->BindAction(PauseAction, ETriggerEvent::Started, this, &ANoirCharacter::Pause);
	Enhanced->BindAction(WorkAction, ETriggerEvent::Started, this, &ANoirCharacter::WorkOnInspected);
	Enhanced->BindAction(MapAction, ETriggerEvent::Started, this, &ANoirCharacter::OpenMap);
	for (int32 Digit = 0; Digit <= 9; ++Digit)
	{
		Enhanced->BindAction(ChoiceActions[Digit], ETriggerEvent::Started, this, &ANoirCharacter::Choose, Digit);
	}
}

void ANoirCharacter::Move(const FInputActionValue& Value)
{
	if (IsInspecting() || IsInterviewing() || IsAccusing() || IsOnBoard())
	{
		return;
	}
	const FVector2D Input = Value.Get<FVector2D>();
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Input.X);
}

void ANoirCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	if (IsInspecting())
	{
		TurnInspected(-FMath::RadiansToDegrees(Input.X * InspectTurnRate), FMath::RadiansToDegrees(Input.Y * InspectTurnRate));
		return;
	}
	if (IsInterviewing() || IsAccusing() || IsOnBoard())
	{
		return;  // the camera is directed (or the mouse is working the board)
	}
	const UNoirSettings* Settings = UNoirSettings::Get();
	AddControllerYawInput(Input.X * Settings->LookSensitivity);
	AddControllerPitchInput(-Input.Y * Settings->LookSensitivity * (Settings->bInvertLook ? -1.f : 1.f));
}

void ANoirCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Torch trails the view a little, from just below and right of the eye.
	const FVector Hand = Camera->GetComponentTransform().TransformPosition(FVector(10.f, 14.f, -12.f));
	const FRotator Target = Camera->GetComponentRotation();
	const FRotator Current = Torch->GetComponentRotation();
	Torch->SetWorldLocationAndRotation(Hand, FMath::RInterpTo(Current, Target, DeltaSeconds, TorchFollowSpeed));

	if (IsInspecting())
	{
		UpdateInspect(DeltaSeconds);
	}
	else if (IsInterviewing())
	{
		UpdateInterview(DeltaSeconds);
	}
	else if (IsAccusing())
	{
		UpdateAccusation(DeltaSeconds);
	}
	else if (IsOnBoard())
	{
		UpdateBoard(DeltaSeconds);
	}
	else
	{
		UpdateFocus();
	}
}

// ─── The Mind Place ───

void ANoirCharacter::ToggleMindPlace()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || bMindTravelling || IsInspecting() || IsInterviewing() || IsAccusing() || IsOnBoard() || !ANoirMindPlace::Find(this))
	{
		return;
	}
	// A quick dip to black, the move, then back up.
	bMindTravelling = true;
	PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.18f, FLinearColor::Black, false, true);
	GetWorldTimerManager().SetTimer(MindFadeTimer, this, &ANoirCharacter::MindPlaceTravel, 0.2f, false);
}

void ANoirCharacter::MindPlaceTravel()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	ANoirMindPlace* MindPlace = ANoirMindPlace::Find(this);
	bMindTravelling = false;
	if (!PC || !MindPlace)
	{
		return;
	}
	GetCharacterMovement()->StopMovementImmediately();
	if (!bInMindPlace)
	{
		MindReturnLocation = GetActorLocation();
		MindReturnRotation = PC->GetControlRotation();
		const FTransform Entry = MindPlace->GetEntryTransform();
		TeleportTo(Entry.GetLocation(), Entry.Rotator());
		PC->SetControlRotation(Entry.Rotator());
		bTorchBeforeMind = Torch->IsVisible();
		Torch->SetVisibility(false);
		bInMindPlace = true;
		MindPlace->OnEnter(this);
	}
	else
	{
		MindPlace->OnLeave(this);
		TeleportTo(MindReturnLocation, FRotator(0.f, MindReturnRotation.Yaw, 0.f));
		PC->SetControlRotation(MindReturnRotation);
		Torch->SetVisibility(bTorchBeforeMind);
		bInMindPlace = false;
	}
	Torch->SetWorldLocationAndRotation(Camera->GetComponentLocation(), Camera->GetComponentRotation());
	PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.35f, FLinearColor::Black, false, false);
}

// ─── The case board ───

void ANoirCharacter::BeginBoard(ANoirCase* Board)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!Board || !PC || IsInspecting() || IsInterviewing() || IsAccusing() || IsOnBoard())
	{
		return;
	}
	OnBoard = Board;
	HoveredCard.Reset();
	DraggedCard.Reset();
	PickedCard.Reset();
	Board->SyncCards(KnownClues);
	FocusedActor.Reset();
	GetCharacterMovement()->StopMovementImmediately();

	bTorchBeforeBoard = Torch->IsVisible();
	Torch->SetVisibility(false); // the board's own lamps light it; a torch at arm's length only glares
	PC->SetViewTargetWithBlend(Board, 0.6f, VTBlend_Cubic);
	PC->bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	PC->SetInputMode(Mode);
}

const FNoirClue* ANoirCharacter::GetInspectedPuzzle() const
{
	const ANoirEvidence* Evidence = Inspected.Get();
	if (!Evidence) return nullptr;
	const FString Id = Evidence->CaseEvidenceId.IsEmpty() ? Evidence->GetName() : Evidence->CaseEvidenceId;
	const FNoirClue* Known = KnownClues.FindByPredicate([&Id](const FNoirClue& C) { return C.Id == Id; });
	return Known && Known->Puzzle.IsOpen() ? Known : nullptr;
}

void ANoirCharacter::WorkOnInspected()
{
	ANoirPlayerController* PC = Cast<ANoirPlayerController>(GetController());
	const FNoirClue* Clue = GetInspectedPuzzle();
	if (Clue && PC)
	{
		PC->OpenPuzzle(*Clue);
	}
}

void ANoirCharacter::SolvePuzzle(const FString& ClueId)
{
	FNoirClue* Clue = KnownClues.FindByPredicate([&ClueId](const FNoirClue& C) { return C.Id == ClueId; });
	if (!Clue || !Clue->Puzzle.IsOpen()) return;
	Clue->Puzzle.Type = NAME_None;
	Clue->Text = Clue->Puzzle.Reveal;
	AddClue(FText::Format(LOCTEXT("SolvedNote", "{0}: {1}"), Clue->Title, Clue->Text));
	ShowMessage(FText::Format(NoirTr(TEXT("Worked out: {0}"), TEXT("Resuelto: {0}")), Clue->Title));
	// The board's card says it all now too.
	ANoirMindPlace* Mind = ANoirMindPlace::Find(this);
	if (Mind && Mind->Board)
	{
		Mind->Board->SyncCards(KnownClues);
	}
}

FTransform ANoirCharacter::GetResumeTransform(FRotator& OutLooking) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (bInMindPlace)
	{
		OutLooking = MindReturnRotation;
		return FTransform(FRotator(0.f, MindReturnRotation.Yaw, 0.f), MindReturnLocation);
	}
	OutLooking = PC ? PC->GetControlRotation() : GetActorRotation();
	return GetActorTransform();
}

bool ANoirCharacter::IsTorchOn() const
{
	return bInMindPlace ? bTorchBeforeMind : Torch->IsVisible();
}

void ANoirCharacter::SetTorchOn(bool bOn)
{
	Torch->SetVisibility(bOn);
}

void ANoirCharacter::RestoreKnowledge(const TArray<FNoirClue>& Known, const TArray<FText>& Notes)
{
	KnownClues = Known;
	Clues = Notes;
}

void ANoirCharacter::OpenMap()
{
	if (IsInspecting() || IsInterviewing() || IsAccusing() || IsOnBoard()) return;
	if (ANoirPlayerController* PC = Cast<ANoirPlayerController>(GetController()))
	{
		PC->ToggleMap();
	}
}

void ANoirCharacter::Pause()
{
	if (ANoirPlayerController* PC = Cast<ANoirPlayerController>(GetController()))
	{
		PC->TogglePauseMenu();
	}
}

void ANoirCharacter::RestoreInputMode()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	if (IsOnBoard())
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		PC->SetInputMode(Mode);
	}
	else
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void ANoirCharacter::EndBoard()
{
	if (ANoirCase* Board = OnBoard.Get())
	{
		Board->SetHighlight(FString(), FString());
	}
	if (OnBoard.IsValid())
	{
		Torch->SetVisibility(bTorchBeforeBoard);
	}
	OnBoard.Reset();
	DraggedCard.Reset();
	PickedCard.Reset();
	HoveredCard.Reset();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetViewTargetWithBlend(this, 0.4f, VTBlend_Cubic);
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
}

bool ANoirCharacter::CursorOnBoard(FVector2D& OutPosition) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	FVector Origin, Direction;
	return PC && OnBoard.IsValid() && PC->DeprojectMousePositionToWorld(Origin, Direction) &&
		OnBoard->RayToBoard(Origin, Direction, OutPosition);
}

void ANoirCharacter::UpdateBoard(float DeltaSeconds)
{
	ANoirCase* Board = OnBoard.Get();
	FVector2D Cursor;
	if (!CursorOnBoard(Cursor))
	{
		return;
	}
	if (!DraggedCard.IsEmpty())
	{
		Board->MoveCard(DraggedCard, Cursor + DragOffset);
	}
	else
	{
		HoveredCard = Board->CardAt(Cursor);
	}
	Board->SetHighlight(DraggedCard.IsEmpty() ? HoveredCard : DraggedCard, PickedCard);
}

void ANoirCharacter::BoardClickStarted()
{
	FVector2D Cursor;
	if (!IsOnBoard() || !CursorOnBoard(Cursor))
	{
		return;
	}
	DraggedCard = OnBoard->CardAt(Cursor);
	if (const FNoirBoardCard* Card = OnBoard->FindCard(DraggedCard))
	{
		DragOffset = Card->Position - Cursor;
	}
}

void ANoirCharacter::BoardClickCompleted()
{
	DraggedCard.Reset();
}

void ANoirCharacter::BoardTie()
{
	if (!IsOnBoard())
	{
		return;
	}
	// First right-click picks a card; the second ties (or unties) it to the first.
	if (HoveredCard.IsEmpty() || HoveredCard == PickedCard)
	{
		PickedCard.Reset();
	}
	else if (PickedCard.IsEmpty())
	{
		PickedCard = HoveredCard;
	}
	else
	{
		OnBoard->ToggleString(PickedCard, HoveredCard);
		PickedCard.Reset();
	}
}

void ANoirCharacter::BoardAccuse()
{
	if (ANoirCase* Board = OnBoard.Get())
	{
		EndBoard();
		BeginAccusation(Board);
	}
}

void ANoirCharacter::TurnInspected(float YawDegrees, float PitchDegrees)
{
	const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(YawDegrees));
	const FQuat Pitch(FVector::RightVector, FMath::DegreesToRadians(PitchDegrees));
	InspectRotation = (Yaw * Pitch * InspectRotation).GetNormalized();
}

void ANoirCharacter::Zoom(const FInputActionValue& Value)
{
	if (IsInspecting())
	{
		InspectDistance = FMath::Clamp(InspectDistance * (1.f - 0.1f * Value.Get<float>()), InspectMinDistance, InspectMaxDistance);
	}
}

void ANoirCharacter::BeginInspect(ANoirEvidence* Evidence)
{
	if (!Evidence || IsInspecting())
	{
		return;
	}
	Inspected = Evidence;
	InspectOrigin = Evidence->GetActorTransform();
	// Front (+X) toward the viewer, upright.
	InspectRotation = FQuat(FVector::UpVector, PI);

	// Frame the object so it fills about a third of the view at the narrow FOV.
	const float HalfFOV = FMath::DegreesToRadians(InspectFOV * 0.5f);
	const float Radius = Evidence->GetInspectRadius();
	InspectDistance = FMath::Clamp(Radius / (0.3f * FMath::Tan(HalfFOV)), 12.f, 150.f);
	InspectMinDistance = FMath::Max(10.f, InspectDistance * 0.5f);
	InspectMaxDistance = InspectDistance * 1.8f;

	Evidence->SetActorEnableCollision(false);
	DefaultFOV = Camera->FieldOfView;
	Camera->SetFieldOfView(InspectFOV);
	InspectLight->SetVisibility(true);
	FocusedActor.Reset();
	GetCharacterMovement()->StopMovementImmediately();

	LearnClue(Evidence->MakeClue());
	ShowMessage(Evidence->Description);
}

void ANoirCharacter::EndInspect()
{
	if (ANoirEvidence* Evidence = Inspected.Get())
	{
		Evidence->SetActorTransform(InspectOrigin);
		Evidence->SetActorEnableCollision(true);
	}
	Inspected.Reset();
	Camera->SetFieldOfView(DefaultFOV);
	Camera->PostProcessSettings.DepthOfFieldFocalDistance = 0.f;
	InspectLight->SetVisibility(false);
}

void ANoirCharacter::UpdateInspect(float DeltaSeconds)
{
	ANoirEvidence* Evidence = Inspected.Get();
	const FTransform View = Camera->GetComponentTransform();
	const FVector Target = View.GetLocation() + View.GetRotation().GetForwardVector() * InspectDistance;
	const FQuat TargetRotation = View.GetRotation() * InspectRotation;

	// Ease in from where it was, so picking it up reads as a motion rather than a cut.
	const float Alpha = FMath::Clamp(DeltaSeconds * 12.f, 0.f, 1.f);
	Evidence->SetActorLocationAndRotation(
		FMath::Lerp(Evidence->GetActorLocation(), Target, Alpha),
		FQuat::Slerp(Evidence->GetActorQuat(), TargetRotation, Alpha));
	Camera->PostProcessSettings.DepthOfFieldFocalDistance = InspectDistance;

	FText Detail;
	if (Evidence->DiscoverFacing(-View.GetRotation().GetForwardVector(), Detail))
	{
		AddClue(Detail);
		ShowMessage(FText::Format(NoirTr(TEXT("You notice: {0}"), TEXT("Te fijas en algo: {0}")), Detail));
	}
}

void ANoirCharacter::UpdateFocus()
{
	FocusedActor.Reset();
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * InteractRange;
	// Complex trace: evidence meshes are small merged meshes without simple collision.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NoirInteract), true, this);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->Implements<UNoirInteractable>())
		{
			FocusedActor = HitActor;
		}
	}
}

void ANoirCharacter::Interact()
{
	if (IsInspecting())
	{
		EndInspect();
		return;
	}
	if (IsInterviewing())
	{
		EndInterview();
		return;
	}
	if (IsOnBoard())
	{
		EndBoard();
		return;
	}
	switch (AccusePhase)
	{
	case ENoirAccusePhase::Choosing:
	case ENoirAccusePhase::Dossier:
		EndAccusation();
		return;
	case ENoirAccusePhase::Reveal:
		AccusePhase = ENoirAccusePhase::Dossier;  // skip
		return;
	default:
		break;
	}
	if (AActor* Target = FocusedActor.Get())
	{
		INoirInteractable::Execute_Interact(Target, this);
	}
}

void ANoirCharacter::ToggleTorch()
{
	Torch->ToggleVisibility();
}

void ANoirCharacter::ShowMessage(const FText& InMessage)
{
	Message = InMessage;
	MessageTime = GetWorld()->GetTimeSeconds();
}

float ANoirCharacter::GetMessageAge() const
{
	return GetWorld()->GetTimeSeconds() - MessageTime;
}

void ANoirCharacter::AddClue(const FText& Clue)
{
	if (!Clues.ContainsByPredicate([&Clue](const FText& Existing) { return Existing.EqualTo(Clue); }))
	{
		Clues.Add(Clue);
	}
}

bool ANoirCharacter::LearnClue(const FNoirClue& Clue)
{
	if (Clue.Id.IsEmpty() || KnownClues.ContainsByPredicate([&Clue](const FNoirClue& Known) { return Known.Id == Clue.Id; }))
	{
		return false;
	}
	KnownClues.Add(Clue);
	AddClue(Clue.Text.IsEmpty() ? Clue.Title : FText::Format(LOCTEXT("ClueNote", "{0}: {1}"), Clue.Title, Clue.Text));
	return true;
}

void ANoirCharacter::BeginInterview(ANoirSuspect* Suspect)
{
	if (!Suspect || IsInspecting() || IsInterviewing())
	{
		return;
	}
	Interviewee = Suspect;
	bPresenting = false;
	DialogueLine = Suspect->Greeting;
	FocusedActor.Reset();
	GetCharacterMovement()->StopMovementImmediately();
}

void ANoirCharacter::EndInterview()
{
	Interviewee.Reset();
	bPresenting = false;
	DialogueLine = FText::GetEmpty();
}

void ANoirCharacter::UpdateInterview(float DeltaSeconds)
{
	AController* PC = GetController();
	if (!PC)
	{
		return;
	}
	AimAt(Interviewee->GetFaceLocation(), DeltaSeconds, 5.f);
}

void ANoirCharacter::AimAt(const FVector& Target, float DeltaSeconds, float Speed)
{
	if (AController* PC = GetController())
	{
		const FRotator Wanted = (Target - Camera->GetComponentLocation()).Rotation();
		PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Wanted, DeltaSeconds, Speed));
	}
}

void ANoirCharacter::BeginAccusation(ANoirCase* InCase)
{
	if (!InCase || IsInspecting() || IsInterviewing() || IsAccusing())
	{
		return;
	}
	Case = InCase;
	AccusePhase = ENoirAccusePhase::Choosing;
	AccuseStep = 0;
	AccusePicks.Init(INDEX_NONE, static_cast<int32>(ENoirAccusationPart::Count));
	FocusedActor.Reset();
	GetCharacterMovement()->StopMovementImmediately();
}

void ANoirCharacter::EndAccusation()
{
	if (AccusePhase == ENoirAccusePhase::Reveal || AccusePhase == ENoirAccusePhase::Dossier)
	{
		Camera->SetFieldOfView(DefaultFOV);
		Camera->PostProcessSettings.bOverride_VignetteIntensity = false;
	}
	if (ANoirSuspect* Accused = RevealAccused.Get())
	{
		Accused->SetActorTransform(RevealOrigin); // back to where their alibi put them
	}
	RevealAccused.Reset();
	const bool bFromDossier = AccusePhase == ENoirAccusePhase::Dossier;
	int32 Correct = 0, Parts = 0;
	if (const ANoirCase* Board = Case.Get())
	{
		Parts = Board->NumParts();
		for (int32 Part = 0; Part < Parts && Part < AccusePicks.Num(); ++Part)
		{
			Correct += AccusePicks[Part] == Board->GetSolution(static_cast<ENoirAccusationPart>(Part));
		}
	}
	AccusePhase = ENoirAccusePhase::None;
	Case.Reset();
	if (bFromDossier)
	{
		if (ANoirPlayerController* PC = Cast<ANoirPlayerController>(GetController()))
		{
			PC->ShowCaseClosed(Correct, Parts);
		}
	}
}

void ANoirCharacter::BringIntoView(ANoirSuspect* Accused)
{
	UWorld* World = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NoirReveal), false, this);
	Params.AddIgnoredActor(Accused);
	const FVector Eye = Camera->GetComponentLocation();
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Eye, Accused->GetFaceLocation(), ECC_Visibility, Params))
	{
		return; // already in sight: the camera can simply turn to them
	}

	// Otherwise they step forward: the first clear spot near the detective, behind first so the
	// camera turns round to them, then beside, then ahead.
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = GetActorRightVector().GetSafeNormal2D();
	const FVector Directions[] = {-Forward, (-Forward + Right).GetSafeNormal(), (-Forward - Right).GetSafeNormal(), Right, -Right, Forward};
	const FCollisionShape Body = FCollisionShape::MakeCapsule(32.f, 86.f);
	for (const float Distance : {230.f, 170.f})
	{
		for (const FVector& Direction : Directions)
		{
			const FVector Spot = GetActorLocation() + Direction * Distance;
			const FVector Face = Spot + FVector(0, 0, 70.f);
			if (World->OverlapBlockingTestByChannel(Spot, FQuat::Identity, ECC_Pawn, Body, Params) ||
				World->LineTraceSingleByChannel(Hit, Eye, Face, ECC_Visibility, Params))
			{
				continue;
			}
			RevealAccused = Accused;
			RevealOrigin = Accused->GetActorTransform();
			const FRotator Facing(0.f, (GetActorLocation() - Spot).Rotation().Yaw, 0.f);
			Accused->SetActorLocationAndRotation(Spot, Facing);
			return;
		}
	}
}

void ANoirCharacter::ChooseAccusation(int32 Option)
{
	ANoirCase* TheCase = Case.Get();
	if (!TheCase || AccusePhase != ENoirAccusePhase::Choosing)
	{
		return;
	}
	const int32 Parts = TheCase->NumParts();
	if (Option == 0)
	{
		AccuseStep = FMath::Max(0, AccuseStep - 1);
		return;
	}
	if (AccuseStep < Parts)
	{
		if (TheCase->GetOptions(static_cast<ENoirAccusationPart>(AccuseStep)).IsValidIndex(Option - 1))
		{
			AccusePicks[AccuseStep++] = Option - 1;
		}
		return;
	}
	if (Option == 2)
	{
		AccuseStep = 0;  // start over
		return;
	}
	if (Option == 1)
	{
		RevealLines = TheCase->GetRevealLines(AccusePicks);
		RevealTime = 0.f;
		if (TheCase->Suspects.IsValidIndex(AccusePicks[0]) && TheCase->Suspects[AccusePicks[0]])
		{
			BringIntoView(TheCase->Suspects[AccusePicks[0]]);
		}
		DefaultFOV = Camera->FieldOfView;
		Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
		AccusePhase = ENoirAccusePhase::Reveal;
	}
}

void ANoirCharacter::UpdateAccusation(float DeltaSeconds)
{
	if (AccusePhase != ENoirAccusePhase::Reveal)
	{
		return;
	}
	RevealTime += DeltaSeconds;
	const float Total = RevealLineSeconds * RevealLines.Num();

	// Slow push-in on the accused while the story is told.
	const ANoirCase* TheCase = Case.Get();
	const int32 Accused = AccusePicks[0];
	if (TheCase && TheCase->Suspects.IsValidIndex(Accused) && TheCase->Suspects[Accused])
	{
		AimAt(TheCase->Suspects[Accused]->GetFaceLocation(), DeltaSeconds, 2.f);
	}
	const float Push = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(RevealTime / FMath::Max(Total, 1.f), 0.f, 1.f));
	Camera->SetFieldOfView(FMath::Lerp(DefaultFOV, RevealFOV, Push));

	if (RevealTime > Total + 1.f)
	{
		AccusePhase = ENoirAccusePhase::Dossier;
	}
}

FText ANoirCharacter::GetCurrentRevealLine() const
{
	const int32 Index = FMath::FloorToInt(RevealTime / RevealLineSeconds);
	return RevealLines.IsValidIndex(Index) ? RevealLines[Index] : FText::GetEmpty();
}

float ANoirCharacter::GetLetterbox() const
{
	switch (AccusePhase)
	{
	case ENoirAccusePhase::Reveal: return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(RevealTime / 1.5f, 0.f, 1.f));
	case ENoirAccusePhase::Dossier: return 1.f;
	default: return 0.f;
	}
}

void ANoirCharacter::TogglePresent()
{
	if (!IsInterviewing())
	{
		return;
	}
	if (!bPresenting && KnownClues.IsEmpty())
	{
		ShowMessage(NoirTr(TEXT("You have no evidence to show yet."), TEXT("Aún no tienes pruebas que mostrar.")));
		return;
	}
	bPresenting = !bPresenting;
	PresentPage = 0;
}

namespace
{
	/** The evidence list shows 8 clues a page; key 9 turns the page. */
	constexpr int32 CluesPerPage = 8;
}

TArray<FNoirInterviewOption> ANoirCharacter::GetInterviewOptions() const
{
	TArray<FNoirInterviewOption> Options;
	const ANoirSuspect* Suspect = Interviewee.Get();
	if (!Suspect)
	{
		return Options;
	}
	if (bPresenting)
	{
		const int32 First = PresentPage * CluesPerPage;
		for (int32 i = First; i < KnownClues.Num() && i < First + CluesPerPage; ++i)
		{
			Options.Add({KnownClues[i].Title.ToString(), false});
		}
		if (KnownClues.Num() > CluesPerPage)
		{
			Options.Add({NoirTrS(TEXT("More..."), TEXT("Más...")), true});
		}
	}
	else
	{
		for (int32 Topic : Suspect->GetAvailableTopics())
		{
			Options.Add({Suspect->Topics[Topic].Question.ToString(), Suspect->WasAsked(Topic)});
		}
	}
	return Options;
}

void ANoirCharacter::Choose(int32 Option)
{
	if (IsAccusing())
	{
		ChooseAccusation(Option);
		return;
	}
	ANoirSuspect* Suspect = Interviewee.Get();
	if (!Suspect)
	{
		return;
	}
	if (Option == 0)
	{
		bPresenting = false;
		return;
	}
	const int32 Index = Option - 1;
	if (bPresenting)
	{
		if (Index == CluesPerPage && KnownClues.Num() > CluesPerPage)
		{
			const int32 Pages = (KnownClues.Num() + CluesPerPage - 1) / CluesPerPage;
			PresentPage = (PresentPage + 1) % Pages;
			return;
		}
		const int32 ClueIndex = PresentPage * CluesPerPage + Index;
		if (Index < CluesPerPage && KnownClues.IsValidIndex(ClueIndex))
		{
			DialogueLine = Suspect->Present(KnownClues[ClueIndex], this);
			bPresenting = false;
		}
	}
	else
	{
		const TArray<int32> Topics = Suspect->GetAvailableTopics();
		if (Topics.IsValidIndex(Index))
		{
			DialogueLine = Suspect->Ask(Topics[Index], this);
		}
	}
}

#undef LOCTEXT_NAMESPACE
