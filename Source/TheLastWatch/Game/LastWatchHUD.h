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

	/** Continuous subtitle (the radio sets it every frame); empty clears it */
	void SetSubtitle(const FText& Speaker, const FText& Line);

	/** Story line that stays up for Duration seconds */
	void ShowTimedSubtitle(const FText& Speaker, const FText& Line, float Duration);

	/** Big centred text, e.g. "Night 2", that fades in and out */
	void ShowTitleCard(const FText& Text, float Duration);

	/** Fades the screen to black (1) or back (0) */
	void FadeTo(float TargetOpacity, float Seconds);

	/** Jumps the fade straight to Opacity (0 clear, 1 black) */
	void SnapFade(float Opacity);

	/** End of the game: black screen with the ending text and credits */
	void ShowEnding(const FText& Text);

	/** Small "saved" note in the corner */
	void ShowSaveIndicator();

	virtual void Tick(float DeltaSeconds) override;

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
	TSharedRef<SWidget> BuildMenu();

	FText GetReaderTitle() const;
	FText GetReaderBody() const;
	FText GetReaderPage() const;
	FText GetRadioFrequency() const;
	FText GetRadioSignal() const;
	FText GetRadioChoices() const;
	FText GetInventoryItems() const;
	FText GetInventoryNotes() const;
	FText UIText(const TCHAR* Key) const;

	/** Subtitle size setting as a font scale */
	float GetSubtitleScale() const;
	bool ShouldShowObjective() const;

	TSharedPtr<SWidget> Root;

	/** Solid white brush, tinted per widget */
	TUniquePtr<FSlateColorBrush> WhiteBrush;

	FText ToastText;
	float ToastStartTime = -100.0f;

	FText SubtitleSpeaker;
	FText SubtitleLine;

	FText TimedSpeaker;
	FText TimedLine;
	float TimedUntil = -1.0f;

	FText TitleText;
	float TitleStart = -100.0f;
	float TitleDuration = 0.0f;

	// starts black; the title screen or the first night fades in
	float FadeOpacity = 1.0f;
	float FadeTarget = 1.0f;
	float FadeSpeed = 1.0f;

	FText EndingText;
	bool bEnding = false;

	float SaveIndicatorTime = -100.0f;

	/** When the objective last changed, for the highlight on a new one */
	FString LastObjective;
	float ObjectiveChangedTime = -100.0f;

	TSharedRef<SWidget> BuildStoryOverlay();
	float GetTitleOpacity() const;

	/** The keeper's watch, faded with the watch subsystem and hidden over menus and documents */
	float GetWatchAlpha() const;
};
