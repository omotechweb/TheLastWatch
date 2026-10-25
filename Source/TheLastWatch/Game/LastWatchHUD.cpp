// The Last Watch

#include "LastWatchHUD.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "GameDataSubsystem.h"
#include "Interactables.h"
#include "InventoryComponent.h"
#include "KeeperSaveSubsystem.h"
#include "LastWatchPlayerController.h"
#include "StorySubsystem.h"

namespace HUDStyle
{
	const FLinearColor Paper(0.80f, 0.75f, 0.62f);
	const FLinearColor Ink(0.12f, 0.09f, 0.06f);
	const FLinearColor Dim(0.0f, 0.0f, 0.0f, 0.78f);
	const FLinearColor Amber(1.0f, 0.72f, 0.35f);
	const FLinearColor Soft(0.85f, 0.85f, 0.82f);
	const FLinearColor RadioPanel(0.03f, 0.05f, 0.04f, 0.88f);

	// paper is 720 wide with 64 padding on each side; fixed wrapping avoids
	// a one-frame layout lag when the text changes
	const float PageTextWidth = 720.0f - 2 * 64.0f;

	FSlateFontInfo Font(int32 Size, const char* Weight = "Regular")
	{
		return FCoreStyle::GetDefaultFontStyle(Weight, Size);
	}
}

ALastWatchHUD::ALastWatchHUD()
{
	WhiteBrush = MakeUnique<FSlateColorBrush>(FLinearColor::White);
}

ALastWatchPlayerController* ALastWatchHUD::GetKeeper() const
{
	return Cast<ALastWatchPlayerController>(PlayerOwner);
}

FText ALastWatchHUD::UIText(const TCHAR* Key) const
{
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	return Data ? Data->GetUIText(Key) : FText::GetEmpty();
}

void ALastWatchHUD::ShowToast(const FText& Text)
{
	ToastText = Text;
	ToastStartTime = GetWorld()->GetRealTimeSeconds();
}

void ALastWatchHUD::SetSubtitle(const FText& Speaker, const FText& Line)
{
	SubtitleSpeaker = Speaker;
	SubtitleLine = Line;
}

void ALastWatchHUD::ShowTimedSubtitle(const FText& Speaker, const FText& Line, float Duration)
{
	TimedSpeaker = Speaker;
	TimedLine = Line;
	TimedUntil = GetWorld()->GetRealTimeSeconds() + Duration;
}

void ALastWatchHUD::ShowTitleCard(const FText& Text, float Duration)
{
	TitleText = Text;
	TitleStart = GetWorld()->GetRealTimeSeconds();
	TitleDuration = Duration;
}

void ALastWatchHUD::FadeTo(float TargetOpacity, float Seconds)
{
	FadeTarget = FMath::Clamp(TargetOpacity, 0.0f, 1.0f);
	FadeSpeed = Seconds > 0.0f ? 1.0f / Seconds : 1000.0f;
}

void ALastWatchHUD::SnapFade(float Opacity)
{
	FadeOpacity = FadeTarget = FMath::Clamp(Opacity, 0.0f, 1.0f);
}

void ALastWatchHUD::ShowSaveIndicator()
{
	SaveIndicatorTime = GetWorld()->GetRealTimeSeconds();
}

float ALastWatchHUD::GetSubtitleScale() const
{
	const UKeeperSaveSubsystem* Save = UKeeperSaveSubsystem::Get(this);
	const int32 Size = Save ? Save->GetSettings()->SubtitleSize : 1;
	return Size == 0 ? 0.8f : Size == 2 ? 1.3f : 1.0f;
}

bool ALastWatchHUD::ShouldShowObjective() const
{
	const UKeeperSaveSubsystem* Save = UKeeperSaveSubsystem::Get(this);
	return !Save || Save->GetSettings()->bShowObjectives;
}

void ALastWatchHUD::ShowEnding(const FText& Text)
{
	EndingText = Text;
	bEnding = true;
	FadeTo(1.0f, 2.0f);
}

void ALastWatchHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FadeOpacity = FMath::FInterpConstantTo(FadeOpacity, FadeTarget, DeltaSeconds, FadeSpeed);

	// remember when a new objective appears, to highlight it for a moment
	if (const UStorySubsystem* Story = UStorySubsystem::Get(this))
	{
		const FString Objective = FString::Printf(TEXT("%d:%s"), Story->GetNight(), *Story->GetObjectiveId());
		if (Objective != LastObjective)
		{
			LastObjective = Objective;
			ObjectiveChangedTime = GetWorld()->GetRealTimeSeconds();
		}
	}
}

float ALastWatchHUD::GetTitleOpacity() const
{
	// 1 s fade in, hold, 1.5 s fade out
	const float Age = GetWorld()->GetRealTimeSeconds() - TitleStart;
	if (Age < 0.0f || Age > TitleDuration)
	{
		return 0.0f;
	}
	return FMath::Min3(1.0f, Age, (TitleDuration - Age) / 1.5f);
}

TSharedRef<SWidget> ALastWatchHUD::BuildStoryOverlay()
{
	auto Keeper = [this] { return GetKeeper(); };

	return SNew(SOverlay)

		// current objective, top right
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0, 40, 48, 0)
		[
			SNew(SBox)
			.MaxDesiredWidth(420.0f)
			.Visibility_Lambda([this]
			{
				const UStorySubsystem* Story = UStorySubsystem::Get(this);
				const ALastWatchPlayerController* K = GetKeeper();
				const bool bShow = Story && !Story->GetObjectiveText().IsEmpty() && K && K->GetMode() == EKeeperMode::Explore && !bEnding && ShouldShowObjective();
				return bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Font(HUDStyle::Font(12, "Bold"))
					.ColorAndOpacity(FSlateColor(HUDStyle::Amber.CopyWithNewOpacity(0.8f)))
					.ShadowOffset(FVector2D(1, 1))
					.Text_Lambda([this]
					{
						const UStorySubsystem* Story = UStorySubsystem::Get(this);
						const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
						return Story && Data ? Data->GetUIText(TEXT("hud.night"), FText::AsNumber(Story->GetNight())) : FText::GetEmpty();
					})
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 4, 0, 0)
				[
					SNew(STextBlock)
					.Font(HUDStyle::Font(16))
					.ColorAndOpacity_Lambda([this]
					{
						// a new objective glows amber, then settles
						const float Age = GetWorld()->GetRealTimeSeconds() - ObjectiveChangedTime;
						return FSlateColor(FMath::Lerp(HUDStyle::Amber, HUDStyle::Soft, FMath::Clamp((Age - 1.5f) / 2.0f, 0.0f, 1.0f)));
					})
					.ShadowOffset(FVector2D(1, 1))
					.Justification(ETextJustify::Right)
					.WrapTextAt(420.0f)
					.Text_Lambda([this]
					{
						const UStorySubsystem* Story = UStorySubsystem::Get(this);
						return Story ? Story->GetObjectiveText() : FText::GetEmpty();
					})
				]
			]
		]

		// hold-to-use progress, just under the crosshair
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0, 150, 0, 0)
		[
			SNew(SBox)
			.WidthOverride(220.0f)
			.HeightOverride(6.0f)
			.Visibility_Lambda([Keeper] { return Keeper() && Keeper()->GetHoldProgress() > 0.0f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder).BorderImage(WhiteBrush.Get()).BorderBackgroundColor(FLinearColor(1, 1, 1, 0.15f))
				]
				+ SOverlay::Slot().HAlign(HAlign_Left)
				[
					SNew(SBox)
					.WidthOverride_Lambda([Keeper] { return FOptionalSize(220.0f * (Keeper() ? Keeper()->GetHoldProgress() : 0.0f)); })
					[
						SNew(SBorder).BorderImage(WhiteBrush.Get()).BorderBackgroundColor(HUDStyle::Amber)
					]
				]
			]
		]

		// fade to black
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush.Get())
			.BorderBackgroundColor_Lambda([this] { return FSlateColor(FLinearColor(0, 0, 0, FadeOpacity)); })
			.Visibility_Lambda([this] { return FadeOpacity > 0.001f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		]

		// night title card
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(HUDStyle::Font(42, "Bold"))
			.ShadowOffset(FVector2D(2, 2))
			.Text_Lambda([this] { return TitleText; })
			.ColorAndOpacity_Lambda([this] { return FSlateColor(HUDStyle::Soft.CopyWithNewOpacity(GetTitleOpacity())); })
			.Visibility_Lambda([this] { return GetTitleOpacity() > 0.0f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		]

		// ending text
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(HUDStyle::Font(20))
			.Justification(ETextJustify::Center)
			.WrapTextAt(1150.0f)
			.LineHeightPercentage(1.35f)
			.ColorAndOpacity_Lambda([this] { return FSlateColor(HUDStyle::Soft.CopyWithNewOpacity(FMath::Clamp(FadeOpacity * 1.2f - 0.2f, 0.0f, 1.0f))); })
			.Text_Lambda([this] { return EndingText; })
			.Visibility_Lambda([this] { return bEnding ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		]

		// once the ending has been read, offer the way back
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 60)
		[
			SNew(STextBlock)
			.Font(HUDStyle::Font(14))
			.Text_Lambda([this] { return UIText(TEXT("ending.continue")); })
			.ColorAndOpacity_Lambda([this, Keeper]
			{
				const ALastWatchPlayerController* K = Keeper();
				const float Age = K ? GetWorld()->GetRealTimeSeconds() - K->GetEndingStart() : 0.0f;
				return FSlateColor(HUDStyle::Soft.CopyWithNewOpacity(0.7f * FMath::Clamp(Age - 8.0f, 0.0f, 1.0f)));
			})
			.Visibility_Lambda([Keeper] { return Keeper() && Keeper()->IsInMenu(EMenuPage::Ending) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		]

		// autosave note, bottom right
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0, 0, 48, 40)
		[
			SNew(STextBlock)
			.Font(HUDStyle::Font(13, "Bold"))
			.Text_Lambda([this] { return UIText(TEXT("hud.saved")); })
			.ColorAndOpacity_Lambda([this]
			{
				// in, hold, out over four seconds
				const float Age = GetWorld()->GetRealTimeSeconds() - SaveIndicatorTime;
				const float Alpha = FMath::Clamp(FMath::Min(Age, 4.0f - Age), 0.0f, 1.0f);
				return FSlateColor(HUDStyle::Amber.CopyWithNewOpacity(0.75f * Alpha));
			})
		];
}

TSharedRef<SWidget> ALastWatchHUD::BuildMenu()
{
	auto Keeper = [this] { return GetKeeper(); };
	auto Page = [this]
	{
		const ALastWatchPlayerController* K = GetKeeper();
		return K ? K->GetMenuPage() : EMenuPage::Ending;
	};

	static const FButtonStyle PlainButton = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateNoResource())
		.SetPressed(FSlateNoResource())
		.SetNormalPadding(FMargin(0))
		.SetPressedPadding(FMargin(0));

	// fixed rows; each shows entry Index of the open page, or collapses
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (int32 Index = 0; Index < 9; ++Index)
	{
		auto Entry = [this, Index]
		{
			const ALastWatchPlayerController* K = GetKeeper();
			const TArray<FMenuEntry> Entries = K ? K->GetMenuEntries() : TArray<FMenuEntry>();
			return Entries.IsValidIndex(Index) ? Entries[Index] : FMenuEntry();
		};
		auto Selected = [this, Index]
		{
			const ALastWatchPlayerController* K = GetKeeper();
			return K && K->GetMenuSelection() == Index;
		};
		auto RowColor = [Entry, Selected]
		{
			const FMenuEntry Current = Entry();
			return FSlateColor(!Current.bEnabled ? HUDStyle::Soft.CopyWithNewOpacity(0.3f) : Selected() ? HUDStyle::Amber : HUDStyle::Soft);
		};

		Rows->AddSlot().AutoHeight().Padding(0, 5)
		[
			SNew(SButton)
			.ButtonStyle(&PlainButton)
			.IsFocusable(false)
			.Visibility_Lambda([Entry] { return Entry().Id.IsNone() ? EVisibility::Collapsed : EVisibility::Visible; })
			.OnHovered_Lambda([Keeper, Index] { if (Keeper()) { Keeper()->SetMenuSelection(Index); } })
			.OnClicked_Lambda([Keeper, Index]
			{
				if (Keeper())
				{
					Keeper()->ActivateMenuEntry(Index, 0);
				}
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)

				// selection mark
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)
				[
					SNew(SBox)
					.WidthOverride(22.0f)
					.HeightOverride(2.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush.Get())
						.BorderBackgroundColor_Lambda([Selected] { return FSlateColor(HUDStyle::Amber.CopyWithNewOpacity(Selected() ? 1.0f : 0.0f)); })
					]
				]

				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(HUDStyle::Font(21))
					.ShadowOffset(FVector2D(1, 1))
					.ColorAndOpacity_Lambda(RowColor)
					.Text_Lambda([Entry] { return Entry().Label; })
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(24, 0, 0, 0)
				[
					SNew(STextBlock)
					.Font(HUDStyle::Font(18))
					.ShadowOffset(FVector2D(1, 1))
					.ColorAndOpacity_Lambda(RowColor)
					.Text_Lambda([Entry, Selected]
					{
						// settings show arrows on the selected line
						const FMenuEntry Current = Entry();
						const bool bSetting = !Current.Value.IsEmpty() && Current.bEnabled && Current.Id.GetPlainNameString() != TEXT("night") && Current.Id != TEXT("continue");
						return bSetting && Selected() ? FText::Format(INVTEXT("<  {0}  >"), Current.Value) : Current.Value;
					})
				]
			]
		];
	}

	auto HeadingKey = [Page]() -> const TCHAR*
	{
		switch (Page())
		{
		case EMenuPage::Title:    return TEXT("game.title");
		case EMenuPage::Pause:    return TEXT("menu.paused");
		case EMenuPage::Settings: return TEXT("menu.settings");
		case EMenuPage::Chapters: return TEXT("menu.chapters");
		default:                  return TEXT("");
		}
	};

	return SNew(SOverlay)
		.Visibility_Lambda([Keeper, Page]
		{
			return Keeper() && Keeper()->GetMode() == EKeeperMode::Menu && Page() != EMenuPage::Ending ? EVisibility::Visible : EVisibility::Collapsed;
		})

		// darken the game behind: a band on the left over the title shot, all of it when paused
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush.Get())
			.BorderBackgroundColor_Lambda([this] { return FSlateColor(FLinearColor(0, 0, 0, GetWorld()->IsPaused() ? 0.7f : 0.0f)); })
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.WidthOverride(760.0f)
			.Visibility_Lambda([this] { return GetWorld()->IsPaused() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush.Get())
				.BorderBackgroundColor(FLinearColor(0.0f, 0.01f, 0.02f, 0.62f))
			]
		]

		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(110, 0, 0, 0)
		[
			SNew(SBox)
			.MinDesiredWidth(560.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Font_Lambda([Page] { return HUDStyle::Font(Page() == EMenuPage::Title ? 50 : 34, "Bold"); })
					.ColorAndOpacity(FSlateColor(HUDStyle::Soft))
					.ShadowOffset(FVector2D(2, 2))
					.Text_Lambda([this, HeadingKey] { return UIText(HeadingKey()); })
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(4, 6, 0, 0)
				[
					SNew(STextBlock)
					.Font(HUDStyle::Font(16))
					.ColorAndOpacity(FSlateColor(HUDStyle::Amber.CopyWithNewOpacity(0.85f)))
					.Text_Lambda([this] { return UIText(TEXT("game.subtitle")); })
					.Visibility_Lambda([Page] { return Page() == EMenuPage::Title ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 52, 0, 0)
				[
					Rows
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(4, 40, 0, 0)
				[
					SNew(STextBlock)
					.Font(HUDStyle::Font(14))
					.ColorAndOpacity(FSlateColor(HUDStyle::Soft.CopyWithNewOpacity(0.55f)))
					.Text_Lambda([this]
					{
						const UKeeperSaveSubsystem* Save = UKeeperSaveSubsystem::Get(this);
						const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
						return Save && Data ? Data->GetUIText(TEXT("menu.endings"), FText::AsNumber(Save->GetEndingCount())) : FText::GetEmpty();
					})
					.Visibility_Lambda([this, Page]
					{
						const UKeeperSaveSubsystem* Save = UKeeperSaveSubsystem::Get(this);
						return Page() == EMenuPage::Title && Save && Save->GetEndingCount() > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
					})
				]
			]
		]

		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(114, 0, 0, 48)
		[
			SNew(STextBlock)
			.Font(HUDStyle::Font(13))
			.ColorAndOpacity(FSlateColor(HUDStyle::Soft.CopyWithNewOpacity(0.6f)))
			.Text_Lambda([this] { return UIText(TEXT("menu.hint")); })
		];
}

/////////////////////////////////////////////////////////////////////////////
// Build

void ALastWatchHUD::BeginPlay()
{
	Super::BeginPlay();

	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!Viewport)
	{
		return;
	}

	Root = SNew(SOverlay)

		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			BuildCrosshair()
		]

		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 110)
		[
			BuildSubtitles()
		]

		// pickup toast
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(48, 40)
		[
			SNew(STextBlock)
			.Font(HUDStyle::Font(18))
			.ShadowOffset(FVector2D(1, 1))
			.Text_Lambda([this] { return ToastText; })
			.ColorAndOpacity_Lambda([this]
			{
				// hold, then fade out over the last second
				const float Age = GetWorld()->GetRealTimeSeconds() - ToastStartTime;
				const float Alpha = FMath::Clamp(ToastDuration - Age, 0.0f, 1.0f);
				return FSlateColor(FLinearColor(1, 1, 1, Alpha));
			})
		]

		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			BuildReader()
		]

		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 220)
		[
			BuildRadio()
		]

		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			BuildInventory()
		]

		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			BuildMenu()
		]

		// objective, fades, title cards and the ending sit above everything else
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			BuildStoryOverlay()
		];

	Viewport->AddViewportWidgetContent(Root.ToSharedRef(), 10);
}

void ALastWatchHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Root.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(Root.ToSharedRef());
		}
		Root.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

TSharedRef<SWidget> ALastWatchHUD::BuildCrosshair()
{
	auto IsExploring = [this]
	{
		const ALastWatchPlayerController* Keeper = GetKeeper();
		return Keeper && Keeper->GetMode() == EKeeperMode::Explore ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
	};

	auto HasFocus = [this]
	{
		const ALastWatchPlayerController* Keeper = GetKeeper();
		return Keeper && Keeper->GetFocused() != nullptr;
	};

	return SNew(SOverlay)
		.Visibility_Lambda(IsExploring)

		// dot grows a little when something can be used
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride_Lambda([HasFocus] { return FOptionalSize(HasFocus() ? 8.0f : 4.0f); })
			.HeightOverride_Lambda([HasFocus] { return FOptionalSize(HasFocus() ? 8.0f : 4.0f); })
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush.Get())
				.BorderBackgroundColor_Lambda([HasFocus]
				{
					return FSlateColor(FLinearColor(1, 1, 1, HasFocus() ? 0.9f : 0.45f));
				})
			]
		]

		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0, 90, 0, 0)
		[
			SNew(STextBlock)
			.Font(HUDStyle::Font(16))
			.ShadowOffset(FVector2D(1, 1))
			.ColorAndOpacity(FSlateColor(HUDStyle::Soft))
			.Text_Lambda([this]
			{
				const ALastWatchPlayerController* Keeper = GetKeeper();
				const AInteractableActor* Target = Keeper ? Keeper->GetFocused() : nullptr;
				return Target ? Target->GetPrompt(Keeper) : FText::GetEmpty();
			})
		];
}

