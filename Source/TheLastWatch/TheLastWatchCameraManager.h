// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "TheLastWatchCameraManager.generated.h"

/**
 *  First person camera manager.
 *  Limits min/max look pitch and gives the keeper a body: head bob while walking,
 *  slow breathing, a dip on landing, and a shake and narrowing view when panicked.
 */
UCLASS()
class ATheLastWatchCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

protected:

	/** Vertical bob at walking speed */
	UPROPERTY(EditAnywhere, Category="Head Bob", meta=(Units="cm"))
	float BobHeight = 1.4f;

	UPROPERTY(EditAnywhere, Category="Head Bob", meta=(Units="cm"))
	float BobSway = 0.9f;

	/** Distance covered per footstep (matches the footstep component) */
	UPROPERTY(EditAnywhere, Category="Head Bob", meta=(Units="cm"))
	float StrideLength = 150.0f;

	/** Extra field of view while sprinting */
	UPROPERTY(EditAnywhere, Category="Head Bob")
	float SprintFovBoost = 4.0f;

public:

	/** Constructor */
	ATheLastWatchCameraManager();

	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;

private:

	float BobPhase = 0.0f;
	float BobWeight = 0.0f;
	float BreathTime = 0.0f;
	float FovOffset = 0.0f;

	/** Downward camera kick after a fall, springs back */
	float LandDip = 0.0f;
	bool bWasFalling = false;
	float FallSpeed = 0.0f;
};
