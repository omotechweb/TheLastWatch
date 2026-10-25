// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactables.generated.h"

class ALastWatchPlayerController;
class UStaticMeshComponent;
struct FRadioTransmission;

/**
 *  Base class for everything the keeper can look at and press E on.
 *  Uses a basic engine shape tinted with Color until real art replaces the mesh.
 */
UCLASS(abstract)
class THELASTWATCH_API AInteractableActor : public AActor
{
	GENERATED_BODY()

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Tint applied to the placeholder shape material */
	UPROPERTY(EditAnywhere, Category="Interaction")
	FLinearColor Color = FLinearColor(0.5f, 0.5f, 0.5f);

	/** Off once a real mesh with its own materials replaces the placeholder cube */
	UPROPERTY(EditAnywhere, Category="Interaction")
	bool bTintPlaceholder = true;

public:

	AInteractableActor();

	/** Text shown under the crosshair while looking at this actor */
	virtual FText GetPrompt(const ALastWatchPlayerController* Keeper) const { return FText::GetEmpty(); }

	/** Whether the keeper can currently interact */
	virtual bool CanInteract(const ALastWatchPlayerController* Keeper) const { return true; }

	/** Performs the interaction */
	virtual void Interact(ALastWatchPlayerController* Keeper) {}

	/** Picked up for good; the story leaves it hidden on later nights */
	bool IsCollected() const { return bCollected; }

protected:

	virtual void BeginPlay() override;

	bool bCollected = false;
};

/** A journal page or document. Reading it adds it to the keeper's documents. */
UCLASS()
class THELASTWATCH_API ANotePickup : public AInteractableActor
{
	GENERATED_BODY()

protected:

	/** Id in Content/Data/Notes.json */
	UPROPERTY(EditAnywhere, Category="Interaction")
	FName NoteId;

public:

	ANotePickup();

	virtual FText GetPrompt(const ALastWatchPlayerController* Keeper) const override;
	virtual void Interact(ALastWatchPlayerController* Keeper) override;
};

/** An item that goes into the inventory (keys, cassettes...) */
UCLASS()
class THELASTWATCH_API AItemPickup : public AInteractableActor
{
	GENERATED_BODY()

protected:

	/** Id in Content/Data/Items.json */
	UPROPERTY(EditAnywhere, Category="Interaction")
	FName ItemId;

public:

	AItemPickup();

	virtual FText GetPrompt(const ALastWatchPlayerController* Keeper) const override;
	virtual void Interact(ALastWatchPlayerController* Keeper) override;
};

/** A hinged door, optionally locked behind an inventory item */
UCLASS()
class THELASTWATCH_API AKeeperDoor : public AInteractableActor
{
	GENERATED_BODY()

protected:

	/** Pivot the door panel swings around; the mesh hangs off it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> Hinge;

	/** Item needed to unlock. None = never locked. */
	UPROPERTY(EditAnywhere, Category="Door")
	FName RequiredItemId;

	/** Yaw the door swings to when open */
	UPROPERTY(EditAnywhere, Category="Door")
	float OpenYaw = 100.0f;

	/** Seconds to fully open or close */
	UPROPERTY(EditAnywhere, Category="Door")
	float SwingTime = 0.9f;

	bool bUnlocked = false;
	bool bOpen = false;
	float SwingAlpha = 0.0f;

public:

	AKeeperDoor();

	virtual void Tick(float DeltaSeconds) override;

	virtual FText GetPrompt(const ALastWatchPlayerController* Keeper) const override;
	virtual void Interact(ALastWatchPlayerController* Keeper) override;

protected:

	virtual void BeginPlay() override;
	bool IsLocked() const { return !bUnlocked && !RequiredItemId.IsNone(); }
};

/**
 *  The shortwave set. Using it puts the keeper in tuning mode.
 *  Transmissions from Content/Data/Radio.json play as subtitles while tuned in.
 */
UCLASS()
class THELASTWATCH_API ARadioSet : public AInteractableActor
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere, Category="Radio", meta=(Units="kHz"))
	float MinFrequency = 1600.0f;

	UPROPERTY(EditAnywhere, Category="Radio", meta=(Units="kHz"))
	float MaxFrequency = 2800.0f;

	UPROPERTY(EditAnywhere, Category="Radio", meta=(Units="kHz"))
	float Frequency = 2000.0f;

	/** How far off a station's frequency it can still be heard */
	UPROPERTY(EditAnywhere, Category="Radio", meta=(Units="kHz"))
	float Bandwidth = 12.0f;

	/** Signal strength needed for speech to come through */
	UPROPERTY(EditAnywhere, Category="Radio", meta=(ClampMin=0, ClampMax=1))
	float ClearSignal = 0.6f;

	struct FPlayback
	{
		int32 Line = 0;
		float LineTime = 0.0f;
		bool bFinished = false;
		bool bInGap = false;
	};

	/** Playback progress per transmission id, so a call resumes where it was left */
	TMap<FName, FPlayback> Playback;

	/** Keeper currently sitting at the radio */
	TWeakObjectPtr<ALastWatchPlayerController> User;

public:

	ARadioSet();

	virtual void Tick(float DeltaSeconds) override;

	virtual FText GetPrompt(const ALastWatchPlayerController* Keeper) const override;
	virtual void Interact(ALastWatchPlayerController* Keeper) override;

	/** Moves the dial by Delta kHz */
	void Tune(float Delta);

	/** Called by the keeper when sitting down at / leaving the radio (nullptr) */
	void SetUser(ALastWatchPlayerController* Keeper);

	float GetFrequency() const { return Frequency; }

	/** 0..1 strength of the closest on-air transmission */
	float GetSignalStrength() const;

private:

	const FRadioTransmission* FindClosestTransmission(float& OutSignal) const;
};
