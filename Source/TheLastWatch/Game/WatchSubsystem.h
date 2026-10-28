// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WatchSubsystem.generated.h"

class ALastWatchPlayerController;
class ALighthouseBeam;
class UStorySubsystem;
struct FNightData;

/** How the lighthouse lamp is doing, worst condition first when several apply */
enum class ELampState : uint8
{
	/** Not lit (the story has not brought it up yet, or put it out) */
	Cold,
	Steady,

	/** Clockwork running down: the beam turns slower and slower */
	Slowing,

	/** Paraffin low: the flame gutters and the beam dims */
	Guttering,

	/** Clockwork spent: the beam stands still, pointing at one patch of sea */
	Stopped,

	/** Reservoir dry: no light at all */
	Out
};

/**
 *  The keeper's watch: the time of night and the upkeep of the lamp.
 *
 *  Clock. Every night runs from dusk (clock_start in Nights.json) to dawn (clock_end). Objectives may
 *  pin the time they start at ("clock": "03:00"); the rest are spaced evenly in between. While an
 *  objective is open the hands creep towards the next one, so the night keeps pace with the story
 *  however fast it is played. Each hour reports "time:HH00" to the story (the watch strikes it; at three
 *  a bell answers from the sea), so Nights.json can hang events on the 3 AM check.
 *
 *  Lamp. Once the story lights the lamp, its clockwork runs down and its paraffin burns. A slowing
 *  beam and a guttering flame are warnings; a stopped or dark lamp raises the dread and lets the Watcher
 *  out. The keeper tends it with the same lantern-room tasks used to light it (wind, refuel - with a can
 *  filled at the drum below). Reports lamp:slowing / lamp:guttering / lamp:stopped / lamp:out / lamp:tended.
 */
UCLASS()
class THELASTWATCH_API UWatchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	static UWatchSubsystem* Get(const UObject* WorldContext);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// the clock

	/** Minutes after noon (so the night counts upwards past midnight: 03:00 = 900) */
	float GetClock() const { return Clock; }

	/** "03:12" */
	FText GetClockText() const;

	/** The keeper takes out the watch for a few seconds (the T key, or the hour striking) */
	void ShowWatch(float Seconds = 5.0f);

	/** 0..1 opacity of the watch on screen */
	float GetWatchAlpha() const;

	// the lamp

	ELampState GetLampState() const { return State; }

	/** One line on the lamp's state for the watch panel */
	FText GetLampText() const;

	/** 0..1, how much clockwork and paraffin are left */
	float GetTension() const { return Tension; }
	float GetFuel() const { return Fuel; }

	/** True when a lantern task can be used to look after the lamp rather than for the story */
	bool CanTend(FName TaskId, const ALastWatchPlayerController* Keeper) const;

	/** Winds, refuels or fills a can, from a lantern task the story is not waiting for */
	void Tend(FName TaskId, ALastWatchPlayerController* Keeper);

	// debug

	void DebugSetLamp(float InTension, float InFuel);

	/** Moves the hands to HH:MM (striking it if on the hour); they hold there until the story catches up */
	void DebugSetClock(int32 MinutesAfterNoon);

private:

	/** Clockwork lasts this long once wound */
	static constexpr float WindSeconds = 480.0f;

	/** A full reservoir burns this long */
	static constexpr float FuelSeconds = 840.0f;

	/** Below these the beam starts to slow / the flame to gutter */
	static constexpr float SlowBelow = 0.3f;
	static constexpr float GutterBelow = 0.2f;

	/** Tending is offered once the lamp has used this much */
	static constexpr float TendWindBelow = 0.85f;
	static constexpr float TendFuelBelow = 0.7f;

	/** Time constant (s) of the hands creeping towards the next objective, and how much of the gap
	 *  they may cover before it is done */
	static constexpr float CreepSeconds = 150.0f;
	static constexpr float CreepShare = 0.9f;

	/** Fastest the hands catch up after an objective completes, in clock minutes per second */
	static constexpr float CatchUpRate = 4.0f;

	void UpdateClock(float DeltaTime, UStorySubsystem& Story);
	void UpdateLamp(float DeltaTime, UStorySubsystem& Story);
	void StrikeHour(int32 HourAfterNoon, UStorySubsystem& Story);
	void SetState(ELampState NewState, UStorySubsystem& Story);
	void Say(const TCHAR* Key, float Duration = 5.0f) const;

	/** Clock time at the start of every objective, plus dawn at the end */
	static TArray<float> BuildAnchors(const FNightData& Night);

	ALighthouseBeam* GetBeam();
	ALastWatchPlayerController* GetKeeper() const;

	int32 Generation = -1;
	int32 ObjectiveIndex = INDEX_NONE;
	float ObjectiveTime = 0.0f;
	TArray<float> Anchors;

	float Clock = 570.0f;
	int32 LastHour = 0;
	float WatchShownAt = -100.0f;
	float WatchSeconds = 0.0f;

	TWeakObjectPtr<ALighthouseBeam> Beam;
	bool bWasLit = false;
	float Tension = 1.0f;
	float Fuel = 1.0f;
	ELampState State = ELampState::Cold;

	/** Guttering flame: brightness wanders towards a new random level every few frames */
	float Gutter = 1.0f;
	float GutterTarget = 1.0f;
	float GutterChange = 0.0f;

	/** The first warning on the first night comes with a hint on what to do */
	bool bTaught = false;
};
