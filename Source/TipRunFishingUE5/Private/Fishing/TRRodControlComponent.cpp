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
	if(Snapshot.bScreenControl)
	{
		const double Budget=Frozen.Screen.MaxRatePerS*Time.StepSeconds;
		const FVector2D Wanted(FMath::Clamp(Delta.X,-Frozen.MaxMouseDelta,Frozen.MaxMouseDelta)*Frozen.Screen.Sensitivity.X*(Frozen.bInvertX?-1:1),
			FMath::Clamp(Delta.Y,-Frozen.MaxMouseDelta,Frozen.MaxMouseDelta)*Frozen.Screen.Sensitivity.Y*(Frozen.bInvertY?-1:1));
		for(int Axis=0;Axis<2;++Axis)
		{
			const double Remaining=FMath::Max(0.,Budget-UsedAimRad[Axis]);
			const double Move=FMath::Clamp(Wanted[Axis],-Remaining,Remaining);
			UsedAimRad[Axis]+=FMath::Abs(Move);
			Snapshot.ScreenControl[Axis]=FMath::Clamp(Snapshot.ScreenControl[Axis]+Move,Frozen.Screen.Min[Axis],Frozen.Screen.Max[Axis]);
		}
		return true;
	}
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
	const FVector LocalMount=bUseStation?StationMountM:Frozen.MountOffsetM;
	const FVector Mount=Boat.PositionM+FVector(C*LocalMount.X-S*LocalMount.Y,S*LocalMount.X+C*LocalMount.Y,LocalMount.Z);
	Next.RootWorldPositionM=Mount;
	// Rebuild from the station basis, never from the previous rod rotation or tip.
	// Boat currently has heading only: station Up is world Up (no hull roll/pitch model).
	const FQuat StationBasis(FVector::UpVector,Heading+(bUseStation?StationYawRad:0));
	const FVector Forward=StationBasis.GetForwardVector(),Right=StationBasis.GetRightVector(),Up=StationBasis.GetUpVector();
	const FVector Horizontal=Forward*FMath::Cos(Next.FinalYawRad)+Right*FMath::Sin(Next.FinalYawRad);
	Next.TipDirection=(Horizontal*FMath::Cos(Next.FinalPitchRad)+Up*FMath::Sin(Next.FinalPitchRad)).GetSafeNormal();
	Next.LengthM=Frozen.LengthM;
	Next.TipWorldRotation=Next.TipDirection.Rotation().Quaternion();
	Next.TipWorldPositionM=Mount+Next.TipDirection*Next.LengthM;
	if(Next.bScreenControl)
	{
		Next.AimCameraWorldM=Mount+FQuat(FVector::UpVector,Heading).RotateVector(CameraMountM-StationMountM);
		if(!ResolveScreenPose(Next,StationBasis,Offset,SurfaceZ)){return false;}
	}
	if(TRUnits::MetersToCentimeters(Next.TipWorldPositionM).ContainsNaN() || Next.TipWorldPositionM.Z<SurfaceZ){return false;}
	Snapshot=Next;return true;
}

