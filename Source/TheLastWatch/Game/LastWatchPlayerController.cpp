// The Last Watch

#include "LastWatchPlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/LocalLightComponent.h"
#include "Engine/OverlapResult.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "UObject/UObjectIterator.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/GameModeBase.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Containers/Ticker.h"
#include "UObject/ConstructorHelpers.h"
#include "FootstepComponent.h"
#include "GameDataSubsystem.h"
#include "HorrorUI.h"
#include "Interactables.h"
#include "KeeperSaveSubsystem.h"
#include "InventoryComponent.h"
#include "LastWatchGameMode.h"
#include "LastWatchHUD.h"
#include "LighthouseBeam.h"
#include "StoryActors.h"
#include "StorySubsystem.h"
#include "TheLastWatch.h"

ALastWatchPlayerController::ALastWatchPlayerController()
{
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	Footsteps = CreateDefaultSubobject<UFootstepComponent>(TEXT("Footsteps"));

	// the template's movement/look/sprint input and sprint meter UI
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultIMC(TEXT("/Game/Input/IMC_Default"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> HorrorIMC(TEXT("/Game/Variant_Horror/Input/IMC_Horror"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MouseLookIMC(TEXT("/Game/Input/IMC_MouseLook"));
	static ConstructorHelpers::FClassFinder<UHorrorUI> HorrorUIFinder(TEXT("/Game/Variant_Horror/UI/UI_Horror"));

	if (DefaultIMC.Succeeded()) { DefaultMappingContexts.Add(DefaultIMC.Object); }
	if (HorrorIMC.Succeeded()) { DefaultMappingContexts.Add(HorrorIMC.Object); }
	if (MouseLookIMC.Succeeded()) { MobileExcludedMappingContexts.Add(MouseLookIMC.Object); }
	if (HorrorUIFinder.Succeeded()) { HorrorUIClass = HorrorUIFinder.Class; }
}

static TAutoConsoleVariable<bool> CVarLogPawn(
	TEXT("tlw.LogPawn"),
	false,
	TEXT("Log the keeper's height, movement mode and crouch state every frame"));

/////////////////////////////////////////////////////////////////////////////
// Input

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type = EInputActionValueType::Boolean)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;

		// menus run while the game is paused
		Action->bTriggerWhenPaused = true;
		return Action;
	}

	void MapNegated(UInputMappingContext* Context, UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}
}

void ALastWatchPlayerController::CreateKeeperInput()
{
	InteractAction   = MakeAction(this, TEXT("IA_Interact"));
	JumpAction       = MakeAction(this, TEXT("IA_KeeperJump"));
	CrouchHoldAction = MakeAction(this, TEXT("IA_CrouchHold"));
	CrouchToggleAction = MakeAction(this, TEXT("IA_CrouchToggle"));
	BackAction       = MakeAction(this, TEXT("IA_Back"));
	InventoryAction  = MakeAction(this, TEXT("IA_Inventory"));
	PagePrevAction   = MakeAction(this, TEXT("IA_PagePrev"));
	PageNextAction   = MakeAction(this, TEXT("IA_PageNext"));
	ListUpAction     = MakeAction(this, TEXT("IA_ListUp"));
	ListDownAction   = MakeAction(this, TEXT("IA_ListDown"));
	TuneAction       = MakeAction(this, TEXT("IA_Tune"), EInputActionValueType::Axis1D);
	TuneWheelAction  = MakeAction(this, TEXT("IA_TuneWheel"), EInputActionValueType::Axis1D);
	PauseAction      = MakeAction(this, TEXT("IA_Pause"));
	MenuConfirmAction = MakeAction(this, TEXT("IA_MenuConfirm"));

	ExploreContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Keeper"));
	ExploreContext->MapKey(InteractAction, EKeys::E);
	ExploreContext->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Bottom);
	ExploreContext->MapKey(JumpAction, EKeys::SpaceBar);
	ExploreContext->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Top);
	ExploreContext->MapKey(CrouchHoldAction, EKeys::LeftControl);
	ExploreContext->MapKey(CrouchToggleAction, EKeys::C);
	ExploreContext->MapKey(CrouchToggleAction, EKeys::Gamepad_RightThumbstick);
	ExploreContext->MapKey(InventoryAction, EKeys::Tab);
	ExploreContext->MapKey(InventoryAction, EKeys::I);
	ExploreContext->MapKey(InventoryAction, EKeys::Gamepad_Special_Right);
	ExploreContext->MapKey(PauseAction, EKeys::Escape);
	ExploreContext->MapKey(PauseAction, EKeys::P);
	ExploreContext->MapKey(PauseAction, EKeys::Gamepad_Special_Left);

	OverlayContext = NewObject<UInputMappingContext>(this, TEXT("IMC_KeeperOverlay"));
	OverlayContext->MapKey(PagePrevAction, EKeys::A);
	OverlayContext->MapKey(PagePrevAction, EKeys::Left);
	OverlayContext->MapKey(PagePrevAction, EKeys::Gamepad_DPad_Left);
	OverlayContext->MapKey(PageNextAction, EKeys::D);
	OverlayContext->MapKey(PageNextAction, EKeys::Right);
	OverlayContext->MapKey(PageNextAction, EKeys::Gamepad_DPad_Right);
	OverlayContext->MapKey(ListUpAction, EKeys::W);
	OverlayContext->MapKey(ListUpAction, EKeys::Up);
	OverlayContext->MapKey(ListUpAction, EKeys::Gamepad_DPad_Up);
	OverlayContext->MapKey(ListDownAction, EKeys::S);
	OverlayContext->MapKey(ListDownAction, EKeys::Down);
	OverlayContext->MapKey(ListDownAction, EKeys::Gamepad_DPad_Down);
	OverlayContext->MapKey(BackAction, EKeys::Escape);
	OverlayContext->MapKey(BackAction, EKeys::BackSpace);
	OverlayContext->MapKey(BackAction, EKeys::RightMouseButton);
	OverlayContext->MapKey(BackAction, EKeys::Gamepad_FaceButton_Right);

	RadioContext = NewObject<UInputMappingContext>(this, TEXT("IMC_KeeperRadio"));
	MapNegated(RadioContext, TuneAction, EKeys::A);
	MapNegated(RadioContext, TuneAction, EKeys::Left);
	RadioContext->MapKey(TuneAction, EKeys::D);
	RadioContext->MapKey(TuneAction, EKeys::Right);
	RadioContext->MapKey(TuneAction, EKeys::Gamepad_LeftX);
	RadioContext->MapKey(TuneWheelAction, EKeys::MouseWheelAxis);
	RadioContext->MapKey(BackAction, EKeys::Escape);
	RadioContext->MapKey(BackAction, EKeys::BackSpace);
	RadioContext->MapKey(BackAction, EKeys::RightMouseButton);
	RadioContext->MapKey(BackAction, EKeys::Gamepad_FaceButton_Right);
	// swallow movement keys while tuning
	RadioContext->MapKey(ListUpAction, EKeys::W);
	RadioContext->MapKey(ListDownAction, EKeys::S);

	MenuContext = NewObject<UInputMappingContext>(this, TEXT("IMC_KeeperMenu"));
	for (const FKey& Key : { EKeys::W, EKeys::Up, EKeys::Gamepad_DPad_Up })
	{
		MenuContext->MapKey(ListUpAction, Key);
	}
	for (const FKey& Key : { EKeys::S, EKeys::Down, EKeys::Gamepad_DPad_Down })
	{
		MenuContext->MapKey(ListDownAction, Key);
	}
	for (const FKey& Key : { EKeys::A, EKeys::Left, EKeys::Gamepad_DPad_Left })
	{
		MenuContext->MapKey(PagePrevAction, Key);
	}
	for (const FKey& Key : { EKeys::D, EKeys::Right, EKeys::Gamepad_DPad_Right })
	{
		MenuContext->MapKey(PageNextAction, Key);
	}
	for (const FKey& Key : { EKeys::E, EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom })
	{
		MenuContext->MapKey(MenuConfirmAction, Key);
	}
	for (const FKey& Key : { EKeys::Escape, EKeys::BackSpace, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_Special_Left })
	{
		MenuContext->MapKey(BackAction, Key);
	}
}

void ALastWatchPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	CreateKeeperInput();

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(InteractAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnInteract);
		Input->BindAction(InteractAction, ETriggerEvent::Completed, this, &ALastWatchPlayerController::OnInteractReleased);
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnJumpStarted);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ALastWatchPlayerController::OnJumpCompleted);
		Input->BindAction(CrouchHoldAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnCrouchHeld);
		Input->BindAction(CrouchHoldAction, ETriggerEvent::Completed, this, &ALastWatchPlayerController::OnCrouchReleased);
		Input->BindAction(CrouchToggleAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnCrouchToggle);
		Input->BindAction(BackAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnBack);
		Input->BindAction(InventoryAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnToggleInventory);
		Input->BindAction(PagePrevAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnPagePrev);
		Input->BindAction(PageNextAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnPageNext);
		Input->BindAction(ListUpAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnListUp);
		Input->BindAction(ListDownAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnListDown);
		Input->BindAction(TuneAction, ETriggerEvent::Triggered, this, &ALastWatchPlayerController::OnTune);
		Input->BindAction(TuneWheelAction, ETriggerEvent::Triggered, this, &ALastWatchPlayerController::OnTuneWheel);
		Input->BindAction(PauseAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnPause);
		Input->BindAction(MenuConfirmAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnMenuConfirm);
	}
	else
	{
		UE_LOG(LogTheLastWatch, Error, TEXT("'%s' needs an Enhanced Input Component"), *GetNameSafe(this));
	}

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(ExploreContext, 1);
		}
	}
}

bool ALastWatchPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (CVarLogPawn.GetValueOnGameThread() && Params.Event == IE_Pressed)
	{
		UE_LOG(LogTheLastWatch, Display, TEXT("Key pressed: %s"), *Params.Key.ToString());
	}
	return Super::InputKey(Params);
}

void ALastWatchPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetMode(EKeeperMode::Explore);
	ApplyPlayerSettings();

	// the template horror character ships with jumping and crouching switched off
	if (ACharacter* KeeperCharacter = Cast<ACharacter>(InPawn))
	{
		UCharacterMovementComponent* Movement = KeeperCharacter->GetCharacterMovement();
		Movement->GetNavAgentPropertiesRef().bCanJump = true;
		Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
		Movement->SetCrouchedHalfHeight(CrouchedHalfHeight);
		Movement->MaxWalkSpeedCrouched = CrouchSpeed;

		// grassy hillsides up to 50 degrees are walkable; the cliffs (about 70) still are not
		Movement->SetWalkableFloorAngle(WalkableSlope);
	}
}

void ALastWatchPlayerController::OnCrouchHeld()
{
	if (ACharacter* KeeperCharacter = Cast<ACharacter>(GetPawn()); KeeperCharacter && Mode == EKeeperMode::Explore)
	{
		KeeperCharacter->Crouch();
	}
}

void ALastWatchPlayerController::OnCrouchReleased()
{
	if (ACharacter* KeeperCharacter = Cast<ACharacter>(GetPawn()))
	{
		KeeperCharacter->UnCrouch();
	}
}

