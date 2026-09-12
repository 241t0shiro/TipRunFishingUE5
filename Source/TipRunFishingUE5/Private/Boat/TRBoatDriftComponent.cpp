#include "Boat/TRBoatDriftComponent.h"
#include <cmath>

namespace
{
	bool ValidOcean(const FTROceanSample& Ocean)
	{
		return Ocean.bValid && Ocean.InvalidReason == ETRSampleError::None && Ocean.SampleTick >= 0 &&
			FMath::IsFinite(Ocean.SurfaceZ_M) && FMath::IsFinite(Ocean.BottomDepthM) && Ocean.BottomDepthM > 0.0f &&
			!Ocean.CurrentMps.ContainsNaN() && Ocean.CurrentMps.Z == 0.0;
	}
	FVector RodPosition(const FTRBoatSnapshot& Snapshot, const FVector& Offset)
	{
		const double C = FMath::Cos(double(Snapshot.HeadingRad)), S = FMath::Sin(double(Snapshot.HeadingRad));
		return Snapshot.PositionM + FVector(C * Offset.X - S * Offset.Y, S * Offset.X + C * Offset.Y, Offset.Z);
	}
	bool FiniteSnapshot(const FTRBoatSnapshot& Snapshot)
	{
		return !TRUnits::MetersToCentimeters(Snapshot.PositionM).ContainsNaN() &&
			!TRUnits::MetersToCentimeters(Snapshot.RodTipM).ContainsNaN() && !Snapshot.VelocityMps.ContainsNaN();
	}
}
UTRBoatDriftComponent::UTRBoatDriftComponent() { PrimaryComponentTick.bCanEverTick = false; }

bool UTRBoatDriftComponent::InitializeMotion(const FTRBoatParameters& Settings, const FVector2D& InitialXYM,
	float HeadingRad, const FTROceanSample& Ocean, TArray<FText>& Errors)
{
	bInitialized = false;
	if (!Settings.Validate(Errors)) { return false; }
	if (!ValidOcean(Ocean) || !FMath::IsFinite(InitialXYM.X) || !FMath::IsFinite(InitialXYM.Y) || !FMath::IsFinite(HeadingRad))
	{
		Errors.Add(FText::FromString(TEXT("Boat initialization requires a valid horizontal ocean, finite position and heading")));
		return false;
	}
	FTRBoatSnapshot Initial;
	Initial.Tick = Ocean.SampleTick;
	Initial.PositionM = FVector(InitialXYM.X, InitialXYM.Y, double(Ocean.SurfaceZ_M) + Settings.HullHeightOffsetM);
	Initial.HeadingRad = float(FMath::Fmod(double(HeadingRad), 2.0 * UE_DOUBLE_PI));
	Initial.RodTipM = RodPosition(Initial, Settings.RodAnchorOffsetM);
	if (!FiniteSnapshot(Initial))
	{
		Errors.Add(FText::FromString(TEXT("Boat initial position/rod transform overflows")));
		return false;
	}
	FrozenSettings = Settings;
	Snapshot = Initial;
	LastIntegratedTick = Ocean.SampleTick - 1;
	bInitialized = true;
	return true;
}

bool UTRBoatDriftComponent::StepDrift(const FTRSimTime& Time, const FTROceanSample& Ocean,
	TFunctionRef<bool(const FVector2D&)> IsDestinationValid)
{
	if (!bInitialized || !Time.IsValid() || Time.TickIndex <= LastIntegratedTick ||
		Ocean.SampleTick != Time.TickIndex || !ValidOcean(Ocean)) { return false; }
	const double RateDt = FrozenSettings.VelocityResponsePerS * Time.StepSeconds;
	if (!FMath::IsFinite(RateDt)) { return false; }
	const double Alpha = -std::expm1(-RateDt);
	FVector Velocity = Snapshot.VelocityMps + (Ocean.CurrentMps * FrozenSettings.CurrentResponse01 - Snapshot.VelocityMps) * Alpha;
	const double Speed = std::hypot(Velocity.X, Velocity.Y);
	if (!FMath::IsFinite(Speed)) { return false; }
	if (Speed > FrozenSettings.MaxDriftSpeedMps) { Velocity *= FrozenSettings.MaxDriftSpeedMps / Speed; }
	FTRBoatSnapshot Next = Snapshot;
	Next.Tick = Time.TickIndex;
	Next.VelocityMps = Velocity;
	Next.PositionM += Velocity * Time.StepSeconds;
	Next.PositionM.Z = double(Ocean.SurfaceZ_M) + FrozenSettings.HullHeightOffsetM;
	Next.RodTipM = RodPosition(Next, FrozenSettings.RodAnchorOffsetM);
	if (!FiniteSnapshot(Next) || !IsDestinationValid(FVector2D(Next.PositionM.X, Next.PositionM.Y))) { return false; }
	Snapshot = Next;
	LastIntegratedTick = Time.TickIndex;
	return true;
}
