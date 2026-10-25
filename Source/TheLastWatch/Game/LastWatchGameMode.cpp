// The Last Watch

#include "LastWatchGameMode.h"
#include "UObject/ConstructorHelpers.h"
#include "LastWatchPlayerController.h"
#include "LastWatchHUD.h"

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
