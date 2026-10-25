// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Brushes/SlateColorBrush.h"
#include "LastWatchHUD.generated.h"

class ALastWatchPlayerController;
class SWidget;

/**
 *  All on-screen interface, built in Slate from C++:
 *  crosshair + interaction prompt, subtitles, pickup toasts,
 *  the document reader, the radio dial and the inventory.
 *  Widgets read their content from the keeper controller every frame through lambdas.
 */
UCLASS()
class THELASTWATCH_API ALastWatchHUD : public AHUD
{
	GENERATED_BODY()

protected:

	/** Seconds a toast stays on screen */
	UPROPERTY(EditAnywhere, Category="HUD", meta=(Units="s"))
	float ToastDuration = 3.5f;

public:

	ALastWatchHUD();

	void ShowToast(const FText& Text);
	void SetSubtitle(const FText& Speaker, const FText& Line);

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	ALastWatchPlayerController* GetKeeper() const;

	TSharedRef<SWidget> BuildCrosshair();
	TSharedRef<SWidget> BuildSubtitles();
	TSharedRef<SWidget> BuildReader();
	TSharedRef<SWidget> BuildRadio();
	TSharedRef<SWidget> BuildInventory();

	FText GetReaderTitle() const;
	FText GetReaderBody() const;
	FText GetReaderPage() const;
	FText GetRadioFrequency() const;
	FText GetRadioSignal() const;
	FText GetInventoryItems() const;
	FText GetInventoryNotes() const;
	FText UIText(const TCHAR* Key) const;

	TSharedPtr<SWidget> Root;

	/** Solid white brush, tinted per widget */
	TUniquePtr<FSlateColorBrush> WhiteBrush;

	FText ToastText;
	float ToastStartTime = -100.0f;

	FText SubtitleSpeaker;
	FText SubtitleLine;
};
