// The Last Watch

#include "GameDataSubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TheLastWatch.h"

static TAutoConsoleVariable<FString> CVarLanguage(
	TEXT("tlw.Language"),
	TEXT("en"),
	TEXT("Game text language: en or tr"));

static FAutoConsoleCommandWithWorld CmdReloadData(
	TEXT("tlw.ReloadData"),
	TEXT("Re-reads Content/Data/*.json"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UGameDataSubsystem* Data = UGameDataSubsystem::Get(World))
		{
			Data->Reload();
		}
	}));

FText FLocString::Get() const
{
	if (!Tr.IsEmpty() && CVarLanguage.GetValueOnGameThread() == TEXT("tr"))
	{
		return FText::FromString(Tr);
	}
	return FText::FromString(En);
}

void UGameDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Reload();
}

UGameDataSubsystem* UGameDataSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGameDataSubsystem>() : nullptr;
}

TSharedPtr<FJsonObject> UGameDataSubsystem::LoadJsonFile(const FString& FileName) const
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / FileName;

	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *Path))
	{
		UE_LOG(LogTheLastWatch, Error, TEXT("Could not read data file %s"), *Path);
		return nullptr;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTheLastWatch, Error, TEXT("Invalid JSON in %s: %s"), *Path, *Reader->GetErrorMessage());
		return nullptr;
	}
	return Root;
}

FLocString UGameDataSubsystem::ParseLocValue(const TSharedPtr<FJsonValue>& Value)
{
	FLocString Result;
	if (!Value.IsValid())
	{
		return Result;
	}

	// a localized value is either a plain string or { "en": ..., "tr": ... }
	if (Value->Type == EJson::String)
	{
		Result.En = Value->AsString();
	}
	else if (Value->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		Object->TryGetStringField(TEXT("en"), Result.En);
		Object->TryGetStringField(TEXT("tr"), Result.Tr);
	}
	return Result;
}

FLocString UGameDataSubsystem::ParseLoc(const TSharedPtr<FJsonObject>& Object, const FString& Field)
{
	return ParseLocValue(Object->TryGetField(Field));
}

TArray<FStoryAction> UGameDataSubsystem::ParseActions(const TSharedPtr<FJsonObject>& Object, const FString& Field)
{
	TArray<FStoryAction> Actions;
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object->TryGetArrayField(Field, Values))
	{
		return Actions;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject> ActionObject = Value->AsObject();
		FStoryAction Action;
		ActionObject->TryGetStringField(TEXT("type"), Action.Type);
		ActionObject->TryGetStringField(TEXT("arg"), Action.Arg);
		ActionObject->TryGetStringField(TEXT("speaker"), Action.Speaker);
		Action.Text = ParseLoc(ActionObject, TEXT("text"));

		double Number = 0.0;
		if (ActionObject->TryGetNumberField(TEXT("delay"), Number)) { Action.Delay = Number; }
		if (ActionObject->TryGetNumberField(TEXT("duration"), Number)) { Action.Duration = Number; }
		Actions.Add(MoveTemp(Action));
	}
	return Actions;
}

