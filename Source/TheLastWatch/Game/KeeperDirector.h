// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeeperDirector.generated.h"

class UInstancedStaticMeshComponent;
class UKeeperSynth;
class ULocalLightComponent;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class USkeletalMeshComponent;

/**
 *  A dark, too-tall figure standing in the fog. It never moves while watched and is gone
 *  a moment after being seen. Spawned and removed by the director.
 */
UCLASS(NotPlaceable)
class THELASTWATCH_API AWatcher : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

public:

	AWatcher();

	/** 1 solid, 0 gone (dithered dissolve) */
	void SetFade(float Fade);

	/** Turns to face a point on the ground */
	void FaceTowards(const FVector& Target);

protected:

	virtual void BeginPlay() override;
};

/**
 *  Runs the island's mood alongside the story:
 *  colour grade and lens effects that sour as dread rises, the procedural soundscape
 *  (UKeeperSynth), rain around the keeper, flickering lights, and the Watcher - a figure
 *  that appears at the edge of sight on later nights and is gone once looked at.
 *
 *  Story actions in Nights.json drive it: dread, watcher, sting, knock, flicker, whisper, panic, crowd.
 *  One is spawned by the game mode.
 */
UCLASS(NotPlaceable)
class THELASTWATCH_API AKeeperDirector : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPostProcessComponent> Grade;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UKeeperSynth> Synth;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UInstancedStaticMeshComponent> RainDrops;

protected:

	/** Rain streaks kept around the camera */
	UPROPERTY(EditAnywhere, Category="Rain")
	int32 RainDropCount = 1200;

	UPROPERTY(EditAnywhere, Category="Rain", meta=(Units="cm"))
	float RainRadius = 1300.0f;

	UPROPERTY(EditAnywhere, Category="Rain", meta=(Units="cm/s"))
	FVector RainVelocity = FVector(-180.0f, 260.0f, -1150.0f);

	/** How far from the keeper the Watcher may stand */
	UPROPERTY(EditAnywhere, Category="Watcher", meta=(Units="cm"))
	FVector2D WatcherDistance = FVector2D(2200.0f, 4600.0f);

	/** Coming closer than this makes it vanish */
	UPROPERTY(EditAnywhere, Category="Watcher", meta=(Units="cm"))
	float WatcherVanishDistance = 1500.0f;

	/** Seconds between appearances for watcher levels 1..3 */
	UPROPERTY(EditAnywhere, Category="Watcher")
	TArray<FVector2D> WatcherIntervals = { FVector2D(75.0f, 130.0f), FVector2D(45.0f, 85.0f), FVector2D(25.0f, 50.0f) };

	/** The keeper is in the VESPER station (built high above the island) above this height */
	UPROPERTY(EditAnywhere, Category="Watcher", meta=(Units="cm"))
	float BunkerHeight = 15000.0f;

public:

	AKeeperDirector();

	static AKeeperDirector* Get(const UObject* WorldContext);

	virtual void Tick(float DeltaSeconds) override;

	/** Clears tonight's effects; the night's script sets them up again */
	void ResetForNight(int32 Night);

	/** 0..1: drone, grade and grain */
	void SetDread(float Level) { DreadTarget = FMath::Clamp(Level, 0.0f, 1.0f); }

	/** 0 = never, 1..3 = ever more often */
	void SetWatcherLevel(int32 Level);

	/** Heart rate and lens shake jump up, then settle */
	void AddPanic(float Amount);
	float GetPanic() const { return Panic; }

	void Sting(float Strength = 1.0f);
	void Knock();
	void Click();
	void Chime(int32 Strikes);
	void Toll(int32 Strikes);
	void SetRatchet(float Level);

	/** How badly the lamp is being kept (UWatchSubsystem): 0 fine, 1 stopped or out. Adds dread, and a
	 *  stopped lamp lets the Watcher out even on nights the story keeps it away */
	void SetLampNeglect(float Level);

	/** Lights near the keeper stutter for a while */
	void Flicker(float Seconds);

	/** The choir breathes for a while */
	void Whisper(float Seconds);

	void SetRain(bool bOn) { bRain = bOn; }

	/** Figures rise out of the sea around the lighthouse (the dark ending); false clears them */
	void SetCrowd(bool bOn);

	/** The radio set's sound: hiss level, voice level, how far off-station (0..1) */
	void SetRadio(float Static, float Voice, float Detune);

	/** Debug: the Watcher straight ahead of the keeper, Distance cm away */
	void SpawnWatcherAhead(float Distance);

	/** True while the keeper has a roof over their head */
	bool IsIndoors() const { return bIndoors; }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	void UpdateGrade(float DeltaSeconds);
	void UpdateRain(float DeltaSeconds, const FVector& Eye);
	void UpdateWatcher(float DeltaSeconds, const FVector& Eye, const FRotator& View);
	void UpdateFlicker(float DeltaSeconds);
	void UpdateCrowd(float DeltaSeconds);

	bool TrySpawnWatcher(const FVector& Eye, const FRotator& View);
	void DismissWatcher(bool bSeen);
	void ScheduleWatcher();
	bool CanSee(const FVector& Eye, const AActor* Target, const FVector& Point) const;

	/** Ground (island or cliff, not a roof) under X/Y; false when there is none */
	bool FindGround(float X, float Y, FVector& OutGround) const;

	bool GetEye(FVector& OutEye, FRotator& OutView) const;
	bool IsKeeperBusy() const;

	float Dread = 0.0f;
	float DreadTarget = 0.1f;
	float Panic = 0.0f;
	float PanicFloor = 0.0f;
	float WhisperUntil = -1.0f;

	bool bRain = false;
	float RainAmount = 0.0f;
	bool bIndoors = false;
	float IndoorCheck = 0.0f;
	float Indoor = 0.0f;

	struct FDrop
	{
		FVector Position;
		float KillZ = 0.0f;
	};
	TArray<FDrop> Drops;
	TArray<FTransform> DropTransforms;

	int32 WatcherLevel = 0;

	/** Lamp neglect on top of the story's dread, and whether it raised the watcher level itself */
	float LampNeglect = 0.0f;
	bool bNeglectWatcher = false;
	float WatcherCountdown = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<AWatcher> Watcher;

	float WatcherAge = 0.0f;
	float WatcherSeenTime = 0.0f;
	float WatcherVanishAfter = 1.0f;
	bool bWatcherNoticed = false;
	float WatcherFade = 0.0f;
	bool bWatcherLeaving = false;

	/** Placed by a debug command: stays until approached */
	bool bWatcherPinned = false;

	struct FFlickerLight
	{
		TWeakObjectPtr<ULocalLightComponent> Light;
		float Intensity = 0.0f;
	};
	TArray<FFlickerLight> FlickerLights;
	float FlickerUntil = -1.0f;
	float FlickerStep = 0.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AWatcher>> Crowd;
	float CrowdAge = 0.0f;
	TArray<FVector> CrowdTargets;
};
