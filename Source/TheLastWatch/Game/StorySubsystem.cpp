// The Last Watch

#include "StorySubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/AmbientSound.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "GameDataSubsystem.h"
#include "InventoryComponent.h"
#include "KeeperSaveSubsystem.h"
#include "LastWatchGameMode.h"
#include "LastWatchHUD.h"
#include "LastWatchPlayerController.h"
#include "LighthouseBeam.h"
#include "StoryActors.h"
#include "TheLastWatch.h"

UStorySubsystem* UStorySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UStorySubsystem>() : nullptr;
}

ALastWatchPlayerController* UStorySubsystem::GetKeeper() const
{
	return Cast<ALastWatchPlayerController>(GetWorld()->GetFirstPlayerController());
}

ALastWatchHUD* UStorySubsystem::GetHUD() const
{
	const ALastWatchPlayerController* Keeper = GetKeeper();
	return Keeper ? Cast<ALastWatchHUD>(Keeper->GetHUD()) : nullptr;
}

const FNightData* UStorySubsystem::GetNightData() const
{
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	return Data ? Data->FindNight(CurrentNight) : nullptr;
}

const FObjective* UStorySubsystem::GetObjective() const
{
	const FNightData* Night = GetNightData();
	return Night && Night->Objectives.IsValidIndex(ObjectiveIndex) ? &Night->Objectives[ObjectiveIndex] : nullptr;
}

FString UStorySubsystem::GetObjectiveId() const
{
	const FObjective* Objective = GetObjective();
	return Objective ? Objective->Id : FString();
}

FText UStorySubsystem::GetObjectiveText() const
{
	const FObjective* Objective = GetObjective();
	return Objective && !bEnded ? Objective->Text.Get() : FText::GetEmpty();
}

/////////////////////////////////////////////////////////////////////////////
// Flow

void UStorySubsystem::StartNight(int32 Night)
{
	++Generation;
	CurrentNight = Night;
	bEnded = false;
	ObjectiveIndex = INDEX_NONE;

	if (ALastWatchGameMode* GameMode = GetWorld()->GetAuthGameMode<ALastWatchGameMode>())
	{
		GameMode->SetCurrentNight(Night);
	}

	// a debug command can start a night from the title screen
	if (ALastWatchPlayerController* Keeper = GetKeeper(); Keeper && Keeper->GetMode() == EKeeperMode::Menu)
	{
		Keeper->CloseMenu();
	}

	// every night starts dark; the keeper has to bring the lamp up again
	for (TActorIterator<ALighthouseBeam> It(GetWorld()); It; ++It)
	{
		It->SetLit(false);
	}
	for (TActorIterator<AStoryTask> It(GetWorld()); It; ++It)
	{
		It->ResetForNight();
	}
	ApplyNightVisibility();

	const FNightData* Data = GetNightData();
	UE_LOG(LogTheLastWatch, Display, TEXT("Story: night %d starts (%s)"), Night, Data ? TEXT("scripted") : TEXT("no script"));
	if (!Data)
	{
		return;
	}

	RunActions(Data->OnStart);
	StartObjective(0);

	// autosave: the start of every night can be continued or replayed from the title
	if (UKeeperSaveSubsystem* Save = UKeeperSaveSubsystem::Get(this); Save && Save->SaveNightStart(Night, GetKeeper()))
	{
		if (ALastWatchHUD* KeeperHUD = GetHUD())
		{
			KeeperHUD->ShowSaveIndicator();
		}
	}
}

void UStorySubsystem::StartObjective(int32 Index)
{
	ObjectiveIndex = Index;
	const FObjective* Objective = GetObjective();
	if (!Objective)
	{
		UE_LOG(LogTheLastWatch, Display, TEXT("Story: night %d has no objectives left"), CurrentNight);
		return;
	}

	UE_LOG(LogTheLastWatch, Display, TEXT("Story: objective '%s' (waits for %s)"), *Objective->Id, *Objective->CompleteOn);
	RunActions(Objective->OnStart);

	// timed steps complete on their own
	if (Objective->CompleteOn.StartsWith(TEXT("delay:")))
	{
		const float Seconds = FCString::Atof(*Objective->CompleteOn.RightChop(6));
		const FString Event = Objective->CompleteOn;
		const int32 ThisGeneration = Generation;
		const int32 ThisIndex = Index;
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Event, ThisGeneration, ThisIndex]
		{
			if (Generation == ThisGeneration && ObjectiveIndex == ThisIndex)
			{
				Notify(Event);
			}
		}), FMath::Max(Seconds, 0.01f), false);
	}
}