TSharedRef<SWidget> ALastWatchHUD::BuildSubtitles()
{
	// the radio's live subtitle wins over a timed story line
	auto ShowingRadio = [this] { return !SubtitleLine.IsEmpty(); };
	auto Line = [this, ShowingRadio]
	{
		if (ShowingRadio()) { return SubtitleLine; }
		return GetWorld()->GetRealTimeSeconds() < TimedUntil ? TimedLine : FText::GetEmpty();
	};
	auto Speaker = [this, ShowingRadio] { return ShowingRadio() ? SubtitleSpeaker : TimedSpeaker; };

	return SNew(SBox)
		.MaxDesiredWidth(1100.0f)
		.Visibility_Lambda([Line] { return Line().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
		[
			// soft dark backing keeps lines readable against the lamp and the fog
			SNew(SBorder)
			.BorderImage(WhiteBrush.Get())
			.BorderBackgroundColor(FLinearColor(0, 0, 0, 0.45f))
			.Padding(FMargin(22, 10))
			[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font_Lambda([this] { return HUDStyle::Font(FMath::RoundToInt(14 * GetSubtitleScale()), "Bold"); })
				.ColorAndOpacity(FSlateColor(HUDStyle::Amber))
				.ShadowOffset(FVector2D(1, 1))
				.Text_Lambda([Speaker] { return Speaker(); })
				.Visibility_Lambda([Speaker] { return Speaker().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
			[
				SNew(STextBlock)
				.Font_Lambda([this] { return HUDStyle::Font(FMath::RoundToInt(22 * GetSubtitleScale())); })
				.Justification(ETextJustify::Center)
				.WrapTextAt(1100.0f)
				.ShadowOffset(FVector2D(2, 2))
				.ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f))
				.Text_Lambda([Line] { return Line(); })
			]
			]
		];
}

TSharedRef<SWidget> ALastWatchHUD::BuildReader()
{
	return SNew(SBorder)
		.BorderImage(WhiteBrush.Get())
		.BorderBackgroundColor(HUDStyle::Dim)
		.Visibility_Lambda([this]
		{
			const ALastWatchPlayerController* Keeper = GetKeeper();
			return Keeper && Keeper->GetMode() == EKeeperMode::Reading ? EVisibility::Visible : EVisibility::Collapsed;
		})
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox)
				.WidthOverride(720.0f)
				.MinDesiredHeight(820.0f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush.Get())
					.BorderBackgroundColor(HUDStyle::Paper)
					.Padding(FMargin(64, 56))
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Font(HUDStyle::Font(24, "Bold"))
							.ColorAndOpacity(FSlateColor(HUDStyle::Ink))
							.WrapTextAt(HUDStyle::PageTextWidth)
							.Text_Lambda([this] { return GetReaderTitle(); })
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(0, 28, 0, 0)
						[
							SNew(STextBlock)
							.Font(HUDStyle::Font(19))
							.ColorAndOpacity(FSlateColor(HUDStyle::Ink))
							.WrapTextAt(HUDStyle::PageTextWidth)
							.LineHeightPercentage(1.15f)
							.Text_Lambda([this] { return GetReaderBody(); })
						]

						+ SVerticalBox::Slot().FillHeight(1.0f)
						[
							SNullWidget::NullWidget
						]

						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Font(HUDStyle::Font(14))
							.ColorAndOpacity(FSlateColor(HUDStyle::Ink.CopyWithNewOpacity(0.6f)))
							.Text_Lambda([this] { return GetReaderPage(); })
						]
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 18, 0, 0)
			[
				SNew(STextBlock)
				.Font(HUDStyle::Font(14))
				.ColorAndOpacity(FSlateColor(HUDStyle::Soft))
				.Text_Lambda([this] { return UIText(TEXT("reader.hint")); })
			]
		];
}

