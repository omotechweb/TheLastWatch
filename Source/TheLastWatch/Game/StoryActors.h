// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "Interactables.h"
#include "StoryActors.generated.h"

class UBoxComponent;
class USoundBase;
class UPrimitiveComponent;

/**
 *  Something the keeper does as part of the story: refuel the lamp, wind the clockwork, sleep...
 *  Completing it reports "task:<TaskId>". By default it can only be used while the current
 *  objective is waiting for it, so the night plays in order.
 */
UCLASS()
class THELASTWATCH_API AStoryTask : public AInteractableActor
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere, Category="Story")
	FName TaskId;

	/** Key in UI.json for the prompt, e.g. prompt.refuel */
	UPROPERTY(EditAnywhere, Category="Story")
	FString PromptKey = TEXT("prompt.use");

	/** Inventory item needed; None = nothing */
	UPROPERTY(EditAnywhere, Category="Story")
	FName RequiredItem;

	UPROPERTY(EditAnywhere, Category="Story")
	bool bConsumeItem = true;

	/** > 0: hold E this long instead of a single press */
	UPROPERTY(EditAnywhere, Category="Story", meta=(Units="s"))
	float HoldSeconds = 0.0f;

	/** Only usable while the story is waiting for it */
	UPROPERTY(EditAnywhere, Category="Story")
	bool bRequireObjective = true;

	/** Usable again after completing (otherwise once per night) */
	UPROPERTY(EditAnywhere, Category="Story")
	bool bRepeatable = false;

	/** Played where the task is when it completes */
	UPROPERTY(EditAnywhere, Category="Story")
	TObjectPtr<USoundBase> CompleteSound;

	bool bDoneTonight = false;

public:

	AStoryTask();

	virtual FText GetPrompt(const ALastWatchPlayerController* Keeper) const override;
	virtual bool CanInteract(const ALastWatchPlayerController* Keeper) const override;
	virtual void Interact(ALastWatchPlayerController* Keeper) override;

	float GetHoldSeconds() const { return HoldSeconds; }
	bool HasRequiredItem(const ALastWatchPlayerController* Keeper) const;

	/** Called when a hold finishes, or straight from Interact for single presses */
	void Complete(ALastWatchPlayerController* Keeper);

	void ResetForNight() { bDoneTonight = false; }
};

/** Invisible box that reports "enter:<VolumeId>" when the keeper walks in */
UCLASS()
class THELASTWATCH_API AStoryVolume : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBoxComponent> Box;

protected:

	UPROPERTY(EditAnywhere, Category="Story")
	FName VolumeId;

	/** Half size of the box, in cm */
	UPROPERTY(EditAnywhere, Category="Story")
	FVector Extent = FVector(300.0f, 300.0f, 200.0f);

public:

	AStoryVolume();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};

/**
 *  A hatch, ladder or door to somewhere else (the bunker): fades out, moves the keeper to the
 *  actor tagged TargetTag, fades back in and reports "use:<DoorId>".
 */
UCLASS()
class THELASTWATCH_API ATeleportDoor : public AInteractableActor
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere, Category="Door")
	FName DoorId;

	UPROPERTY(EditAnywhere, Category="Door")
	FName TargetTag;

	UPROPERTY(EditAnywhere, Category="Door")
	FString PromptKey = TEXT("prompt.hatch_down");

	/** Item needed to use it; None = open */
	UPROPERTY(EditAnywhere, Category="Door")
	FName RequiredItem;

	UPROPERTY(EditAnywhere, Category="Door")
	TObjectPtr<USoundBase> UseSound;

	bool bBusy = false;

public:

	ATeleportDoor();

	virtual FText GetPrompt(const ALastWatchPlayerController* Keeper) const override;
	virtual void Interact(ALastWatchPlayerController* Keeper) override;
};

/** Lightning flashes on the moon light and rolling thunder while the storm is on */
UCLASS()
class THELASTWATCH_API AStormController : public AActor
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere, Category="Storm")
	TArray<TObjectPtr<USoundBase>> Thunder;

	UPROPERTY(EditAnywhere, Category="Storm", meta=(Units="s"))
	FVector2D FlashInterval = FVector2D(9.0f, 22.0f);

	/** Moon intensity during a flash (normal is about 0.5 lux) */
	UPROPERTY(EditAnywhere, Category="Storm")
	float FlashIntensity = 12.0f;

	bool bStorm = false;
	float BaseIntensity = 0.5f;
	FTimerHandle FlashTimer;

public:

	AStormController();

	void SetStorm(bool bOn);

private:

	void ScheduleFlash();
	void Flash();
	void SetMoon(float Intensity);
};