bool UStorySubsystem::IsExpected(const FString& Event) const
{
	const FObjective* Objective = GetObjective();
	if (!Objective || bEnded)
	{
		return false;
	}

	// "a|b" completes on either
	TArray<FString> Options;
	Objective->CompleteOn.ParseIntoArray(Options, TEXT("|"));
	return Options.Contains(Event);
}

void UStorySubsystem::Notify(const FString& Event)
{
	UE_LOG(LogTheLastWatch, Display, TEXT("Story: event %s"), *Event);

	if (const FNightData* Night = GetNightData(); Night && !bEnded)
	{
		if (const TArray<FStoryAction>* Reaction = Night->Events.Find(Event))
		{
			RunActions(*Reaction);
		}
	}

	if (!IsExpected(Event))
	{
		return;
	}

	const FObjective* Objective = GetObjective();
	UE_LOG(LogTheLastWatch, Display, TEXT("Story: objective '%s' complete"), *Objective->Id);
	const int32 Next = ObjectiveIndex + 1;
	const int32 ThisGeneration = Generation;
	const TArray<FStoryAction> Actions = Objective->OnComplete;

	// point at the next objective before running the completion actions, so a chained
	// "notify" lands on it; then start it unless the actions moved on to another night
	ObjectiveIndex = Next;
	RunActions(Actions);
	if (Generation == ThisGeneration && ObjectiveIndex == Next)
	{
		StartObjective(Next);
	}
}

void UStorySubsystem::SkipObjective()
{
	// debug commands can arrive before the game mode starts the story
	if (!HasStarted())
	{
		StartNight(CurrentNight);
	}
	if (const FObjective* Objective = GetObjective())
	{
		FString First;
		if (!Objective->CompleteOn.Split(TEXT("|"), &First, nullptr))
		{
			First = Objective->CompleteOn;
		}
		Notify(First);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Actions

void UStorySubsystem::RunActions(const TArray<FStoryAction>& Actions)
{
	for (const FStoryAction& Action : Actions)
	{
		if (Action.Delay <= 0.0f)
		{
			Execute(Action);
			continue;
		}

		const int32 ThisGeneration = Generation;
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Action, ThisGeneration]
		{
			if (Generation == ThisGeneration)
			{
				Execute(Action);
			}
		}), Action.Delay, false);
	}
}

