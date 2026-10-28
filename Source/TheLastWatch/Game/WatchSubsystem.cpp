// The Last Watch

#include "WatchSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameDataSubsystem.h"
#include "InventoryComponent.h"
#include "KeeperDirector.h"
#include "LastWatchHUD.h"
#include "LastWatchPlayerController.h"
#include "LighthouseBeam.h"
#include "StorySubsystem.h"
#include "TheLastWatch.h"

namespace WatchIds
{
	const FName Wind(TEXT("wind"));
	const FName Refuel(TEXT("refuel"));
	const FName FuelDrum(TEXT("fuel_drum"));
	const FName FuelCan(TEXT("fuel_can"));
}

UWatchSubsystem* UWatchSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWatchSubsystem>() : nullptr;
}

TStatId UWatchSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWatchSubsystem, STATGROUP_Tickables);
}

bool UWatchSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

ALastWatchPlayerController* UWatchSubsystem::GetKeeper() const
{
	return Cast<ALastWatchPlayerController>(GetWorld()->GetFirstPlayerController());
}

ALighthouseBeam* UWatchSubsystem::GetBeam()
{
	if (!Beam.IsValid())
	{
		TActorIterator<ALighthouseBeam> It(GetWorld());
		Beam = It ? *It : nullptr;
	}
	return Beam.Get();
}

void UWatchSubsystem::Say(const TCHAR* Key, float Duration) const
{
	const ALastWatchPlayerController* Keeper = GetKeeper();
	ALastWatchHUD* KeeperHUD = Keeper ? Cast<ALastWatchHUD>(Keeper->GetHUD()) : nullptr;
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	if (KeeperHUD && Data)
	{
		KeeperHUD->ShowTimedSubtitle(FText::GetEmpty(), Data->GetUIText(Key), Duration);
	}
}

void UWatchSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UStorySubsystem* Story = UStorySubsystem::Get(this);
	if (!Story || !Story->HasStarted())
	{
		return;
	}

	UpdateClock(DeltaTime, *Story);

	// the watch strikes every hour it passes
	const int32 Hour = FMath::FloorToInt(Clock / 60.0f);
	while (LastHour < Hour)
	{
		++LastHour;
		if (!Story->HasEnded())
		{
			StrikeHour(LastHour, *Story);
		}
	}

	UpdateLamp(DeltaTime, *Story);
}

/////////////////////////////////////////////////////////////////////////////
// Clock

TArray<float> UWatchSubsystem::BuildAnchors(const FNightData& Night)
{
	const int32 Count = Night.Objectives.Num();
	TArray<float> Result;
	Result.Init(-1.0f, Count + 1);
	for (int32 i = 0; i < Count; ++i)
	{
		Result[i] = static_cast<float>(Night.Objectives[i].Clock);
	}
	if (Result[0] < 0.0f)
	{
		Result[0] = static_cast<float>(Night.ClockStart);
	}
	if (Count > 0)
	{
		Result[Count] = FMath::Max(static_cast<float>(Night.ClockEnd), Result[0] + 1.0f);
	}

	// objectives without a time of their own are spaced evenly between the pinned ones
	int32 Previous = 0;
	for (int32 i = 1; i <= Count; ++i)
	{
		if (Result[i] < 0.0f)
		{
			continue;
		}
		for (int32 j = Previous + 1; j < i; ++j)
		{
			Result[j] = FMath::Lerp(Result[Previous], Result[i], static_cast<float>(j - Previous) / (i - Previous));
		}
		Previous = i;
	}

	// a badly pinned objective never turns the hands back
	for (int32 i = 1; i <= Count; ++i)
	{
		Result[i] = FMath::Max(Result[i], Result[i - 1]);
	}
	return Result;
}