void ALastWatchPlayerController::OnCrouchToggle()
{
	if (ACharacter* KeeperCharacter = Cast<ACharacter>(GetPawn()); KeeperCharacter && Mode == EKeeperMode::Explore)
	{
		if (KeeperCharacter->bIsCrouched)
		{
			KeeperCharacter->UnCrouch();
		}
		else
		{
			KeeperCharacter->Crouch();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// Modes

void ALastWatchPlayerController::SetMode(EKeeperMode NewMode)
{
	Mode = NewMode;

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->RemoveMappingContext(OverlayContext);
		Subsystem->RemoveMappingContext(RadioContext);
		Subsystem->RemoveMappingContext(MenuContext);

		if (Mode == EKeeperMode::Reading || Mode == EKeeperMode::Inventory)
		{
			Subsystem->AddMappingContext(OverlayContext, 10);
		}
		else if (Mode == EKeeperMode::Radio)
		{
			Subsystem->AddMappingContext(RadioContext, 10);
		}
		else if (Mode == EKeeperMode::Menu)
		{
			Subsystem->AddMappingContext(MenuContext, 10);
		}
	}

	// ignore flags are counters, so only change them on a real transition
	const bool bShouldBlock = Mode != EKeeperMode::Explore;
	if (bShouldBlock != bMovementBlocked)
	{
		bMovementBlocked = bShouldBlock;
		SetIgnoreMoveInput(bShouldBlock);
		SetIgnoreLookInput(bShouldBlock);
	}

	if (Mode != EKeeperMode::Explore)
	{
		Focused.Reset();
	}

	// the template sprint meter only belongs on the island, not over menus or documents
	if (HorrorUI)
	{
		HorrorUI->SetVisibility(Mode == EKeeperMode::Explore ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void ALastWatchPlayerController::OpenNote(FName NoteId)
{
	OpenNoteId = NoteId;
	NotePage = 0;
	bReadingFromInventory = Mode == EKeeperMode::Inventory;
	SetMode(EKeeperMode::Reading);
}

void ALastWatchPlayerController::EnterRadio(ARadioSet* Radio)
{
	ActiveRadio = Radio;
	Radio->SetUser(this);
	SetMode(EKeeperMode::Radio);
}

void ALastWatchPlayerController::ReturnToExplore()
{
	if (ARadioSet* Radio = ActiveRadio.Get())
	{
		Radio->SetUser(nullptr);
	}
	ActiveRadio.Reset();
	SetSubtitle(FText::GetEmpty(), FText::GetEmpty());
	SetMode(EKeeperMode::Explore);
}

/////////////////////////////////////////////////////////////////////////////
// Focus

void ALastWatchPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (Mode == EKeeperMode::Explore)
	{
		UpdateFocus();
	}
	else if (IsInMenu(EMenuPage::Title))
	{
		UpdateTitleCamera(DeltaTime);
	}

	// hold-to-use: keep looking at the task with E down until the bar fills
	if (AStoryTask* Task = HoldTarget.Get())
	{
		if (Focused.Get() != Task || !Task->CanInteract(this))
		{
			OnInteractReleased();
		}
		else if ((HoldTime += DeltaTime) >= Task->GetHoldSeconds())
		{
			OnInteractReleased();
			Task->Complete(this);
		}
	}

	// walked off a cliff or into the sea
	APawn* KeeperPawn = GetPawn();

	// tlw.LogPawn 1: trace the keeper's movement for automated input tests
	if (CVarLogPawn.GetValueOnGameThread() && KeeperPawn)
	{
		if (const ACharacter* KeeperCharacter = Cast<ACharacter>(KeeperPawn))
		{
			FVector EyeLocation;
			FRotator EyeRotation;
			GetPlayerViewPoint(EyeLocation, EyeRotation);
			FHitResult LookHit;
			FCollisionQueryParams LookParams(SCENE_QUERY_STAT(LookLog), false, KeeperPawn);
			GetWorld()->LineTraceSingleByChannel(LookHit, EyeLocation, EyeLocation + EyeRotation.Vector() * InteractRange, ECC_Visibility, LookParams);
			UE_LOG(LogTheLastWatch, Display, TEXT("Pawn x=%.1f y=%.1f z=%.1f vz=%.1f mode=%d crouched=%d halfHeight=%.1f focus=%s looking=%s"),
				KeeperPawn->GetActorLocation().X, KeeperPawn->GetActorLocation().Y,
				KeeperPawn->GetActorLocation().Z, KeeperPawn->GetVelocity().Z,
				static_cast<int32>(KeeperCharacter->GetCharacterMovement()->MovementMode.GetValue()),
				KeeperCharacter->bIsCrouched,
				KeeperCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
				*GetNameSafe(Focused.Get()), *GetNameSafe(LookHit.GetActor()));

			// pushing but not moving: report what is in the way, once a second
			const FVector Push = KeeperCharacter->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
			const float Now = GetWorld()->GetTimeSeconds();
			if (!Push.IsNearlyZero() && KeeperPawn->GetVelocity().Size2D() < 5.0f && Now - LastBlockReport > 1.0f)
			{
				LastBlockReport = Now;
				const UCapsuleComponent* Capsule = KeeperCharacter->GetCapsuleComponent();
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(BlockReport), false, KeeperPawn);
				const FVector From = KeeperPawn->GetActorLocation();
				if (GetWorld()->SweepSingleByChannel(Hit, From, From + Push * 60.0f, FQuat::Identity, ECC_Pawn,
					FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params))
				{
					const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Hit.GetComponent());
					UE_LOG(LogTheLastWatch, Warning, TEXT("Blocked by %s / %s (mesh %s) at %s normal %s"),
						*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
						MeshComponent ? *GetNameSafe(MeshComponent->GetStaticMesh()) : TEXT("-"),
						*Hit.ImpactPoint.ToString(), *Hit.ImpactNormal.ToString());
				}
				else
				{
					UE_LOG(LogTheLastWatch, Warning, TEXT("Not moving but nothing ahead (push %s)"), *Push.ToString());
				}
			}
		}
	}
	if (KeeperPawn && KeeperPawn->GetActorLocation().Z < RespawnBelowZ)
	{
		if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
		{
			if (const AActor* Start = GameMode->FindPlayerStart(this))
			{
				KeeperPawn->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation());
				SetControlRotation(Start->GetActorRotation());
			}
		}
	}
}

void ALastWatchPlayerController::UpdateFocus()
{
	Focused.Reset();

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(KeeperInteract), false, GetPawn());
	FHitResult Hit;
	const FVector End = ViewLocation + ViewRotation.Vector() * InteractRange;

	if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params))
	{
		AInteractableActor* Target = Cast<AInteractableActor>(Hit.GetActor());
		if (Target && Target->CanInteract(this))
		{
			Focused = Target;
			return;
		}

		// several story tasks can share one spot (wind / inspect / repair the clockwork);
		// the one hit may be idle tonight, so take any usable interactable around the hit point
		for (TActorIterator<AInteractableActor> It(GetWorld()); It; ++It)
		{
			if (*It != Target && It->CanInteract(this) && !It->IsHidden()
				&& It->GetComponentsBoundingBox().ExpandBy(15.0f).IsInside(Hit.ImpactPoint))
			{
				Focused = *It;
				return;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// Input handlers

void ALastWatchPlayerController::OnInteract()
{
	switch (Mode)
	{
	case EKeeperMode::Explore:
		if (AInteractableActor* Target = Focused.Get())
		{
			AStoryTask* Task = Cast<AStoryTask>(Target);
			if (Task && Task->GetHoldSeconds() > 0.0f && Task->HasRequiredItem(this))
			{
				HoldTarget = Task;
				HoldTime = 0.0f;
			}
			Target->Interact(this);
		}
		break;

	case EKeeperMode::Inventory:
		{
			const TArray<FName>& Notes = Inventory->GetNotes();
			if (Notes.IsValidIndex(InventorySelection))
			{
				OpenNote(Notes[InventorySelection]);
			}
		}
		break;

	case EKeeperMode::Menu:
		break;

	default:
		OnBack();
		break;
	}
}

void ALastWatchPlayerController::OnInteractReleased()
{
	HoldTarget.Reset();
	HoldTime = 0.0f;
}

float ALastWatchPlayerController::GetHoldProgress() const
{
	const AStoryTask* Task = HoldTarget.Get();
	return Task ? FMath::Clamp(HoldTime / Task->GetHoldSeconds(), 0.0f, 1.0f) : 0.0f;
}

void ALastWatchPlayerController::OnJumpStarted()
{
	ACharacter* KeeperCharacter = Cast<ACharacter>(GetPawn());
	UE_LOG(LogTheLastWatch, Display, TEXT("Jump pressed: mode=%d canJump=%d"),
		static_cast<int32>(Mode), KeeperCharacter ? KeeperCharacter->CanJump() : -1);
	if (Mode == EKeeperMode::Explore && KeeperCharacter)
	{
		// leave the debug fly mode on the first jump
		if (KeeperCharacter->GetCharacterMovement()->IsFlying())
		{
			KeeperCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		}
		KeeperCharacter->Jump();
	}
}

void ALastWatchPlayerController::OnJumpCompleted()
{
	if (ACharacter* KeeperCharacter = Cast<ACharacter>(GetPawn()))
	{
		KeeperCharacter->StopJumping();
	}
}

void ALastWatchPlayerController::OnBack()
{
	if (Mode == EKeeperMode::Menu)
	{
		switch (MenuPage)
		{
		case EMenuPage::Pause:    CloseMenu(); break;
		case EMenuPage::Settings: SetMenuPage(SettingsParent); break;
		case EMenuPage::Chapters: SetMenuPage(EMenuPage::Title); break;
		case EMenuPage::Ending:   OnMenuConfirm(); break;
		default: break;
		}
		return;
	}

	if (Mode == EKeeperMode::Reading && bReadingFromInventory)
	{
		bReadingFromInventory = false;
		SetMode(EKeeperMode::Inventory);
		return;
	}
	ReturnToExplore();
}

void ALastWatchPlayerController::OnToggleInventory()
{
	if (Mode == EKeeperMode::Menu)
	{
		return;
	}
	if (Mode == EKeeperMode::Explore)
	{
		InventorySelection = FMath::Clamp(InventorySelection, 0, FMath::Max(0, Inventory->GetNotes().Num() - 1));
		SetMode(EKeeperMode::Inventory);
	}
	else
	{
		bReadingFromInventory = false;
		ReturnToExplore();
	}
}

void ALastWatchPlayerController::OnPagePrev()
{
	if (Mode == EKeeperMode::Menu)
	{
		ActivateMenuEntry(MenuSelection, -1);
		return;
	}
	if (Mode == EKeeperMode::Reading)
	{
		NotePage = FMath::Max(0, NotePage - 1);
	}
}

void ALastWatchPlayerController::OnPageNext()
{
	if (Mode == EKeeperMode::Menu)
	{
		ActivateMenuEntry(MenuSelection, 1);
		return;
	}
	if (Mode != EKeeperMode::Reading)
	{
		return;
	}

	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	const FNoteData* Note = Data ? Data->FindNote(OpenNoteId) : nullptr;
	if (Note)
	{
		NotePage = FMath::Min(NotePage + 1, Note->Pages.Num() - 1);
	}
}

void ALastWatchPlayerController::OnListUp()
{
	if (Mode == EKeeperMode::Menu)
	{
		// skip greyed out lines
		const TArray<FMenuEntry> Entries = GetMenuEntries();
		for (int32 Index = MenuSelection - 1; Index >= 0; --Index)
		{
			if (Entries[Index].bEnabled)
			{
				MenuSelection = Index;
				break;
			}
		}
		return;
	}
	if (Mode == EKeeperMode::Inventory)
	{
		InventorySelection = FMath::Max(0, InventorySelection - 1);
	}
}

void ALastWatchPlayerController::OnListDown()
{
	if (Mode == EKeeperMode::Menu)
	{
		const TArray<FMenuEntry> Entries = GetMenuEntries();
		for (int32 Index = MenuSelection + 1; Index < Entries.Num(); ++Index)
		{
			if (Entries[Index].bEnabled)
			{
				MenuSelection = Index;
				break;
			}
		}
		return;
	}
	if (Mode == EKeeperMode::Inventory)
	{
		InventorySelection = FMath::Min(InventorySelection + 1, FMath::Max(0, Inventory->GetNotes().Num() - 1));
	}
}

void ALastWatchPlayerController::OnTune(const FInputActionValue& Value)
{
	if (ARadioSet* Radio = ActiveRadio.Get())
	{
		Radio->Tune(Value.Get<float>() * TuneSpeed * GetWorld()->GetDeltaSeconds());
	}
}

void ALastWatchPlayerController::OnTuneWheel(const FInputActionValue& Value)
{
	if (ARadioSet* Radio = ActiveRadio.Get())
	{
		Radio->Tune(FMath::Sign(Value.Get<float>()) * TuneWheelStep);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Menus

namespace
{
	FText MenuText(const UObject* Context, const TCHAR* Key, const FText& Arg = FText::GetEmpty())
	{
		const UGameDataSubsystem* Data = UGameDataSubsystem::Get(Context);
		return Data ? Data->GetUIText(Key, Arg) : FText::FromString(Key);
	}

	FMenuEntry MakeEntry(const UObject* Context, const TCHAR* Id, const TCHAR* Key, FText Value = FText::GetEmpty(), bool bEnabled = true)
	{
		FMenuEntry Entry;
		Entry.Id = Id;
		Entry.Label = MenuText(Context, Key);
		Entry.Value = MoveTemp(Value);
		Entry.bEnabled = bEnabled;
		return Entry;
	}

	FText OnOff(const UObject* Context, bool bOn)
	{
		return MenuText(Context, bOn ? TEXT("opt.on") : TEXT("opt.off"));
	}
}

void ALastWatchPlayerController::OpenTitle()
{
	// the lamp turns behind the title; the story puts it out again when a night starts
	const ALighthouseBeam* Lamp = nullptr;
	for (TActorIterator<ALighthouseBeam> It(GetWorld()); It; ++It)
	{
		It->SetLit(true);
		Lamp = *It;
	}

	if (!TitleCamera)
	{
		TitleCamera = GetWorld()->SpawnActor<ACameraActor>();
		TitleCamera->GetCameraComponent()->SetFieldOfView(55.0f);
		TitleCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
	}

	// start on the keeper's side of the tower
	if (Lamp && GetPawn())
	{
		TitleOrbitAngle = (GetPawn()->GetActorLocation() - Lamp->GetActorLocation()).Rotation().Yaw;
	}
	UpdateTitleCamera(0.0f);
	SetViewTarget(TitleCamera);

	if (ALastWatchHUD* KeeperHUD = GetKeeperHUD())
	{
		KeeperHUD->SnapFade(1.0f);
		KeeperHUD->FadeTo(0.0f, 3.0f);
	}
	SetMenuPage(EMenuPage::Title);
}

void ALastWatchPlayerController::UpdateTitleCamera(float DeltaTime)
{
	const ALighthouseBeam* Lamp = nullptr;
	for (TActorIterator<ALighthouseBeam> It(GetWorld()); It; ++It)
	{
		Lamp = *It;
		break;
	}
	if (!Lamp || !TitleCamera)
	{
		return;
	}

	TitleOrbitAngle += TitleOrbitSpeed * DeltaTime;
	const float Radians = FMath::DegreesToRadians(TitleOrbitAngle);
	const FVector Target = Lamp->GetActorLocation();
	const FVector Eye = Target + FVector(FMath::Cos(Radians) * TitleOrbitRadius, FMath::Sin(Radians) * TitleOrbitRadius, TitleOrbitHeight);

	// look a little left of the tower so it stands on the right, clear of the menu
	FRotator View = (Target - Eye).Rotation();
	View.Yaw -= 11.0f;
	TitleCamera->SetActorLocationAndRotation(Eye, View);
}

void ALastWatchPlayerController::OpenPause()
{
	if (Mode != EKeeperMode::Explore)
	{
		return;
	}
	SetPause(true);
	SetMenuPage(EMenuPage::Pause);
}

void ALastWatchPlayerController::CloseMenu()
{
	SetPause(false);
	ShowCursor(false);
	if (GetPawn() && GetViewTarget() != GetPawn())
	{
		SetViewTarget(GetPawn());
	}
	ReturnToExplore();
}

void ALastWatchPlayerController::EnterEnding()
{
	ReturnToExplore();
	EndingStart = GetWorld()->GetRealTimeSeconds();
	MenuPage = EMenuPage::Ending;
	SetMode(EKeeperMode::Menu);
}

void ALastWatchPlayerController::ReturnToTitle()
{
	SetPause(false);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

void ALastWatchPlayerController::SetMenuPage(EMenuPage Page)
{
	UE_LOG(LogTheLastWatch, Display, TEXT("Menu: page %d"), static_cast<int32>(Page));
	MenuPage = Page;
	if (Mode != EKeeperMode::Menu)
	{
		ReturnToExplore();
		SetMode(EKeeperMode::Menu);
	}
	ShowCursor(true);

	MenuSelection = 0;
	const TArray<FMenuEntry> Entries = GetMenuEntries();
	while (Entries.IsValidIndex(MenuSelection) && !Entries[MenuSelection].bEnabled)
	{
		++MenuSelection;
	}
}

void ALastWatchPlayerController::ShowCursor(bool bShow)
{
	bShowMouseCursor = bShow;
	if (bShow)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

TArray<FMenuEntry> ALastWatchPlayerController::GetMenuEntries() const
{
	TArray<FMenuEntry> Entries;
	const UKeeperSaveSubsystem* Save = UKeeperSaveSubsystem::Get(this);
	const ULastWatchSettingsSave* Settings = Save ? Save->GetSettings() : nullptr;

	switch (MenuPage)
	{
	case EMenuPage::Title:
		if (Save && Save->HasProgress())
		{
			Entries.Add(MakeEntry(this, TEXT("continue"), TEXT("menu.continue"), MenuText(this, TEXT("menu.night"), FText::AsNumber(Save->GetSavedNight()))));
		}
		Entries.Add(MakeEntry(this, TEXT("new_game"), TEXT("menu.new_game")));
		Entries.Add(MakeEntry(this, TEXT("chapters"), TEXT("menu.chapters"), FText::GetEmpty(), Save && Save->HasProgress()));
		Entries.Add(MakeEntry(this, TEXT("settings"), TEXT("menu.settings")));
		Entries.Add(MakeEntry(this, TEXT("quit"), TEXT("menu.quit")));
		break;

	case EMenuPage::Pause:
		Entries.Add(MakeEntry(this, TEXT("resume"), TEXT("menu.resume")));
		Entries.Add(MakeEntry(this, TEXT("settings"), TEXT("menu.settings")));
		Entries.Add(MakeEntry(this, TEXT("title"), TEXT("menu.to_title")));
		Entries.Add(MakeEntry(this, TEXT("quit"), TEXT("menu.quit")));
		break;

	case EMenuPage::Chapters:
		{
			const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
			for (int32 Night = 1; Night <= 5; ++Night)
			{
				const bool bUnlocked = Save && Save->IsNightUnlocked(Night);
				const FNightData* NightData = Data ? Data->FindNight(Night) : nullptr;
				FMenuEntry Entry;
				Entry.Id = FName(TEXT("night"), Night);
				Entry.Label = MenuText(this, TEXT("menu.night"), FText::AsNumber(Night));
				Entry.Value = !bUnlocked ? MenuText(this, TEXT("menu.locked")) : NightData ? NightData->Title.Get() : FText::GetEmpty();
				Entry.bEnabled = bUnlocked;
				Entries.Add(Entry);
			}
			Entries.Add(MakeEntry(this, TEXT("back"), TEXT("menu.back")));
		}
		break;

	case EMenuPage::Settings:
		if (Settings)
		{
			const TCHAR* SubtitleKeys[] = { TEXT("opt.small"), TEXT("opt.normal"), TEXT("opt.large") };
			Entries.Add(MakeEntry(this, TEXT("language"), TEXT("set.language"), MenuText(this, TEXT("lang.current"))));
			Entries.Add(MakeEntry(this, TEXT("subtitles"), TEXT("set.subtitles"), MenuText(this, SubtitleKeys[FMath::Clamp(Settings->SubtitleSize, 0, 2)])));
			Entries.Add(MakeEntry(this, TEXT("objectives"), TEXT("set.objectives"), OnOff(this, Settings->bShowObjectives)));
			Entries.Add(MakeEntry(this, TEXT("sensitivity"), TEXT("set.sensitivity"), FText::FromString(FString::Printf(TEXT("%.1f"), Settings->MouseSensitivity))));
			Entries.Add(MakeEntry(this, TEXT("invert"), TEXT("set.invert"), OnOff(this, Settings->bInvertY)));
			Entries.Add(MakeEntry(this, TEXT("fov"), TEXT("set.fov"), FText::FromString(FString::Printf(TEXT("%d°"), FMath::RoundToInt(Settings->FieldOfView)))));
			Entries.Add(MakeEntry(this, TEXT("volume"), TEXT("set.volume"), FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->MasterVolume * 100.0f)))));
		}
		Entries.Add(MakeEntry(this, TEXT("back"), TEXT("menu.back")));
		break;

	default:
		break;
	}
	return Entries;
}

void ALastWatchPlayerController::ActivateMenuEntry(int32 Index, int32 Direction)
{
	const TArray<FMenuEntry> Entries = GetMenuEntries();
	if (!Entries.IsValidIndex(Index) || !Entries[Index].bEnabled)
	{
		return;
	}
	MenuSelection = Index;
	const FName Id = Entries[Index].Id;
	UE_LOG(LogTheLastWatch, Display, TEXT("Menu: %s (%d)"), *Id.ToString(), Direction);

	UKeeperSaveSubsystem* Save = UKeeperSaveSubsystem::Get(this);
	ULastWatchSettingsSave* Settings = Save ? Save->GetSettings() : nullptr;

	// settings: left/right step the value, confirm or click steps it forward
	if (Settings)
	{
		const int32 Step = Direction == 0 ? 1 : Direction;
		bool bChanged = true;
		if (Id == TEXT("language"))         { Settings->Language = Settings->Language == TEXT("tr") ? TEXT("en") : TEXT("tr"); }
		else if (Id == TEXT("subtitles"))   { Settings->SubtitleSize = (Settings->SubtitleSize + Step + 3) % 3; }
		else if (Id == TEXT("objectives"))  { Settings->bShowObjectives = !Settings->bShowObjectives; }
		else if (Id == TEXT("sensitivity")) { Settings->MouseSensitivity = FMath::Clamp(FMath::RoundToFloat((Settings->MouseSensitivity + 0.1f * Step) * 10.0f) / 10.0f, 0.2f, 3.0f); }
		else if (Id == TEXT("invert"))      { Settings->bInvertY = !Settings->bInvertY; }
		else if (Id == TEXT("fov"))         { Settings->FieldOfView = FMath::Clamp(Settings->FieldOfView + 5.0f * Step, 70.0f, 110.0f); }
		else if (Id == TEXT("volume"))      { Settings->MasterVolume = FMath::Clamp(FMath::RoundToFloat((Settings->MasterVolume + 0.1f * Step) * 10.0f) / 10.0f, 0.0f, 1.0f); }
		else { bChanged = false; }

		if (bChanged)
		{
			Save->SaveSettings();
			ApplyPlayerSettings();
			return;
		}
	}

	if (Direction != 0)
	{
		return;
	}

	// play from the start of a night, carrying what the keeper had then
	auto StartNight = [this, Save](int32 Night)
	{
		if (Save)
		{
			Save->RestoreNight(Night, this);
		}
		CloseMenu();
		if (UStorySubsystem* Story = UStorySubsystem::Get(this))
		{
			Story->StartNight(Night);
		}
	};

	if (Id == TEXT("continue") && Save)
	{
		StartNight(Save->GetSavedNight());
	}
	else if (Id == TEXT("new_game"))
	{
		if (Save)
		{
			Save->ResetProgress();
		}
		StartNight(1);
	}
	else if (Id == TEXT("chapters"))  { SetMenuPage(EMenuPage::Chapters); }
	else if (Id == TEXT("settings"))
	{
		SettingsParent = MenuPage;
		SetMenuPage(EMenuPage::Settings);
	}
	else if (Id == TEXT("resume"))    { CloseMenu(); }
	else if (Id == TEXT("title"))     { ReturnToTitle(); }
	else if (Id == TEXT("back"))      { OnBack(); }
	else if (Id == TEXT("quit"))      { ConsoleCommand(TEXT("quit")); }
	else if (Id.GetPlainNameString() == TEXT("night"))
	{
		StartNight(Id.GetNumber());
	}
}

void ALastWatchPlayerController::OnPause()
{
	if (Mode == EKeeperMode::Explore)
	{
		OpenPause();
	}
}

void ALastWatchPlayerController::OnMenuConfirm()
{
	if (Mode != EKeeperMode::Menu)
	{
		return;
	}

	if (MenuPage == EMenuPage::Ending)
	{
		// let the ending be read before it can be skipped
		if (GetWorld()->GetRealTimeSeconds() - EndingStart > 8.0f)
		{
			ReturnToTitle();
		}
		return;
	}
	ActivateMenuEntry(MenuSelection, 0);
}

void ALastWatchPlayerController::ApplyPlayerSettings()
{
	const UKeeperSaveSubsystem* Save = UKeeperSaveSubsystem::Get(this);
	if (!Save)
	{
		return;
	}
	if (UCameraComponent* Camera = GetPawn() ? GetPawn()->FindComponentByClass<UCameraComponent>() : nullptr)
	{
		Camera->SetFieldOfView(Save->GetSettings()->FieldOfView);
	}
}

void ALastWatchPlayerController::AddYawInput(float Val)
{
	const UKeeperSaveSubsystem* Save = UKeeperSaveSubsystem::Get(this);
	Super::AddYawInput(Val * (Save ? Save->GetSettings()->MouseSensitivity : 1.0f));
}

void ALastWatchPlayerController::AddPitchInput(float Val)
{
	const UKeeperSaveSubsystem* Save = UKeeperSaveSubsystem::Get(this);
	const ULastWatchSettingsSave* Settings = Save ? Save->GetSettings() : nullptr;
	Super::AddPitchInput(Settings ? Val * Settings->MouseSensitivity * (Settings->bInvertY ? -1.0f : 1.0f) : Val);
}

/////////////////////////////////////////////////////////////////////////////
// Debug commands

void ALastWatchPlayerController::DebugOpenNote(FName NoteId)
{
	Inventory->AddNote(NoteId);
	OpenNote(NoteId);
}

void ALastWatchPlayerController::DebugGiveItem(FName ItemId)
{
	Inventory->AddItem(ItemId);
}

void ALastWatchPlayerController::DebugInventory()
{
	ReturnToExplore();
	OnToggleInventory();
}

void ALastWatchPlayerController::DebugRadio(float Frequency)
{
	for (TActorIterator<ARadioSet> It(GetWorld()); It; ++It)
	{
		ReturnToExplore();
		It->Tune(Frequency - It->GetFrequency());
		EnterRadio(*It);
		return;
	}
}

void ALastWatchPlayerController::DebugSetNight(int32 Night)
{
	if (UStorySubsystem* Story = UStorySubsystem::Get(this))
	{
		Story->StartNight(Night);
	}
}

void ALastWatchPlayerController::DebugStoryEvent(const FString& Event)
{
	if (UStorySubsystem* Story = UStorySubsystem::Get(this))
	{
		Story->Notify(Event);
	}
}

void ALastWatchPlayerController::DebugSkipObjective()
{
	if (UStorySubsystem* Story = UStorySubsystem::Get(this))
	{
		Story->SkipObjective();
	}
}

void ALastWatchPlayerController::DebugAutoplay(float Interval)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		UStorySubsystem* Story = UStorySubsystem::Get(this);
		if (Story && !Story->HasEnded())
		{
			UE_LOG(LogTheLastWatch, Display, TEXT("Autoplay: night %d objective '%s' pawn %s"), Story->GetNight(),
				*Story->GetObjectiveId(), GetPawn() ? *GetPawn()->GetActorLocation().ToString() : TEXT("-"));
			Story->SkipObjective();
		}
	}), FMath::Max(Interval, 0.5f), true);
}

