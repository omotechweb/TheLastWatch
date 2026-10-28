// The Last Watch

#include "KeeperDirector.h"
#include "Animation/AnimSequence.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "KeeperSynth.h"
#include "LastWatchPlayerController.h"
#include "LighthouseBeam.h"
#include "TheLastWatch.h"

/////////////////////////////////////////////////////////////////////////////
// AWatcher

AWatcher::AWatcher()
{
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);

	// too tall, too thin
	Body->SetRelativeScale3D(FVector(0.88f, 0.88f, 1.12f));

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
	if (Mesh.Succeeded())
	{
		Body->SetSkeletalMesh(Mesh.Object);
	}
}

void AWatcher::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effects/M_Watcher")))
	{
		Material = UMaterialInstanceDynamic::Create(Base, this);
		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
		{
			Body->SetMaterial(Slot, Material);
		}
	}

	// the idle, slowed almost to a stop: it breathes, barely
	if (UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle")))
	{
		Body->PlayAnimation(Idle, true);
		Body->SetPlayRate(0.12f);
		Body->SetPosition(FMath::FRandRange(0.0f, Idle->GetPlayLength()));
	}
}

void AWatcher::SetFade(float Fade)
{
	if (Material)
	{
		Material->SetScalarParameterValue(TEXT("Fade"), FMath::Clamp(Fade, 0.0f, 1.0f));
	}
	SetActorHiddenInGame(Fade <= 0.0f);
}

void AWatcher::FaceTowards(const FVector& Target)
{
	const FVector To = (Target - GetActorLocation()).GetSafeNormal2D();
	if (!To.IsNearlyZero())
	{
		// the mannequin looks down +Y
		SetActorRotation(FRotator(0.0f, To.Rotation().Yaw - 90.0f, 0.0f));
	}
}

/////////////////////////////////////////////////////////////////////////////
// AKeeperDirector

AKeeperDirector::AKeeperDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Grade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Grade"));
	Grade->SetupAttachment(RootComponent);
	Grade->bUnbound = true;

	// above the map's own post process volume
	Grade->Priority = 10.0f;

	Synth = CreateDefaultSubobject<UKeeperSynth>(TEXT("Synth"));
	Synth->SetupAttachment(RootComponent);

	RainDrops = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RainDrops"));
	RainDrops->SetupAttachment(RootComponent);
	RainDrops->SetUsingAbsoluteLocation(true);
	RainDrops->SetUsingAbsoluteRotation(true);
	RainDrops->SetUsingAbsoluteScale(true);
	RainDrops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RainDrops->SetCastShadow(false);
	RainDrops->SetGenerateOverlapEvents(false);
	RainDrops->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Streak(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StreakMaterial(TEXT("/Game/Effects/M_RainStreak"));
	if (Streak.Succeeded())
	{
		RainDrops->SetStaticMesh(Streak.Object);
	}
	if (StreakMaterial.Succeeded())
	{
		RainDrops->SetMaterial(0, StreakMaterial.Object);
	}
}

AKeeperDirector* AKeeperDirector::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AKeeperDirector> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AKeeperDirector::BeginPlay()
{
	Super::BeginPlay();

	Synth->Start();

	TArray<FTransform> Initial;
	Initial.Init(FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -100000.0f), FVector::ZeroVector), RainDropCount);
	RainDrops->AddInstances(Initial, false);
	Drops.SetNum(RainDropCount);
	for (FDrop& Drop : Drops)
	{
		// respawn on the first update
		Drop.Position = FVector(0.0f, 0.0f, -100000.0f);
		Drop.KillZ = 0.0f;
	}
	DropTransforms = Initial;
}

void AKeeperDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Flicker(0.0f);
	UpdateFlicker(0.0f);
	Super::EndPlay(EndPlayReason);
}

void AKeeperDirector::ResetForNight(int32 Night)
{
	DreadTarget = 0.1f;
	PanicFloor = 0.0f;
	Panic = 0.0f;
	WhisperUntil = -1.0f;
	bRain = false;
	SetWatcherLevel(0);
	SetCrowd(false);
	FlickerUntil = -1.0f;
	UpdateFlicker(0.0f);
	SetRadio(0.0f, 0.0f, 1.0f);
	UE_LOG(LogTheLastWatch, Display, TEXT("Director: reset for night %d"), Night);
}