TSharedRef<SWidget> ALastWatchHUD::BuildRadio()
{
	return SNew(SBorder)
		.BorderImage(WhiteBrush.Get())
		.BorderBackgroundColor(HUDStyle::RadioPanel)
		.Padding(FMargin(36, 20))
		.Visibility_Lambda([this]
		{
			const ALastWatchPlayerController* Keeper = GetKeeper();
			return Keeper && Keeper->GetMode() == EKeeperMode::Radio ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(HUDStyle::Font(13, "Bold"))
				.ColorAndOpacity(FSlateColor(HUDStyle::Amber.CopyWithNewOpacity(0.7f)))
				.Text_Lambda([this] { return UIText(TEXT("radio.title")); })
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 6, 0, 0)
			[
				SNew(STextBlock)
				.Font(HUDStyle::Font(40, "Bold"))
				.ColorAndOpacity(FSlateColor(HUDStyle::Amber))
				.Text_Lambda([this] { return GetRadioFrequency(); })
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
			[
				SNew(STextBlock)
				.Font(HUDStyle::Font(16))
				.ColorAndOpacity(FSlateColor(HUDStyle::Amber.CopyWithNewOpacity(0.85f)))
				.Text_Lambda([this] { return GetRadioSignal(); })
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 12, 0, 0)
			[
				SNew(STextBlock)
				.Font(HUDStyle::Font(13))
				.ColorAndOpacity(FSlateColor(HUDStyle::Soft))
				.Text_Lambda([this] { return UIText(TEXT("radio.hint")); })
			]
		];
}

TSharedRef<SWidget> ALastWatchHUD::BuildInventory()
{
	auto Heading = [this](const TCHAR* Key)
	{
		return SNew(STextBlock)
			.Font(HUDStyle::Font(14, "Bold"))
			.ColorAndOpacity(FSlateColor(HUDStyle::Amber))
			.Text_Lambda([this, Key] { return UIText(Key); });
	};

	return SNew(SBorder)
		.BorderImage(WhiteBrush.Get())
		.BorderBackgroundColor(HUDStyle::Dim)
		.Padding(FMargin(120, 100))
		.Visibility_Lambda([this]
		{
			const ALastWatchPlayerController* Keeper = GetKeeper();
			return Keeper && Keeper->GetMode() == EKeeperMode::Inventory ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Font(HUDStyle::Font(30, "Bold"))
				.ColorAndOpacity(FSlateColor(HUDStyle::Soft))
				.Text_Lambda([this] { return UIText(TEXT("inventory.title")); })
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 36, 0, 8)
			[
				Heading(TEXT("inventory.items"))
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Font(HUDStyle::Font(18))
				.ColorAndOpacity(FSlateColor(HUDStyle::Soft))
				.LineHeightPercentage(1.3f)
				.Text_Lambda([this] { return GetInventoryItems(); })
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 36, 0, 8)
			[
				Heading(TEXT("inventory.docs"))
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Font(HUDStyle::Font(18))
				.ColorAndOpacity(FSlateColor(HUDStyle::Soft))
				.LineHeightPercentage(1.3f)
				.Text_Lambda([this] { return GetInventoryNotes(); })
			]

			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNullWidget::NullWidget
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Font(HUDStyle::Font(14))
				.ColorAndOpacity(FSlateColor(HUDStyle::Soft.CopyWithNewOpacity(0.7f)))
				.Text_Lambda([this] { return UIText(TEXT("inventory.hint")); })
			]
		];
}

