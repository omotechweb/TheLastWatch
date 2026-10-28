// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameDataSubsystem.generated.h"

class FJsonObject;
class FJsonValue;

/** A string with an English original and an optional Turkish translation */
struct FLocString
{
	FString En;
	FString Tr;

	/** Returns the text in the current game language (tlw.Language), falling back to English */
	FText Get() const;
};

struct FNoteData
{
	FName Id;
	FLocString Title;
	TArray<FLocString> Pages;
};

struct FItemData
{
	FName Id;
	FLocString Name;
	FLocString Description;
};

struct FRadioLine
{
	FString Speaker;
	FLocString Text;
	float Duration = 3.0f;
};

/** Something the keeper can say back during a call; Margaret (or whoever) answers with Reply */
struct FRadioChoice
{
	FName Id;
	FLocString Text;
	TArray<FRadioLine> Reply;
};

struct FRadioTransmission
{
	FName Id;
	float Frequency = 0.0f;

	/** Nights this transmission is on air. Empty means every night. */
	TArray<int32> Nights;

	bool bLoop = false;
	float LoopGap = 2.0f;
	TArray<FRadioLine> Lines;

	/** Replies offered once the line with this index has been heard (see "choices" in Radio.json) */
	TMap<int32, TArray<FRadioChoice>> Choices;

	bool IsOnAir(int32 Night) const { return Nights.Num() == 0 || Nights.Contains(Night); }
};

/** One step of a story script, run by UStorySubsystem (see Content/Data/Nights.json) */
struct FStoryAction
{
	FString Type;
	FString Arg;
	FString Speaker;
	FLocString Text;
	float Delay = 0.0f;
	float Duration = 4.0f;
};

struct FObjective
{
	FString Id;

	/** Shown in the corner while active; empty = silent step */
	FLocString Text;

	/** Event that completes it: read:<note> take:<item> task:<id> enter:<volume> radio:<transmission> use:<door> delay:<seconds> */
	FString CompleteOn;

	/** Time on the keeper's watch when this step starts, in minutes after noon (see UWatchSubsystem); -1 = in between its neighbours */
	int32 Clock = -1;

	TArray<FStoryAction> OnStart;
	TArray<FStoryAction> OnComplete;
};

struct FNightData
{
	int32 Night = 1;
	FLocString Title;
	TArray<FStoryAction> OnStart;
	TArray<FObjective> Objectives;

	/** The watch at dusk and at dawn, in minutes after noon (21:30 = 570, 05:40 = 1060) */
	int32 ClockStart = 570;
	int32 ClockEnd = 1060;

	/** Actions for an event whenever it happens this night, objective or not (e.g. the final choice) */
	TMap<FString, TArray<FStoryAction>> Events;
};

/**
 *  Loads all narrative and interface data from Content/Data/*.json at startup.
 *  Every in-game string lives in those files so it can be translated without touching code.
 */
UCLASS()
class THELASTWATCH_API UGameDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Re-reads every data file from disk */
	void Reload();

	const FNoteData* FindNote(FName Id) const { return Notes.Find(Id); }
	const FItemData* FindItem(FName Id) const { return Items.Find(Id); }
	const TArray<FRadioTransmission>& GetTransmissions() const { return Transmissions; }
	const FNightData* FindNight(int32 Night) const { return Nights.FindByPredicate([Night](const FNightData& N) { return N.Night == Night; }); }
	int32 GetNightCount() const { return Nights.Num(); }

	/** Returns a UI string by key, with {0} replaced by Arg when given */
	FText GetUIText(const FString& Key, const FText& Arg = FText::GetEmpty()) const;

	/** Convenience accessor from any world context object */
	static UGameDataSubsystem* Get(const UObject* WorldContext);

private:

	TSharedPtr<FJsonObject> LoadJsonFile(const FString& FileName) const;
	static FLocString ParseLoc(const TSharedPtr<FJsonObject>& Object, const FString& Field);
	static FLocString ParseLocValue(const TSharedPtr<FJsonValue>& Value);
	static TArray<FStoryAction> ParseActions(const TSharedPtr<FJsonObject>& Object, const FString& Field);
	static FRadioLine ParseRadioLine(const TSharedPtr<FJsonObject>& Object);

	/** "HH:MM" as minutes after noon (so a night runs upwards past midnight); Fallback when missing or malformed */
	static int32 ParseClock(const TSharedPtr<FJsonObject>& Object, const FString& Field, int32 Fallback);

	TArray<FNightData> Nights;
	TMap<FName, FNoteData> Notes;
	TMap<FName, FItemData> Items;
	TArray<FRadioTransmission> Transmissions;
	TMap<FString, FLocString> UIStrings;
};
