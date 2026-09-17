#include "Fishing/TRRodControlComponent.h"

UTRRodControlComponent::UTRRodControlComponent(){PrimaryComponentTick.bCanEverTick=false;}
bool UTRRodControlComponent::Initialize(const FTRRodParameters& P,double StepSeconds,TArray<FText>& Errors)
{
	if(bInitialized || !P.Validate(Errors) || !TRTime::TrySecondsToTicks(P.ShakuriUpSeconds,StepSeconds,UpTicks) ||
		!TRTime::TrySecondsToTicks(P.ShakuriReturnSeconds,StepSeconds,ReturnTicks) || UpTicks<=0 || ReturnTicks<=0 || UpTicks>MAX_int64-ReturnTicks)
	{return false;}
	ReelTicks=0;
	if(P.ShakuriReelSeconds>0 && (!TRTime::TrySecondsToTicks(P.ShakuriReelSeconds,StepSeconds,ReelTicks) || ReelTicks<=0 || ReelTicks>ReturnTicks)){return false;}
	Frozen=P; Snapshot={}; Snapshot.BasePitchRad=P.InitialPitchRad; Snapshot.BaseYawRad=P.InitialYawRad;
	bInitialized=true; return true;
}
bool UTRRodControlComponent::ApplyAim(FVector2D Delta,const FTRSimTime& Time)
{
	if(!bInitialized || !Time.IsValid() || Delta.ContainsNaN()){return false;}
	if(BudgetTick!=Time.TickIndex){BudgetTick=Time.TickIndex;UsedAimRad={};}
	const double Budget=Frozen.MaxAimRateRadPerS*Time.StepSeconds;
	if(!FMath::IsFinite(Budget)){return false;}
	const double X=FMath::Clamp(Delta.X,-Frozen.MaxMouseDelta,Frozen.MaxMouseDelta)*Frozen.SensitivityXRad*(Frozen.bInvertX?-1:1);
	const double Y=FMath::Clamp(Delta.Y,-Frozen.MaxMouseDelta,Frozen.MaxMouseDelta)*Frozen.SensitivityYRad*(Frozen.bInvertY?-1:1);
	const double Yaw=FMath::Clamp(X,-FMath::Max(0.0,Budget-UsedAimRad.X),FMath::Max(0.0,Budget-UsedAimRad.X));
	const double Pitch=FMath::Clamp(Y,-FMath::Max(0.0,Budget-UsedAimRad.Y),FMath::Max(0.0,Budget-UsedAimRad.Y));
	UsedAimRad+=FVector2D(FMath::Abs(Yaw),FMath::Abs(Pitch));
	Snapshot.BaseYawRad=FMath::Clamp(Snapshot.BaseYawRad+Yaw,Frozen.MinYawRad,Frozen.MaxYawRad);
	Snapshot.BasePitchRad=FMath::Clamp(Snapshot.BasePitchRad+Pitch,Frozen.MinPitchRad,Frozen.MaxPitchRad);return true;
}
bool UTRRodControlComponent::Step(const FTRSimTime& Time,const FTRBoatSnapshot& Boat,const FTREgiSnapshot& Fishing,double SurfaceZ)
{
	if(!bInitialized || !Time.IsValid() || Boat.Tick!=Time.TickIndex || Boat.PositionM.ContainsNaN() || !FMath::IsFinite(Boat.HeadingRad) || !FMath::IsFinite(SurfaceZ)) {return false;}
	ObserveOperationStart(Fishing);
	auto Next=Snapshot; Next.bValid=true; Next.Tick=Time.TickIndex;Next.CastId=Fishing.CastId;
	Next.bShakuriActive=Fishing.FishingState==ETRFishingState::Jerking;
	double Offset=0;Next.ShakuriPhase=0;
	if(Next.bShakuriActive)
	{
		const int64 Elapsed=Time.TickIndex-Fishing.StateEnteredTick;
		if(Elapsed<0){return false;}
		const bool Up=Elapsed<UpTicks; Next.ShakuriPhase=Up?1:2;
		const double T=FMath::Clamp(Up?double(Elapsed)/UpTicks:1.0-double(Elapsed-UpTicks)/ReturnTicks,0.0,1.0);
		Offset=Frozen.ShakuriAmplitudeRad*T*T*(3-2*T);
	}
	Next.FinalPitchRad=FMath::Clamp(Next.BasePitchRad+Offset,Frozen.MinPitchRad,Frozen.MaxPitchRad);Next.FinalYawRad=Next.BaseYawRad;
	const double Heading=Boat.HeadingRad,C=FMath::Cos(Heading),S=FMath::Sin(Heading);
	const FVector Mount=Boat.PositionM+FVector(C*Frozen.MountOffsetM.X-S*Frozen.MountOffsetM.Y,S*Frozen.MountOffsetM.X+C*Frozen.MountOffsetM.Y,Frozen.MountOffsetM.Z);
	const double Yaw=Heading+Next.FinalYawRad,Pitch=Next.FinalPitchRad;
	Next.TipDirection=FVector(FMath::Cos(Pitch)*FMath::Cos(Yaw),FMath::Cos(Pitch)*FMath::Sin(Yaw),FMath::Sin(Pitch));
	Next.TipWorldRotation=FRotator(FMath::RadiansToDegrees(Pitch),FMath::RadiansToDegrees(Yaw),0).Quaternion();
	Next.TipWorldPositionM=Mount+Next.TipDirection*Frozen.LengthM;
	if(TRUnits::MetersToCentimeters(Next.TipWorldPositionM).ContainsNaN() || Next.TipWorldPositionM.Z<SurfaceZ){return false;}
	Snapshot=Next;return true;
}
float UTRRodControlComponent::GetReelPulseMps(const FTRSimTime& Time,const FTREgiSnapshot& Fishing) const
{
	const int64 Elapsed=Time.TickIndex-Fishing.StateEnteredTick;
	// Recover slack during rod return, not on top of the upward tip-speed peak.
	return bInitialized && Time.IsValid() && Fishing.FishingState==ETRFishingState::Jerking && Elapsed>=UpTicks && Elapsed-UpTicks<ReelTicks ? float(Frozen.ShakuriReelSpeedMps) : 0.f;
}
void UTRRodControlComponent::Reset(){bInitialized=false;Snapshot={};Frozen={};UpTicks=ReturnTicks=ReelTicks=0;BudgetTick=-1;UsedAimRad={};ObservedCast={};ObservedJerkCount=0;}
void UTRRodControlComponent::ObserveOperationStart(const FTREgiSnapshot& Fishing)
{
	if(ObservedCast!=Fishing.CastId){ObservedCast=Fishing.CastId;ObservedJerkCount=0;}
	if(Fishing.FishingState==ETRFishingState::Jerking && Fishing.JerkCount!=ObservedJerkCount)
	{
		Snapshot.ShakuriStartBasePitchRad=Snapshot.BasePitchRad;Snapshot.ShakuriStartBaseYawRad=Snapshot.BaseYawRad;
		ObservedJerkCount=Fishing.JerkCount;
	}
}
