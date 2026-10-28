// Copyright Epic Games, Inc. All Rights Reserved.


#include "TheLastWatchCameraManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KeeperDirector.h"
#include "KeeperSaveSubsystem.h"

ATheLastWatchCameraManager::ATheLastWatchCameraManager()
{
	// set the min/max pitch
	ViewPitchMin = -70.0f;
	ViewPitchMax = 80.0f;
}

void ATheLastWatchCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);

	// only the keeper's own eyes, not the title camera
	const ACharacter* Keeper = Cast<ACharacter>(OutVT.Target);
	if (!Keeper || !PCOwner || Keeper != PCOwner->GetPawn() || DeltaTime <= 0.0f)
	{
		return;
	}

	const UKeeperSaveSubsystem* Save = UKeeperSaveSubsystem::Get(this);
	const bool bBob = !Save || Save->GetSettings()->bHeadBob;
	const AKeeperDirector* Director = AKeeperDirector::Get(this);
	const float Panic = Director ? Director->GetPanic() : 0.0f;

	const UCharacterMovementComponent* Movement = Keeper->GetCharacterMovement();
	const float Speed = Keeper->GetVelocity().Size2D();
	const bool bGrounded = Movement && Movement->IsMovingOnGround();

	// landing: kick down by how hard the fall was
	if (Movement && Movement->IsFalling())
	{
		bWasFalling = true;
		FallSpeed = FMath::Max(FallSpeed, -Keeper->GetVelocity().Z);
	}
	else if (bWasFalling)
	{
		bWasFalling = false;
		LandDip = FMath::Clamp((FallSpeed - 250.0f) / 700.0f, 0.0f, 1.0f) * 7.0f;
		FallSpeed = 0.0f;
	}
	LandDip = FMath::FInterpTo(LandDip, 0.0f, DeltaTime, 7.0f);

	FVector Offset = FVector::ZeroVector;
	FRotator Tilt = FRotator::ZeroRotator;

	if (bBob)
	{
		// half a cycle per footstep: the head dips on every step and sways side to side every two
		BobWeight = FMath::FInterpTo(BobWeight, bGrounded ? FMath::Clamp(Speed / 250.0f, 0.0f, 1.6f) : 0.0f, DeltaTime, 6.0f);
		BobPhase = FMath::Fmod(BobPhase + DeltaTime * Speed / StrideLength * PI, 2.0f * PI);
		Offset.Z += (FMath::Cos(2.0f * BobPhase) - 1.0f) * 0.5f * BobHeight * BobWeight;
		Offset.Y += FMath::Sin(BobPhase) * BobSway * BobWeight;
		Tilt.Roll += FMath::Sin(BobPhase) * 0.3f * BobWeight;

		// breathing, quicker when scared
		BreathTime += DeltaTime * (1.0f + Panic * 1.5f);
		Offset.Z += FMath::Sin(BreathTime * 1.5f) * 0.35f;
		Tilt.Pitch += FMath::Sin(BreathTime * 1.5f + 0.6f) * 0.12f;

		Offset.Z -= LandDip;
	}

	// fear: a tremor and the world closing in
	if (Panic > 0.01f)
	{
		const float T = GetWorld()->GetTimeSeconds();
		Tilt.Pitch += FMath::PerlinNoise1D(T * 6.0f) * 0.8f * Panic;
		Tilt.Yaw += FMath::PerlinNoise1D(T * 5.0f + 31.0f) * 0.8f * Panic;
		Tilt.Roll += FMath::PerlinNoise1D(T * 4.0f + 77.0f) * 0.6f * Panic;
	}

	const float TargetFov = (Speed > 400.0f && bGrounded ? SprintFovBoost : 0.0f) - 7.0f * Panic;
	FovOffset = FMath::FInterpTo(FovOffset, TargetFov, DeltaTime, 4.0f);

	OutVT.POV.Location += OutVT.POV.Rotation.RotateVector(Offset);
	OutVT.POV.Rotation += Tilt;
	OutVT.POV.FOV += FovOffset;
}