/////////////////////////////////////////////////////////////////////////////
// Commands

void AKeeperDirector::SetWatcherLevel(int32 Level)
{
	WatcherLevel = FMath::Clamp(Level, 0, WatcherIntervals.Num());
	if (WatcherLevel == 0)
	{
		if (Watcher)
		{
			Watcher->Destroy();
			Watcher = nullptr;
		}
		return;
	}

	// the first one comes sooner than the rest
	ScheduleWatcher();
	WatcherCountdown *= 0.5f;
	UE_LOG(LogTheLastWatch, Display, TEXT("Director: watcher level %d, first in %.0f s"), WatcherLevel, WatcherCountdown);
}

void AKeeperDirector::ScheduleWatcher()
{
	if (WatcherLevel > 0)
	{
		const FVector2D& Range = WatcherIntervals[WatcherLevel - 1];
		WatcherCountdown = FMath::FRandRange(Range.X, Range.Y);
	}
}

void AKeeperDirector::AddPanic(float Amount)
{
	Panic = FMath::Clamp(Panic + Amount, 0.0f, 1.0f);
}

void AKeeperDirector::Sting(float Strength)
{
	Synth->Sting(Strength);
}

void AKeeperDirector::Knock()
{
	Synth->Knock();
}

void AKeeperDirector::Click()
{
	Synth->Click();
}

void AKeeperDirector::Whisper(float Seconds)
{
	WhisperUntil = FMath::Max(WhisperUntil, GetWorld()->GetTimeSeconds() + Seconds);
}

void AKeeperDirector::SetRadio(float Static, float Voice, float Detune)
{
	Synth->SetRadio(Static, Voice, Detune);
}

void AKeeperDirector::Flicker(float Seconds)
{
	FlickerUntil = GetWorld()->GetTimeSeconds() + Seconds;
	if (!FlickerLights.IsEmpty() || Seconds <= 0.0f)
	{
		return;
	}

	FVector Eye;
	FRotator View;
	if (!GetEye(Eye, View))
	{
		return;
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<ALighthouseBeam>() || FVector::DistSquared(It->GetActorLocation(), Eye) > FMath::Square(3500.0f))
		{
			continue;
		}
		TInlineComponentArray<ULocalLightComponent*> Lights(*It);
		for (ULocalLightComponent* Light : Lights)
		{
			if (Light->IsVisible())
			{
				FlickerLights.Add({ Light, Light->Intensity });
			}
		}
	}
	UE_LOG(LogTheLastWatch, Display, TEXT("Director: flicker %.1f s (%d lights)"), Seconds, FlickerLights.Num());
}

void AKeeperDirector::SetCrowd(bool bOn)
{
	for (AWatcher* Figure : Crowd)
	{
		if (Figure)
		{
			Figure->Destroy();
		}
	}
	Crowd.Reset();
	CrowdTargets.Reset();
	CrowdAge = 0.0f;
	if (!bOn)
	{
		return;
	}

	FVector Center = FVector::ZeroVector;
	for (TActorIterator<ALighthouseBeam> It(GetWorld()); It; ++It)
	{
		Center = It->GetActorLocation();
	}

	constexpr int32 Count = 18;
	for (int32 i = 0; i < Count; ++i)
	{
		const float Angle = FMath::DegreesToRadians(360.0f * i / Count + FMath::FRandRange(-8.0f, 8.0f));
		const float Radius = FMath::FRandRange(1600.0f, 4200.0f);
		const float X = Center.X + FMath::Cos(Angle) * Radius;
		const float Y = Center.Y + FMath::Sin(Angle) * Radius;

		// on the rocks if there are any, otherwise standing in the sea
		FVector Ground;
		if (!FindGround(X, Y, Ground))
		{
			Ground = FVector(X, Y, -40.0f);
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWatcher* Figure = GetWorld()->SpawnActor<AWatcher>(Ground - FVector(0.0f, 0.0f, 260.0f), FRotator::ZeroRotator, Params))
		{
			Figure->FaceTowards(Center);
			Figure->SetActorScale3D(Figure->GetActorScale3D() * FMath::FRandRange(0.95f, 1.25f));
			Figure->SetFade(0.0f);
			Crowd.Add(Figure);
			CrowdTargets.Add(Ground);
		}
	}
	UE_LOG(LogTheLastWatch, Display, TEXT("Director: %d figures rise around the lighthouse"), Crowd.Num());
}

