// The Last Watch

#include "Interactables.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "GameDataSubsystem.h"
#include "InventoryComponent.h"
#include "KeeperDirector.h"
#include "LastWatchGameMode.h"
#include "LastWatchPlayerController.h"
#include "StorySubsystem.h"

namespace
{
	/** Placeholder shape mesh from the engine's basic shapes */
	UStaticMesh* FindBasicShape(const TCHAR* Path)
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(Path);
		return Finder.Object;
	}

	FText ItemName(const UObject* Context, FName ItemId)
	{
		const UGameDataSubsystem* Data = UGameDataSubsystem::Get(Context);
		const FItemData* Item = Data ? Data->FindItem(ItemId) : nullptr;
		return Item ? Item->Name.Get() : FText::FromName(ItemId);
	}
}

/////////////////////////////////////////////////////////////////////////////
// AInteractableActor

AInteractableActor::AInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->SetStaticMesh(FindBasicShape(TEXT("/Engine/BasicShapes/Cube.Cube")));

	// engine material with a "Color" parameter, tinted in BeginPlay
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AInteractableActor::BeginPlay()
{
	Super::BeginPlay();

	// tint the placeholder shape so different objects are easy to tell apart
	if (bTintPlaceholder)
	{
		if (UMaterialInstanceDynamic* Material = Mesh->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

void AInteractableActor::MarkCollected()
{
	bCollected = true;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

/////////////////////////////////////////////////////////////////////////////
// ANotePickup

ANotePickup::ANotePickup()
{
	// a sheet of paper
	Mesh->SetRelativeScale3D(FVector(0.21f, 0.297f, 0.004f));
	Color = FLinearColor(0.85f, 0.80f, 0.66f);
}

FText ANotePickup::GetPrompt(const ALastWatchPlayerController* Keeper) const
{
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	const FNoteData* Note = Data ? Data->FindNote(NoteId) : nullptr;
	const FText Title = Note ? Note->Title.Get() : FText::FromName(NoteId);
	return Data ? Data->GetUIText(TEXT("prompt.read"), Title) : Title;
}

void ANotePickup::Interact(ALastWatchPlayerController* Keeper)
{
	Keeper->GetInventory()->AddNote(NoteId);
	Keeper->OpenNote(NoteId);

	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	if (const FNoteData* Note = Data ? Data->FindNote(NoteId) : nullptr)
	{
		Keeper->ShowToast(Data->GetUIText(TEXT("toast.note_added"), Note->Title.Get()));
	}

	// the page is taken; it can be re-read from the inventory
	bCollected = true;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	if (UStorySubsystem* Story = UStorySubsystem::Get(this))
	{
		Story->Notify(TEXT("read:") + NoteId.ToString());
	}
}

/////////////////////////////////////////////////////////////////////////////
// AItemPickup

AItemPickup::AItemPickup()
{
	Mesh->SetRelativeScale3D(FVector(0.12f, 0.06f, 0.03f));
	Color = FLinearColor(0.6f, 0.15f, 0.1f);
}

FText AItemPickup::GetPrompt(const ALastWatchPlayerController* Keeper) const
{
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	return Data ? Data->GetUIText(TEXT("prompt.take"), ItemName(this, ItemId)) : FText::GetEmpty();
}

void AItemPickup::Interact(ALastWatchPlayerController* Keeper)
{
	Keeper->GetInventory()->AddItem(ItemId);

	if (const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this))
	{
		Keeper->ShowToast(Data->GetUIText(TEXT("toast.picked_up"), ItemName(this, ItemId)));
	}

	if (UStorySubsystem* Story = UStorySubsystem::Get(this))
	{
		Story->Notify(TEXT("take:") + ItemId.ToString());
	}
	Destroy();
}

/////////////////////////////////////////////////////////////////////////////
// AKeeperDoor

AKeeperDoor::AKeeperDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);
	Mesh->SetupAttachment(Hinge);

	// the actor origin is the hinge; the placeholder panel extends along +Y
	Mesh->SetRelativeLocation(FVector(0.0f, 55.0f, 105.0f));
	Mesh->SetRelativeScale3D(FVector(0.08f, 1.1f, 2.1f));
	Color = FLinearColor(0.25f, 0.17f, 0.11f);
}

void AKeeperDoor::BeginPlay()
{
	Super::BeginPlay();
	bUnlocked = RequiredItemId.IsNone();
}

FText AKeeperDoor::GetPrompt(const ALastWatchPlayerController* Keeper) const
{
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	if (!Data)
	{
		return FText::GetEmpty();
	}

	if (IsLocked())
	{
		return Keeper->GetInventory()->HasItem(RequiredItemId)
			? Data->GetUIText(TEXT("prompt.unlock"), ItemName(this, RequiredItemId))
			: Data->GetUIText(TEXT("prompt.locked"));
	}
	return Data->GetUIText(bOpen ? TEXT("prompt.close") : TEXT("prompt.open"));
}

void AKeeperDoor::Interact(ALastWatchPlayerController* Keeper)
{
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);

	if (IsLocked())
	{
		if (!Keeper->GetInventory()->HasItem(RequiredItemId))
		{
			if (Data)
			{
				Keeper->ShowToast(Data->GetUIText(TEXT("toast.locked")));
			}
			return;
		}

		bUnlocked = true;
		if (Data)
		{
			Keeper->ShowToast(Data->GetUIText(TEXT("toast.unlocked"), ItemName(this, RequiredItemId)));
		}
	}

	bOpen = !bOpen;
	SetActorTickEnabled(true);
}

void AKeeperDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = bOpen ? 1.0f : 0.0f;
	SwingAlpha = FMath::FInterpConstantTo(SwingAlpha, Target, DeltaSeconds, 1.0f / FMath::Max(SwingTime, 0.01f));

	// ease in and out so the door doesn't snap
	const float Yaw = FMath::InterpEaseInOut(0.0f, OpenYaw, SwingAlpha, 2.0f);
	Hinge->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));

	if (FMath::IsNearlyEqual(SwingAlpha, Target))
	{
		SetActorTickEnabled(false);
	}
}

/////////////////////////////////////////////////////////////////////////////
// ARadioSet

ARadioSet::ARadioSet()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh->SetRelativeScale3D(FVector(0.3f, 0.5f, 0.28f));
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 14.0f));
	Color = FLinearColor(0.12f, 0.16f, 0.12f);
}

FText ARadioSet::GetPrompt(const ALastWatchPlayerController* Keeper) const
{
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	return Data ? Data->GetUIText(TEXT("prompt.radio")) : FText::GetEmpty();
}

void ARadioSet::Interact(ALastWatchPlayerController* Keeper)
{
	Keeper->EnterRadio(this);
}

void ARadioSet::SetUser(ALastWatchPlayerController* Keeper)
{
	User = Keeper;
	if (AKeeperDirector* Director = AKeeperDirector::Get(this))
	{
		Director->Click();
		if (!Keeper)
		{
			Director->SetRadio(0.0f, 0.0f, 1.0f);
		}
	}
}

void ARadioSet::Tune(float Delta)
{
	Frequency = FMath::Clamp(Frequency + Delta, MinFrequency, MaxFrequency);
}

const FRadioTransmission* ARadioSet::FindClosestTransmission(float& OutSignal) const
{
	OutSignal = 0.0f;

	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	if (!Data)
	{
		return nullptr;
	}

	const ALastWatchGameMode* GameMode = GetWorld()->GetAuthGameMode<ALastWatchGameMode>();
	const int32 Night = GameMode ? GameMode->GetCurrentNight() : 1;

	const FRadioTransmission* Best = nullptr;
	for (const FRadioTransmission& Transmission : Data->GetTransmissions())
	{
		if (!Transmission.IsOnAir(Night))
		{
			continue;
		}

		const float Signal = 1.0f - FMath::Abs(Transmission.Frequency - Frequency) / Bandwidth;
		if (Signal > OutSignal)
		{
			OutSignal = Signal;
			Best = &Transmission;
		}
	}
	return Best;
}

float ARadioSet::GetSignalStrength() const
{
	float Signal;
	FindClosestTransmission(Signal);
	return Signal;
}

void ARadioSet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ALastWatchPlayerController* Keeper = User.Get();
	if (!Keeper)
	{
		return;
	}

	float Signal;
	const FRadioTransmission* Transmission = FindClosestTransmission(Signal);

	// hiss that clears as a station comes in, a whistle while it is close but off, a voice once it is clear
	AKeeperDirector* Director = AKeeperDirector::Get(this);
	const bool bClear = Transmission && Signal >= ClearSignal && Transmission->Lines.Num() > 0;
	const FPlayback* Current = Transmission ? Playback.Find(Transmission->Id) : nullptr;
	const bool bSpeaking = bClear && !(Current && (Current->bFinished || Current->bInGap));
	if (Director)
	{
		Director->SetRadio(FMath::Lerp(1.0f, 0.25f, FMath::Clamp(Signal, 0.0f, 1.0f)), bSpeaking ? Signal : 0.0f, Signal > 0.0f ? 1.0f - Signal : 1.0f);
	}

	// only a clear signal carries speech; otherwise the call is paused
	if (!bClear)
	{
		Keeper->SetSubtitle(FText::GetEmpty(), FText::GetEmpty());
		return;
	}

	FPlayback& State = Playback.FindOrAdd(Transmission->Id);
	if (State.bFinished)
	{
		Keeper->SetSubtitle(FText::GetEmpty(), FText::GetEmpty());
		return;
	}

	State.LineTime += DeltaSeconds;

	if (State.bInGap)
	{
		Keeper->SetSubtitle(FText::GetEmpty(), FText::GetEmpty());
		if (State.LineTime >= Transmission->LoopGap)
		{
			State = FPlayback();
		}
		return;
	}

	const FRadioLine& Line = Transmission->Lines[State.Line];
	Keeper->SetSubtitle(FText::FromString(Line.Speaker), Line.Text.Get());

	if (State.LineTime >= Line.Duration)
	{
		State.LineTime = 0.0f;
		if (++State.Line >= Transmission->Lines.Num())
		{
			State.Line = 0;
			if (Transmission->bLoop)
			{
				State.bInGap = true;
			}
			else
			{
				State.bFinished = true;
			}
			// heard to the end (loops count once per pass)
			if (UStorySubsystem* Story = UStorySubsystem::Get(this))
			{
				Story->Notify(TEXT("radio:") + Transmission->Id.ToString());
			}
		}
	}
}
