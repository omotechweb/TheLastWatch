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

void UGameDataSubsystem::Reload()
{
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

			const TArray<TSharedPtr<FJsonValue>>* Nights = nullptr;
			if (Object->TryGetArrayField(TEXT("nights"), Nights))
			{
				for (const TSharedPtr<FJsonValue>& Night : *Nights)
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

	if (const TSharedPtr<FJsonObject> Root = LoadJsonFile(TEXT("UI.json")))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->GetObjectField(TEXT("strings"))->Values)
		{
			UIStrings.Add(Pair.Key, ParseLocValue(Pair.Value));
		}
	}

	UE_LOG(LogTheLastWatch, Log, TEXT("Game data loaded: %d notes, %d items, %d transmissions, %d UI strings"),
		Notes.Num(), Items.Num(), Transmissions.Num(), UIStrings.Num());
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