/////////////////////////////////////////////////////////////////////////////
// Tick

bool AKeeperDirector::GetEye(FVector& OutEye, FRotator& OutView) const
{
	const APlayerController* Keeper = GetWorld()->GetFirstPlayerController();
	if (!Keeper)
	{
		return false;
	}
	Keeper->GetPlayerViewPoint(OutEye, OutView);
	return true;
}

bool AKeeperDirector::IsKeeperBusy() const
{
	const ALastWatchPlayerController* Keeper = Cast<ALastWatchPlayerController>(GetWorld()->GetFirstPlayerController());
	return !Keeper || Keeper->GetMode() != EKeeperMode::Explore;
}

void AKeeperDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FVector Eye;
	FRotator View;
	const bool bHasEye = GetEye(Eye, View);

	UpdateGrade(DeltaSeconds);
	UpdateFlicker(DeltaSeconds);
	UpdateCrowd(DeltaSeconds);
	if (bHasEye)
	{
		UpdateRain(DeltaSeconds, Eye);
		UpdateWatcher(DeltaSeconds, Eye, View);
	}

	// the choir: bursts from the story or a sighting, and a faint breath while something is out there unseen
	const bool bWhispering = GetWorld()->GetTimeSeconds() < WhisperUntil;
	const float Lurking = Watcher && !bWatcherNoticed ? 0.12f : 0.0f;
	const float Choir = Crowd.IsEmpty() ? 0.0f : 0.8f;
	Synth->SetWhisper(FMath::Max3(bWhispering ? 0.7f : 0.0f, Lurking, Choir));
	Synth->SetDread(Dread);
	Synth->SetPanic(Panic);
}

void AKeeperDirector::UpdateGrade(float DeltaSeconds)
{
	Dread = FMath::FInterpConstantTo(Dread, DreadTarget, DeltaSeconds, 0.12f);
	Panic = FMath::Max(PanicFloor, Panic - DeltaSeconds * 0.14f);

	FPostProcessSettings& S = Grade->Settings;

	// cold and drained: reds go first, blue stays
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(0.85f, 0.92f, 1.05f, FMath::Lerp(0.8f, 0.45f, Dread));
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(1.0f, 1.0f, 1.0f, 1.08f + 0.12f * Dread);
	S.bOverride_ColorGain = true;
	S.ColorGain = FVector4(0.9f, 0.96f, 1.07f, 1.0f);
	S.bOverride_ColorOffsetShadows = true;
	S.ColorOffsetShadows = FVector4(0.0f, 0.0015f, 0.004f, 0.0f);

	// darker as the nights go on
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = -0.2f - 0.45f * Dread - 0.25f * Panic;

	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = 0.55f + 0.3f * Dread + 0.35f * Panic;
	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = 0.22f + 0.18f * Dread + 0.35f * Panic;
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = 0.4f + 0.8f * Dread + 4.0f * Panic;
}

