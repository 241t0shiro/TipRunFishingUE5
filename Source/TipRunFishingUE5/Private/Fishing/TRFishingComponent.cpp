#include "Fishing/TRFishingComponent.h"

UTRFishingComponent::UTRFishingComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UTRFishingComponent::Prepare() { bPendingStateNotification = false; State = ETRFishingState::Ready; Snapshot = {}; Snapshot.FishingState = State; }
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
void UTRFishingComponent::FinishCast() { bPendingStateNotification = false; State = ETRFishingState::Result; Snapshot.FishingState = State; }
void UTRFishingComponent::Stop() { bPendingStateNotification = false; State = ETRFishingState::Inactive; Snapshot = {}; CastEquipment = {}; }
void UTRFishingComponent::ApplyEgiStep(const FTREgiSnapshot& Updated, ETREgiStepEvent Event)
{
	if (Updated.CastId != Snapshot.CastId || Updated.Tick <= Snapshot.Tick ||
		(State != ETRFishingState::FreeFall && State != ETRFishingState::BottomContact && State != ETRFishingState::TensionFall)) { return; }
	if (Event == ETREgiStepEvent::ReachedBottom && State != ETRFishingState::BottomContact)
	{
		PendingStateFrom = State;
		State = ETRFishingState::BottomContact;
		bPendingStateNotification = true;
	}
	if (Event == ETREgiStepEvent::LeftBottom && State == ETRFishingState::BottomContact)
	{
		PendingStateFrom = State;
		State = GetAction().LineMode == ETRLineMode::Payout ? ETRFishingState::FreeFall : ETRFishingState::TensionFall;
		bPendingStateNotification = true;
	}
	Snapshot = Updated;
	Snapshot.FishingState = State;
}
void UTRFishingComponent::PublishStateChanges()
{
	if (!bPendingStateNotification) { return; }
	bPendingStateNotification = false;
	OnFishingStateChanged.Broadcast(PendingStateFrom, State);
}
FTREgiAction UTRFishingComponent::GetAction() const
{
	FTREgiAction Action;
	Action.FishingState = State;
	if (State == ETRFishingState::FreeFall)
	{
		Action.LineMode = ETRLineMode::Payout;
		Action.SinkScale = CastEquipment.Parameters.FreeFallSinkScale;
	}
	else if (State == ETRFishingState::BottomContact || State == ETRFishingState::TensionFall)
	{
		Action.LineMode = State == ETRFishingState::BottomContact ? ETRLineMode::Locked : ETRLineMode::ControlledPayout;
		Action.SinkScale = CastEquipment.Parameters.TensionFallSinkScale;
	}
	return Action;
}
