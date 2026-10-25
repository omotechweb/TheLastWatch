// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LighthouseBeam.generated.h"

class UPointLightComponent;
class USpotLightComponent;

/**
 *  The lighthouse lamp: two opposed beams sweeping through the fog plus a glow at the lens.
 *  Place it at the centre of the lantern room.
 */
UCLASS()
class THELASTWATCH_API ALighthouseBeam : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	/** Spins around Z, carrying both beams */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Rotor;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USpotLightComponent> BeamA;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USpotLightComponent> BeamB;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> Glow;

protected:

	/** Degrees per second. 36 = a full turn every 10 s, so a flash every 5 s */
	UPROPERTY(EditAnywhere, Category="Lighthouse")
	float RotationSpeed = 36.0f;

	UPROPERTY(EditAnywhere, Category="Lighthouse", meta=(Units="cd"))
	float BeamIntensity = 400000.0f;

	/** Warm light inside the lantern room. Exposure is set for moonlight, so keep it low */
	UPROPERTY(EditAnywhere, Category="Lighthouse", meta=(Units="lm"))
	float GlowIntensity = 8.0f;

	UPROPERTY(EditAnywhere, Category="Lighthouse")
	bool bLit = true;

	/** Beams start this far from the centre, just outside the lantern glazing; inside it their
	 *  volumetric scattering would fill the lantern room with glowing fog */
	static constexpr float LensRadius = 240.0f;

public:

	ALighthouseBeam();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Turns the lamp on or off (fuel out, sabotage, the ending...) */
	UFUNCTION(BlueprintCallable, Category="Lighthouse")
	void SetLit(bool bValue);

	bool IsLit() const { return bLit; }

private:

	void ConfigureBeam(USpotLightComponent* Beam) const;
};