void ALastWatchPlayerController::DebugTeleport(float X, float Y, float Z, float Yaw, float Pitch)
{
	if (APawn* KeeperPawn = GetPawn())
	{
		// hover there, so camera shots from the air don't fall before they're taken
		if (ACharacter* KeeperCharacter = Cast<ACharacter>(KeeperPawn))
		{
			KeeperCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		}
		if (!KeeperPawn->TeleportTo(FVector(X, Y, Z), FRotator(0.0f, Yaw, 0.0f)))
		{
			// say what is in the way, so a bad test position is obvious
			TArray<FOverlapResult> Overlaps;
			const UCapsuleComponent* Capsule = Cast<ACharacter>(KeeperPawn)->GetCapsuleComponent();
			GetWorld()->OverlapMultiByChannel(Overlaps, FVector(X, Y, Z), FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
				FCollisionQueryParams(SCENE_QUERY_STAT(TeleportCheck), false, KeeperPawn));
			for (const FOverlapResult& Overlap : Overlaps)
			{
				const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Overlap.GetComponent());
				UE_LOG(LogTheLastWatch, Warning, TEXT("Teleport blocked by %s / %s (mesh %s)"), *GetNameSafe(Overlap.GetActor()),
					*GetNameSafe(Overlap.GetComponent()), MeshComponent ? *GetNameSafe(MeshComponent->GetStaticMesh()) : TEXT("-"));
			}
			if (Overlaps.Num() == 0)
			{
				UE_LOG(LogTheLastWatch, Warning, TEXT("Teleport to %.0f %.0f %.0f failed"), X, Y, Z);
			}
		}
		SetControlRotation(FRotator(Pitch, Yaw, 0.0f));
	}
}

