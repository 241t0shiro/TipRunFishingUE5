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
  State.ActualRetrieveM=0;State.SlackConsumedM=State.TautRetrieveAppliedM=0;
  // Sequence totals are nominal booked demand; any unissued/cancelled portion
  // remains unrealized. Egi reel totals separately measure issued tick demand.
  State.UnrealizedRetrieveM=State.RequestedRetrieveM;
  State.TotalUnrealizedRetrieveM=State.TotalRequestedRetrieveM-State.TotalActualRetrieveM;
 }
 const int64 Elapsed=Time.TickIndex-StartTick;
 if(!bRunning || Elapsed<0 || Elapsed>=UpTicks+RecoverTicks){return;}
 const bool Up=Elapsed<UpTicks;State.Phase=Up?ETRShakuriPhase::Up:ETRShakuriPhase::Recover;
 const double Fraction=Up?Frozen.UpDemandFraction01:1-Frozen.UpDemandFraction01;
 const double Nominal=State.RequestedRetrieveM*Fraction/double(Up?UpTicks:RecoverTicks);
 // Allocation is per action, including demand which geometry cannot fulfill.
 // Unfulfilled demand is not carried/retried: unrealized demand is observed, never a forced spool.
 State.TickDemandM=FMath::Min(Nominal,FMath::Max(0.,State.RequestedRetrieveM-AllocatedM));
 AllocatedM+=State.TickDemandM;
}
bool UTRShakuriSequenceComponent::RecordActual(const FTRReelSnapshot& Reel)
{
 const double ActualM=Reel.ActualRetrieveM;
 if(!FMath::IsFinite(ActualM) || !FMath::IsFinite(Reel.SlackConsumedM) || !FMath::IsFinite(Reel.TautRetrieveAppliedM) ||
  !FMath::IsFinite(Reel.UnrealizedRetrieveM) || Reel.SlackConsumedM<0 || Reel.TautRetrieveAppliedM<0 || Reel.UnrealizedRetrieveM<0 ||
  Reel.Tick!=State.Tick || Reel.Source!=ETRReelSource::Shakuri ||
  !FMath::IsNearlyEqual(Reel.RequestedRetrieveM,State.TickDemandM,1.e-9) ||
  !FMath::IsNearlyEqual(ActualM,Reel.SlackConsumedM+Reel.TautRetrieveAppliedM,1.e-9) ||
  !FMath::IsNearlyEqual(Reel.RequestedRetrieveM,ActualM+Reel.UnrealizedRetrieveM,1.e-9) ||
  ActualM<0 || ActualM>State.TickDemandM+1.e-9 ||
  State.ActualRetrieveM+ActualM>State.RequestedRetrieveM+1.e-9 ||
  State.TotalActualRetrieveM+ActualM>State.TotalRequestedRetrieveM+1.e-9){return false;}
 State.ActualRetrieveM+=ActualM;State.TotalActualRetrieveM+=ActualM;
 State.SlackConsumedM+=Reel.SlackConsumedM;State.TotalSlackConsumedM+=Reel.SlackConsumedM;
 State.TautRetrieveAppliedM+=Reel.TautRetrieveAppliedM;State.TotalTautRetrieveAppliedM+=Reel.TautRetrieveAppliedM;
 State.UnrealizedRetrieveM=FMath::Max(0.,State.RequestedRetrieveM-State.ActualRetrieveM);
 State.TotalUnrealizedRetrieveM=FMath::Max(0.,State.TotalRequestedRetrieveM-State.TotalActualRetrieveM);
 return true;
}
void UTRShakuriSequenceComponent::Stop()
{
 if(bRunning){State.Phase=ETRShakuriPhase::Interrupted;}bRunning=false;
 State.TickDemandM=0;State.QueuedCount=0;State.bPendingRetrieve=State.bPendingReFall=false;
}
void UTRShakuriSequenceComponent::Reset(){Frozen={};State={};UpTicks=RecoverTicks=0;StartTick=-1;AllocatedM=0;bRunning=false;}
