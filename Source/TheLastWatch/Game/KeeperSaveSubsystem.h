// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KeeperSaveSubsystem.generated.h"

class ALastWatchPlayerController;

/** What the keeper carried when a night began */
USTRUCT()
struct FNightSnapshot
{
	GENERATED_BODY()

	UPROPERTY() TArray<FName> Items;
	UPROPERTY() TArray<FName> Notes;

	/** Every item ever received, so pickups already taken stay gone after loading */
	UPROPERTY() TArray<FName> Taken;
};

/** Story progress: one snapshot per night reached (autosaved as each night starts) */
UCLASS()
class THELASTWATCH_API ULastWatchProgressSave : public USaveGame
{
	GENERATED_BODY()

public:

	/** Latest night started; 0 = no game in progress */
	UPROPERTY() int32 Night = 0;

	UPROPERTY() TMap<int32, FNightSnapshot> Nights;

	/** "light" / "dark" once seen */
	UPROPERTY() TArray<FString> Endings;
};

/** Player options, kept apart from progress so starting a new game keeps them */
UCLASS()
class THELASTWATCH_API ULastWatchSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY() FString Language = TEXT("en");
	UPROPERTY() float MouseSensitivity = 1.0f;
	UPROPERTY() bool bInvertY = false;
	UPROPERTY() float MasterVolume = 1.0f;
	UPROPERTY() float FieldOfView = 90.0f;

	/** 0 small, 1 normal, 2 large */
	UPROPERTY() int32 SubtitleSize = 1;

	/** Show the current objective in the corner */
	UPROPERTY() bool bShowObjectives = true;

	/** Gentle camera sway while walking */
	UPROPERTY() bool bHeadBob = true;
};

/**
 *  Loads and writes the progress and settings save slots, and applies settings to the game.
 *  Lives on the game instance so it survives returning to the title screen (a level reload).
 */
UCLASS()
class THELASTWATCH_API UKeeperSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	static UKeeperSaveSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// progress

	bool HasProgress() const { return Progress && Progress->Night > 0; }
	int32 GetSavedNight() const { return Progress ? Progress->Night : 0; }
	bool IsNightUnlocked(int32 Night) const { return Night == 1 || (Progress && Progress->Nights.Contains(Night)); }
	bool HasSeenEnding(const FString& Ending) const { return Progress && Progress->Endings.Contains(Ending); }
	int32 GetEndingCount() const { return Progress ? Progress->Endings.Num() : 0; }

	/** Records the keeper's inventory as the start of Night and writes the slot; false when saving is off */
	bool SaveNightStart(int32 Night, const ALastWatchPlayerController* Keeper);

	/** Puts the inventory saved for Night back on the keeper; false if there is none */
	bool RestoreNight(int32 Night, ALastWatchPlayerController* Keeper) const;

	void RecordEnding(const FString& Ending);

	/** Wipes progress for a new game (endings seen are kept) */
	void ResetProgress();

	// settings

	ULastWatchSettingsSave* GetSettings() const { return Settings; }

	/** Writes the settings slot and applies global settings (language, volume) */
	void SaveSettings();

	/** Language and volume; per-player settings are read by the controller each frame */
	void ApplyGlobalSettings(const UObject* WorldContext) const;

private:

	void WriteProgress();

	/** Automated test runs (-NoMenu) leave the player's save alone */
	bool bReadOnly = false;

	UPROPERTY() TObjectPtr<ULastWatchProgressSave> Progress;
	UPROPERTY() TObjectPtr<ULastWatchSettingsSave> Settings;
};