void UWatchSubsystem::UpdateClock(float DeltaTime, UStorySubsystem& Story)
{
	// a new night (or the same one restarted) sets the hands to dusk
	if (Story.GetGeneration() != Generation)
	{
		Generation = Story.GetGeneration();
		const FNightData* Night = Story.GetNightData();
		Anchors = Night ? BuildAnchors(*Night) : TArray<float>();
		ObjectiveIndex = Story.GetObjectiveIndex();
		ObjectiveTime = 0.0f;
		Clock = Anchors.IsEmpty() ? 570.0f : Anchors[FMath::Clamp(ObjectiveIndex, 0, Anchors.Num() - 1)];
		LastHour = FMath::FloorToInt(Clock / 60.0f);
		bWasLit = false;
		SetState(ELampState::Cold, Story);
	}

	if (Story.GetObjectiveIndex() != ObjectiveIndex)
	{
		ObjectiveIndex = Story.GetObjectiveIndex();
		ObjectiveTime = 0.0f;
	}
	ObjectiveTime += DeltaTime;

	if (Anchors.Num() < 2)
	{
		return;
	}

	// the last step is done and the night is closing (sleep, an ending): the hands stop with it
	if (ObjectiveIndex >= Anchors.Num() - 1)
	{
		return;
	}

	float Target;
	if (ObjectiveIndex == INDEX_NONE)
	{
		Target = Anchors[0];
	}
	else
	{
		// creep towards the next step without ever reaching it while this one is open
		const float From = Anchors[ObjectiveIndex];
		const float To = Anchors[ObjectiveIndex + 1];
		Target = From + (To - From) * CreepShare * (1.0f - FMath::Exp(-ObjectiveTime / CreepSeconds));
	}

	// the hands only go forwards
	Clock = FMath::Max(Clock, FMath::FInterpConstantTo(Clock, Target, DeltaTime, CatchUpRate));
}

void UWatchSubsystem::StrikeHour(int32 HourAfterNoon, UStorySubsystem& Story)
{
	const int32 Hour = (12 + HourAfterNoon) % 24;
	const int32 Strikes = Hour % 12 == 0 ? 12 : Hour % 12;
	UE_LOG(LogTheLastWatch, Display, TEXT("Watch: %02d:00"), Hour);

	if (AKeeperDirector* Director = AKeeperDirector::Get(this))
	{
		Director->Chime(Strikes);

		// the three o'clock check: something out at sea keeps the hour too
		if (Hour == 3)
		{
			Director->Toll(3);
		}
	}
	ShowWatch(4.5f);
	Story.Notify(FString::Printf(TEXT("time:%02d00"), Hour));
}

FText UWatchSubsystem::GetClockText() const
{
	const int32 Minutes = (FMath::FloorToInt(Clock) + 12 * 60) % (24 * 60);
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes / 60, Minutes % 60));
}

void UWatchSubsystem::ShowWatch(float Seconds)
{
	const float Now = GetWorld()->GetRealTimeSeconds();

	// already out: keep it up a little longer rather than fading it in again
	if (GetWatchAlpha() > 0.0f)
	{
		WatchSeconds = FMath::Max(WatchShownAt + WatchSeconds, Now + Seconds) - WatchShownAt;
		return;
	}
	WatchShownAt = Now;
	WatchSeconds = Seconds;
}

float UWatchSubsystem::GetWatchAlpha() const
{
	// 0.3 s in, hold, 0.8 s out
	const float Age = GetWorld()->GetRealTimeSeconds() - WatchShownAt;
	if (Age < 0.0f || Age > WatchSeconds)
	{
		return 0.0f;
	}
	return FMath::Clamp(FMath::Min(Age / 0.3f, (WatchSeconds - Age) / 0.8f), 0.0f, 1.0f);
}

void UWatchSubsystem::DebugSetClock(int32 MinutesAfterNoon)
{
	// landing exactly on the hour strikes it
	Clock = static_cast<float>(MinutesAfterNoon);
	LastHour = FMath::FloorToInt((Clock - 1.0f) / 60.0f);
	ShowWatch();
}

/////////////////////////////////////////////////////////////////////////////
// Lamp

