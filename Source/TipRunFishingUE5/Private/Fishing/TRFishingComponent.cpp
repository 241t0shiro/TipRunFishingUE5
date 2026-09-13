#include "Fishing/TRFishingComponent.h"

UTRFishingComponent::UTRFishingComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UTRFishingComponent::Prepare() { State = ETRFishingState::Ready; Snapshot = {}; Snapshot.FishingState = State; }
void UTRFishingComponent::BeginCast(FTRCastId Id, int64 Tick, const FTREquipmentSnapshot& Equipment)
{
	CastEquipment = Equipment;
	State = ETRFishingState::Deploying;
	Snapshot = {};
	Snapshot.CastId = Id; Snapshot.Tick = Tick; Snapshot.FishingState = State;
}
bool UTRFishingComponent::CompleteDeployment(const FTRBoatSnapshot& Boat, const FTROceanSample& Ocean, int64 Tick)
{
	if (State != ETRFishingState::Deploying || !Ocean.bValid || Ocean.SampleTick != Tick || Boat.Tick != Tick ||
		Boat.RodTipM.ContainsNaN() || !FMath::IsFinite(Ocean.SurfaceZ_M)) { return false; }
	const double HeightM = Boat.RodTipM.Z - double(Ocean.SurfaceZ_M);
	const double LineM = FMath::Max(double(CastEquipment.Parameters.MinLineM), HeightM);
	if (!FMath::IsFinite(LineM) || HeightM < 0.0 || LineM > CastEquipment.Parameters.MaxLineLengthM) { return false; }
	Snapshot.PositionXYM = FVector2D(Boat.RodTipM.X, Boat.RodTipM.Y);
	Snapshot.DepthM = 0.0f;
	Snapshot.LineLengthM = float(LineM);
	Snapshot.Tick = Tick;
	State = ETRFishingState::FreeFall;
	Snapshot.FishingState = State;
	return true;
}
void UTRFishingComponent::FinishCast() { State = ETRFishingState::Result; Snapshot.FishingState = State; }
void UTRFishingComponent::Stop() { State = ETRFishingState::Inactive; Snapshot = {}; CastEquipment = {}; }
FTREgiAction UTRFishingComponent::GetAction() const
{
	FTREgiAction Action;
	Action.FishingState = State;
	if (State == ETRFishingState::FreeFall)
	{
		Action.LineMode = ETRLineMode::Payout;
		Action.SinkScale = CastEquipment.Parameters.FreeFallSinkScale;
	}
	return Action;
}