void ALastWatchPlayerController::DebugDumpLights(float Radius)
{
	const APawn* KeeperPawn = GetPawn();
	if (!KeeperPawn)
	{
		return;
	}
	const FVector Center = KeeperPawn->GetActorLocation();
	UE_LOG(LogTheLastWatch, Display, TEXT("Lights within %.0f of %s:"), Radius, *Center.ToString());

	for (TObjectIterator<ULightComponent> It; It; ++It)
	{
		ULightComponent* Light = *It;
		if (Light->GetWorld() != GetWorld() || FVector::Dist(Light->GetComponentLocation(), Center) > Radius)
		{
			continue;
		}
		const ULocalLightComponent* Local = Cast<ULocalLightComponent>(Light);
		UE_LOG(LogTheLastWatch, Display, TEXT("  %s.%s intensity=%.2f units=%d visible=%d indirect=%.2f volumetric=%.2f at %s"),
			*GetNameSafe(Light->GetOwner()), *Light->GetName(), Light->Intensity,
			Local ? static_cast<int32>(Local->IntensityUnits) : -1, Light->IsVisible(),
			Light->IndirectLightingIntensity, Light->VolumetricScatteringIntensity,
			*Light->GetComponentLocation().ToString());
	}
}

void ALastWatchPlayerController::DebugStairHeadroom(float Height)
{
	// layout from Scripts/Editor/build_lighthouse.py
	constexpr float StartAngle = 270.0f, StepAngle = 14.0f, StepHeight = 20.0f, MidRadius = 38.0f + 135.0f * 0.5f;
	constexpr int32 Steps = 94;

	const AActor* Stairs = nullptr;
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		const UStaticMesh* StairMesh = It->GetStaticMeshComponent()->GetStaticMesh();
		if (StairMesh && StairMesh->GetName() == TEXT("SM_LighthouseStairs"))
		{
			Stairs = *It;
		}
	}
	if (!Stairs)
	{
		UE_LOG(LogTheLastWatch, Warning, TEXT("Headroom: no stairs in this level"));
		return;
	}

	const FTransform Frame = Stairs->GetActorTransform();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Headroom), true, GetPawn());
	int32 Blocked = 0, Hits = 0;
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		const float Angle = FMath::DegreesToRadians(StartAngle + StepAngle * (Step + 0.5f));
		const FVector Tread = Frame.TransformPosition(FVector(MidRadius * FMath::Cos(Angle), MidRadius * FMath::Sin(Angle), StepHeight * (Step + 1)));
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Tread + FVector(0, 0, 2), Tread + FVector(0, 0, Height + 10.0f), ECC_Pawn, Params))
		{
			++Hits;
			if (Hit.ImpactPoint.Z - Tread.Z < Height)
			{
				++Blocked;
				UE_LOG(LogTheLastWatch, Warning, TEXT("Headroom: step %d has %.0f cm (hit %s)"), Step,
					Hit.ImpactPoint.Z - Tread.Z, *GetNameSafe(Hit.GetActor()));
			}
		}
	}
	UE_LOG(LogTheLastWatch, Display, TEXT("Headroom: %d of %d steps below %.0f cm (%d traces hit something)"), Blocked, Steps, Height, Hits);
}

