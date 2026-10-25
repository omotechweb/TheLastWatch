// The Last Watch

#include "FootstepComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	USoundBase* FindSound(const TCHAR* Path)
	{
		ConstructorHelpers::FObjectFinder<USoundBase> Finder(Path);
		return Finder.Object;
	}
}

UFootstepComponent::UFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	USoundBase* Wood = FindSound(TEXT("/Game/Essential_Foosteps_SK/CUE/Wood/Footstep_Wood_Boots_Walk_1_Cue"));
	USoundBase* Concrete = FindSound(TEXT("/Game/Essential_Foosteps_SK/CUE/Concrete/Footstep_Concrete_Boots_Walk_1_Cue"));
	USoundBase* Gravel = FindSound(TEXT("/Game/Essential_Foosteps_SK/CUE/Gravel/Footstep_Gravel_Boots_Walk_1_Cue"));
	USoundBase* Dirt = FindSound(TEXT("/Game/Essential_Foosteps_SK/CUE/Dirt/Footstep_Dirt_Boots_Walk_1_Cue"));
	USoundBase* Metal = FindSound(TEXT("/Game/Essential_Foosteps_SK/CUE/Metal/Footstep_Metal_Boots_Walk_1_Cue"));

	Surfaces = {
		{ TEXT("Modular_Rural_Cabin"), Wood },
		{ TEXT("OldFurniturePack"), Wood },
		{ TEXT("AbandonedPowerPlant"), Concrete },
		{ TEXT("Lighthouse"), Metal },
		{ TEXT("Cliff"), Gravel },
		{ TEXT("Island"), Dirt },
	};
	DefaultSound = Concrete;
}

USoundBase* UFootstepComponent::FindSurfaceSound(const FHitResult& Hit) const
{
	const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Hit.GetComponent());
	const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
	if (Mesh)
	{
		const FString Path = Mesh->GetPathName();
		for (const FFootstepSurface& Surface : Surfaces)
		{
			if (Surface.Sound && Path.Contains(Surface.Match))
			{
				return Surface.Sound;
			}
		}
	}
	return DefaultSound;
}

void UFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// works whether the owner is the controller or the character itself
	const AController* Controller = Cast<AController>(GetOwner());
	ACharacter* Character = Cast<ACharacter>(Controller ? Controller->GetPawn() : GetOwner());
	if (!Character || !Character->GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}

	DistanceSinceStep += Character->GetVelocity().Size2D() * DeltaTime;
	if (DistanceSinceStep < StrideLength)
	{
		return;
	}
	DistanceSinceStep = 0.0f;

	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetDefaultHalfHeight());
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Footstep), false, Character);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0, 0, 20), Feet - FVector(0, 0, 60), ECC_Visibility, Params))
	{
		if (USoundBase* Sound = FindSurfaceSound(Hit))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, Hit.ImpactPoint, Volume, FMath::FRandRange(0.92f, 1.08f));
		}
	}
}