void AKeeperDirector::UpdateRain(float DeltaSeconds, const FVector& Eye)
{
	const bool bInBunker = Eye.Z > BunkerHeight;

	// a roof overhead: hide the streaks and muffle the sound
	if ((IndoorCheck -= DeltaSeconds) <= 0.0f)
	{
		IndoorCheck = 0.25f;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RainRoof), false, GetWorld()->GetFirstPlayerController()->GetPawn());
		const bool bWasIndoors = bIndoors;
		bIndoors = bInBunker || GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + FVector(0.0f, 0.0f, 2500.0f), ECC_Visibility, Params);
		if (bIndoors != bWasIndoors)
		{
			UE_LOG(LogTheLastWatch, Display, TEXT("Director: %s (%s)"), bIndoors ? TEXT("under a roof") : TEXT("outdoors"), *GetNameSafe(Hit.GetActor()));
		}
	}
	Indoor = FMath::FInterpConstantTo(Indoor, bIndoors ? 1.0f : 0.0f, DeltaSeconds, 2.5f);
	RainAmount = FMath::FInterpConstantTo(RainAmount, bRain ? 1.0f : 0.0f, DeltaSeconds, 0.25f);

	Synth->SetRain(bInBunker ? 0.0f : RainAmount);
	Synth->SetRainMuffle(Indoor);

	const bool bShow = RainAmount > 0.01f && !bIndoors;
	RainDrops->SetHiddenInGame(!bShow);
	if (!bShow)
	{
		return;
	}

	const APawn* KeeperPawn = GetWorld()->GetFirstPlayerController()->GetPawn();
	const FVector Lead = KeeperPawn ? KeeperPawn->GetVelocity() * 0.5f : FVector::ZeroVector;
	const FQuat Slant = FRotationMatrix::MakeFromZ(-RainVelocity.GetSafeNormal()).ToQuat();
	const FVector StreakScale(0.013f, 0.013f, 0.7f);
	const int32 Active = FMath::RoundToInt(RainAmount * Drops.Num());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RainDrop), false, KeeperPawn);

	for (int32 i = 0; i < Drops.Num(); ++i)
	{
		FDrop& Drop = Drops[i];
		if (i >= Active)
		{
			DropTransforms[i] = FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -100000.0f), FVector::ZeroVector);
			continue;
		}

		Drop.Position += RainVelocity * DeltaSeconds;
		const bool bFirst = Drop.Position.Z < -50000.0f;
		if (Drop.Position.Z < Drop.KillZ || FVector::DistSquared2D(Drop.Position, Eye) > FMath::Square(RainRadius * 1.3f))
		{
			const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
			const float Radius = RainRadius * FMath::Sqrt(FMath::FRand());
			Drop.Position = FVector(Eye.X + Lead.X + FMath::Cos(Angle) * Radius, Eye.Y + Lead.Y + FMath::Sin(Angle) * Radius,
				Eye.Z + (bFirst ? FMath::FRandRange(-400.0f, 1000.0f) : FMath::FRandRange(600.0f, 1000.0f)));

			FHitResult Hit;
			const FVector Floor = Drop.Position - FVector(0.0f, 0.0f, 3000.0f);
			Drop.KillZ = GetWorld()->LineTraceSingleByChannel(Hit, Drop.Position, Floor, ECC_Visibility, Params) ? Hit.ImpactPoint.Z : Floor.Z;
		}
		DropTransforms[i] = FTransform(Slant, Drop.Position, StreakScale);
	}
	RainDrops->BatchUpdateInstancesTransforms(0, DropTransforms, true, true, false);
}

bool AKeeperDirector::FindGround(float X, float Y, FVector& OutGround) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WatcherGround), true);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, FVector(X, Y, 8000.0f), FVector(X, Y, -500.0f), ECC_Visibility, Params))
	{
		return false;
	}

	// open ground only: the island terrain or the cliffs, never a roof or a table
	const UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Hit.GetComponent());
	const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
	const FString Path = Mesh ? Mesh->GetPathName() : FString();
	if (!Path.StartsWith(TEXT("/Game/Island/")) && !Path.StartsWith(TEXT("/Game/Imported/Cliffs/")))
	{
		return false;
	}
	if (Hit.ImpactPoint.Z < 60.0f || Hit.ImpactNormal.Z < 0.6f)
	{
		return false;
	}
	OutGround = Hit.ImpactPoint;
	return true;
}

bool AKeeperDirector::CanSee(const FVector& Eye, const AActor* Target, const FVector& Point) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WatcherSight), false, GetWorld()->GetFirstPlayerController()->GetPawn());
	Params.AddIgnoredActor(Target);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Params);
}

