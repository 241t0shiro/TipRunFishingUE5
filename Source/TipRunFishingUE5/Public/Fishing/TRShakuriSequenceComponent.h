#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TRRodTuningDataAsset.h"
#include "Data/TRSnapshots.h"
#include "Data/TRSimulationTypes.h"
#include "Data/TRShakuriSequenceTypes.h"
#include "TRShakuriSequenceComponent.generated.h"

// Fixed-step sequence accounting; Fishing owns command queue/state, Rod owns pose,
// Egi owns physical shortening. No device, camera, actor or UI Tick ownership here.
UCLASS()
class TIPRUNFISHINGUE5_API UTRShakuriSequenceComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UTRShakuriSequenceComponent();
 bool Configure(const FTRRodParameters& Parameters,double StepSeconds);
 bool IsEnabled() const { return State.bEnabled; }
 void BeginCast(FTRCastId CastId);
 void Advance(const FTRSimTime& Time,const FTREgiSnapshot& Fishing,bool PendingRetrieve,bool PendingFall);
 bool RecordActual(double ActualM);
 void Stop();
 void Reset();
 FTRShakuriSequenceSnapshot GetSnapshot() const { return State; }
private:
 FTRShakuriSequenceParameters Frozen;
 FTRShakuriSequenceSnapshot State;
 int64 UpTicks=0,RecoverTicks=0,StartTick=-1;
 double AllocatedM=0;
 bool bRunning=false;
};
