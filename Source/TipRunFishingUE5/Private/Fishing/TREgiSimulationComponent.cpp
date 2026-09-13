#include "Fishing/TREgiSimulationComponent.h"
#include "GameFramework/Actor.h"

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
		!FMath::IsFinite(Initial.LineLengthM) || Initial.LineLengthM < 0.0f ||
		!FMath::IsFinite(Equipment.BaseMassG) || Equipment.BaseMassG <= 0.0f ||
		!FMath::IsFinite(Equipment.SinkerMassG) || Equipment.SinkerMassG < 0.0f ||
		!FMath::IsFinite(Equipment.TotalMassG) || Equipment.TotalMassG != Equipment.BaseMassG + Equipment.SinkerMassG ||
		!FMath::IsFinite(Equipment.SinkSpeedMps) || Equipment.SinkSpeedMps <= 0.0f ||
		!Equipment.Parameters.Validate(Errors))
	{
		Errors.Add(FText::FromString(TEXT("Egi initialization requires a new CastId, valid ocean/initial coordinates and finite equipment coefficients")));
		return false;
	}
	Snapshot = Initial;
	Snapshot.VelocityMps = FVector::ZeroVector;
	FrozenEquipment = Equipment;
	HighestCastValue = Initial.CastId.Value;
	bActive = true;
	// Contact is reported on the first step even if initialized exactly on the seabed.
	bOnBottom = false;
	return true;
}
ETREgiStepEvent UTREgiSimulationComponent::StepEgi(FTRCastId ExpectedCastId, const FTRSimTime& Time,
	const FTROceanSample& Ocean, const FTRBoatSnapshot& Boat, const FTREgiAction& Action)
{
	if (!bActive || ExpectedCastId != Snapshot.CastId || Time.TickIndex <= Snapshot.Tick ||
		(GetOwner() && GetOwner()->IsActorBeingDestroyed())) { return ETREgiStepEvent::None; }
	if (!Time.IsValid() || !ValidEgiOcean(Ocean) || Ocean.SampleTick != Time.TickIndex || Boat.Tick != Time.TickIndex ||
		Boat.RodTipM.ContainsNaN() || Boat.PositionM.ContainsNaN() ||
		(Action.FishingState != ETRFishingState::FreeFall && Action.FishingState != ETRFishingState::BottomContact) ||
		(Action.FishingState == ETRFishingState::FreeFall && (!FMath::IsFinite(Action.SinkScale) || Action.SinkScale <= 0.0f)))
	{
		return ETREgiStepEvent::EnvironmentInvalid;
	}
	// M07: vertical fall only. Horizontal current, boat following and line dynamics are M08.
	const double SpeedMps = Action.FishingState == ETRFishingState::FreeFall ?
		FMath::Min(double(FrozenEquipment.SinkSpeedMps) * double(Action.SinkScale), double(FrozenEquipment.Parameters.MaxEgiSpeedMps)) : 0.0;
	const double DistanceM = SpeedMps * Time.StepSeconds;
	if (!FMath::IsFinite(DistanceM)) { return ETREgiStepEvent::EnvironmentInvalid; }
	const double TrialDepthM = double(Snapshot.DepthM) + DistanceM;
	const float DepthM = float(FMath::Clamp(TrialDepthM, 0.0, double(Ocean.BottomDepthM)));
	const double VelocityZMps = -(double(DepthM) - double(Snapshot.DepthM)) / Time.StepSeconds;
	if (!FMath::IsFinite(VelocityZMps)) { return ETREgiStepEvent::EnvironmentInvalid; }
	const bool bReachedBottom = DepthM >= Ocean.BottomDepthM;
	const bool bFirstContact = bReachedBottom && !bOnBottom;
	Snapshot.DepthM = DepthM;
	Snapshot.VelocityMps = FVector(0.0, 0.0, VelocityZMps);
	Snapshot.Tick = Time.TickIndex;
	bOnBottom = bReachedBottom;
	return bFirstContact ? ETREgiStepEvent::ReachedBottom : ETREgiStepEvent::None;
}
FTREgiSnapshot UTREgiSimulationComponent::BuildSnapshot(ETRFishingState FishingState) const
{
	FTREgiSnapshot Copy = Snapshot;
	Copy.FishingState = FishingState;
	return Copy;
}
void UTREgiSimulationComponent::Reset() { bActive = false; bOnBottom = false; Snapshot = {}; FrozenEquipment = {}; }