void UWatchSubsystem::UpdateLamp(float DeltaTime, UStorySubsystem& Story)
{
	ALighthouseBeam* Lamp = GetBeam();
	AKeeperDirector* Director = AKeeperDirector::Get(this);

	if (!Lamp || !Lamp->IsLit() || Story.HasEnded())
	{
		bWasLit = false;
		SetState(ELampState::Cold, Story);
		if (Director)
		{
			Director->SetLampNeglect(0.0f);
		}
		return;
	}

	// the story has just lit it: freshly filled and wound
	if (!bWasLit)
	{
		bWasLit = true;
		Tension = 1.0f;
		Fuel = 1.0f;
		State = ELampState::Steady;
	}

	// the lamp keeps burning while the keeper walks, climbs and listens, but not while a page is read
	const ALastWatchPlayerController* Keeper = GetKeeper();
	const bool bRunning = !Keeper || (Keeper->GetMode() != EKeeperMode::Reading && Keeper->GetMode() != EKeeperMode::Menu);
	if (bRunning)
	{
		Tension = FMath::Max(0.0f, Tension - DeltaTime / WindSeconds);
		Fuel = FMath::Max(0.0f, Fuel - DeltaTime / FuelSeconds);
	}

	// low clockwork slows the turn to a halt; low paraffin makes the flame gutter
	const float Drive = Tension >= SlowBelow ? 1.0f : FMath::SmoothStep(0.0f, SlowBelow, Tension);
	float Power = 1.0f;
	if (Fuel <= 0.0f)
	{
		Power = 0.0f;
	}
	else if (Fuel < GutterBelow)
	{
		GutterChange -= DeltaTime;
		if (GutterChange <= 0.0f)
		{
			// mostly a restless flicker, now and then a near-death dip
			GutterTarget = FMath::FRand() < 0.12f ? 0.08f : FMath::FRandRange(0.45f, 1.0f);
			GutterChange = FMath::FRandRange(0.06f, 0.3f);
		}
		Gutter = FMath::FInterpTo(Gutter, GutterTarget, DeltaTime, 14.0f);
		Power = FMath::Lerp(0.3f, 0.85f, Fuel / GutterBelow) * Gutter;
	}
	Lamp->SetCondition(Drive, Power);

	ELampState NewState = ELampState::Steady;
	if (Fuel <= 0.0f)              { NewState = ELampState::Out; }
	else if (Tension <= 0.0f)      { NewState = ELampState::Stopped; }
	else if (Fuel < GutterBelow)   { NewState = ELampState::Guttering; }
	else if (Tension < SlowBelow)  { NewState = ELampState::Slowing; }
	SetState(NewState, Story);

	if (Director)
	{
		const bool bDead = State == ELampState::Out || State == ELampState::Stopped;
		const bool bFailing = State == ELampState::Slowing || State == ELampState::Guttering;
		Director->SetLampNeglect(bDead ? 1.0f : bFailing ? 0.3f : 0.0f);
	}
}

void UWatchSubsystem::SetState(ELampState NewState, UStorySubsystem& Story)
{
	if (NewState == State)
	{
		return;
	}
	const ELampState Old = State;
	State = NewState;
	if (NewState == ELampState::Cold || NewState == ELampState::Steady)
	{
		return;
	}

	// only getting worse is news; a lamp that improves is reported by Tend
	if (static_cast<uint8>(NewState) < static_cast<uint8>(Old))
	{
		return;
	}

	AKeeperDirector* Director = AKeeperDirector::Get(this);
	const TCHAR* Event = nullptr;
	switch (NewState)
	{
	case ELampState::Slowing:
		Event = TEXT("lamp:slowing");
		Say(TEXT("lamp.say.slowing"));
		break;
	case ELampState::Guttering:
		Event = TEXT("lamp:guttering");
		Say(TEXT("lamp.say.guttering"));
		break;
	case ELampState::Stopped:
		Event = TEXT("lamp:stopped");
		Say(TEXT("lamp.say.stopped"));
		if (Director)
		{
			Director->Sting(0.5f);
			Director->Whisper(5.0f);
		}
		break;
	case ELampState::Out:
		Event = TEXT("lamp:out");
		Say(TEXT("lamp.say.out"));
		if (Director)
		{
			Director->Sting(0.9f);
			Director->AddPanic(0.35f);
		}
		break;
	default:
		break;
	}

	UE_LOG(LogTheLastWatch, Display, TEXT("Lamp: %s (clockwork %.0f%%, paraffin %.0f%%)"), Event ? Event : TEXT("?"), Tension * 100.0f, Fuel * 100.0f);
	ShowWatch(5.0f);

	// the first time, say what to do about it
	if (!bTaught)
	{
		bTaught = true;
		if (ALastWatchPlayerController* Keeper = GetKeeper())
		{
			if (const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this))
			{
				Keeper->ShowToast(Data->GetUIText(TEXT("lamp.hint")));
			}
		}
	}
	if (Event)
	{
		Story.Notify(Event);
	}
}

