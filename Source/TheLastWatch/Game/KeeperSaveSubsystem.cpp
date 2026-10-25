// The Last Watch

#include "KeeperSaveSubsystem.h"
#include "AudioDevice.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Kismet/GameplayStatics.h"
#include "Interactables.h"
#include "InventoryComponent.h"
#include "LastWatchPlayerController.h"
#include "TheLastWatch.h"

namespace
{
	const TCHAR* ProgressSlot = TEXT("Progress");
	const TCHAR* SettingsSlot = TEXT("Settings");
}

UKeeperSaveSubsystem* UKeeperSaveSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UKeeperSaveSubsystem>() : nullptr;
}

void UKeeperSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bReadOnly = FParse::Param(FCommandLine::Get(), TEXT("NoMenu"));

	Progress = Cast<ULastWatchProgressSave>(UGameplayStatics::LoadGameFromSlot(ProgressSlot, 0));
	if (!Progress)
	{
		Progress = Cast<ULastWatchProgressSave>(UGameplayStatics::CreateSaveGameObject(ULastWatchProgressSave::StaticClass()));
	}

	Settings = Cast<ULastWatchSettingsSave>(UGameplayStatics::LoadGameFromSlot(SettingsSlot, 0));
	if (!Settings)
	{
		Settings = Cast<ULastWatchSettingsSave>(UGameplayStatics::CreateSaveGameObject(ULastWatchSettingsSave::StaticClass()));
	}

	UE_LOG(LogTheLastWatch, Display, TEXT("Save: progress night %d, language %s"), Progress->Night, *Settings->Language);
}

/////////////////////////////////////////////////////////////////////////////
// Progress

void UKeeperSaveSubsystem::WriteProgress()
{
	if (bReadOnly)
	{
		return;
	}
	if (!UGameplayStatics::SaveGameToSlot(Progress, ProgressSlot, 0))
	{
		UE_LOG(LogTheLastWatch, Warning, TEXT("Save: could not write progress"));
	}
}

bool UKeeperSaveSubsystem::SaveNightStart(int32 Night, const ALastWatchPlayerController* Keeper)
{
	if (!Keeper || bReadOnly)
	{
		return false;
	}

	const UInventoryComponent* Inventory = Keeper->GetInventory();
	FNightSnapshot& Snapshot = Progress->Nights.FindOrAdd(Night);
	Snapshot.Items = Inventory->GetItems();
	Snapshot.Notes = Inventory->GetNotes();
	Snapshot.Taken = Inventory->GetTaken();
	Progress->Night = Night;
	WriteProgress();
	UE_LOG(LogTheLastWatch, Display, TEXT("Save: night %d (%d items, %d notes)"), Night, Snapshot.Items.Num(), Snapshot.Notes.Num());
	return true;
}

bool UKeeperSaveSubsystem::RestoreNight(int32 Night, ALastWatchPlayerController* Keeper) const
{
	const FNightSnapshot* Snapshot = Progress ? Progress->Nights.Find(Night) : nullptr;
	if (!Snapshot || !Keeper)
	{
		return false;
	}

	Keeper->GetInventory()->SetContents(Snapshot->Items, Snapshot->Notes, Snapshot->Taken);

	// documents already read and items already taken are gone from the island
	for (TActorIterator<AInteractableActor> It(Keeper->GetWorld()); It; ++It)
	{
		const ANotePickup* Note = Cast<ANotePickup>(*It);
		const AItemPickup* Item = Cast<AItemPickup>(*It);
		if ((Note && Snapshot->Notes.Contains(Note->GetNoteId())) || (Item && Snapshot->Taken.Contains(Item->GetItemId())))
		{
			It->MarkCollected();
		}
	}
	UE_LOG(LogTheLastWatch, Display, TEXT("Save: restored night %d"), Night);
	return true;
}

void UKeeperSaveSubsystem::RecordEnding(const FString& Ending)
{
	Progress->Endings.AddUnique(Ending);
	WriteProgress();
}

void UKeeperSaveSubsystem::ResetProgress()
{
	Progress->Night = 0;
	Progress->Nights.Reset();
	WriteProgress();
}

/////////////////////////////////////////////////////////////////////////////
// Settings

void UKeeperSaveSubsystem::SaveSettings()
{
	UGameplayStatics::SaveGameToSlot(Settings, SettingsSlot, 0);
	ApplyGlobalSettings(GetGameInstance());
}

void UKeeperSaveSubsystem::ApplyGlobalSettings(const UObject* WorldContext) const
{
	// test runs set the language on the command line
	IConsoleVariable* Language = bReadOnly ? nullptr : IConsoleManager::Get().FindConsoleVariable(TEXT("tlw.Language"));
	if (Language)
	{
		Language->Set(*Settings->Language, ECVF_SetByConsole);
	}

	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (FAudioDeviceHandle Audio = World ? World->GetAudioDevice() : FAudioDeviceHandle())
	{
		Audio->SetTransientPrimaryVolume(Settings->MasterVolume);
	}
}
