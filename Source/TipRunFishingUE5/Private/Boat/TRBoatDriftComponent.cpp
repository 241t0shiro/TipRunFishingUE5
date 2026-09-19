#include "Boat/TRBoatDriftComponent.h"
#include "GameFramework/Actor.h"
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
	bool ValidWindCurrent(const FTROceanSample& Ocean)
	{
		return (Ocean.FieldRevision == 1 || Ocean.FieldRevision == 2) &&
			!Ocean.WindMps.ContainsNaN() && !Ocean.SurfaceCurrentMps.ContainsNaN() && Ocean.SurfaceCurrentMps.Z == 0 &&
			FMath::IsFinite(Ocean.WindMps.SizeSquared()) && FMath::IsFinite(Ocean.SurfaceCurrentMps.SizeSquared());
	}
	bool IntegrateWindCurrent(const FTRBoatParameters& P, const FTROceanSample& Ocean,
		const FTRBoatSnapshot& Previous, double Dt, FTRBoatSnapshot& Next, double ForceN, double SpeedLimit, double LateralResponsePerS)
	{
		const FVector Forward = Previous.ForwardVector;
		const FVector Side(-Forward.Y, Forward.X, 0);
		const FVector Wind(Ocean.WindMps.X, Ocean.WindMps.Y, 0);
		FVector DeltaPosition = FVector::ZeroVector;
		Next.VelocityMps = Next.WindDeltaVelocityMps = Next.CurrentDeltaVelocityMps = Next.DragDeltaVelocityMps = FVector::ZeroVector;
		for (int32 Axis = 0; Axis < 2; ++Axis)
		{
			const FVector Direction = Axis == 0 ? Forward : Side;
			const double W = FVector::DotProduct(Wind, Direction);
			const double U = FVector::DotProduct(Ocean.SurfaceCurrentMps, Direction);
			const double V = FVector::DotProduct(Previous.VelocityMps, Direction);
			// Relative wind arriving from the bow travels towards -Forward.
			const double Scale = Axis == 1 ? P.SideWindScale : (W - V < 0 ? P.BowWindScale : P.SternWindScale);
			const double Kw = P.WindResponseKgPerS * Scale;
			const double K = Kw + P.CurrentResponseKgPerS + P.DragKgPerS + (Axis==1 ? P.InertiaKg*LateralResponsePerS : 0);
			const double Rate = K / P.InertiaKg;
			const double X = Rate * Dt;
			if (!FMath::IsFinite(X)) { return false; }
			const double Alpha = -std::expm1(-X);
			// Weighted velocities avoid overflowing Kw*W before dividing by K.
			const double Target = (Kw / K) * W + (P.CurrentResponseKgPerS / K) * U + (Axis == 0 ? ForceN / K : 0);
			const double NewV = V * (1.0 - Alpha) + Target * Alpha;
			// phi=(1-exp(-x))/x; series preserves small response increments and displacement.
			const double Phi = X < 1.e-5 ? 1.0 - X/2.0 + X*X/6.0 - X*X*X/24.0 : Alpha / X;
			const double Distance = Dt * (V * Phi + Target * (1.0 - Phi));
			const double WindDV = (Kw / P.InertiaKg) * (W * Dt - Distance);
			const double CurrentDV = (P.CurrentResponseKgPerS / P.InertiaKg) * (U * Dt - Distance);
			const double DragDV = -(P.DragKgPerS / P.InertiaKg) * Distance;
			if (!FMath::IsFinite(NewV) || !FMath::IsFinite(Distance) || !FMath::IsFinite(WindDV) ||
				!FMath::IsFinite(CurrentDV) || !FMath::IsFinite(DragDV)) { return false; }
			Next.VelocityMps += Direction * NewV;
			DeltaPosition += Direction * Distance;
			Next.WindDeltaVelocityMps += Direction * WindDV;
			Next.CurrentDeltaVelocityMps += Direction * CurrentDV;
			Next.DragDeltaVelocityMps += Direction * DragDV;
		}
		Next.SpeedMps = std::hypot(Next.VelocityMps.X, Next.VelocityMps.Y);
		// A guard failure is reported by BoatPawn and disables motion. Never silently clamp bad tuning.
		if (!FMath::IsFinite(Next.SpeedMps) || Next.SpeedMps > SpeedLimit ||
			DeltaPosition.Size2D() / Dt > SpeedLimit) { return false; }
		Next.PositionM += DeltaPosition;
		return true;
	}
}
UTRBoatDriftComponent::UTRBoatDriftComponent() { PrimaryComponentTick.bCanEverTick = false; }

