#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TRPlayerModeTypes.h"
#include "TRPlayerModeComponent.generated.h"

class ATRFishingSessionActor;
// Session owns this component; only its fixed command handler can transition it.
UCLASS()
class TIPRUNFISHINGUE5_API UTRPlayerModeComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UTRPlayerModeComponent();
 FTRPlayerModeSnapshot GetSnapshot() const { return State; }
private:
 friend class ATRFishingSessionActor;
 void Start();
 void Stop();
 void Transition(ETRPlayerMode Target, int64 Tick);
 void Reject(ETRModeChangeRejection Reason) { State.LastRejectedReason = Reason; }
 FTRPlayerModeSnapshot State;
};

