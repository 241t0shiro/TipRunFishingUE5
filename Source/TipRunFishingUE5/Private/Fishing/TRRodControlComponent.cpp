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
	BaseDirectionLocal=FRotator(FMath::RadiansToDegrees(P.InitialPitchRad),FMath::RadiansToDegrees(P.InitialYawRad),0).Vector();
 RootLocal=P.MountOffsetM; bPoseInitialized=true; bInitialized=true; return true;
}
bool UTRRodControlComponent::ApplyAim(FVector2D Delta,const FTRSimTime& Time,const FTRRodAimObservation& View,int64 Sequence)
{
	if(!bInitialized || !Time.IsValid() || Delta.ContainsNaN()){return false;}
	if(Delta.IsZero()){return true;}
 if(Snapshot.bScreenControl && !bPoseInitialized){return false;}
 if(BudgetTick!=Time.TickIndex){BudgetTick=Time.TickIndex;UsedAimRad={};}
	if(Snapshot.bScreenControl)
	{
  Snapshot.bEnvelopeEnabled=Frozen.Envelope.bEnabled; Snapshot.bEnvelopeLimited=false; Snapshot.EnvelopeReason=TEXT("None");
  Snapshot.AimMouseDelta=Delta;Snapshot.AimCameraFrame=View.CameraFrame;Snapshot.AimSequence=Sequence;
  Snapshot.AimMapping=TEXT("None");Snapshot.AimProjectionStatus=TEXT("ProjectionInvalid");Snapshot.AimSphereResult=TEXT("NotAttempted");
  Snapshot.AimSphereDiscriminant=0;Snapshot.bAimProjectionValid=false;Snapshot.bAimOutsideViewRect=false;
  Snapshot.AimCurrentPixel=Snapshot.AimTargetPixel=FVector2D::ZeroVector;
  auto Reject=[&](const TCHAR* Reason){Snapshot.AimResult=Reason;return false;};
  if(!View.bValid || View.CameraWorldM.ContainsNaN() || View.CameraRotation.ContainsNaN() ||
   View.BoatWorldM.ContainsNaN() || !FMath::IsFinite(View.BoatHeadingRad) ||
   !FMath::IsFinite(View.FOVDeg) || View.FOVDeg<=0 || View.FOVDeg>=180){return Reject(TEXT("InvalidProjection"));}
  // All geometry uses this command's immutable observation, never the live camera.
  const FQuat H(FVector::UpVector,View.BoatHeadingRad),Basis(FVector::UpVector,View.BoatHeadingRad+StationYawRad);
  const FVector CameraLocal=Basis.UnrotateVector(View.CameraWorldM-(View.BoatWorldM+H.RotateVector(StationOriginM)));
  const FQuat CameraLocalRotation=Basis.Inverse()*View.CameraRotation.Quaternion();
  const FVector P=CameraLocalRotation.UnrotateVector(RootLocal+BaseDirectionLocal*Frozen.LengthM-CameraLocal);
  if(P.ContainsNaN()){return Reject(TEXT("InvalidPose"));}
  const double TanHalf=FMath::Tan(FMath::DegreesToRadians(View.FOVDeg*.5));
  // Non-viewport unit fixtures use aspect 16:9 only. Runtime always captures ViewRect.
  const double Width=View.ViewRect.Width()>0?View.ViewRect.Width():1.;
  const double Height=View.ViewRect.Height()>0?View.ViewRect.Height():9./16.;
  const FVector2D Scale=View.bHasProjection?View.ProjectionScale:FVector2D(1./TanHalf,Width/(Height*TanHalf));
  const FVector2D ProjectionOffset=View.bHasProjection?View.ProjectionOffset:FVector2D::ZeroVector;
  const bool ProjectionValid=P.X>UE_DOUBLE_SMALL_NUMBER && !Scale.ContainsNaN() && !ProjectionOffset.ContainsNaN() && Scale.X>0 && Scale.Y>0;
  FVector2D Move=FVector2D::ZeroVector;
  const double Budget=Frozen.Screen.MaxRatePerS*Time.StepSeconds;
  for(int Axis=0;Axis<2;++Axis)
  {
   const double Wanted=FMath::Clamp(Delta[Axis],-Frozen.MaxMouseDelta,Frozen.MaxMouseDelta)*Frozen.Screen.Sensitivity[Axis]*((Axis?Frozen.bInvertY:Frozen.bInvertX)?-1:1);
   Move[Axis]=FMath::Clamp(Wanted,-FMath::Max(0.,Budget-UsedAimRad[Axis]),FMath::Max(0.,Budget-UsedAimRad[Axis]));
   UsedAimRad[Axis]+=FMath::Abs(Move[Axis]);
  }
  FVector Direction;
  if(ProjectionValid)
  {
   // Unbounded NDC: an off-screen base is still a valid physical aim. Never
   // clamp Current or Target to ViewRect and never read the temporary Final tip.
   const FVector2D Current(P.Y/P.X*Scale.X+ProjectionOffset.X,P.Z/P.X*Scale.Y+ProjectionOffset.Y);
   const FVector2D Target=Current+FVector2D(Move.X,Move.Y*Scale.Y/Scale.X);
   auto Pixel=[&](FVector2D N){return FVector2D(View.ViewRect.Min.X+(N.X+1)*Width*.5,View.ViewRect.Min.Y+(1-N.Y)*Height*.5);};
   if(Current.ContainsNaN() || Target.ContainsNaN()){return Reject(TEXT("InvalidProjection"));}
   Snapshot.bAimProjectionValid=true;Snapshot.bAimOutsideViewRect=FMath::Abs(Current.X)>1 || FMath::Abs(Current.Y)>1;
   Snapshot.AimProjectionStatus=Snapshot.bAimOutsideViewRect?TEXT("OffScreen"):TEXT("OnScreen");
   Snapshot.AimMapping=TEXT("UnboundedProjection");Snapshot.AimCurrentPixel=Pixel(Current);Snapshot.AimTargetPixel=Pixel(Target);
   const FVector Ray=CameraLocalRotation.RotateVector(FVector(1,(Target.X-ProjectionOffset.X)/Scale.X,(Target.Y-ProjectionOffset.Y)/Scale.Y).GetSafeNormal());
   const FVector OC=CameraLocal-RootLocal;const double B=FVector::DotProduct(OC,Ray);
   const double Discriminant=B*B-(OC.SizeSquared()-Frozen.LengthM*Frozen.LengthM);
   Snapshot.AimSphereDiscriminant=Discriminant;
   Snapshot.AimSphereResult=Discriminant>=0?TEXT("Intersected"):TEXT("NoIntersection");
   if(!FMath::IsFinite(Discriminant) || Discriminant<0){return Reject(TEXT("NoSphereIntersection"));}
   const double Distance=-B+FMath::Sqrt(Discriminant);
   if(Distance<=UE_DOUBLE_SMALL_NUMBER){Snapshot.AimSphereResult=TEXT("BehindRay");return Reject(TEXT("BehindRay"));}
   Direction=(OC+Ray*Distance).GetSafeNormal();
  }
  else
  {
   // A behind-camera/invalid projection cannot latch input forever. Mouse-only
   // camera-relative tangent motion is stored back in the station, not the camera.
   Snapshot.AimProjectionStatus=P.X<=UE_DOUBLE_SMALL_NUMBER?TEXT("BehindCamera"):TEXT("ProjectionInvalid");
   Snapshot.AimMapping=TEXT("CameraTangent");Direction=BaseDirectionLocal;
   for(int Axis=0;Axis<2;++Axis)
   {
    if(Move[Axis]==0){continue;}
    const FVector CameraAxis=CameraLocalRotation.RotateVector(Axis?FVector::UpVector:FVector::RightVector);
    FVector Tangent=(CameraAxis-FVector::DotProduct(CameraAxis,Direction)*Direction).GetSafeNormal();
    if(Tangent.IsNearlyZero())
    {
     // At the exact axis pole screen motion has no unique tangent. Use the
     // camera-forward meridian instead of permanently rejecting mouse recovery.
     const FVector Forward=CameraLocalRotation.RotateVector(FVector::ForwardVector);
     Tangent=(Forward-FVector::DotProduct(Forward,Direction)*Direction).GetSafeNormal()*
      (FVector::DotProduct(CameraAxis,Direction)>0?-1.:1.);
    }
    if(Tangent.IsNearlyZero()){return Reject(TEXT("InvalidTangentBasis"));}
    // Great-circle step keeps length normalized and makes opposite input reversible.
    Direction=(Direction*FMath::Cos(Move[Axis])+Tangent*FMath::Sin(Move[Axis])).GetSafeNormal();
   }
  }
  if(Frozen.Envelope.bEnabled)
  {
   FString Reason;Direction=ConstrainLocalDirection(Direction,Reason);
   if(!Reason.IsEmpty()){Snapshot.bEnvelopeLimited=true;Snapshot.EnvelopeReason=Reason;}
  }
  const FVector Tip=RootLocal+Direction*Frozen.LengthM;
  // Outward station hemisphere and existing sea clearance. No unsafe nearest-point snap.
  if(Direction.ContainsNaN() || Direction.IsNearlyZero() || Direction.X<=0 || Tip.Z<SurfaceLocalZ+Frozen.Screen.SurfaceClearanceM)
  {return Reject(TEXT("SafetyLimit"));}
  BaseDirectionLocal=Direction;
  CompatibilityCameraLocal=CameraLocal;CompatibilityCameraRotation=CameraLocalRotation.Rotator();CompatibilityFOV=View.FOVDeg;
  bCompatibilitySafetyLimited=false;Snapshot.AimResult=Snapshot.bEnvelopeLimited?Snapshot.EnvelopeReason:TEXT("Valid");return true;
 }

 const double Budget=Frozen.MaxAimRateRadPerS*Time.StepSeconds;
	if(!FMath::IsFinite(Budget)){return false;}
	const double X=FMath::Clamp(Delta.X,-Frozen.MaxMouseDelta,Frozen.MaxMouseDelta)*Frozen.SensitivityXRad*(Frozen.bInvertX?-1:1);
	const double Y=FMath::Clamp(Delta.Y,-Frozen.MaxMouseDelta,Frozen.MaxMouseDelta)*Frozen.SensitivityYRad*(Frozen.bInvertY?-1:1);
	const double Yaw=FMath::Clamp(X,-FMath::Max(0.0,Budget-UsedAimRad.X),FMath::Max(0.0,Budget-UsedAimRad.X));
	const double Pitch=FMath::Clamp(Y,-FMath::Max(0.0,Budget-UsedAimRad.Y),FMath::Max(0.0,Budget-UsedAimRad.Y));
	UsedAimRad+=FVector2D(FMath::Abs(Yaw),FMath::Abs(Pitch));
	const double BaseYaw=FMath::Atan2(BaseDirectionLocal.Y,BaseDirectionLocal.X);
 const double BasePitch=FMath::Atan2(BaseDirectionLocal.Z,FVector2D(BaseDirectionLocal).Size());
 BaseDirectionLocal=FRotator(FMath::RadiansToDegrees(FMath::Clamp(BasePitch+Pitch,Frozen.MinPitchRad,Frozen.MaxPitchRad)),
  FMath::RadiansToDegrees(FMath::Clamp(BaseYaw+Yaw,Frozen.MinYawRad,Frozen.MaxYawRad)),0).Vector();return true;
}
bool UTRRodControlComponent::Step(const FTRSimTime& Time,const FTRBoatSnapshot& Boat,const FTREgiSnapshot& Fishing,double SurfaceZ)
{
	if(!bInitialized || !Time.IsValid() || Boat.Tick!=Time.TickIndex || Boat.PositionM.ContainsNaN() || !FMath::IsFinite(Boat.HeadingRad) || !FMath::IsFinite(SurfaceZ)) {return false;}
	ObserveOperationStart(Fishing);
	auto Next=Snapshot; Next.bValid=true; Next.Tick=Time.TickIndex;Next.CastId=Fishing.CastId;
	Next.bShakuriActive=ProfileStartTick>=0 && Time.TickIndex>=ProfileStartTick && Time.TickIndex-ProfileStartTick<UpTicks+ReturnTicks;
	double Offset=0;Next.ShakuriPhase=0;
	if(Next.bShakuriActive)
	{
		const int64 Elapsed=Time.TickIndex-ProfileStartTick;
		if(Elapsed<0){return false;}
		const bool Up=Elapsed<UpTicks; Next.ShakuriPhase=Up?1:2;
		const double T=FMath::Clamp(Up?double(Elapsed)/UpTicks:1.0-double(Elapsed-UpTicks)/ReturnTicks,0.0,1.0);
		Offset=Frozen.ShakuriAmplitudeRad*T*T*(3-2*T);
	}
 const FQuat Heading(FVector::UpVector,Boat.HeadingRad);
 const FQuat Basis(FVector::UpVector,Boat.HeadingRad+(bUseStation?StationYawRad:0));
 const FVector Origin=Boat.PositionM+Heading.RotateVector(StationOriginM);
 SurfaceLocalZ=SurfaceZ-Origin.Z;
 if(!bPoseInitialized)
 {
  auto Initial=MakeLocalSolve();Initial.ScreenControl=Frozen.Screen.Initial;
  if(!ResolveInitialScreenPose(Initial,SurfaceLocalZ)){return false;}
  BaseDirectionLocal=(Initial.TipWorldPositionM-RootLocal).GetSafeNormal();
  if(Frozen.Envelope.bEnabled){FString Reason;BaseDirectionLocal=ConstrainLocalDirection(BaseDirectionLocal,Reason);}
  bCompatibilitySafetyLimited=Initial.bScreenSafetyLimited;bPoseInitialized=true;
 }
 FVector FinalDirection=BaseDirectionLocal;
 Next.bScreenSafetyLimited=bCompatibilitySafetyLimited;
 if(Next.bScreenControl)
 {
  Next.ScreenControl=DeriveCompatibilityControl(); // diagnostic, never the stored pose
  Next.AimCameraWorldM=Origin+Basis.RotateVector(CompatibilityCameraLocal);
  Next.AimCameraRotation=(Basis*CompatibilityCameraRotation.Quaternion()).Rotator();Next.AimCameraFOVDeg=CompatibilityFOV;
 }
 if(Offset>0)
 {
  // Local elevation at the base's azimuth. Station Right after base yaw is
  // the snap axis; neither the active nor the last-input camera owns this action.
  const double Pitch=FMath::Atan2(BaseDirectionLocal.Z,FVector2D(BaseDirectionLocal).Size());
  const double Yaw=FMath::Atan2(BaseDirectionLocal.Y,BaseDirectionLocal.X);
  const double Limit=Next.bScreenControl?FMath::DegreesToRadians(Frozen.ActionSafety.MaxPitchDeg):Frozen.MaxPitchRad;
  const double FinalPitch=FMath::Min(Pitch+Offset,Limit);
  FinalDirection=FRotator(FMath::RadiansToDegrees(FinalPitch),FMath::RadiansToDegrees(Yaw),0).Vector();
  if(FinalPitch<Pitch+Offset){Next.bEnvelopeLimited=true;Next.EnvelopeReason=TEXT("Action.PitchSafety");}
 }
 if(Next.bScreenControl)
 {
  // Projection is diagnostic only, after the station-local action is resolved.
  const FVector Projected=CompatibilityCameraRotation.Quaternion().UnrotateVector(RootLocal+FinalDirection*Frozen.LengthM-CompatibilityCameraLocal);
  const double TanHalf=FMath::Tan(FMath::DegreesToRadians(CompatibilityFOV*.5));
  Next.ResolvedScreenControl=FMath::Abs(Projected.X)>UE_DOUBLE_SMALL_NUMBER?FVector2D(Projected.Y,Projected.Z)/(Projected.X*TanHalf):FVector2D::ZeroVector;
 }
 Next.bEnvelopeEnabled=Next.bScreenControl && Frozen.Envelope.bEnabled;
 Next.TemporaryShakuriOffsetRad=Offset;
 PublishLocalPose(Next,FinalDirection);
 Next.RootWorldPositionM=Origin+Basis.RotateVector(RootLocal);
 Next.TipDirection=Basis.RotateVector(FinalDirection);
 Next.TipWorldPositionM=Next.RootWorldPositionM+Next.TipDirection*Frozen.LengthM;
 Next.TipWorldRotation=Next.TipDirection.Rotation().Quaternion();
	if(TRUnits::MetersToCentimeters(Next.TipWorldPositionM).ContainsNaN() || Next.TipWorldPositionM.Z<SurfaceZ){return false;}
	Snapshot=Next;return true;
}

