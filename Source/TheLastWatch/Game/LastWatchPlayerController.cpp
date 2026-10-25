// The Last Watch

#include "LastWatchPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/LocalLightComponent.h"
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
#include "UObject/ConstructorHelpers.h"
#include "FootstepComponent.h"
#include "GameDataSubsystem.h"
#include "HorrorUI.h"
#include "Interactables.h"
#include "InventoryComponent.h"
#include "LastWatchGameMode.h"
#include "LastWatchHUD.h"
#include "LighthouseBeam.h"
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
}

void ALastWatchPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	CreateKeeperInput();

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(InteractAction, ETriggerEvent::Started, this, &ALastWatchPlayerController::OnInteract);
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

		if (Mode == EKeeperMode::Reading || Mode == EKeeperMode::Inventory)
		{
			Subsystem->AddMappingContext(OverlayContext, 10);
		}
		else if (Mode == EKeeperMode::Radio)
		{
			Subsystem->AddMappingContext(RadioContext, 10);
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

	// walked off a cliff or into the sea
	APawn* KeeperPawn = GetPawn();

	// tlw.LogPawn 1: trace the keeper's movement for automated input tests
	if (CVarLogPawn.GetValueOnGameThread() && KeeperPawn)
	{
		if (const ACharacter* KeeperCharacter = Cast<ACharacter>(KeeperPawn))
		{
			UE_LOG(LogTheLastWatch, Display, TEXT("Pawn x=%.1f y=%.1f z=%.1f vz=%.1f mode=%d crouched=%d halfHeight=%.1f"),
				KeeperPawn->GetActorLocation().X, KeeperPawn->GetActorLocation().Y,
				KeeperPawn->GetActorLocation().Z, KeeperPawn->GetVelocity().Z,
				static_cast<int32>(KeeperCharacter->GetCharacterMovement()->MovementMode.GetValue()),
				KeeperCharacter->bIsCrouched,
				KeeperCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

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

	default:
		OnBack();
		break;
	}
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
	if (Mode == EKeeperMode::Reading)
	{
		NotePage = FMath::Max(0, NotePage - 1);
	}
}

void ALastWatchPlayerController::OnPageNext()
{
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
	if (Mode == EKeeperMode::Inventory)
	{
		InventorySelection = FMath::Max(0, InventorySelection - 1);
	}
}

void ALastWatchPlayerController::OnListDown()
{
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
	if (ALastWatchGameMode* GameMode = GetWorld()->GetAuthGameMode<ALastWatchGameMode>())
	{
		GameMode->SetCurrentNight(Night);
	}
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
		KeeperPawn->TeleportTo(FVector(X, Y, Z), FRotator(0.0f, Yaw, 0.0f));
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
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		ConsoleCommand(TEXT("Shot showui"));
	}), FMath::Max(Seconds, 0.1f), false);
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