void UGameDataSubsystem::Reload()
{
	Nights.Reset();
	Notes.Reset();
	Items.Reset();
	Transmissions.Reset();
	UIStrings.Reset();

	if (const TSharedPtr<FJsonObject> Root = LoadJsonFile(TEXT("Notes.json")))
	{
		for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("notes")))
		{
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			FNoteData Note;
			Note.Id = FName(Object->GetStringField(TEXT("id")));
			Note.Title = ParseLoc(Object, TEXT("title"));
			for (const TSharedPtr<FJsonValue>& Page : Object->GetArrayField(TEXT("pages")))
			{
				Note.Pages.Add(ParseLocValue(Page));
			}
			Notes.Add(Note.Id, MoveTemp(Note));
		}
	}

	if (const TSharedPtr<FJsonObject> Root = LoadJsonFile(TEXT("Items.json")))
	{
		for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("items")))
		{
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			FItemData Item;
			Item.Id = FName(Object->GetStringField(TEXT("id")));
			Item.Name = ParseLoc(Object, TEXT("name"));
			Item.Description = ParseLoc(Object, TEXT("description"));
			Items.Add(Item.Id, MoveTemp(Item));
		}
	}

	if (const TSharedPtr<FJsonObject> Root = LoadJsonFile(TEXT("Radio.json")))
	{
		for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("transmissions")))
		{
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			FRadioTransmission Transmission;
			Transmission.Id = FName(Object->GetStringField(TEXT("id")));
			Transmission.Frequency = Object->GetNumberField(TEXT("frequency"));
			Object->TryGetBoolField(TEXT("loop"), Transmission.bLoop);

			double LoopGap = Transmission.LoopGap;
			Object->TryGetNumberField(TEXT("loop_gap"), LoopGap);
			Transmission.LoopGap = LoopGap;

			const TArray<TSharedPtr<FJsonValue>>* OnAirNights = nullptr;
			if (Object->TryGetArrayField(TEXT("nights"), OnAirNights))
			{
				for (const TSharedPtr<FJsonValue>& Night : *OnAirNights)
				{
					Transmission.Nights.Add(static_cast<int32>(Night->AsNumber()));
				}
			}

			for (const TSharedPtr<FJsonValue>& LineValue : Object->GetArrayField(TEXT("lines")))
			{
				const TSharedPtr<FJsonObject> LineObject = LineValue->AsObject();
				FRadioLine Line;
				LineObject->TryGetStringField(TEXT("speaker"), Line.Speaker);
				Line.Text = ParseLoc(LineObject, TEXT("text"));

				double Duration = Line.Duration;
				LineObject->TryGetNumberField(TEXT("duration"), Duration);
				Line.Duration = Duration;

				Transmission.Lines.Add(MoveTemp(Line));
			}
			Transmissions.Add(MoveTemp(Transmission));
		}
	}

	if (const TSharedPtr<FJsonObject> Root = LoadJsonFile(TEXT("Nights.json")))
	{
		for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("nights")))
		{
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			FNightData Night;
			Night.Night = static_cast<int32>(Object->GetNumberField(TEXT("night")));
			Night.Title = ParseLoc(Object, TEXT("title"));
			Night.OnStart = ParseActions(Object, TEXT("on_start"));

			for (const TSharedPtr<FJsonValue>& ObjectiveValue : Object->GetArrayField(TEXT("objectives")))
			{
				const TSharedPtr<FJsonObject> ObjectiveObject = ObjectiveValue->AsObject();
				FObjective Objective;
				ObjectiveObject->TryGetStringField(TEXT("id"), Objective.Id);
				ObjectiveObject->TryGetStringField(TEXT("complete_on"), Objective.CompleteOn);
				Objective.Text = ParseLoc(ObjectiveObject, TEXT("text"));
				Objective.OnStart = ParseActions(ObjectiveObject, TEXT("on_start"));
				Objective.OnComplete = ParseActions(ObjectiveObject, TEXT("on_complete"));
				Night.Objectives.Add(MoveTemp(Objective));
			}

			const TSharedPtr<FJsonObject>* Events = nullptr;
			if (Object->TryGetObjectField(TEXT("events"), Events))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Events)->Values)
				{
					TSharedPtr<FJsonObject> Wrapper = MakeShared<FJsonObject>();
					Wrapper->SetField(TEXT("actions"), Pair.Value);
					Night.Events.Add(Pair.Key, ParseActions(Wrapper, TEXT("actions")));
				}
			}
			Nights.Add(MoveTemp(Night));
		}
	}

	if (const TSharedPtr<FJsonObject> Root = LoadJsonFile(TEXT("UI.json")))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->GetObjectField(TEXT("strings"))->Values)
		{
			UIStrings.Add(Pair.Key, ParseLocValue(Pair.Value));
		}
	}

	UE_LOG(LogTheLastWatch, Log, TEXT("Game data loaded: %d nights, %d notes, %d items, %d transmissions, %d UI strings"),
		Nights.Num(), Notes.Num(), Items.Num(), Transmissions.Num(), UIStrings.Num());
}

FText UGameDataSubsystem::GetUIText(const FString& Key, const FText& Arg) const
{
	const FLocString* Found = UIStrings.Find(Key);
	if (!Found)
	{
		UE_LOG(LogTheLastWatch, Warning, TEXT("Missing UI string '%s'"), *Key);
		return FText::FromString(Key);
	}
	return FText::Format(FTextFormat(Found->Get()), Arg);
}
