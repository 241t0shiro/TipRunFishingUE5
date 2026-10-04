#include "Fishing/TREgiSimulationComponent.h"
#include "GameFramework/Actor.h"
#include <cmath>

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
	const FVector Start = Snapshot.WorldPositionM;
	FVector Position = Start, V = MotionVelocityMps;
	const FVector RodStart = bHasPreviousRod ? PreviousRodTipM : Boat.RodTipM;
	const FVector RodVelocity = (Boat.RodTipM - RodStart) / Time.StepSeconds;
	if (RodVelocity.ContainsNaN()) { return DiagnosticFail(TEXT("Spatial.RodVelocityNonFinite"), __LINE__); }
    if ((Boat.RodTipM - RodStart).Size() > P.MaxStepTravelM) { return DiagnosticFail(TEXT("Spatial.RodTipDeltaExceeded"), __LINE__); }
	double Line = Snapshot.LineLengthM, Correction = 0, Residual = ResidualLiftMps;
	double Travel = 0;
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
		// FreeFall pays only demand. TF keeps length; no automatic TF slack accumulation.
		if (Action.FishingState == ETRFishingState::FreeFall && Action.LineMode == ETRLineMode::Payout)
		{
			const double Needed = (Trial-Rod).Size()+P.LineSlackAllowanceM;
			Line += FMath::Min(FMath::Max(0.0, Needed-Line), double(P.PayoutMps)*Dt);
		}
		const double RequiredUnconstrained=(Trial-Rod).Size();
		double ActualReelMps = 0.0;
		if (Action.LineMode == ETRLineMode::ReelIn)
		{
			const double PreviousLine = Line;
			const double SurfaceHeight = Rod.Z-Local.SurfaceZ_M;
			Line = FMath::Max(FMath::Max(double(P.MinLineM), SurfaceHeight), Line-double(Action.ReelMps)*Dt);
			const FVector TrialOffset = Trial-Rod;
			const FVector Constrained = TrialOffset.Size()>Line ? Rod+TrialOffset.GetSafeNormal()*Line : Trial;
			if (Position.Z>=Local.SurfaceZ_M-EpsilonM || Constrained.Z>=Local.SurfaceZ_M-EpsilonM)
			{
				// Near the surface tangent, constant dL/dt implies unbounded horizontal speed.
				// Applies to both normal retrieval and Shakuri reel pulses. Limit spool demand
				// by surface arc geometry, not by moving the Egi directly.
				const double Radius = FMath::Max(0.0,(Position-Rod).Size2D()-double(Action.ReelMps)*Dt);
				const double SurfaceLine = FMath::Sqrt(SurfaceHeight*SurfaceHeight+Radius*Radius);
				// A rising rod can require MORE line to remain compatible with the
                // surface contact. Capping this span at PreviousLine collapses
                // the surface circle to zero and teleports the endpoint sideways.
                Line = FMath::Max(Line,SurfaceLine);
			}
			ActualReelMps = FMath::Max(0.0,(PreviousLine-Line)/Dt);
		}
		if (!FMath::IsFinite(Line) || Line < P.MinLineM || Line > P.MaxLineLengthM)
		{ return DiagnosticFail(TEXT("Spatial.InvalidLineLength"), __LINE__); }
		bool bSolved = false;
		for (int32 Iter=0; Iter<8; ++Iter)
		{
			const FVector Delta = Trial-Rod;
			const double D = Delta.Size();
			if (!FMath::IsFinite(D)) { return DiagnosticFail(TEXT("Spatial.NonFiniteConstraintDistance"), __LINE__); }
			if (D>Line) { Correction += D-Line; Trial = Rod+Delta*(Line/D); }
			if (!Read(Trial, Local) || Rod.Z < Local.SurfaceZ_M || Rod.Z-Local.SurfaceZ_M > Line+EpsilonM)
			{ return DiagnosticFail(TEXT("Spatial.InvalidLineSurfaceIntersection"), __LINE__); }
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
        SpatialDiagnostics = FString::Printf(TEXT("tick=%lld sub=%d RodPrevious=%s RodCurrent=%s RodTipDelta=%.12g RodVelocity=%s EgiPrevious=%s EgiCandidate=%s EgiPreviousVelocity=%s EgiVelocity=%s EgiStepSpeed=%.12g LinePrevious=%.12g LineCurrent=%.12g LineRequired=%.12g LineRequiredBeforeCorrection=%.12g LineCorrection=%.12g Reel=%.12g Tension=%.12g"),
            Time.TickIndex,Sub,*RodStart.ToString(),*Rod.ToString(),(Boat.RodTipM-RodStart).Size(),*RodVelocity.ToString(),*Old.ToString(),*Trial.ToString(),*OldVelocity.ToString(),*V.ToString(),StepTravel/Dt,OldLine,Line,(Trial-Rod).Size(),RequiredUnconstrained,Correction,ActualReelMps,Correction/double(P.TensionReferenceM));
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
	Snapshot.LineLengthM=float(Line);
	Snapshot.HorizontalOffsetFromBoatM=FVector2D(Position.X-Boat.PositionM.X,Position.Y-Boat.PositionM.Y);
	Snapshot.HorizontalOffsetFromRodTipM=FVector2D(Position.X-Boat.RodTipM.X,Position.Y-Boat.RodTipM.Y);
	Snapshot.HorizontalDistanceFromBoatM=Snapshot.HorizontalOffsetFromBoatM.Size();
	Snapshot.HorizontalDistanceFromRodTipM=Snapshot.HorizontalOffsetFromRodTipM.Size();
	Snapshot.BoatToEgiDistanceM=(Position-Boat.PositionM).Size(); Snapshot.RodToEgiDistanceM=(Position-Boat.RodTipM).Size();
	Snapshot.LineDirection=(Position-Boat.RodTipM).GetSafeNormal();
	Snapshot.SlackM=FMath::Max(0.0,Line-Snapshot.RodToEgiDistanceM);
	Snapshot.LineAngleRad=float(FMath::Atan2(Snapshot.HorizontalOffsetFromRodTipM.Size(),FMath::Max(0.0,Boat.RodTipM.Z-Position.Z)));
	Snapshot.CurrentAtEgiDepthMps=Local.CurrentMps;
	Snapshot.Tension01=float(FMath::Clamp(Correction/double(P.TensionReferenceM),0.0,1.0));
	Snapshot.bBottomContact=bBottom; Snapshot.bSurfaceContact=bSurface; Snapshot.Tick=Time.TickIndex;
	MotionVelocityMps=V; PreviousRodTipM=Boat.RodTipM; bHasPreviousRod=true; bOnBottom=bBottom;
	ResidualLiftMps=Residual; LastSurfaceZ_M=Local.SurfaceZ_M;
	if (Action.FishingState==ETRFishingState::Retrieving && Action.ReelMps>0 && Depth<=P.RetrievalToleranceM &&
		Snapshot.HorizontalOffsetFromRodTipM.Size()<=P.RetrievalToleranceM)
	{ bActive=false; return ETREgiStepEvent::Retrieved; }
	return Event;
}