void UTRRodControlComponent::SetScreenStation(const FTRFishingStation& Station,const FTRFishingStationParameters& Camera)
{
	SetStation(Station.RodMountM,FMath::DegreesToRadians(Station.FacingDeg));
	ScreenCameraSettings=Camera;CameraMountM=Station.CameraM;CameraYawPitchDeg=FVector2D(0,Camera.InitialPitchDeg);
	Snapshot.bScreenControl=true;Snapshot.ScreenControl=Frozen.Screen.Initial;
}
bool UTRRodControlComponent::ApplyView(FVector2D View)
{
	if(!bInitialized || !Snapshot.bScreenControl || View.ContainsNaN()){return false;}
	CameraYawPitchDeg=FVector2D(FMath::Clamp(View.X,ScreenCameraSettings.MinYawDeg,ScreenCameraSettings.MaxYawDeg),
		FMath::Clamp(View.Y,ScreenCameraSettings.MinPitchDeg,ScreenCameraSettings.MaxPitchDeg));return true;
}
bool UTRRodControlComponent::ResolveScreenPose(FTRRodSnapshot& Next,const FQuat& Basis,double Offset,double SurfaceZ) const
{
	Next.AimCameraRotation=FRotator(CameraYawPitchDeg.Y,Basis.Rotator().Yaw+CameraYawPitchDeg.X,0);
	Next.AimCameraFOVDeg=ScreenCameraSettings.FOV;
	const FQuat Camera=Next.AimCameraRotation.Quaternion();
	const double TanHalf=FMath::Tan(FMath::DegreesToRadians(Next.AimCameraFOVDeg*.5));
	Next.bScreenSafetyLimited=false;
	// A rectangular camera-space working domain keeps cross-axis motion independent,
	// including downward camera look. Use the farthest possible ray/sphere distance
	// for a conservative surface bound; it does not depend on the player's X control.
	const double Reach=Next.LengthM+(Next.AimCameraWorldM-Next.RootWorldPositionM).Size();
	const double SafeElevation=FMath::Asin(FMath::Clamp((SurfaceZ+Frozen.Screen.SurfaceClearanceM-Next.AimCameraWorldM.Z)/Reach,-1.,1.));
	const double LocalElevation=FMath::Clamp(SafeElevation-FMath::DegreesToRadians(CameraYawPitchDeg.Y),-1.4,1.4);
	const double SafeMinY=FMath::Clamp(FMath::Tan(LocalElevation)/TanHalf,Frozen.Screen.Min.Y,Frozen.Screen.MaxProjectedY-.01);
	const double SafeMaxY=FMath::Min(Frozen.Screen.MaxProjectedY,FMath::Max(Frozen.Screen.Max.Y,SafeMinY+Frozen.Screen.Max.Y-Frozen.Screen.Min.Y));
	const double YFraction=(Next.ScreenControl.Y-Frozen.Screen.Min.Y)/(Frozen.Screen.Max.Y-Frozen.Screen.Min.Y);
	const FVector2D EffectiveBase(Next.ScreenControl.X,FMath::Lerp(SafeMinY,SafeMaxY,YFraction));
	Next.bScreenSafetyLimited=SafeMinY>Frozen.Screen.Min.Y;
	auto Solve=[&](FVector2D Control)
	{
		const FVector Ray=Camera.RotateVector(FVector(1,Control.X*TanHalf,Control.Y*TanHalf).GetSafeNormal());
		const FVector OC=Next.AimCameraWorldM-Next.RootWorldPositionM;
		const double B=FVector::DotProduct(OC,Ray),D=B*B-(OC.SizeSquared()-Next.LengthM*Next.LengthM);
		const double T=-B+FMath::Sqrt(FMath::Max(0.,D));
		FVector Direction;
		if(D>=0 && T>UE_DOUBLE_SMALL_NUMBER){Direction=(OC+Ray*T).GetSafeNormal();}
		else
		{
			// No forward intersection: nearest ray point projected onto the physical sphere.
			Next.bScreenSafetyLimited=true;Direction=(OC+Ray*FMath::Max(0.,-B)).GetSafeNormal();
			if(Direction.IsNearlyZero()){Direction=Ray;}
		}
		FVector Tip=Next.RootWorldPositionM+Direction*Next.LengthM;
		const double MinZ=SurfaceZ+Frozen.Screen.SurfaceClearanceM;
		if(Tip.Z<MinZ)
		{
			// Safety boundary only, never stretch the rod or mutate the player's base control.
			Next.bScreenSafetyLimited=true;
			const double Z=FMath::Clamp(MinZ-Next.RootWorldPositionM.Z,-Next.LengthM,Next.LengthM);
			FVector XY=FVector(Direction.X,Direction.Y,0).GetSafeNormal();
			if(XY.IsNearlyZero()){XY=Basis.GetForwardVector();}
			Tip=Next.RootWorldPositionM+XY*FMath::Sqrt(FMath::Max(0.,Next.LengthM*Next.LengthM-Z*Z))+FVector(0,0,Z);
		}
		return Tip;
	};
	const FVector Base=Solve(EffectiveBase);
	const FVector BaseLocal=Basis.UnrotateVector((Base-Next.RootWorldPositionM).GetSafeNormal());
	Next.BaseYawRad=FMath::Atan2(BaseLocal.Y,BaseLocal.X);Next.BasePitchRad=FMath::Atan2(BaseLocal.Z,FVector2D(BaseLocal).Size());
	// Keep the existing finite Up/Return profile, applied temporarily in the camera plane.
	const double Lift=FMath::Tan(FMath::Clamp(Offset,0.,1.2))/TanHalf;
	const FVector2D FinalControl(EffectiveBase.X,FMath::Min(Frozen.Screen.MaxProjectedY,EffectiveBase.Y+Lift));
	Next.TipWorldPositionM=Solve(FinalControl);
	Next.TipDirection=(Next.TipWorldPositionM-Next.RootWorldPositionM).GetSafeNormal();
	Next.TipWorldRotation=Next.TipDirection.Rotation().Quaternion();
	const FVector FinalLocal=Basis.UnrotateVector(Next.TipDirection);
	Next.FinalYawRad=FMath::Atan2(FinalLocal.Y,FinalLocal.X);Next.FinalPitchRad=FMath::Atan2(FinalLocal.Z,FVector2D(FinalLocal).Size());
	const FVector Projected=Camera.UnrotateVector(Next.TipWorldPositionM-Next.AimCameraWorldM);
	if(Projected.X<=UE_DOUBLE_SMALL_NUMBER || Projected.ContainsNaN()){return false;}
	Next.ResolvedScreenControl=FVector2D(Projected.Y,Projected.Z)/(Projected.X*TanHalf);
	return !Next.ResolvedScreenControl.ContainsNaN();
}
float UTRRodControlComponent::GetReelPulseMps(const FTRSimTime& Time,const FTREgiSnapshot& Fishing) const
{
	const int64 Elapsed=Time.TickIndex-Fishing.StateEnteredTick;
	// Recover slack during rod return, not on top of the upward tip-speed peak.
	return bInitialized && Time.IsValid() && Fishing.FishingState==ETRFishingState::Jerking && Elapsed>=UpTicks && Elapsed-UpTicks<ReelTicks ? float(Frozen.ShakuriReelSpeedMps) : 0.f;
}
void UTRRodControlComponent::Reset(){bUseStation=false;StationYawRad=0;StationMountM=FVector::ZeroVector;bInitialized=false;Snapshot={};Frozen={};UpTicks=ReturnTicks=ReelTicks=0;BudgetTick=-1;UsedAimRad={};ObservedCast={};ObservedJerkCount=0;}
void UTRRodControlComponent::ObserveOperationStart(const FTREgiSnapshot& Fishing)
{
	if(ObservedCast!=Fishing.CastId){ObservedCast=Fishing.CastId;ObservedJerkCount=0;}
	if(Fishing.FishingState==ETRFishingState::Jerking && Fishing.JerkCount!=ObservedJerkCount)
	{
		Snapshot.ShakuriStartBasePitchRad=Snapshot.BasePitchRad;Snapshot.ShakuriStartBaseYawRad=Snapshot.BaseYawRad;
		ObservedJerkCount=Fishing.JerkCount;
	}
}
