// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FootstepComponent.generated.h"

class USoundBase;

/** Footstep sound for meshes whose asset path contains Match */
USTRUCT()
struct FFootstepSurface
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Footsteps")
	FString Match;

	UPROPERTY(EditAnywhere, Category="Footsteps")
	TObjectPtr<USoundBase> Sound;
};

/**
 *  Plays a footstep every stride while the controlled pawn walks on the ground.
 *  The surface is guessed from the asset path of the mesh underfoot
 *  (cabin pack = wood, power plant pack = concrete, island terrain = dirt...).
 */
UCLASS(ClassGroup=(LastWatch), meta=(BlueprintSpawnableComponent))
class THELASTWATCH_API UFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

protected:

	/** Checked in order; first match wins */
	UPROPERTY(EditAnywhere, Category="Footsteps")
	TArray<FFootstepSurface> Surfaces;

	UPROPERTY(EditAnywhere, Category="Footsteps")
	TObjectPtr<USoundBase> DefaultSound;

	/** Distance between steps while walking */
	UPROPERTY(EditAnywhere, Category="Footsteps", meta=(Units="cm"))
	float StrideLength = 150.0f;

	UPROPERTY(EditAnywhere, Category="Footsteps")
	float Volume = 0.55f;

	float DistanceSinceStep = 0.0f;

public:

	UFootstepComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	USoundBase* FindSurfaceSound(const FHitResult& Hit) const;
};
