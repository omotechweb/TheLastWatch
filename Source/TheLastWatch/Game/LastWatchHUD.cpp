// The Last Watch

#include "LastWatchHUD.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "GameDataSubsystem.h"
#include "Interactables.h"
#include "InventoryComponent.h"
#include "LastWatchPlayerController.h"

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
	return SNew(SBox)
		.MaxDesiredWidth(1100.0f)
		.Visibility_Lambda([this] { return SubtitleLine.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(HUDStyle::Font(14, "Bold"))
				.ColorAndOpacity(FSlateColor(HUDStyle::Amber))
				.ShadowOffset(FVector2D(1, 1))
				.Text_Lambda([this] { return SubtitleSpeaker; })
				.Visibility_Lambda([this] { return SubtitleSpeaker.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
			[
				SNew(STextBlock)
				.Font(HUDStyle::Font(22))
				.Justification(ETextJustify::Center)
				.WrapTextAt(1100.0f)
				.ShadowOffset(FVector2D(2, 2))
				.ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f))
				.Text_Lambda([this] { return SubtitleLine; })
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
