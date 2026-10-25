// The Last Watch

#include "LighthouseBeam.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"

ALighthouseBeam::ALighthouseBeam()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Rotor = CreateDefaultSubobject<USceneComponent>(TEXT("Rotor"));
	Rotor->SetupAttachment(Root);

	BeamA = CreateDefaultSubobject<USpotLightComponent>(TEXT("BeamA"));
	BeamA->SetupAttachment(Rotor);

	BeamB = CreateDefaultSubobject<USpotLightComponent>(TEXT("BeamB"));
	BeamB->SetupAttachment(Rotor);
	BeamB->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetIntensityUnits(ELightUnits::Lumens);
	Glow->SetIntensity(20000.0f);
	Glow->SetLightColor(FLinearColor(1.0f, 0.85f, 0.6f));
	Glow->SetAttenuationRadius(1500.0f);
	Glow->SetCastShadows(false);
}

void ALighthouseBeam::ConfigureBeam(USpotLightComponent* Beam) const
{
	Beam->SetIntensityUnits(ELightUnits::Candelas);
	Beam->SetIntensity(BeamIntensity);
	Beam->SetLightColor(FLinearColor(1.0f, 0.9f, 0.72f));
	Beam->SetInnerConeAngle(1.5f);
	Beam->SetOuterConeAngle(4.0f);
	Beam->SetAttenuationRadius(60000.0f);
	Beam->SetVolumetricScatteringIntensity(8.0f);

	// the lantern glass would block shadowed beams; the sweep reads better unshadowed
	Beam->SetCastShadows(false);
}

void ALighthouseBeam::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ConfigureBeam(BeamA);
	ConfigureBeam(BeamB);
	SetLit(bLit);
}

void ALighthouseBeam::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bLit)
	{
		Rotor->AddRelativeRotation(FRotator(0.0f, RotationSpeed * DeltaSeconds, 0.0f));
	}
}

void ALighthouseBeam::SetLit(bool bValue)
{
	bLit = bValue;
	BeamA->SetVisibility(bLit);
	BeamB->SetVisibility(bLit);
	Glow->SetVisibility(bLit);
}