void UTRRodControlComponent::SetStation(FVector MountM,double FacingRad)
{
 StationYawRad=FacingRad;StationOriginM=FVector::ZeroVector;bUseStation=true;
 RootLocal=FQuat(FVector::UpVector,FacingRad).UnrotateVector(MountM);
 BaseDirectionLocal=FRotator(FMath::RadiansToDegrees(FMath::Clamp(0.,Frozen.MinPitchRad,Frozen.MaxPitchRad)),0,0).Vector();
 bPoseInitialized=true;bCompatibilitySafetyLimited=false;Snapshot.bValid=false;Snapshot.bScreenControl=false;
 PublishLocalPose(Snapshot,BaseDirectionLocal);
}
void UTRRodControlComponent::SetScreenStation(const FTRFishingStation& Station,const FTRFishingStationParameters& Camera)
{
 SetStation(Station.RodMountM,FMath::DegreesToRadians(Station.FacingDeg));
 StationOriginM=Station.PlayerM;
 const FQuat Basis(FVector::UpVector,StationYawRad);
 RootLocal=Basis.UnrotateVector(Station.RodMountM-StationOriginM);
 CompatibilityCameraLocal=Basis.UnrotateVector(Station.CameraM-StationOriginM);
 CompatibilityCameraRotation=FRotator(Camera.InitialPitchDeg,0,0);CompatibilityFOV=Camera.FOV;
 Snapshot.bScreenControl=true;bPoseInitialized=false;
}
bool UTRRodControlComponent::ApplyView(FVector2D View)
{
 // Deprecated RodView command: validation only. Camera-only can never mutate pose.
 return bInitialized && Snapshot.bScreenControl && !View.ContainsNaN();
}
FTRRodSnapshot UTRRodControlComponent::MakeLocalSolve() const
{
 FTRRodSnapshot N;N.RootWorldPositionM=RootLocal;N.LengthM=Frozen.LengthM;
 N.AimCameraWorldM=CompatibilityCameraLocal;N.AimCameraRotation=CompatibilityCameraRotation;N.AimCameraFOVDeg=CompatibilityFOV;return N;
}
FVector2D UTRRodControlComponent::DeriveCompatibilityControl() const
{
 const double TanHalf=FMath::Tan(FMath::DegreesToRadians(CompatibilityFOV*.5));
 const FVector P=CompatibilityCameraRotation.Quaternion().UnrotateVector(RootLocal+BaseDirectionLocal*Frozen.LengthM-CompatibilityCameraLocal);
 if(P.ContainsNaN() || FMath::Abs(P.X)<=UE_DOUBLE_SMALL_NUMBER){return FVector2D::ZeroVector;} // diagnostic only at projection singularity
 const FVector2D Projected=FVector2D(P.Y,P.Z)/(P.X*TanHalf);
 return Projected; // Read-only projection, never a persistent rectangular control.

}
FVector UTRRodControlComponent::ConstrainLocalDirection(const FVector& Direction,FString& Reason) const
{
 Reason.Empty();
 const double Yaw=FMath::Atan2(Direction.Y,Direction.X);
 const double Pitch=FMath::Atan2(Direction.Z,FVector2D(Direction).Size());
 const double BoundedYaw=FMath::Clamp(Yaw,FMath::DegreesToRadians(Frozen.Envelope.MinYawDeg),FMath::DegreesToRadians(Frozen.Envelope.MaxYawDeg));
 // At a bounded yaw, this pitch maximizes dot(candidate, direction): nearest
 // direction on the fixed sphere, rather than an accumulated Euler rotation.
 const double NearestPitch=FMath::Atan2(Direction.Z,FVector2D(Direction).Size()*FMath::Cos(Yaw-BoundedYaw));
 const double SurfacePitch=FMath::Asin(FMath::Clamp((SurfaceLocalZ+Frozen.Screen.SurfaceClearanceM-RootLocal.Z)/Frozen.LengthM,-1.,1.));
 const double MinPitch=FMath::Max(FMath::DegreesToRadians(Frozen.Envelope.MinPitchDeg),SurfacePitch);
 const double MaxPitch=FMath::DegreesToRadians(Frozen.Envelope.MaxPitchDeg);
 const double BoundedPitch=FMath::Clamp(NearestPitch,MinPitch,MaxPitch);
 const bool YawLimited=FMath::Abs(Yaw-BoundedYaw)>1.e-12;
 const bool PitchLimited=FMath::Abs(Pitch-BoundedPitch)>1.e-12;
 if(!YawLimited && !PitchLimited){return Direction;}
 Reason=YawLimited?(PitchLimited?TEXT("EnvelopeYawPitch"):TEXT("EnvelopeYaw")):TEXT("EnvelopePitch");
 return FRotator(FMath::RadiansToDegrees(BoundedPitch),FMath::RadiansToDegrees(BoundedYaw),0).Vector();
}