bool AKeeperDirector::TrySpawnWatcher(const FVector& Eye, const FRotator& View)
{
	for (int32 Attempt = 0; Attempt < 32; ++Attempt)
	{
		// off to the side or behind: it is there when the keeper turns round
		const float Side = FMath::RandBool() ? 1.0f : -1.0f;
		const float Yaw = View.Yaw + Side * FMath::FRandRange(65.0f, 165.0f);
		const float Distance = FMath::FRandRange(WatcherDistance.X, WatcherDistance.Y);
		const FVector Direction = FRotator(0.0f, Yaw, 0.0f).Vector();

		FVector Ground;
		if (!FindGround(Eye.X + Direction.X * Distance, Eye.Y + Direction.Y * Distance, Ground))
		{
			continue;
		}
		const FVector Head = Ground + FVector(0.0f, 0.0f, 175.0f);
		if (!CanSee(Eye, nullptr, Head))
		{
			continue;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Watcher = GetWorld()->SpawnActor<AWatcher>(Ground, FRotator::ZeroRotator, Params);
		if (!Watcher)
		{
			return false;
		}
		Watcher->FaceTowards(Eye);
		Watcher->SetFade(0.0f);
		WatcherAge = 0.0f;
		WatcherSeenTime = 0.0f;
		WatcherVanishAfter = FMath::FRandRange(0.6f, 1.5f);
		WatcherFade = 0.0f;
		bWatcherNoticed = false;
		bWatcherLeaving = false;
		bWatcherPinned = false;
		UE_LOG(LogTheLastWatch, Display, TEXT("Director: watcher appears at %s, %.0f m away"), *Ground.ToString(), Distance / 100.0f);
		return true;
	}
	return false;
}

void AKeeperDirector::SpawnWatcherAhead(float Distance)
{
	FVector Eye;
	FRotator View;
	if (!GetEye(Eye, View))
	{
		return;
	}
	if (Watcher)
	{
		Watcher->Destroy();
		Watcher = nullptr;
	}

	// anywhere solid will do for a test, roofs included
	const FVector Ahead = Eye + FRotator(0.0f, View.Yaw, 0.0f).Vector() * Distance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WatcherAhead), true);
	FVector Ground = FVector(Ahead.X, Ahead.Y, 0.0f);
	if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(Ahead.X, Ahead.Y, 8000.0f), FVector(Ahead.X, Ahead.Y, -500.0f), ECC_Visibility, Params))
	{
		Ground = Hit.ImpactPoint;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Watcher = GetWorld()->SpawnActor<AWatcher>(Ground, FRotator::ZeroRotator, SpawnParams);
	if (Watcher)
	{
		Watcher->FaceTowards(Eye);
		WatcherAge = 0.0f;
		WatcherSeenTime = 0.0f;

		// stays put for screenshots: already noticed, and leaves only after a long look
		bWatcherPinned = true;
		bWatcherNoticed = true;
		WatcherVanishAfter = 1000.0f;
		WatcherFade = 1.0f;
		bWatcherLeaving = false;
		Watcher->SetFade(1.0f);
		UE_LOG(LogTheLastWatch, Display, TEXT("Director: debug watcher at %s"), *Ground.ToString());
	}
}

void AKeeperDirector::DismissWatcher(bool bSeen)
{
	bWatcherLeaving = true;
	UE_LOG(LogTheLastWatch, Display, TEXT("Director: watcher leaves (%s)"), bSeen ? TEXT("seen") : TEXT("unseen"));
}

