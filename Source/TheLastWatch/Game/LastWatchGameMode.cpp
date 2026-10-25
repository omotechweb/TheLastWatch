// The Last Watch

#include "LastWatchGameMode.h"
#include "UObject/ConstructorHelpers.h"
#include "LastWatchPlayerController.h"
#include "LastWatchHUD.h"
#include "StorySubsystem.h"
#include "TimerManager.h"

ALastWatchGameMode::ALastWatchGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> KeeperPawn(TEXT("/Game/Variant_Horror/Blueprints/BP_HorrorCharacter"));
	if (KeeperPawn.Succeeded())
	{
		DefaultPawnClass = KeeperPawn.Class;
	}

	PlayerControllerClass = ALastWatchPlayerController::StaticClass();
	HUDClass = ALastWatchHUD::StaticClass();
}

void ALastWatchGameMode::StartPlay()
{
	Super::StartPlay();

	// a frame later, so the HUD and the keeper's pawn are in place for the opening actions
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		UStorySubsystem* Story = UStorySubsystem::Get(this);
		if (Story && !Story->HasStarted())
		{
			Story->StartNight(CurrentNight);
		}
	}), 0.3f, false);
}