void UTRRodControlComponent::PublishLocalPose(FTRRodSnapshot& Next,const FVector& FinalDirection) const
{
 Next.RodRootLocal=RootLocal;Next.BaseRodDirectionLocal=BaseDirectionLocal;Next.LengthM=Frozen.LengthM;
 Next.BaseRodTipLocal=RootLocal+BaseDirectionLocal*Frozen.LengthM;
 Next.FinalRodDirectionLocal=FinalDirection;Next.FinalRodTipLocal=RootLocal+FinalDirection*Frozen.LengthM;
 Next.BaseYawRad=FMath::Atan2(BaseDirectionLocal.Y,BaseDirectionLocal.X);Next.BasePitchRad=FMath::Atan2(BaseDirectionLocal.Z,FVector2D(BaseDirectionLocal).Size());
 Next.FinalYawRad=FMath::Atan2(FinalDirection.Y,FinalDirection.X);Next.FinalPitchRad=FMath::Atan2(FinalDirection.Z,FVector2D(FinalDirection).Size());
}
bool UTRRodControlComponent::ResolveInitialScreenPose(FTRRodSnapshot& Next,double SurfaceZ) const
{
	const FQuat Camera=Next.AimCameraRotation.Quaternion();
	const double TanHalf=FMath::Tan(FMath::DegreesToRadians(Next.AimCameraFOVDeg*.5));
	Next.bScreenSafetyLimited=false;
	// A rectangular camera-space working domain keeps cross-axis motion independent,
	// including downward camera look. Use the farthest possible ray/sphere distance
	// for a conservative surface bound; it does not depend on the player's X control.
	const double Reach=Next.LengthM+(Next.AimCameraWorldM-Next.RootWorldPositionM).Size();
	const double SafeElevation=FMath::Asin(FMath::Clamp((SurfaceZ+Frozen.Screen.SurfaceClearanceM-Next.AimCameraWorldM.Z)/Reach,-1.,1.));
	const double LocalElevation=FMath::Clamp(SafeElevation-FMath::DegreesToRadians(Next.AimCameraRotation.Pitch),-1.4,1.4);
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
			if(XY.IsNearlyZero()){XY=FVector::ForwardVector;}
			Tip=Next.RootWorldPositionM+XY*FMath::Sqrt(FMath::Max(0.,Next.LengthM*Next.LengthM-Z*Z))+FVector(0,0,Z);
		}
		return Tip;
	};
	const FVector Base=Solve(EffectiveBase);
	const FVector BaseLocal=(Base-Next.RootWorldPositionM).GetSafeNormal();
	Next.BaseYawRad=FMath::Atan2(BaseLocal.Y,BaseLocal.X);Next.BasePitchRad=FMath::Atan2(BaseLocal.Z,FVector2D(BaseLocal).Size());
	Next.TipWorldPositionM=Base;
	Next.TipDirection=(Next.TipWorldPositionM-Next.RootWorldPositionM).GetSafeNormal();
	Next.TipWorldRotation=Next.TipDirection.Rotation().Quaternion();
	const FVector FinalLocal=Next.TipDirection;
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
void UTRRodControlComponent::Reset(){RootLocal=StationOriginM=FVector::ZeroVector;BaseDirectionLocal=FVector::ForwardVector;bPoseInitialized=false;bUseStation=false;StationYawRad=0;bInitialized=false;Snapshot={};Frozen={};UpTicks=ReturnTicks=ReelTicks=0;BudgetTick=-1;UsedAimRad={};ObservedCast={};ObservedJerkCount=0;ProfileStartTick=-1;}
void UTRRodControlComponent::ObserveOperationStart(const FTREgiSnapshot& Fishing)
{
	if(ObservedCast!=Fishing.CastId){ObservedCast=Fishing.CastId;ObservedJerkCount=0;ProfileStartTick=-1;}
	if(Fishing.FishingState==ETRFishingState::Jerking && Fishing.JerkCount!=ObservedJerkCount)
	{
		Snapshot.ShakuriStartBasePitchRad=FMath::Atan2(BaseDirectionLocal.Z,FVector2D(BaseDirectionLocal).Size());Snapshot.ShakuriStartBaseYawRad=FMath::Atan2(BaseDirectionLocal.Y,BaseDirectionLocal.X);
		ObservedJerkCount=Fishing.JerkCount; ProfileStartTick=Fishing.StateEnteredTick;
	}
}
