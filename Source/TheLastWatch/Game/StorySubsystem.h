// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "StorySubsystem.generated.h"

class ALastWatchHUD;
class ALastWatchPlayerController;
struct FNightData;
struct FObjective;
struct FStoryAction;

/**
 *  Runs the five nights from Content/Data/Nights.json.
 *
 *  Each night is a list of objectives. Gameplay reports events ("read:elias_01", "task:refuel",
 *  "enter:cottage", "radio:margaret_n1"...) through Notify(); when an event matches the current
 *  objective it completes, its on_complete actions run and the next objective starts.
 *
 *  Actors take part through tags:
 *    night:2      / night:3-5   visible (and solid) only on those nights
 *    <any name>                 targets for show / hide / teleport actions
 */
UCLASS()
class THELASTWATCH_API UStorySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	static UStorySubsystem* Get(const UObject* WorldContext);

	/** Resets the island for the given night and starts its script */
	void StartNight(int32 Night);

	/** Reports something the keeper did */
	void Notify(const FString& Event);

	/** True when Event would complete the current objective (used to gate story tasks) */
	bool IsExpected(const FString& Event) const;

	int32 GetNight() const { return CurrentNight; }

	/** True once any night has been started (by the game mode or a debug command) */
	bool HasStarted() const { return Generation > 0; }
	FText GetObjectiveText() const;
	bool HasEnded() const { return bEnded; }

	/** Index of the current objective in tonight's list (INDEX_NONE before the first) */
	int32 GetObjectiveIndex() const { return ObjectiveIndex; }

	/** Changes every time a night starts, including a restart of the same night */
	int32 GetGeneration() const { return Generation; }

	const FNightData* GetNightData() const;

	/** Debug: the current objective id */
	FString GetObjectiveId() const;

	/** Debug: completes the current objective as if its event happened */
	void SkipObjective();

	/** Debug: runs one story action right now, e.g. ("crowd", "on") */
	void RunDebugAction(const FString& Type, const FString& Arg, float Duration);

private:

	const FObjective* GetObjective() const;

	void StartObjective(int32 Index);
	void RunActions(const TArray<FStoryAction>& Actions);
	void Execute(const FStoryAction& Action);
	void ApplyNightVisibility();
	void SetTaggedVisible(FName Tag, bool bVisible);

	ALastWatchPlayerController* GetKeeper() const;
	ALastWatchHUD* GetHUD() const;

	int32 CurrentNight = 1;
	int32 ObjectiveIndex = INDEX_NONE;
	bool bEnded = false;

	/** Bumped on every night start, so delayed actions from an earlier night are dropped */
	int32 Generation = 0;
};
