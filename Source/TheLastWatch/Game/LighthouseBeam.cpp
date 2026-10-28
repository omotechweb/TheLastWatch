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

	// the beams start outside the lantern room (see LensRadius)
	BeamA = CreateDefaultSubobject<USpotLightComponent>(TEXT("BeamA"));
	BeamA->SetupAttachment(Rotor);
	BeamA->SetRelativeLocation(FVector(LensRadius, 0.0f, 0.0f));

	BeamB = CreateDefaultSubobject<USpotLightComponent>(TEXT("BeamB"));
	BeamB->SetupAttachment(Rotor);
	BeamB->SetRelativeLocationAndRotation(FVector(-LensRadius, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetIntensityUnits(ELightUnits::Lumens);
	Glow->SetLightColor(FLinearColor(1.0f, 0.85f, 0.6f));
	Glow->SetAttenuationRadius(700.0f);
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

	// no bounce light: Lumen would flood the lantern room from wherever the beam lands
	Beam->SetIndirectLightingIntensity(0.0f);
}

void ALighthouseBeam::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ConfigureBeam(BeamA);
	ConfigureBeam(BeamB);
	Glow->SetIntensity(GlowIntensity);
	SetLit(bLit);
}

void ALighthouseBeam::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bLit)
	{
		Rotor->AddRelativeRotation(FRotator(0.0f, RotationSpeed * Drive * DeltaSeconds, 0.0f));
	}
}

void ALighthouseBeam::SetLit(bool bValue)
{
	bLit = bValue;

	// a freshly lit lamp is full and wound
	Drive = 1.0f;
	Power = 1.0f;
	ApplyPower();
}

void ALighthouseBeam::SetCondition(float InDrive, float InPower)
{
	Drive = FMath::Clamp(InDrive, 0.0f, 1.0f);
	const float NewPower = FMath::Clamp(InPower, 0.0f, 1.0f);
	// small changes are skipped to spare render updates, but out and full always land exactly
	if (NewPower != Power && (NewPower == 0.0f || NewPower == 1.0f || !FMath::IsNearlyEqual(NewPower, Power, 0.005f)))
	{
		Power = NewPower;
		ApplyPower();
	}
}

void ALighthouseBeam::ApplyPower()
{
	const bool bShining = bLit && Power > 0.0f;
	BeamA->SetVisibility(bShining);
	BeamB->SetVisibility(bShining);
	Glow->SetVisibility(bShining);
	if (bShining)
	{
		BeamA->SetIntensity(BeamIntensity * Power);
		BeamB->SetIntensity(BeamIntensity * Power);
		Glow->SetIntensity(GlowIntensity * Power);
	}
}