/////////////////////////////////////////////////////////////////////////////
// Content

FText ALastWatchHUD::GetReaderTitle() const
{
	const ALastWatchPlayerController* Keeper = GetKeeper();
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	const FNoteData* Note = Keeper && Data ? Data->FindNote(Keeper->GetOpenNoteId()) : nullptr;
	return Note ? Note->Title.Get() : FText::GetEmpty();
}

FText ALastWatchHUD::GetReaderBody() const
{
	const ALastWatchPlayerController* Keeper = GetKeeper();
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	const FNoteData* Note = Keeper && Data ? Data->FindNote(Keeper->GetOpenNoteId()) : nullptr;
	if (!Note || !Note->Pages.IsValidIndex(Keeper->GetNotePage()))
	{
		return FText::GetEmpty();
	}
	return Note->Pages[Keeper->GetNotePage()].Get();
}

FText ALastWatchHUD::GetReaderPage() const
{
	const ALastWatchPlayerController* Keeper = GetKeeper();
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	const FNoteData* Note = Keeper && Data ? Data->FindNote(Keeper->GetOpenNoteId()) : nullptr;
	if (!Note || Note->Pages.Num() < 2)
	{
		return FText::GetEmpty();
	}
	const FText PageNumber = FText::Format(INVTEXT("{0} / {1}"), Keeper->GetNotePage() + 1, Note->Pages.Num());
	return Data->GetUIText(TEXT("reader.page"), PageNumber);
}