void AKeeperDirector::UpdateWatcher(float DeltaSeconds, const FVector& Eye, const FRotator& View)
{
	const bool bInBunker = Eye.Z > BunkerHeight;

	if (!Watcher)
	{
		if (WatcherLevel == 0 || IsKeeperBusy() || bInBunker || bIndoors)
		{
			return;
		}
		if ((WatcherCountdown -= DeltaSeconds) <= 0.0f)
		{
			if (TrySpawnWatcher(Eye, View))
			{
				ScheduleWatcher();
			}
			else
			{
				WatcherCountdown = 3.0f;
			}
		}
		return;
	}

	WatcherAge += DeltaSeconds;
	const FVector Feet = Watcher->GetActorLocation();
	const FVector Chest = Feet + FVector(0.0f, 0.0f, 140.0f);
	const float Distance = FVector::Dist2D(Feet, Eye);

	if (bWatcherLeaving)
	{
		WatcherFade -= DeltaSeconds / 0.15f;
		Watcher->SetFade(WatcherFade);
		if (WatcherFade <= 0.0f)
		{
			Watcher->Destroy();
			Watcher = nullptr;
		}
		return;
	}

	// it only ever moves while nobody is looking
	const float HalfFov = 0.5f * GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetFOVAngle();
	const FVector ToWatcher = (Chest - Eye).GetSafeNormal();
	const bool bInView = FVector::DotProduct(View.Vector(), ToWatcher) > FMath::Cos(FMath::DegreesToRadians(HalfFov * 0.8f));
	const bool bSeen = bInView && !IsKeeperBusy() && CanSee(Eye, Watcher, Chest);

	if (!bSeen)
	{
		Watcher->FaceTowards(Eye);
	}
	WatcherFade = FMath::Min(1.0f, WatcherFade + DeltaSeconds / 1.5f);
	Watcher->SetFade(WatcherFade);

	if (bSeen)
	{
		WatcherSeenTime += DeltaSeconds;
		if (!bWatcherNoticed && WatcherSeenTime > 0.2f)
		{
			bWatcherNoticed = true;
			Sting(0.9f);
			AddPanic(0.85f);
			Flicker(0.7f);
			Whisper(2.5f);
			UE_LOG(LogTheLastWatch, Display, TEXT("Director: the keeper sees the watcher"));
		}
		if (bWatcherNoticed && WatcherSeenTime > WatcherVanishAfter)
		{
			DismissWatcher(true);
		}
	}

	if (Distance < WatcherVanishDistance && !bWatcherPinned)
	{
		if (!bWatcherNoticed)
		{
			Sting(1.0f);
			AddPanic(1.0f);
		}
		DismissWatcher(true);
	}
	else if (!bWatcherPinned && (IsKeeperBusy() || bInBunker || (!bWatcherNoticed && WatcherAge > 45.0f) || WatcherLevel == 0))
	{
		DismissWatcher(false);
	}
}

void AKeeperDirector::UpdateFlicker(float DeltaSeconds)
{
	if (FlickerLights.IsEmpty())
	{
		return;
	}

	if (GetWorld()->GetTimeSeconds() >= FlickerUntil)
	{
		for (const FFlickerLight& Entry : FlickerLights)
		{
			if (ULocalLightComponent* Light = Entry.Light.Get())
			{
				Light->SetIntensity(Entry.Intensity);
			}
		}
		FlickerLights.Reset();
		return;
	}

	if ((FlickerStep -= DeltaSeconds) > 0.0f)
	{
		return;
	}
	FlickerStep = FMath::FRandRange(0.03f, 0.14f);
	const bool bOut = FMath::FRand() < 0.4f;
	for (const FFlickerLight& Entry : FlickerLights)
	{
		if (ULocalLightComponent* Light = Entry.Light.Get())
		{
			Light->SetIntensity(Entry.Intensity * (bOut ? 0.03f : FMath::FRandRange(0.5f, 1.05f)));
		}
	}
}

void AKeeperDirector::UpdateCrowd(float DeltaSeconds)
{
	if (Crowd.IsEmpty())
	{
		return;
	}

	// they come up out of the water one after another
	CrowdAge += DeltaSeconds;
	for (int32 i = 0; i < Crowd.Num(); ++i)
	{
		AWatcher* Figure = Crowd[i];
		if (!Figure)
		{
			continue;
		}
		const float Start = i * 0.6f;
		const float T = FMath::Clamp((CrowdAge - Start) / 7.0f, 0.0f, 1.0f);
		const float Eased = T * T * (3.0f - 2.0f * T);
		Figure->SetActorLocation(CrowdTargets[i] - FVector(0.0f, 0.0f, 260.0f * (1.0f - Eased)));
		Figure->SetFade(FMath::Min(1.0f, T * 3.0f));
	}
}