void UStorySubsystem::Execute(const FStoryAction& Action)
{
	ALastWatchPlayerController* Keeper = GetKeeper();
	ALastWatchHUD* KeeperHUD = GetHUD();
	const FString& Type = Action.Type;

	if (Type == TEXT("subtitle") && KeeperHUD)
	{
		KeeperHUD->ShowTimedSubtitle(FText::FromString(Action.Speaker), Action.Text.Get(), Action.Duration);
	}
	else if (Type == TEXT("toast") && Keeper)
	{
		Keeper->ShowToast(Action.Text.Get());
	}
	else if (Type == TEXT("title") && KeeperHUD)
	{
		KeeperHUD->ShowTitleCard(Action.Text.Get(), Action.Duration);
	}
	else if (Type == TEXT("fade_out") && KeeperHUD)
	{
		KeeperHUD->FadeTo(1.0f, Action.Duration);
	}
	else if (Type == TEXT("fade_in") && KeeperHUD)
	{
		KeeperHUD->FadeTo(0.0f, Action.Duration);
	}
	else if (Type == TEXT("show") || Type == TEXT("hide"))
	{
		SetTaggedVisible(FName(Action.Arg), Type == TEXT("show"));
	}
	else if (Type == TEXT("lamp_on") || Type == TEXT("lamp_off"))
	{
		for (TActorIterator<ALighthouseBeam> It(GetWorld()); It; ++It)
		{
			It->SetLit(Type == TEXT("lamp_on"));
		}
	}
	else if (Type == TEXT("give") && Keeper)
	{
		Keeper->GetInventory()->AddItem(FName(Action.Arg));
	}
	else if (Type == TEXT("remove") && Keeper)
	{
		Keeper->GetInventory()->RemoveItem(FName(Action.Arg));
	}
	else if (Type == TEXT("sound"))
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Action.Arg))
		{
			UGameplayStatics::PlaySound2D(this, Sound, Action.Duration > 0.0f ? 1.0f : 1.0f);
		}
	}
	else if (Type == TEXT("teleport") && Keeper && Keeper->GetPawn())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(FName(Action.Arg)))
			{
				Keeper->GetPawn()->TeleportTo(It->GetActorLocation(), It->GetActorRotation());
				Keeper->SetControlRotation(It->GetActorRotation());
				break;
			}
		}
	}
	else if (Type == TEXT("storm"))
	{
		for (TActorIterator<AStormController> It(GetWorld()); It; ++It)
		{
			It->SetStorm(Action.Arg == TEXT("on"));
		}
	}
	else if (Type == TEXT("fog"))
	{
		for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
		{
			It->GetComponent()->SetFogDensity(FCString::Atof(*Action.Arg));
		}
	}
	else if (Type == TEXT("ambience"))
	{
		// "tag=volume": scale every ambient sound carrying the tag
		FString Tag, Volume;
		Action.Arg.Split(TEXT("="), &Tag, &Volume);
		for (TActorIterator<AAmbientSound> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(FName(Tag)))
			{
				It->GetAudioComponent()->SetVolumeMultiplier(FCString::Atof(*Volume));
			}
		}
	}
	else if (Type == TEXT("notify"))
	{
		Notify(Action.Arg);
	}
	else if (Type == TEXT("next_night"))
	{
		StartNight(CurrentNight + 1);
	}
	else if (Type == TEXT("ending") && KeeperHUD)
	{
		bEnded = true;
		KeeperHUD->ShowEnding(Action.Text.Get());
		if (UKeeperSaveSubsystem* Save = UKeeperSaveSubsystem::Get(this); Save && !Action.Arg.IsEmpty())
		{
			Save->RecordEnding(Action.Arg);
		}
		if (Keeper)
		{
			Keeper->EnterEnding();
		}
	}
	else if (Type == TEXT("to_title") && Keeper)
	{
		Keeper->ReturnToTitle();
	}
	else if (Type == TEXT("quit") && Keeper)
	{
		Keeper->ConsoleCommand(TEXT("quit"));
	}
	else
	{
		UE_LOG(LogTheLastWatch, Warning, TEXT("Story: unknown or unavailable action '%s'"), *Type);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Night visibility

void UStorySubsystem::SetTaggedVisible(FName Tag, bool bVisible)
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(Tag))
		{
			It->SetActorHiddenInGame(!bVisible);
			It->SetActorEnableCollision(bVisible);
		}
	}
}

void UStorySubsystem::ApplyNightVisibility()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			FString Range;
			if (!Tag.ToString().Split(TEXT("night:"), nullptr, &Range))
			{
				continue;
			}

			// "3" or "3-5"
			FString First, Last;
			if (!Range.Split(TEXT("-"), &First, &Last))
			{
				First = Last = Range;
			}
			const bool bVisible = CurrentNight >= FCString::Atoi(*First) && CurrentNight <= FCString::Atoi(*Last);

			// picked-up notes and items stay gone
			if (const AInteractableActor* Interactable = Cast<AInteractableActor>(*It); Interactable && Interactable->IsCollected())
			{
				break;
			}
			It->SetActorHiddenInGame(!bVisible);
			It->SetActorEnableCollision(bVisible);
			break;
		}
	}
}