FText ALastWatchHUD::GetRadioFrequency() const
{
	const ALastWatchPlayerController* Keeper = GetKeeper();
	const ARadioSet* Radio = Keeper ? Keeper->GetActiveRadio() : nullptr;
	if (!Radio)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("%04d kHz"), FMath::RoundToInt(Radio->GetFrequency())));
}

FText ALastWatchHUD::GetRadioSignal() const
{
	const ALastWatchPlayerController* Keeper = GetKeeper();
	const ARadioSet* Radio = Keeper ? Keeper->GetActiveRadio() : nullptr;
	if (!Radio)
	{
		return FText::GetEmpty();
	}

	const float Signal = Radio->GetSignalStrength();
	if (Signal <= 0.0f)
	{
		return UIText(TEXT("radio.static"));
	}

	// ten-segment meter
	const int32 Bars = FMath::Clamp(FMath::RoundToInt(Signal * 10.0f), 0, 10);
	FString Meter;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Meter += Index < Bars ? TEXT("|") : TEXT(".");
	}
	return FText::FromString(Meter);
}

FText ALastWatchHUD::GetInventoryItems() const
{
	const ALastWatchPlayerController* Keeper = GetKeeper();
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	if (!Keeper || !Data || Keeper->GetInventory()->GetItems().Num() == 0)
	{
		return UIText(TEXT("inventory.empty"));
	}

	TArray<FString> Lines;
	for (const FName ItemId : Keeper->GetInventory()->GetItems())
	{
		const FItemData* Item = Data->FindItem(ItemId);
		Lines.Add(Item
			? FString::Printf(TEXT("%s  —  %s"), *Item->Name.Get().ToString(), *Item->Description.Get().ToString())
			: ItemId.ToString());
	}
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

FText ALastWatchHUD::GetInventoryNotes() const
{
	const ALastWatchPlayerController* Keeper = GetKeeper();
	const UGameDataSubsystem* Data = UGameDataSubsystem::Get(this);
	if (!Keeper || !Data || Keeper->GetInventory()->GetNotes().Num() == 0)
	{
		return UIText(TEXT("inventory.empty"));
	}

	TArray<FString> Lines;
	const TArray<FName>& Notes = Keeper->GetInventory()->GetNotes();
	for (int32 Index = 0; Index < Notes.Num(); ++Index)
	{
		const FNoteData* Note = Data->FindNote(Notes[Index]);
		const FString Title = Note ? Note->Title.Get().ToString() : Notes[Index].ToString();
		Lines.Add((Index == Keeper->GetInventorySelection() ? TEXT(">  ") : TEXT("    ")) + Title);
	}
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}
