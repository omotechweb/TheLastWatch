// The Last Watch

#include "LastWatchPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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

void ALastWatchPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetMode(EKeeperMode::Explore);
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