void ALastWatchPlayerController::DebugLamp(bool bLit)
{
	for (TActorIterator<ALighthouseBeam> It(GetWorld()); It; ++It)
	{
		It->SetLit(bLit);
	}
}

void ALastWatchPlayerController::DebugShotAfter(float Seconds)
{
	// core ticker rather than a world timer, so it also fires while a menu has the game paused
	TWeakObjectPtr<ALastWatchPlayerController> WeakThis(this);
	const double Due = FPlatformTime::Seconds() + FMath::Max(Seconds, 0.1f);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Due](float)
	{
		if (FPlatformTime::Seconds() < Due)
		{
			return true;
		}
		if (WeakThis.IsValid())
		{
			WeakThis->ConsoleCommand(TEXT("Shot showui"));
		}
		return false;
	}));
}

void ALastWatchPlayerController::DebugMenu(const FString& Page)
{
	if (Page == TEXT("title"))
	{
		OpenTitle();
	}
	else if (Page == TEXT("pause"))
	{
		ReturnToExplore();
		OpenPause();
		if (ALastWatchHUD* KeeperHUD = GetKeeperHUD())
		{
			// the night's fade-in would wait for the paused clock
			KeeperHUD->SnapFade(0.0f);
		}
	}
	else if (Page == TEXT("settings") || Page == TEXT("chapters"))
	{
		if (Mode != EKeeperMode::Menu)
		{
			OpenTitle();
		}
		SettingsParent = MenuPage;
		SetMenuPage(Page == TEXT("settings") ? EMenuPage::Settings : EMenuPage::Chapters);
	}
}

/////////////////////////////////////////////////////////////////////////////
// HUD forwarding

ALastWatchHUD* ALastWatchPlayerController::GetKeeperHUD() const
{
	return Cast<ALastWatchHUD>(GetHUD());
}

void ALastWatchPlayerController::ShowToast(const FText& Text)
{
	if (ALastWatchHUD* KeeperHUD = GetKeeperHUD())
	{
		KeeperHUD->ShowToast(Text);
	}
}

void ALastWatchPlayerController::SetSubtitle(const FText& Speaker, const FText& Line)
{
	if (ALastWatchHUD* KeeperHUD = GetKeeperHUD())
	{
		KeeperHUD->SetSubtitle(Speaker, Line);
	}
}
