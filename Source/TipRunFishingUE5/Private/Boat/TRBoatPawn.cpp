#include "Boat/TRBoatPawn.h"
#include "Boat/TRBoatDriftComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"

ATRBoatPawn::ATRBoatPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	BoatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoatMesh"));
	BoatMesh->SetupAttachment(SceneRoot);
	BoatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoatMesh->SetSimulatePhysics(false);
	RodAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("RodAnchor"));
	RodAnchor->SetupAttachment(SceneRoot);
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(SceneRoot);
	SpringArm->bDoCollisionTest = false;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	DriftComponent = CreateDefaultSubobject<UTRBoatDriftComponent>(TEXT("DriftComponent"));
}

bool ATRBoatPawn::InitializeBoat(const FVector2D& InitialXYM, float HeadingRad, const FTROceanSample& Ocean, TArray<FText>& Errors)
{
	DisableBoat();
	if (!IsValid(Tuning))
	{
		Errors.Add(FText::FromString(TEXT("Boat Tuning is missing")));
		return false;
	}
	if (!DriftComponent->InitializeMotion(Tuning->Parameters, InitialXYM, HeadingRad, Ocean, Errors)) { return false; }
	FrozenRodOffsetM = Tuning->Parameters.RodAnchorOffsetM;
	BoatMode = ETRBoatMode::DriftOnly;
	ApplySimTransform();
	return true;
}

void ATRBoatPawn::StepBoat(const FTRSimTime& Time, const FTROceanSample& Ocean,
	TFunctionRef<bool(const FVector2D&)> IsDestinationValid)
{
	if (BoatMode != ETRBoatMode::DriftOnly) { return; }
	if (!DriftComponent->StepDrift(Time, Ocean, IsDestinationValid))
	{
		DisableBoat();
		OnEnvironmentInvalid.Broadcast(Ocean.bValid ? ETRSampleError::InvalidQuery : Ocean.InvalidReason);
		return;
	}
	ApplySimTransform();
}

FTRBoatSnapshot ATRBoatPawn::GetBoatSnapshot() const { return DriftComponent->BuildSnapshot(); }
void ATRBoatPawn::DisableBoat() { BoatMode = ETRBoatMode::Disabled; DriftComponent->StopMotion(); }
void ATRBoatPawn::ApplySimTransform()
{
	const FTRBoatSnapshot Snapshot = GetBoatSnapshot();
	SetActorTransform(FTransform(FRotator(0.0, FMath::RadiansToDegrees(double(Snapshot.HeadingRad)), 0.0),
		TRUnits::MetersToCentimeters(Snapshot.PositionM), FVector::OneVector), false, nullptr, ETeleportType::TeleportPhysics);
	RodAnchor->SetRelativeLocation(TRUnits::MetersToCentimeters(FrozenRodOffsetM));
}
void ATRBoatPawn::EndPlay(const EEndPlayReason::Type Reason)
{
	DisableBoat();
	OnEnvironmentInvalid.Clear();
	Super::EndPlay(Reason);
}