bool UTRBoatDriftComponent::InitializeMotion(const FTRBoatParameters& Settings, const FVector2D& InitialXYM,
	float HeadingRad, const FTROceanSample& Ocean, TArray<FText>& Errors)
{
	bInitialized = false;
	NavigationLateralResponsePerS=0;
	NavigationMaxSpeedMps = EngineForceN = NavigationYawRateRadPerS = 0;
	if (!Settings.Validate(Errors)) { return false; }
	if (!ValidOcean(Ocean) || (Settings.ModelRevision == 2 && !ValidWindCurrent(Ocean)) ||
		!FMath::IsFinite(InitialXYM.X) || !FMath::IsFinite(InitialXYM.Y) || !FMath::IsFinite(HeadingRad))
	{
		Errors.Add(FText::FromString(TEXT("Boat initialization requires a valid horizontal ocean, finite position and heading")));
		return false;
	}
	FTRBoatSnapshot Initial;
	Initial.Tick = Ocean.SampleTick;
	Initial.PositionM = FVector(InitialXYM.X, InitialXYM.Y, double(Ocean.SurfaceZ_M) + Settings.HullHeightOffsetM);
	Initial.HeadingRad = float(FMath::Fmod(double(HeadingRad), 2.0 * UE_DOUBLE_PI));
	Initial.ModelRevision = Settings.ModelRevision;
	Initial.ForwardVector = FVector(FMath::Cos(double(Initial.HeadingRad)), FMath::Sin(double(Initial.HeadingRad)), 0);
	Initial.WindMps = Settings.ModelRevision == 2 ? Ocean.WindMps : FVector2D::ZeroVector;
	Initial.SurfaceCurrentMps = Settings.ModelRevision == 2 ? Ocean.SurfaceCurrentMps : Ocean.CurrentMps;
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
	FTRBoatSnapshot Next = Snapshot;
	Next.Tick = Time.TickIndex;
	if (FrozenSettings.ModelRevision == 2)
	{
		if (NavigationYawRateRadPerS != 0)
		{
			Next.HeadingRad = float(FMath::UnwindRadians(double(Snapshot.HeadingRad) + NavigationYawRateRadPerS * Time.StepSeconds));
		}
		if (!FMath::IsFinite(Next.HeadingRad) || !FMath::IsFinite(EngineForceN)) { return false; }
		Next.ForwardVector = FVector(FMath::Cos(double(Next.HeadingRad)), FMath::Sin(double(Next.HeadingRad)), 0);
		const FTRBoatSnapshot IntegrationStart = Next;
		const double SpeedLimit = FMath::Max(FrozenSettings.MaxDriftSpeedMps, NavigationMaxSpeedMps);
		if (!FMath::IsFinite(NavigationLateralResponsePerS) || NavigationLateralResponsePerS<0 || NavigationLateralResponsePerS>20 ||
			!ValidWindCurrent(Ocean) || !IntegrateWindCurrent(FrozenSettings, Ocean, IntegrationStart, Time.StepSeconds, Next, EngineForceN, SpeedLimit, EngineForceN!=0 ? NavigationLateralResponsePerS : 0)) { return false; }
		Next.WindMps = Ocean.WindMps;
		Next.SurfaceCurrentMps = Ocean.SurfaceCurrentMps;
	}
	else
	{
	const double RateDt = FrozenSettings.VelocityResponsePerS * Time.StepSeconds;
	if (!FMath::IsFinite(RateDt)) { return false; }
	const double Alpha = -std::expm1(-RateDt);
	FVector Velocity = Snapshot.VelocityMps + (Ocean.CurrentMps * FrozenSettings.CurrentResponse01 - Snapshot.VelocityMps) * Alpha;
	const double Speed = std::hypot(Velocity.X, Velocity.Y);
	if (!FMath::IsFinite(Speed)) { return false; }
	if (Speed > FrozenSettings.MaxDriftSpeedMps) { Velocity *= FrozenSettings.MaxDriftSpeedMps / Speed; }
	Next.VelocityMps = Velocity;
	Next.PositionM += Velocity * Time.StepSeconds;
	Next.SpeedMps = Velocity.Size2D();
	Next.SurfaceCurrentMps = Ocean.CurrentMps;
	}
	Next.PositionM.Z = double(Ocean.SurfaceZ_M) + FrozenSettings.HullHeightOffsetM;
	Next.RodTipM = RodPosition(Next, FrozenSettings.RodAnchorOffsetM);
	if (!FiniteSnapshot(Next) || !IsDestinationValid(FVector2D(Next.PositionM.X, Next.PositionM.Y))) { return false; }
	if (!bInitialized || (GetOwner() && GetOwner()->IsActorBeingDestroyed())) { return false; }
	Snapshot = Next;
	LastIntegratedTick = Time.TickIndex;
	return true;
}

bool UTRBoatDriftComponent::ConfigureNavigation(double MaxSpeedMps)
{
 if(!bInitialized||FrozenSettings.ModelRevision!=2||!FMath::IsFinite(MaxSpeedMps)||MaxSpeedMps<=0||MaxSpeedMps>100){return false;}
 NavigationMaxSpeedMps=MaxSpeedMps;return true;
}
void UTRBoatDriftComponent::SetNavigationForces(double ForceN,double YawRateRadPerS,double LateralResponsePerS)
{
 EngineForceN=ForceN;NavigationYawRateRadPerS=YawRateRadPerS;NavigationLateralResponsePerS=LateralResponsePerS;
}

void UTRBoatDriftComponent::ResetDynamicVelocityForFishing()
{
 SetNavigationForces(0,0,0);
 Snapshot.VelocityMps=FVector::ZeroVector;Snapshot.SpeedMps=0;
 Snapshot.WindDeltaVelocityMps=Snapshot.CurrentDeltaVelocityMps=Snapshot.DragDeltaVelocityMps=FVector::ZeroVector;
 // Position, heading, registration, environment and integration clock remain intact.
}