FText UWatchSubsystem::GetLampText() const
{
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	if (!Data)
	{
		return FText::GetEmpty();
	}
	switch (State)
	{
	case ELampState::Steady:    return Data->GetUIText(Tension < TendWindBelow || Fuel < TendFuelBelow ? TEXT("lamp.state.steady") : TEXT("lamp.state.fresh"));
	case ELampState::Slowing:   return Data->GetUIText(TEXT("lamp.state.slowing"));
	case ELampState::Guttering: return Data->GetUIText(TEXT("lamp.state.guttering"));
	case ELampState::Stopped:   return Data->GetUIText(TEXT("lamp.state.stopped"));
	case ELampState::Out:       return Data->GetUIText(TEXT("lamp.state.out"));
	default:                    return Data->GetUIText(TEXT("lamp.state.cold"));
	}
}

bool UWatchSubsystem::CanTend(FName TaskId, const ALastWatchPlayerController* Keeper) const
{
	if (!bWasLit || State == ELampState::Cold || !Keeper)
	{
		return false;
	}
	if (TaskId == WatchIds::Wind)
	{
		return Tension < TendWindBelow;
	}
	if (TaskId == WatchIds::Refuel)
	{
		return Fuel < TendFuelBelow;
	}
	if (TaskId == WatchIds::FuelDrum)
	{
		return Fuel < TendFuelBelow && !Keeper->GetInventory()->HasItem(WatchIds::FuelCan);
	}
	return false;
}

void UWatchSubsystem::Tend(FName TaskId, ALastWatchPlayerController* Keeper)
{
	UStorySubsystem* Story = UStorySubsystem::Get(this);
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);

	if (TaskId == WatchIds::FuelDrum)
	{
		Keeper->GetInventory()->AddItem(WatchIds::FuelCan);
		const FItemData* Item = Data ? Data->FindItem(WatchIds::FuelCan) : nullptr;
		if (Item)
		{
			Keeper->ShowToast(Data->GetUIText(TEXT("toast.picked_up"), Item->Name.Get()));
		}
		return;
	}

	if (TaskId == WatchIds::Wind)
	{
		Tension = 1.0f;
	}
	else if (TaskId == WatchIds::Refuel)
	{
		Fuel = 1.0f;
		Gutter = GutterTarget = 1.0f;
	}
	else
	{
		return;
	}

	UE_LOG(LogTheLastWatch, Display, TEXT("Lamp: tended (%s) - clockwork %.0f%%, paraffin %.0f%%"), *TaskId.ToString(), Tension * 100.0f, Fuel * 100.0f);
	if (Story)
	{
		UpdateLamp(0.0f, *Story);
		Story->Notify(TEXT("lamp:tended"));
	}
	ShowWatch(4.0f);
}

void UWatchSubsystem::DebugSetLamp(float InTension, float InFuel)
{
	Tension = FMath::Clamp(InTension, 0.0f, 1.0f);
	Fuel = FMath::Clamp(InFuel, 0.0f, 1.0f);
	ShowWatch();
}
