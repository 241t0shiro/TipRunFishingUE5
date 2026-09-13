#include "Fishing/TREgiSimulationComponent.h"
#include "GameFramework/Actor.h"
#include <cmath>

namespace
{
	bool ValidEgiOcean(const FTROceanSample& Ocean)
	{
		return Ocean.bValid && Ocean.InvalidReason == ETRSampleError::None && Ocean.SampleTick >= 0 &&
			FMath::IsFinite(Ocean.SurfaceZ_M) && FMath::IsFinite(Ocean.BottomDepthM) && Ocean.BottomDepthM > 0.0f &&
			!Ocean.CurrentMps.ContainsNaN() && Ocean.CurrentMps.Z == 0.0;
	}
}
UTREgiSimulationComponent::UTREgiSimulationComponent() { PrimaryComponentTick.bCanEverTick = false; }
bool UTREgiSimulationComponent::InitializeCast(const FTREgiSnapshot& Initial, const FTREquipmentSnapshot& Equipment,
	const FTROceanSample& Ocean, TArray<FText>& Errors)
{
	if ((GetOwner() && GetOwner()->IsActorBeingDestroyed()) || !Initial.CastId.IsValid() || Initial.CastId.Value <= HighestCastValue ||
		Initial.Tick < 0 || !ValidEgiOcean(Ocean) || Initial.Tick != Ocean.SampleTick ||
		!FMath::IsFinite(Initial.PositionXYM.X) || !FMath::IsFinite(Initial.PositionXYM.Y) ||
		!FMath::IsFinite(Initial.DepthM) || Initial.DepthM < 0.0f || Initial.DepthM > Ocean.BottomDepthM ||
		!FMath::IsFinite(Initial.LineLengthM) || Initial.LineLengthM < Equipment.Parameters.MinLineM || Initial.LineLengthM > Equipment.Parameters.MaxLineLengthM ||
		!FMath::IsFinite(Equipment.BaseMassG) || Equipment.BaseMassG <= 0.0f ||
		!FMath::IsFinite(Equipment.SinkerMassG) || Equipment.SinkerMassG < 0.0f ||
		!FMath::IsFinite(Equipment.TotalMassG) || Equipment.TotalMassG != Equipment.BaseMassG + Equipment.SinkerMassG ||
		!FMath::IsFinite(Equipment.SinkSpeedMps) || Equipment.SinkSpeedMps <= 0.0f ||
		!FMath::IsFinite(Equipment.HorizontalResponsePerS) || Equipment.HorizontalResponsePerS <= 0.0f ||
		!Equipment.Parameters.Validate(Errors))
	{
		Errors.Add(FText::FromString(TEXT("Egi initialization requires a new CastId, valid ocean/initial coordinates and finite equipment coefficients")));
		return false;
	}
	Snapshot = Initial;
	Snapshot.VelocityMps = FVector::ZeroVector;
	FrozenEquipment = Equipment;
	LastSurfaceZ_M = Ocean.SurfaceZ_M;
	HighestCastValue = Initial.CastId.Value;
	bActive = true;
	// Contact is reported on the first step even if initialized exactly on the seabed.
	bOnBottom = false;
	return true;
}
ETREgiStepEvent UTREgiSimulationComponent::StepEgi(FTRCastId ExpectedCastId, const FTRSimTime& Time,
	const FTROceanSample& Ocean, const FTRBoatSnapshot& Boat, const FTREgiAction& Action,
	TFunctionRef<FTROceanSample(const FTROceanQuery&)> SampleDestination)
{
	if (!bActive || ExpectedCastId != Snapshot.CastId || Time.TickIndex <= Snapshot.Tick ||
		(GetOwner() && GetOwner()->IsActorBeingDestroyed())) { return ETREgiStepEvent::None; }
	const bool bSupportedState = Action.FishingState == ETRFishingState::FreeFall ||
		Action.FishingState == ETRFishingState::BottomContact || Action.FishingState == ETRFishingState::TensionFall ||
		Action.FishingState == ETRFishingState::Stay;
	const bool bSupportedLine = Action.LineMode == ETRLineMode::Payout ||
		Action.LineMode == ETRLineMode::ControlledPayout || Action.LineMode == ETRLineMode::Locked;
	if (!Time.IsValid() || !ValidEgiOcean(Ocean) || Ocean.SampleTick != Time.TickIndex || Boat.Tick != Time.TickIndex ||
		Boat.RodTipM.ContainsNaN() || Boat.PositionM.ContainsNaN() || Boat.VelocityMps.ContainsNaN() ||
		!FMath::IsFinite(Boat.HeadingRad) || !bSupportedState || !bSupportedLine ||
		!FMath::IsFinite(Action.SinkScale) || Action.SinkScale <= 0.0f || Action.LiftMps != 0.0f || Action.ReelMps != 0.0f)
	{
		return ETREgiStepEvent::EnvironmentInvalid;
	}
	const FTRFishingParameters& P = FrozenEquipment.Parameters;
	const double RateDt = double(FrozenEquipment.HorizontalResponsePerS) * Time.StepSeconds;
	if (!FMath::IsFinite(RateDt)) { return ETREgiStepEvent::EnvironmentInvalid; }
	// Boat velocity is not added to current. Its moving rod tip supplies the pull constraint.
	const double Alpha = -std::expm1(-RateDt);
	FVector Velocity = Snapshot.VelocityMps;
	Velocity.X += (Ocean.CurrentMps.X - Velocity.X) * Alpha;
	Velocity.Y += (Ocean.CurrentMps.Y - Velocity.Y) * Alpha;
	Velocity.Z = -double(FrozenEquipment.SinkSpeedMps) * Action.SinkScale;
	const double Speed = std::hypot(Velocity.X, Velocity.Y, Velocity.Z);
	if (!FMath::IsFinite(Speed)) { return ETREgiStepEvent::EnvironmentInvalid; }
	if (Speed > P.MaxEgiSpeedMps) { Velocity *= double(P.MaxEgiSpeedMps) / Speed; }
	const FVector Previous(Snapshot.PositionXYM.X, Snapshot.PositionXYM.Y, double(LastSurfaceZ_M) - Snapshot.DepthM);
	FVector Candidate(Snapshot.PositionXYM.X, Snapshot.PositionXYM.Y, double(Ocean.SurfaceZ_M) - Snapshot.DepthM);
	Candidate += Velocity * Time.StepSeconds;
	const double PayoutMps = Action.LineMode == ETRLineMode::Payout ? P.PayoutMps :
		(Action.LineMode == ETRLineMode::ControlledPayout ? P.TensionPayoutMps : 0.0);
	const double TrialLineM = double(Snapshot.LineLengthM) + PayoutMps * Time.StepSeconds;
	if (Candidate.ContainsNaN() || !FMath::IsFinite(TrialLineM)) { return ETREgiStepEvent::EnvironmentInvalid; }
	const float LineM = float(FMath::Clamp(TrialLineM, double(P.MinLineM), double(P.MaxLineLengthM)));
	// Numerical convergence limits, not balance coefficients. Never commit a partial solution.
	constexpr int32 MaxConstraintIterations = 4;
	constexpr double ConstraintToleranceM = 1.e-5;
	double CorrectionM = 0.0;
	FTROceanSample Destination;
	float DepthM = Snapshot.DepthM;
	bool bConverged = false;
	for (int32 Iteration = 0; Iteration < MaxConstraintIterations; ++Iteration)
	{
		const FVector Offset = Candidate - Boat.RodTipM;
		const double DistanceM = std::hypot(Offset.X, Offset.Y, Offset.Z);
		if (!FMath::IsFinite(DistanceM)) { return ETREgiStepEvent::EnvironmentInvalid; }
		if (DistanceM > LineM)
		{
			CorrectionM += DistanceM - LineM;
			Candidate = Boat.RodTipM + Offset * (double(LineM) / DistanceM);
		}
		FTROceanQuery Query;
		Query.PositionXYM = FVector2D(Candidate.X, Candidate.Y);
		Query.DepthM = float(FMath::Clamp(double(Ocean.SurfaceZ_M) - Candidate.Z, 0.0, double(MAX_flt)));
		Query.SimTick = Time.TickIndex;
		Destination = SampleDestination(Query);
		if (!ValidEgiOcean(Destination) || Destination.SampleTick != Time.TickIndex ||
			Boat.RodTipM.Z < Destination.SurfaceZ_M || Boat.RodTipM.Z - Destination.SurfaceZ_M > LineM + ConstraintToleranceM)
		{
			return ETREgiStepEvent::EnvironmentInvalid;
		}
		DepthM = float(FMath::Clamp(double(Destination.SurfaceZ_M) - Candidate.Z, 0.0, double(Destination.BottomDepthM)));
		Candidate.Z = double(Destination.SurfaceZ_M) - DepthM;
		const FVector FinalOffset = Candidate - Boat.RodTipM;
		const double FinalDistanceM = std::hypot(FinalOffset.X, FinalOffset.Y, FinalOffset.Z);
		if (FMath::IsFinite(FinalDistanceM) && FinalDistanceM <= double(LineM) + ConstraintToleranceM)
		{
			bConverged = true; break;
		}
	}
	if (!bConverged || !FMath::IsFinite(CorrectionM) || TRUnits::MetersToCentimeters(Candidate).ContainsNaN())
	{
		return ETREgiStepEvent::EnvironmentInvalid;
	}
	FVector CorrectedVelocity = (Candidate - Previous) / Time.StepSeconds;
	const double CorrectedSpeed = std::hypot(CorrectedVelocity.X, CorrectedVelocity.Y, CorrectedVelocity.Z);
	if (!FMath::IsFinite(CorrectedSpeed)) { return ETREgiStepEvent::EnvironmentInvalid; }
	if (CorrectedSpeed > P.MaxEgiSpeedMps) { CorrectedVelocity *= double(P.MaxEgiSpeedMps) / CorrectedSpeed; }
	// A destination callback may end a cast; it must not resurrect it on return.
	if (!bActive || Snapshot.CastId != ExpectedCastId || (GetOwner() && GetOwner()->IsActorBeingDestroyed()))
	{
		return ETREgiStepEvent::None;
	}
	const bool bReachedBottom = DepthM >= Destination.BottomDepthM;
	const ETREgiStepEvent Event = bReachedBottom && !bOnBottom ? ETREgiStepEvent::ReachedBottom :
		(!bReachedBottom && bOnBottom ? ETREgiStepEvent::LeftBottom : ETREgiStepEvent::None);
	Snapshot.PositionXYM = FVector2D(Candidate.X, Candidate.Y);
	Snapshot.DepthM = DepthM;
	Snapshot.VelocityMps = CorrectedVelocity;
	Snapshot.LineLengthM = LineM;
	Snapshot.LineAngleRad = float(FMath::Atan2(std::hypot(Candidate.X - Boat.RodTipM.X, Candidate.Y - Boat.RodTipM.Y),
		FMath::Max(Boat.RodTipM.Z - Candidate.Z, UE_DOUBLE_SMALL_NUMBER)));
	Snapshot.Tension01 = float(FMath::Clamp(CorrectionM / double(P.TensionReferenceM), 0.0, 1.0));
	Snapshot.Tick = Time.TickIndex;
	LastSurfaceZ_M = Destination.SurfaceZ_M;
	bOnBottom = bReachedBottom;
	return Event;
}
FTREgiSnapshot UTREgiSimulationComponent::BuildSnapshot(ETRFishingState FishingState) const
{
	FTREgiSnapshot Copy = Snapshot;
	Copy.FishingState = FishingState;
	return Copy;
}
void UTREgiSimulationComponent::Reset()
{
	bActive = false; bOnBottom = false; Snapshot = {}; FrozenEquipment = {}; LastSurfaceZ_M = 0.0f;
}