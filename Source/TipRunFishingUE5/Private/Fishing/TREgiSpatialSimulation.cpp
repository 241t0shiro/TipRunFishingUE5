#include "Fishing/TREgiSimulationComponent.h"
#include "GameFramework/Actor.h"
#include <cmath>
#include "Fishing/TRReelResolver.h"

ETREgiStepEvent UTREgiSimulationComponent::StepSpatial(FTRCastId ExpectedCastId, const FTRSimTime& Time,
	const FTROceanSample& Ocean, const FTRBoatSnapshot& Boat, const FTREgiAction& Action,
	TFunctionRef<FTROceanSample(const FTROceanQuery&)> Sample)
{
	const auto& P = FrozenEquipment.Parameters;
	// Bounded numerical work, not balance values. Large external steps are split deterministically.
	constexpr double MaxSubstepS = 1.0 / 60.0;
	constexpr int32 MaxSubsteps = 240;
	constexpr double EpsilonM = 1.e-5;
	if (Time.StepSeconds > MaxSubstepS * MaxSubsteps) { return DiagnosticFail(TEXT("Spatial.StepBudget"), __LINE__); }
	const int32 Count = FMath::Max(1, FMath::CeilToInt(Time.StepSeconds / MaxSubstepS));
	const double Dt = Time.StepSeconds / Count;
	if(Action.bLimitedReelDemand && (!FMath::IsFinite(Action.RequestedRetrieveM) || Action.RequestedRetrieveM<0))
 {return DiagnosticFail(TEXT("Spatial.InvalidReelDemand"),__LINE__);}
 const FVector Start = Snapshot.WorldPositionM;
	FVector Position = Start, V = MotionVelocityMps;
	const FVector RodStart = bHasPreviousRod ? PreviousRodTipM : Boat.RodTipM;
	const FVector RodVelocity = (Boat.RodTipM - RodStart) / Time.StepSeconds;
	if (RodVelocity.ContainsNaN()) { return DiagnosticFail(TEXT("Spatial.RodVelocityNonFinite"), __LINE__); }
    if ((Boat.RodTipM - RodStart).Size() > P.MaxStepTravelM) { return DiagnosticFail(TEXT("Spatial.RodTipDeltaExceeded"), __LINE__); }
	double Line = Snapshot.LineLengthM, Correction = 0, Residual = ResidualLiftMps;
	double Travel = 0, GeometryAccommodation = 0;
	FTRReelSnapshot Reel;
 Reel.Tick=Time.TickIndex;Reel.LineLengthBeforeM=Line;Reel.TensionBefore01=Snapshot.Tension01;
 Reel.Source=Action.LineMode==ETRLineMode::ReelIn ?
  (Action.FishingState==ETRFishingState::Jerking?ETRReelSource::Shakuri:ETRReelSource::NormalRetrieve):ETRReelSource::None;
 const auto PreviousReel=Snapshot.Reel;
 FTROceanSample Local = Ocean;
	bool bBottom = false, bSurface = false;
	auto Read = [&](const FVector& Point, FTROceanSample& Out)
	{
		if (TRUnits::MetersToCentimeters(Point).ContainsNaN()) { return false; }
		FTROceanQuery Q; Q.PositionXYM = FVector2D(Point.X, Point.Y); Q.SimTick = Time.TickIndex;
		// First obtain the local surface; then derive depth at exactly this world position.
		Out = Sample(Q);
		auto Valid = [&]() { return Out.bValid && Out.InvalidReason == ETRSampleError::None && Out.SampleTick == Time.TickIndex &&
			FMath::IsFinite(Out.SurfaceZ_M) && FMath::IsFinite(Out.BottomDepthM) && Out.BottomDepthM > 0 &&
			!Out.CurrentMps.ContainsNaN() && Out.CurrentMps.Z == 0; };
		if (!Valid()) { return false; }
		const double Depth = FMath::Max(0.0, double(Out.SurfaceZ_M) - Point.Z);
		if (Depth > MAX_flt) { return false; }
		Q.DepthM = float(Depth); Out = Sample(Q);
		return Valid();
	};
	for (int32 Sub = 0; Sub < Count; ++Sub)
	{
		const FVector Rod = FMath::Lerp(RodStart, Boat.RodTipM, double(Sub+1)/Count);
		if (!Read(Position, Local)) { return DiagnosticFail(TEXT("Spatial.InvalidOceanAtEgi"), __LINE__); }
		const FVector Old = Position;
		const FVector OldVelocity = V;
		const double OldLine = Line;
		if (Action.FishingState == ETRFishingState::Jerking) { Residual = Action.LiftMps; }
		else if (Action.FishingState == ETRFishingState::TensionFall)
		{
			Residual *= std::exp(-P.TensionLiftDecayPerS * Dt);
			if (Residual <= P.TensionLiftCompletionMps) { Residual = 0; }
		}
		else { Residual = 0; }
		// Integrate distributed LINE drag separately from the EGI endpoint response.
		// Three midpoint samples of the wet straight segment; velocity interpolates endpoints.
		const FVector Offset = Position - Rod;
		const double Distance = Offset.Size();
		const double Height = Rod.Z - Position.Z;
		const double WetStart = Height > EpsilonM ? FMath::Clamp((Rod.Z-Local.SurfaceZ_M)/Height,0.0,1.0) : 1.0;
		const double WetLength = Distance * (1.0-WetStart);
		const double Transfer = Line-Distance <= P.LineSlackAllowanceM+EpsilonM ? P.TautLineTransfer01 : P.SlackLineTransfer01;
		const double DragRate = P.LineDragKgPerMS * WetLength * Transfer / (double(FrozenEquipment.TotalMassG)*0.001);
		if (!FMath::IsFinite(DragRate)) { return DiagnosticFail(TEXT("Spatial.NonFiniteLineDrag"), __LINE__); }
		FVector LineForcing = FVector::ZeroVector;
		double EndWeight = 0;
		if (DragRate > 0)
		{
			for (int32 I=0; I<3; ++I)
			{
				const double T = WetStart + (1-WetStart)*(double(I)+0.5)/3;
				FTROceanSample Water;
				if (!Read(Rod+Offset*T, Water)) { return DiagnosticFail(TEXT("Spatial.InvalidOceanAtLine"), __LINE__); }
				// Ignore points above their local surface (future spatial surfaces).
				if ((Rod+Offset*T).Z > Water.SurfaceZ_M) { continue; }
				LineForcing += (Water.CurrentMps - RodVelocity*(1-T)) / 3.0;
				EndWeight += T/3.0;
			}
		}
		const FVector Target(Local.CurrentMps.X, Local.CurrentMps.Y,
			-double(FrozenEquipment.SinkSpeedMps)*Action.SinkScale+Residual);
		FVector Trial = Position;
		for (int32 Axis=0; Axis<3; ++Axis)
		{
			const double BaseRate = Axis==2 ? P.VerticalResponsePerS : double(FrozenEquipment.HorizontalResponsePerS);
			const double Rate = BaseRate+DragRate*EndWeight;
			const double Equilibrium = (BaseRate/Rate)*Target[Axis] + (DragRate/Rate)*LineForcing[Axis];
			const double X = Rate*Dt;
			if (!FMath::IsFinite(X) || !FMath::IsFinite(Equilibrium)) { return DiagnosticFail(TEXT("Spatial.NonFiniteResponse"), __LINE__); }
			const double Alpha = -std::expm1(-X);
			const double Phi = X<1.e-5 ? 1-X/2+X*X/6-X*X*X/24 : Alpha/X;
			Trial[Axis] += Dt*(V[Axis]*Phi+Equilibrium*(1-Phi));
			V[Axis] += (Equilibrium-V[Axis])*Alpha;
		}
		if (Trial.ContainsNaN()) { return DiagnosticFail(TEXT("Spatial.PositionNonFinite"), __LINE__); }
        if (V.ContainsNaN()) { return DiagnosticFail(TEXT("Spatial.VelocityNonFinite"), __LINE__); }
        if (V.Size() > P.MaxEgiSpeedMps) { return DiagnosticFail(TEXT("Spatial.EgiCandidateVelocityExceeded"), __LINE__); }
		// FreeFall pays only demand. No automatic TF slack accumulation;
		// the surface span accommodation below is a separate geometric necessity.
		if (Action.FishingState == ETRFishingState::FreeFall && Action.LineMode == ETRLineMode::Payout)
		{
			const double Needed = (Trial-Rod).Size()+P.LineSlackAllowanceM;
			const double Payout=FMath::Min(FMath::Max(0.0, Needed-Line), double(P.PayoutMps)*Dt);
   Line+=Payout;Reel.PayoutM+=Payout;
		}
		const double RequiredUnconstrained=(Trial-Rod).Size();
  const double Request=Action.LineMode==ETRLineMode::ReelIn ?
   (Action.bLimitedReelDemand?Action.RequestedRetrieveM/double(Count):double(Action.ReelMps)*Dt):0.;
  // Reserve the existing rod and candidate endpoint speed before taut shortening.
  // Slack winds onto the spool without any additional endpoint motion.
  const double Budget=FMath::Max(0.,double(P.MaxEgiSpeedMps)-RodVelocity.Size()-V.Size())*Dt;
  const double ProspectiveSlack=FMath::Max(0.,Line-TRLineGeometry::Required(Rod,Position,Trial,Local.SurfaceZ_M,P.MinLineM,false));
  const double ProspectiveReel=FMath::Min(Request,ProspectiveSlack)+FMath::Min(FMath::Max(0.,Request-ProspectiveSlack),Budget);
  const double ProspectiveLine=FMath::Max(FMath::Max(double(P.MinLineM),Rod.Z-Local.SurfaceZ_M),Line-ProspectiveReel);
  const bool Surface=TRLineGeometry::NeedsSurfaceSpan(Rod,Position,Trial,Local.SurfaceZ_M,ProspectiveLine);
  const double Required=TRLineGeometry::Required(Rod,Position,Trial,Local.SurfaceZ_M,P.MinLineM,Surface);
  // Accommodate moving-rod surface geometry separately, before measuring reel work.
  const double Span=Surface?TRLineGeometry::SurfaceSpan(Rod,Position,Local.SurfaceZ_M,0):0.;
  const double Growth=FMath::Max(0.,Span-Line);GeometryAccommodation+=Growth;Line+=Growth;
  const auto Applied=TRReelResolver::Resolve(Request,Line,Required,P.MinLineM,Budget,Rod,Position,Local.SurfaceZ_M,Surface);
  if(Sub==0){Reel.RequiredLineBeforeM=Required;Reel.SlackBeforeM=Applied.SlackM;}
  Reel.RequestedRetrieveM+=Request;Reel.SlackConsumedM+=Applied.SlackConsumedM;
  Reel.TautRetrieveAppliedM+=Applied.TautAppliedM;Reel.TautBudgetM+=Budget;
  Line=Applied.LineM;
  const double ActualReelMps=Applied.TautAppliedM/Dt;
		if (!FMath::IsFinite(Line) || Line < P.MinLineM || Line > P.MaxLineLengthM)
		{ return DiagnosticFail(TEXT("Spatial.InvalidLineLength"), __LINE__); }
		bool bSolved = false;
		for (int32 Iter=0; Iter<8; ++Iter)
		{
			const FVector Delta = Trial-Rod;
			const double D = Delta.Size();
			if (!FMath::IsFinite(D)) { return DiagnosticFail(TEXT("Spatial.NonFiniteConstraintDistance"), __LINE__); }
			if (D>Line) { Correction += D-Line; Trial = Rod+Delta*(Line/D); }
			if (!Read(Trial, Local)) { return DiagnosticFail(TEXT("Spatial.InvalidOceanAtLineConstraint"), __LINE__); }
			if (Rod.Z < Local.SurfaceZ_M) { return DiagnosticFail(TEXT("Spatial.RodBelowSurface"), __LINE__); }
			if (Rod.Z-Local.SurfaceZ_M > Line+EpsilonM)
			{
				SpatialDiagnostics=FString::Printf(TEXT("tick=%lld Rod=%s Egi=%s Line=%.12g SurfaceHeight=%.12g deficit=%.12g"),Time.TickIndex,*Rod.ToString(),*Trial.ToString(),Line,Rod.Z-Local.SurfaceZ_M,Rod.Z-Local.SurfaceZ_M-Line);
				return DiagnosticFail(TEXT("Spatial.LineBelowSurfaceSpan"), __LINE__);
			}
			Trial.Z = FMath::Clamp(Trial.Z, double(Local.SurfaceZ_M)-Local.BottomDepthM, double(Local.SurfaceZ_M));
			// Exact surface circle intersection avoids asymptotic projection at retrieval's tangent point.
			if (Trial.Z == Local.SurfaceZ_M)
			{
				const double Radius2 = FMath::Max(0.0, Line*Line-FMath::Square(Rod.Z-Trial.Z));
				const FVector2D XY(Trial.X-Rod.X,Trial.Y-Rod.Y);
				// World-coordinate roundoff can put a projected point microscopically outside
				// the circle forever. Use the solver's existing metre tolerance here too.
				if (XY.Size()>FMath::Sqrt(Radius2)+EpsilonM)
				{
					const auto Limited=XY.GetSafeNormal()*FMath::Sqrt(Radius2);
					const FVector BeforeSurfaceCorrection=Trial;
                    Trial.X=Rod.X+Limited.X; Trial.Y=Rod.Y+Limited.Y;
                    Correction+=(Trial-BeforeSurfaceCorrection).Size();
					continue; // Re-query the moved horizontal position before committing.
				}
			}
			if ((Trial-Rod).Size()<=Line+EpsilonM) { bSolved=true; break; }
		}
		if (!bSolved || !Read(Trial, Local)) { return DiagnosticFail(bSolved ? TEXT("Spatial.InvalidOceanAtConstraintDestination") : TEXT("Spatial.ConstraintNotConverged"), __LINE__); }
		const double Depth = double(Local.SurfaceZ_M)-Trial.Z;
		if (Depth < -EpsilonM || Depth > Local.BottomDepthM+EpsilonM) { return DiagnosticFail(TEXT("Spatial.InvalidDepth"), __LINE__); }
		bBottom = Depth >= Local.BottomDepthM; bSurface = Depth <= 0;
		if (bBottom) { V.Z=FMath::Max(0.0,V.Z); }
		if (bSurface) { V.Z=FMath::Min(0.0,V.Z); }
		const FVector N = (Trial-Rod).GetSafeNormal();
		if ((Trial-Rod).Size()>=Line-EpsilonM)
		{
			const double Outward = FVector::DotProduct(V-RodVelocity,N)+ActualReelMps;
			if (Outward>0) { V -= N*Outward; }
		}
		const double StepTravel = (Trial-Old).Size(); Travel+=StepTravel;
        SpatialDiagnostics = FString::Printf(TEXT("tick=%lld sub=%d RodPrevious=%s RodCurrent=%s RodTipDelta=%.12g RodVelocity=%s EgiPrevious=%s EgiCandidate=%s EgiPreviousVelocity=%s EgiVelocity=%s EgiStepSpeed=%.12g LinePrevious=%.12g LineCurrent=%.12g LineRequired=%.12g LineRequiredBeforeCorrection=%.12g LineCorrection=%.12g Reel=%.12g Tension=%.12g GeometrySpanAccommodation=%.12g"),
            Time.TickIndex,Sub,*RodStart.ToString(),*Rod.ToString(),(Boat.RodTipM-RodStart).Size(),*RodVelocity.ToString(),*Old.ToString(),*Trial.ToString(),*OldVelocity.ToString(),*V.ToString(),StepTravel/Dt,OldLine,Line,(Trial-Rod).Size(),RequiredUnconstrained,Correction,ActualReelMps,Correction/double(P.TensionReferenceM),GeometryAccommodation);
		if (!FMath::IsFinite(Travel) || Travel>P.MaxStepTravelM || StepTravel/Dt>P.MaxEgiSpeedMps ||
			V.ContainsNaN() || V.Size()>P.MaxEgiSpeedMps || TRUnits::MetersToCentimeters(Trial).ContainsNaN())
		{ const TCHAR* Failure = !FMath::IsFinite(Travel) || TRUnits::MetersToCentimeters(Trial).ContainsNaN() ? TEXT("Spatial.PositionNonFinite") :
            Travel>P.MaxStepTravelM ? TEXT("Spatial.EgiTravelExceeded") : StepTravel/Dt>P.MaxEgiSpeedMps ? TEXT("Spatial.LineCorrectionExceeded") :
            V.ContainsNaN() ? TEXT("Spatial.VelocityNonFinite") : TEXT("Spatial.EgiConstraintVelocityExceeded"); DiagnosticFail(Failure, __LINE__); DiagnosticFailure += FString::Printf(TEXT(" travel=%.9g/%.9g stepSpeed=%.9g/%.9g velocity=%.9g rodSpeed=%.9g"),Travel,double(P.MaxStepTravelM),StepTravel/Dt,double(P.MaxEgiSpeedMps),V.Size(),RodVelocity.Size()); return ETREgiStepEvent::EnvironmentInvalid; }
		Position=Trial;
	}
	// Callbacks cannot revive an old cast or a destroyed owner.
	if (!bActive || Snapshot.CastId!=ExpectedCastId || (GetOwner() && GetOwner()->IsActorBeingDestroyed()))
	{ return ETREgiStepEvent::None; }
	const double Depth = FMath::Clamp(double(Local.SurfaceZ_M)-Position.Z,0.0,double(Local.BottomDepthM));
	const double DepthVelocity = (Depth-Snapshot.DepthM)/Time.StepSeconds;
	if (!FMath::IsFinite(DepthVelocity) || FMath::Abs(DepthVelocity)>MAX_flt || !FMath::IsFinite(Correction))
	{ return DiagnosticFail(TEXT("Spatial.NonFiniteDepthVelocity"), __LINE__); }
	const ETREgiStepEvent Event = bBottom&&!bOnBottom ? ETREgiStepEvent::ReachedBottom :
		(!bBottom&&bOnBottom ? ETREgiStepEvent::LeftBottom : ETREgiStepEvent::None);
	Snapshot.WorldPositionM=Position; Snapshot.PositionXYM=FVector2D(Position.X,Position.Y);
	Snapshot.DepthM=float(Depth); Snapshot.DepthVelocityMps=float(DepthVelocity);
	Snapshot.VelocityMps=(Position-Start)/Time.StepSeconds;
	// Round revision 2 line conservatively at the existing float boundary.
 // Never shorten an extra float ULP beyond the double physical budget.
 float PublishedLine=float(Line);
 if(double(PublishedLine)<Line){PublishedLine=std::nextafter(PublishedLine,INFINITY);}
 Snapshot.LineLengthM=PublishedLine;
 Reel.PublicationDeltaM=double(PublishedLine)-Line;Line=double(PublishedLine);
	Snapshot.HorizontalOffsetFromBoatM=FVector2D(Position.X-Boat.PositionM.X,Position.Y-Boat.PositionM.Y);
	Snapshot.HorizontalOffsetFromRodTipM=FVector2D(Position.X-Boat.RodTipM.X,Position.Y-Boat.RodTipM.Y);
	Snapshot.HorizontalDistanceFromBoatM=Snapshot.HorizontalOffsetFromBoatM.Size();
	Snapshot.HorizontalDistanceFromRodTipM=Snapshot.HorizontalOffsetFromRodTipM.Size();
	Snapshot.BoatToEgiDistanceM=(Position-Boat.PositionM).Size(); Snapshot.RodToEgiDistanceM=(Position-Boat.RodTipM).Size();
	Snapshot.LineDirection=(Position-Boat.RodTipM).GetSafeNormal();
	// Slack is published below from the same required geometry used by reel.
	Snapshot.LineConstraintCorrectionM=Correction;
	Snapshot.GeometrySpanAccommodationM=GeometryAccommodation;
	Snapshot.LineAngleRad=float(FMath::Atan2(Snapshot.HorizontalOffsetFromRodTipM.Size(),FMath::Max(0.0,Boat.RodTipM.Z-Position.Z)));
	Snapshot.CurrentAtEgiDepthMps=Local.CurrentMps;
	Snapshot.Tension01=float(FMath::Clamp(Correction/double(P.TensionReferenceM),0.0,1.0));
	Reel.LineLengthAfterM=Line;Reel.NetLineLengthDeltaM=Line-Reel.LineLengthBeforeM;
 Reel.RequiredLineLengthM=TRLineGeometry::Required(Boat.RodTipM,Position,Position,Local.SurfaceZ_M,P.MinLineM,bSurface);
 Reel.SlackAfterM=FMath::Max(0.,Line-Reel.RequiredLineLengthM);
 Snapshot.SlackM=Reel.SlackAfterM;
 Reel.ActualRetrieveM=Reel.SlackConsumedM+Reel.TautRetrieveAppliedM;
 Reel.UnrealizedRetrieveM=FMath::Max(0.,Reel.RequestedRetrieveM-Reel.ActualRetrieveM);
 Reel.GeometryAccommodationM=GeometryAccommodation;Reel.TensionAfter01=Snapshot.Tension01;
 Reel.TotalRequestedRetrieveM=PreviousReel.TotalRequestedRetrieveM+Reel.RequestedRetrieveM;
 Reel.TotalSlackConsumedM=PreviousReel.TotalSlackConsumedM+Reel.SlackConsumedM;
 Reel.TotalTautRetrieveAppliedM=PreviousReel.TotalTautRetrieveAppliedM+Reel.TautRetrieveAppliedM;
 Reel.TotalActualRetrieveM=Reel.TotalSlackConsumedM+Reel.TotalTautRetrieveAppliedM;
 Reel.TotalUnrealizedRetrieveM=PreviousReel.TotalUnrealizedRetrieveM+Reel.UnrealizedRetrieveM;
 Reel.TotalNetLineLengthDeltaM=PreviousReel.TotalNetLineLengthDeltaM+Reel.NetLineLengthDeltaM;
 Reel.TotalPayoutM=PreviousReel.TotalPayoutM+Reel.PayoutM;
 Reel.TotalGeometryAccommodationM=PreviousReel.TotalGeometryAccommodationM+GeometryAccommodation;
 Snapshot.Reel=Reel;
 Snapshot.bBottomContact=bBottom; Snapshot.bSurfaceContact=bSurface; Snapshot.Tick=Time.TickIndex;
	MotionVelocityMps=V; PreviousRodTipM=Boat.RodTipM; bHasPreviousRod=true; bOnBottom=bBottom;
	ResidualLiftMps=Residual; LastSurfaceZ_M=Local.SurfaceZ_M;
	if (Action.FishingState==ETRFishingState::Retrieving && Action.ReelMps>0 && Depth<=P.RetrievalToleranceM &&
		Snapshot.HorizontalOffsetFromRodTipM.Size()<=P.RetrievalToleranceM)
	{ bActive=false; return ETREgiStepEvent::Retrieved; }
	return Event;
}
