#include "Game/TRPlayerModeComponent.h"

UTRPlayerModeComponent::UTRPlayerModeComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UTRPlayerModeComponent::Start()
{
 const int64 NextEpoch = State.ModeEpoch + 1;
 State = {};
 State.bValid = true;
 State.ModeEpoch = NextEpoch;
}
void UTRPlayerModeComponent::Stop()
{
 State.bValid = false;
 State.bNavigationInputAllowed = false;
 State.bStopNavigationThrustRequested = true;
 State.bHoldBoatHeading = true;
}
void UTRPlayerModeComponent::Transition(ETRPlayerMode Target, int64 Tick)
{
 State.Mode = Target;
 ++State.ModeEpoch;
 State.ChangedAtTick = Tick;
 State.LastRejectedReason = ETRModeChangeRejection::None;
}

