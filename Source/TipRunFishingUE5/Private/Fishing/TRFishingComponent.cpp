#include "Fishing/TRFishingComponent.h"

UTRFishingComponent::UTRFishingComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UTRFishingComponent::Prepare()
{
	PendingTransitions.Empty(); State = ETRFishingState::Ready; Snapshot = {}; Snapshot.FishingState = State;
	JerkCount = SeriesJerkCount = PendingJerkCount = StayPenaltyJerkCount = StateEnteredTick = 0;
	RangeObservationSeconds = 0.0; bSeriesClosed = bReeling = false;
}
void UTRFishingComponent::BeginCast(FTRCastId Id, const FTRSimTime& Time, const FTREquipmentSnapshot& Equipment)
{
	Prepare(); CastEquipment = Equipment; StepSeconds = Time.StepSeconds;
	TRTime::TrySecondsToTicks(Equipment.Parameters.JerkDurationS, StepSeconds, JerkTicks);
	Snapshot.CastId = Id; Snapshot.Tick = Time.TickIndex;
	TransitionTo(ETRFishingState::Deploying, Time.TickIndex);
}
void UTRFishingComponent::TransitionTo(ETRFishingState Next, int64 Tick)
{
	if (Next == State) { return; }
	PendingTransitions.Emplace(State, Next); State = Next; StateEnteredTick = Tick;
	Snapshot.FishingState = State;
	if (Next == ETRFishingState::Stay && !bSeriesClosed)
	{
		StayPenaltyJerkCount = SeriesJerkCount; bSeriesClosed = true;
	}
	if (Next != ETRFishingState::Stay && Next != ETRFishingState::TensionFall) { RangeObservationSeconds = 0.0; }
}
void UTRFishingComponent::BeginJerk(int64 Tick)
{
	if (bSeriesClosed) { SeriesJerkCount = 0; bSeriesClosed = false; }
	// Saturation prevents signed overflow; it never blocks a gameplay action.
	if (JerkCount < MAX_int64) { ++JerkCount; }
	if (SeriesJerkCount < MAX_int64) { ++SeriesJerkCount; }
	TransitionTo(ETRFishingState::Jerking, Tick); StateEnteredTick = Tick;
}
ETRCommandResult UTRFishingComponent::HandleCommand(const FTRFishingCommand& Command, const FTRSimTime& Time)
{
	const bool bFishing = State == ETRFishingState::FreeFall || State == ETRFishingState::BottomContact ||
		State == ETRFishingState::Jerking || State == ETRFishingState::TensionFall || State == ETRFishingState::Stay;
	if (Command.Type == ETRFishingCommandType::RetrieveStopped && State == ETRFishingState::Retrieving)
	{
		bReeling = false; return ETRCommandResult::Accepted;
	}
	if (Command.Type == ETRFishingCommandType::RetrieveStarted && (bFishing || State == ETRFishingState::Retrieving))
	{
		PendingJerkCount = 0; bReeling = true; TransitionTo(ETRFishingState::Retrieving, Time.TickIndex); return ETRCommandResult::Accepted;
	}
	if (!bFishing) { return ETRCommandResult::RejectedInvalidState; }
	if (Command.Type == ETRFishingCommandType::Jerk)
	{
		if (State == ETRFishingState::Jerking)
		{
			if (PendingJerkCount == MAX_int64) { return ETRCommandResult::RejectedBusy; } // representational exhaustion only
			++PendingJerkCount;
		}
		else { BeginJerk(Time.TickIndex); }
		return ETRCommandResult::Accepted;
	}
	if (Command.Type == ETRFishingCommandType::Fall)
	{
		PendingJerkCount = 0; TransitionTo(ETRFishingState::FreeFall, Time.TickIndex); return ETRCommandResult::Accepted;
	}
	if (Command.Type == ETRFishingCommandType::TensionFall && (State == ETRFishingState::FreeFall || State == ETRFishingState::BottomContact))
	{
		TransitionTo(ETRFishingState::TensionFall, Time.TickIndex); return ETRCommandResult::Accepted;
	}
	return ETRCommandResult::RejectedInvalidState; // Hook/Fight/AI belong to later milestones.
}
void UTRFishingComponent::PrepareStep(const FTRSimTime& Time)
{
	if (State == ETRFishingState::Jerking && Time.TickIndex - StateEnteredTick >= JerkTicks)
	{
		TransitionTo(ETRFishingState::TensionFall, Time.TickIndex);
	}
	if (State == ETRFishingState::TensionFall && PendingJerkCount > 0)
	{
		--PendingJerkCount; BeginJerk(Time.TickIndex);
	}
}
bool UTRFishingComponent::CompleteDeployment(const FTRBoatSnapshot& Boat, const FTROceanSample& Ocean, int64 Tick)
{
	if (State != ETRFishingState::Deploying || !Ocean.bValid || Ocean.SampleTick != Tick || Boat.Tick != Tick ||
		Boat.RodTipM.ContainsNaN() || !FMath::IsFinite(Ocean.SurfaceZ_M)) { return false; }
	const double HeightM = Boat.RodTipM.Z - double(Ocean.SurfaceZ_M);
	const double LineM = FMath::Max(double(CastEquipment.Parameters.MinLineM), HeightM);
	if (!FMath::IsFinite(LineM) || HeightM < 0.0 || LineM > CastEquipment.Parameters.MaxLineLengthM) { return false; }
	Snapshot.PositionXYM = FVector2D(Boat.RodTipM.X, Boat.RodTipM.Y);
	Snapshot.DepthM = 0.0f; Snapshot.LineLengthM = float(LineM); Snapshot.Tick = Tick;
	TransitionTo(ETRFishingState::FreeFall, Tick); return true;
}
void UTRFishingComponent::FinishCast()
{
	TransitionTo(ETRFishingState::Result, Snapshot.Tick);
	PendingJerkCount = JerkCount = SeriesJerkCount = StayPenaltyJerkCount = 0; bReeling = false;
}
void UTRFishingComponent::Stop() { Prepare(); State = ETRFishingState::Inactive; Snapshot = {}; CastEquipment = {}; }
void UTRFishingComponent::ApplyEgiStep(const FTREgiSnapshot& Updated, ETREgiStepEvent Event, bool bTransientComplete)
{
	if (Updated.CastId != Snapshot.CastId || Updated.Tick <= Snapshot.Tick) { return; }
	Snapshot = Updated;
	if (State == ETRFishingState::TensionFall || State == ETRFishingState::Stay) { RangeObservationSeconds += StepSeconds; }
	if (Updated.bBottomContact && (State == ETRFishingState::FreeFall || State == ETRFishingState::TensionFall || State == ETRFishingState::Stay))
	{
		TransitionTo(ETRFishingState::BottomContact, Updated.Tick);
	}
	else if (Event == ETREgiStepEvent::LeftBottom && State == ETRFishingState::BottomContact)
	{
		TransitionTo(ETRFishingState::TensionFall, Updated.Tick);
	}
	else if (State == ETRFishingState::TensionFall && bTransientComplete && PendingJerkCount == 0)
	{
		TransitionTo(ETRFishingState::Stay, Updated.Tick);
	}
	Snapshot.FishingState = State;
}
void UTRFishingComponent::PublishStateChanges()
{
	auto Notifications = MoveTemp(PendingTransitions); PendingTransitions.Empty();
	for (const auto& Pair : Notifications) { OnFishingStateChanged.Broadcast(Pair.Key, Pair.Value); }
}
FTREgiSnapshot UTRFishingComponent::GetSnapshot() const
{
	auto Copy = Snapshot; Copy.FishingState = State; Copy.JerkCount = JerkCount; Copy.SeriesJerkCount = SeriesJerkCount;
	Copy.PendingJerkCount = PendingJerkCount; Copy.StayPenaltyJerkCount = StayPenaltyJerkCount;
	Copy.StateEnteredTick = StateEnteredTick; Copy.RangeObservationSeconds = RangeObservationSeconds; return Copy;
}
FTREgiAction UTRFishingComponent::GetAction() const
{
	FTREgiAction Action; Action.FishingState = State;
	const auto& P = CastEquipment.Parameters;
	Action.SinkScale = P.TensionFallSinkScale;
	if (State == ETRFishingState::FreeFall) { Action.LineMode = ETRLineMode::Payout; Action.SinkScale = P.FreeFallSinkScale; }
	else if (State == ETRFishingState::TensionFall) { Action.LineMode = ETRLineMode::ControlledPayout; }
	else if (State == ETRFishingState::Stay) { Action.SinkScale = P.StaySinkScale; }
	else if (State == ETRFishingState::Jerking) { Action.LineMode = ETRLineMode::ReelIn; Action.LiftMps = P.JerkLiftMps; Action.ReelMps = P.JerkReelMps; }
	else if (State == ETRFishingState::Retrieving) { Action.LineMode = ETRLineMode::ReelIn; Action.ReelMps = bReeling ? P.ReelMps : 0.0f; }
	return Action;
}
