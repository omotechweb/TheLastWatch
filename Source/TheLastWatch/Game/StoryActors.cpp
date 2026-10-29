// The Last Watch

#include "StoryActors.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "GameDataSubsystem.h"
#include "InventoryComponent.h"
#include "LastWatchHUD.h"
#include "LastWatchPlayerController.h"
#include "StorySubsystem.h"
#include "WatchSubsystem.h"

namespace
{
	FText StoryItemName(const UObject* Context, FName ItemId)
	{
		const UGameDataSubsystem* Data = UGameDataSubsystem::Get(Context);
		const FItemData* Item = Data ? Data->FindItem(ItemId) : nullptr;
		return Item ? Item->Name.Get() : FText::FromName(ItemId);
	}
}

/////////////////////////////////////////////////////////////////////////////
// AStoryTask

AStoryTask::AStoryTask()
{
	Color = FLinearColor(0.3f, 0.3f, 0.3f);
}

bool AStoryTask::IsExpectedByStory() const
{
	const UStorySubsystem* Story = UStorySubsystem::Get(this);
	return Story && Story->IsExpected(TEXT("task:") + TaskId.ToString());
}

bool AStoryTask::HasRequiredItem(const ALastWatchPlayerController* Keeper) const
{
	return RequiredItem.IsNone() || Keeper->GetInventory()->HasItem(RequiredItem);
}

FText AStoryTask::GetPrompt(const ALastWatchPlayerController* Keeper) const
{
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	if (!Data)
	{
		return FText::GetEmpty();
	}
	if (!HasRequiredItem(Keeper))
	{
		return Data->GetUIText(TEXT("prompt.needs"), StoryItemName(this, RequiredItem));
	}
	return Data->GetUIText(HoldSeconds > 0.0f ? PromptKey + TEXT(".hold") : PromptKey);
}

bool AStoryTask::CanInteract(const ALastWatchPlayerController* Keeper) const
{
	// the lantern chores stay usable to look after a lit lamp (see UWatchSubsystem)
	if (const UWatchSubsystem* Watch = UWatchSubsystem::Get(this); Watch && Watch->CanTend(TaskId, Keeper) && !IsExpectedByStory())
	{
		return true;
	}
	if (bDoneTonight && !bRepeatable)
	{
		return false;
	}
	if (!bRequireObjective)
	{
		return true;
	}
	const UStorySubsystem* Story = UStorySubsystem::Get(this);
	return Story && Story->IsExpected(TEXT("task:") + TaskId.ToString());
}

void AStoryTask::Interact(ALastWatchPlayerController* Keeper)
{
	if (!HasRequiredItem(Keeper))
	{
		if (const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this))
		{
			Keeper->ShowToast(Data->GetUIText(TEXT("prompt.needs"), StoryItemName(this, RequiredItem)));
		}
		return;
	}

	// holds are driven by the controller, which calls Complete when the bar fills
	if (HoldSeconds <= 0.0f)
	{
		Complete(Keeper);
	}
}

void AStoryTask::Complete(ALastWatchPlayerController* Keeper)
{
	// looking after the lamp rather than a step of the story
	if (UWatchSubsystem* Watch = UWatchSubsystem::Get(this); Watch && Watch->CanTend(TaskId, Keeper) && !IsExpectedByStory())
	{
		if (!RequiredItem.IsNone() && bConsumeItem)
		{
			Keeper->GetInventory()->RemoveItem(RequiredItem);
		}
		if (CompleteSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, CompleteSound, GetActorLocation());
		}
		Watch->Tend(TaskId, Keeper);
		return;
	}

	if (!RequiredItem.IsNone() && bConsumeItem)
	{
		Keeper->GetInventory()->RemoveItem(RequiredItem);
	}
	bDoneTonight = true;

	if (CompleteSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, CompleteSound, GetActorLocation());
	}
	if (UStorySubsystem* Story = UStorySubsystem::Get(this))
	{
		Story->Notify(TEXT("task:") + TaskId.ToString());
	}
}

/////////////////////////////////////////////////////////////////////////////
// AStoryVolume

AStoryVolume::AStoryVolume()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Box->SetGenerateOverlapEvents(true);
	Box->SetHiddenInGame(true);
}

void AStoryVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Box->SetBoxExtent(Extent);
}

void AStoryVolume::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &AStoryVolume::OnOverlap);
}

void AStoryVolume::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		if (UStorySubsystem* Story = UStorySubsystem::Get(this))
		{
			Story->Notify(TEXT("enter:") + VolumeId.ToString());
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// ATeleportDoor

ATeleportDoor::ATeleportDoor()
{
	Color = FLinearColor(0.2f, 0.2f, 0.22f);
	Mesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.08f));
}

