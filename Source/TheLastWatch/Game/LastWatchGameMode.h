// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LastWatchGameMode.generated.h"

/**
 *  Game mode for The Last Watch.
 *  Uses the template horror character (flashlight + sprint) with our keeper controller and HUD.
 */
UCLASS()
class THELASTWATCH_API ALastWatchGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:

	/** Night 1..5. Decides which radio transmissions and events are active. */
	UPROPERTY(EditAnywhere, Category="Story", meta=(ClampMin=1, ClampMax=5))
	int32 CurrentNight = 1;

public:

	ALastWatchGameMode();

	/** Starts the story once the keeper exists */
	virtual void StartPlay() override;

	int32 GetCurrentNight() const { return CurrentNight; }
	void SetCurrentNight(int32 Night) { CurrentNight = FMath::Clamp(Night, 1, 5); }
};
