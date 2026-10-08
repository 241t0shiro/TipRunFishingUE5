#include "Fishing/TRShakuriSequenceComponent.h"

UTRShakuriSequenceComponent::UTRShakuriSequenceComponent(){PrimaryComponentTick.bCanEverTick=false;}
bool UTRShakuriSequenceComponent::Configure(const FTRRodParameters& P,double StepSeconds)
{
 Reset();
 if(!P.Sequence.Validate() || !TRTime::TrySecondsToTicks(P.ShakuriUpSeconds,StepSeconds,UpTicks) ||
  !TRTime::TrySecondsToTicks(P.ShakuriReturnSeconds,StepSeconds,RecoverTicks) || UpTicks<=0 || RecoverTicks<=0 || UpTicks>MAX_int64-RecoverTicks){return false;}
 Frozen=P.Sequence;State.bEnabled=Frozen.bEnabled;return true;
}
void UTRShakuriSequenceComponent::BeginCast(FTRCastId CastId)
{
 const bool Enabled=State.bEnabled;State={};State.bEnabled=Enabled;State.CastId=CastId;
 StartTick=-1;AllocatedM=0;bRunning=false;
}
void UTRShakuriSequenceComponent::Advance(const FTRSimTime& Time,const FTREgiSnapshot& Fishing,bool PendingRetrieve,bool PendingFall)
{
 if(!IsEnabled() || Fishing.CastId!=State.CastId || Time.TickIndex<=State.Tick){return;}
 State.Tick=Time.TickIndex;State.TickDemandM=0;State.QueuedCount=Fishing.PendingJerkCount;
 State.bPendingRetrieve=PendingRetrieve;State.bPendingReFall=PendingFall;
 if(bRunning && Time.TickIndex-StartTick>=UpTicks+RecoverTicks)
 {++State.CompletedCount;bRunning=false;State.Phase=ETRShakuriPhase::Complete;}
 if(Fishing.FishingState!=ETRFishingState::Jerking)
 {if(bRunning){Stop();}return;}
 if(Fishing.JerkCount!=State.SequenceIndex)
 {
  State.SequenceIndex=Fishing.JerkCount;StartTick=Fishing.StateEnteredTick;bRunning=true;AllocatedM=0;
  State.RequestedHandleTurns=Frozen.HandleTurnsPerShakuri;
  State.RequestedRetrieveM=Frozen.NominalRetrievePerHandleTurnM*Frozen.HandleTurnsPerShakuri;
  State.TotalRequestedHandleTurns+=State.RequestedHandleTurns;State.TotalRequestedRetrieveM+=State.RequestedRetrieveM;
  State.ActualRetrieveM=0;
 }
 const int64 Elapsed=Time.TickIndex-StartTick;
 if(!bRunning || Elapsed<0 || Elapsed>=UpTicks+RecoverTicks){return;}
 const bool Up=Elapsed<UpTicks;State.Phase=Up?ETRShakuriPhase::Up:ETRShakuriPhase::Recover;
 const double Fraction=Up?Frozen.UpDemandFraction01:1-Frozen.UpDemandFraction01;
 const double Nominal=State.RequestedRetrieveM*Fraction/double(Up?UpTicks:RecoverTicks);
 // Allocation is per action, including demand which geometry cannot fulfill.
 // Unfulfilled demand is not carried/retried: that is R6 policy, not a forced spool.
 State.TickDemandM=FMath::Min(Nominal,FMath::Max(0.,State.RequestedRetrieveM-AllocatedM));
 AllocatedM+=State.TickDemandM;
}
bool UTRShakuriSequenceComponent::RecordActual(double ActualM)
{
 if(!FMath::IsFinite(ActualM) || ActualM<0 || ActualM>State.TickDemandM+1.e-9 ||
  State.ActualRetrieveM+ActualM>State.RequestedRetrieveM+1.e-9 ||
  State.TotalActualRetrieveM+ActualM>State.TotalRequestedRetrieveM+1.e-9){return false;}
 State.ActualRetrieveM+=ActualM;State.TotalActualRetrieveM+=ActualM;return true;
}
void UTRShakuriSequenceComponent::Stop()
{
 if(bRunning){State.Phase=ETRShakuriPhase::Interrupted;}bRunning=false;
 State.TickDemandM=0;State.QueuedCount=0;State.bPendingRetrieve=State.bPendingReFall=false;
}
void UTRShakuriSequenceComponent::Reset(){Frozen={};State={};UpTicks=RecoverTicks=0;StartTick=-1;AllocatedM=0;bRunning=false;}