FText ATeleportDoor::GetPrompt(const ALastWatchPlayerController* Keeper) const
{
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	if (!Data)
	{
		return FText::GetEmpty();
	}
	if (!RequiredItem.IsNone() && !Keeper->GetInventory()->HasItem(RequiredItem))
	{
		return Data->GetUIText(TEXT("prompt.locked"));
	}
	return Data->GetUIText(PromptKey);
}

void ATeleportDoor::Interact(ALastWatchPlayerController* Keeper)
{
	if (bBusy)
	{
		return;
	}
	if (!RequiredItem.IsNone() && !Keeper->GetInventory()->HasItem(RequiredItem))
	{
		if (const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this))
		{
			Keeper->ShowToast(Data->GetUIText(TEXT("toast.locked")));
		}
		return;
	}

	AActor* Target = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TargetTag))
		{
			Target = *It;
			break;
		}
	}
	if (!Target)
	{
		return;
	}

	bBusy = true;
	if (UseSound)
	{
		UGameplayStatics::PlaySound2D(this, UseSound);
	}
	ALastWatchHUD* KeeperHUD = Cast<ALastWatchHUD>(Keeper->GetHUD());
	if (KeeperHUD)
	{
		KeeperHUD->FadeTo(1.0f, 0.5f);
	}

	TWeakObjectPtr<ALastWatchPlayerController> WeakKeeper = Keeper;
	TWeakObjectPtr<AActor> WeakTarget = Target;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakKeeper, WeakTarget, KeeperHUD]
	{
		bBusy = false;
		ALastWatchPlayerController* KeeperNow = WeakKeeper.Get();
		AActor* TargetNow = WeakTarget.Get();
		if (!KeeperNow || !TargetNow || !KeeperNow->GetPawn())
		{
			return;
		}
		KeeperNow->GetPawn()->TeleportTo(TargetNow->GetActorLocation(), TargetNow->GetActorRotation());
		KeeperNow->SetControlRotation(TargetNow->GetActorRotation());
		if (KeeperHUD)
		{
			KeeperHUD->FadeTo(0.0f, 0.8f);
		}
		if (UStorySubsystem* Story = UStorySubsystem::Get(this))
		{
			Story->Notify(TEXT("use:") + DoorId.ToString());
		}
	}), 0.55f, false);
}

/////////////////////////////////////////////////////////////////////////////
// AStormController

AStormController::AStormController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AStormController::SetMoon(float Intensity)
{
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		It->GetLightComponent()->SetIntensity(Intensity);
	}
}

void AStormController::SetStorm(bool bOn)
{
	if (bOn == bStorm)
	{
		return;
	}
	bStorm = bOn;
	if (bStorm)
	{
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			BaseIntensity = It->GetLightComponent()->Intensity;
		}
		ScheduleFlash();
	}
	else
	{
		GetWorldTimerManager().ClearTimer(FlashTimer);
		SetMoon(BaseIntensity);
	}
}

void AStormController::ScheduleFlash()
{
	GetWorldTimerManager().SetTimer(FlashTimer, this, &AStormController::Flash,
		FMath::FRandRange(FlashInterval.X, FlashInterval.Y), false);
}

void AStormController::Flash()
{
	if (!bStorm)
	{
		return;
	}

	// a double flicker, then thunder a moment later as if from a few km away
	SetMoon(FlashIntensity);
	FTimerHandle Off, Again, Off2, Rumble;
	GetWorldTimerManager().SetTimer(Off, FTimerDelegate::CreateWeakLambda(this, [this] { SetMoon(BaseIntensity); }), 0.07f, false);
	GetWorldTimerManager().SetTimer(Again, FTimerDelegate::CreateWeakLambda(this, [this] { SetMoon(FlashIntensity * 0.7f); }), 0.16f, false);
	GetWorldTimerManager().SetTimer(Off2, FTimerDelegate::CreateWeakLambda(this, [this] { SetMoon(BaseIntensity); }), 0.24f, false);

	if (Thunder.Num() > 0)
	{
		USoundBase* Sound = Thunder[FMath::RandRange(0, Thunder.Num() - 1)];
		GetWorldTimerManager().SetTimer(Rumble, FTimerDelegate::CreateWeakLambda(this, [this, Sound]
		{
			UGameplayStatics::PlaySound2D(this, Sound, 0.9f);
		}), FMath::FRandRange(0.8f, 2.6f), false);
	}
	ScheduleFlash();
}
