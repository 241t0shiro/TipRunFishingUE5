#include "TRSessionTestFixture.h"
#include "Fishing/TREgiSimulationComponent.h"
#include "Fishing/TREgiActor.h"
#include "Boat/TRBoatPawn.h"
#include "Data/TREnvironmentUnits.h"
#include "Ocean/TREnvironmentField.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/DataValidation.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include <limits>

namespace
{
	void SpatialTestTuning(FTRTestSession& F)
	{
		auto& P=F.Tuning->Parameters;
		P.EgiModelRevision=2; P.VerticalResponsePerS=2; P.LineDragKgPerMS=.0001;
		P.TautLineTransfer01=.05; P.SlackLineTransfer01=0; P.LineSlackAllowanceM=.1; P.MaxStepTravelM=20;
		P.MaxEgiSpeedMps=10; P.PayoutMps=4; P.MaxLineLengthM=200;
		P.FreeFallSinkScale=P.TensionFallSinkScale=P.StaySinkScale=1;
		auto& B=F.BoatTuning->Parameters; B={}; B.ModelRevision=2;
		B.WindResponseKgPerS=.1; B.CurrentResponseKgPerS=1; B.DragKgPerS=1; B.InertiaKg=10;
		B.BowWindScale=1; B.SternWindScale=.5; B.SideWindScale=2; B.MaxDriftSpeedMps=1;
		B.HullHeightOffsetM=.5; B.RodAnchorOffsetM=FVector(2,1,1);
		F.Area->Settings.FieldRevision=2; F.Area->Settings.SurfaceZ_M=0; F.Area->Settings.FlatDepthM=30;
		F.Area->Settings.WindMps=FVector2D(2,0); F.Area->Settings.CurrentMps=FVector::ZeroVector;
	}
	struct FSpatialCase
	{
		FTRTestSession F;
		TStrongObjectPtr<UTREgiSimulationComponent> Egi{NewObject<UTREgiSimulationComponent>()};
		FTREquipmentSnapshot Equipment;
		FTRBoatSnapshot Boat;
		FTRSimTime Time;
		FTREgiAction Action;
		TFunction<FTROceanSample(const FTROceanQuery&)> Override;
		FSpatialCase() { SpatialTestTuning(F); Time.StepSeconds=1.0/60; Action.FishingState=ETRFishingState::FreeFall; Action.LineMode=ETRLineMode::Payout; Action.SinkScale=1; }
		FTROceanSample Sea(const FTROceanQuery& Q) const
		{ return Override ? Override(Q) : F.World->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(Q); }
		FTREgiSnapshot Get() const { return Egi->BuildSnapshot(Action.FishingState); }
		bool Start(FAutomationTestBase& Test, FName EgiId=TEXT("Egi_3_5"), FName SinkerId=TEXT("Sinker_None"),
			FVector Position=FVector(0,0,0), float Line=1.5f, bool bDeploy=true)
		{
			if (!F.Environment(Test)) { return false; }
			TArray<FText> Errors;
			FTROceanQuery Q; auto Ocean=Sea(Q);
			if (!F.Boat->InitializeBoat({},float(UE_DOUBLE_PI/2),Ocean,Errors)) { Test.AddError(TEXT("B boat initialization")); return false; }
			Boat=F.Boat->GetBoatSnapshot();
			if (!TREquipment::TryBuildSnapshot(F.Egis.Get(),F.Sinkers.Get(),F.Tuning.Get(),EgiId,SinkerId,Time.StepSeconds,Equipment,Errors))
			{ for(const auto& E:Errors){Test.AddError(E.ToString());} return false; }
			FTREgiSnapshot Initial; Initial.CastId=FTRCastId(1); Initial.bWorldPositionValid=true;
			Initial.WorldPositionM=bDeploy ? FVector(Boat.RodTipM.X,Boat.RodTipM.Y,0) : Position;
			Initial.LineLengthM=Line;
			Q.PositionXYM=FVector2D(Initial.WorldPositionM.X,Initial.WorldPositionM.Y); Q.DepthM=float(-Initial.WorldPositionM.Z);
			const bool Ok=Egi->InitializeCast(Initial,Equipment,Sea(Q),Errors,&Boat);
			for(const auto& E:Errors){Test.AddError(E.ToString());} return Test.TestTrue(TEXT("Spatial cast initializes"),Ok);
		}
		ETREgiStepEvent Step(bool bMoveBoat=true, FVector ManualVelocity=FVector::ZeroVector)
		{
			++Time.TickIndex;
			if(bMoveBoat)
			{
				FTROceanQuery Q; Q.PositionXYM=FVector2D(Boat.PositionM.X,Boat.PositionM.Y); Q.SimTick=Time.TickIndex;
				F.Boat->StepBoat(Time,Sea(Q),[](const FVector2D&){return true;}); Boat=F.Boat->GetBoatSnapshot();
			}
			else { Boat.Tick=Time.TickIndex; Boat.PositionM+=ManualVelocity*Time.StepSeconds; Boat.RodTipM+=ManualVelocity*Time.StepSeconds; Boat.VelocityMps=ManualVelocity; }
			const auto S=Get(); FTROceanQuery Q; Q.PositionXYM=S.PositionXYM; Q.DepthM=S.DepthM; Q.SimTick=Time.TickIndex;
			return Egi->StepEgi(S.CastId,Time,Sea(Q),Boat,Action,[this](const FTROceanQuery& D){return Sea(D);});
		}
		bool Geometry(FAutomationTestBase& Test) const
		{
			const auto S=Get(); const FVector D=S.WorldPositionM-Boat.RodTipM;
			FTROceanQuery Q; Q.PositionXYM=S.PositionXYM; Q.DepthM=S.DepthM; Q.SimTick=S.Tick; const auto O=Sea(Q);
			return Test.TestTrue(TEXT("World/line/depth/angle/velocity finite and consistent"),S.bWorldPositionValid && !S.WorldPositionM.ContainsNaN() &&
				!S.VelocityMps.ContainsNaN() && S.DepthM>=0 && S.DepthM<=O.BottomDepthM+1.e-3 &&
				FMath::Abs(S.DepthM-(O.SurfaceZ_M-S.WorldPositionM.Z))<1.e-4 &&
				D.Size()<=S.LineLengthM+1.e-5 && S.LineLengthM>=0 &&
				FMath::Abs(S.LineAngleRad-FMath::Atan2(D.Size2D(),FMath::Max(0.0,-D.Z)))<1.e-5 &&
				S.HorizontalOffsetFromRodTipM.Equals(FVector2D(D.X,D.Y),1.e-8) &&
				FMath::Abs(S.HorizontalDistanceFromRodTipM-D.Size2D())<1.e-8 &&
				FMath::Abs(S.HorizontalDistanceFromBoatM-S.HorizontalOffsetFromBoatM.Size())<1.e-8 &&
				S.LineDirection.Equals(D.GetSafeNormal(),1.e-8));
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSpatialStandardTest,"TipRun.M105C.Standard243FreeFalls",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRSpatialStandardTest::RunTest(const FString& Parameters)
{
	int32 Cases=0; double MaxLine=0,MaxTime=0;
	FTRTestSession Tables;
	const auto Egis=Tables.Egis->GetRowNames(), Sinkers=Tables.Sinkers->GetRowNames();
	TestEqual(TEXT("27 equipment combinations"),Egis.Num()*Sinkers.Num(),27);
	for (double Knots:{.4,.7,1.0}) for(int32 Direction=0;Direction<3;++Direction)
	for(FName EgiId:Egis) for(FName SinkerId:Sinkers)
	{
		FSpatialCase C; double Speed=0; TREnvironmentUnits::TryKnotsToMps(Knots,Speed);
		C.F.Area->Settings.CurrentMps=Direction==0?FVector(Speed,0,0):Direction==1?FVector(-Speed,0,0):FVector(0,-Speed,0);
		if(!C.Start(*this,EgiId,SinkerId)){return false;}
		int32 BottomEvents=0;
		for(int32 I=0;I<18000;++I)
		{
			const auto E=C.Step();
			if(!TestTrue(TEXT("Standard step valid"),E!=ETREgiStepEvent::EnvironmentInvalid)){return false;}
			const auto S=C.Get(); MaxLine=FMath::Max(MaxLine,double(S.LineLengthM));
			if(!TestTrue(TEXT("R03 no 80m payout and no penetration"),S.LineLengthM<80 && S.DepthM<=30 &&
				S.RodToEgiDistanceM<=S.LineLengthM+1.e-5)){return false;}
			if(E==ETREgiStepEvent::ReachedBottom){++BottomEvents;break;}
		}
		if(!TestEqual(TEXT("Bottom reached within 300 sim seconds"),BottomEvents,1)){return false;}
		C.Geometry(*this); const auto S=C.Get(); MaxTime=FMath::Max(MaxTime,C.Time.TickIndex*C.Time.StepSeconds);
		TestFalse(TEXT("Boat and Egi not velocity copies"),S.VelocityMps.Equals(C.Boat.VelocityMps,1.e-5));
		AddInfo(FString::Printf(TEXT("R03 %s/%s knot=%.1f dir=%d t=%.3f L=%.3f D=%.3f slack=%.3f angle=%.4f XY=(%.3f,%.3f) boatOffset=%.3f V=(%.3f,%.3f,%.3f) boatV=(%.3f,%.3f) current=(%.3f,%.3f) mass=%.0f"),
			*EgiId.ToString(),*SinkerId.ToString(),Knots,Direction,C.Time.TickIndex*C.Time.StepSeconds,S.LineLengthM,S.RodToEgiDistanceM,S.SlackM,S.LineAngleRad,
			S.WorldPositionM.X,S.WorldPositionM.Y,S.HorizontalOffsetFromBoatM.Size(),S.VelocityMps.X,S.VelocityMps.Y,S.VelocityMps.Z,
			C.Boat.VelocityMps.X,C.Boat.VelocityMps.Y,S.CurrentAtEgiDepthMps.X,S.CurrentAtEgiDepthMps.Y,S.TotalMassG));
		++Cases;
	}
	TestEqual(TEXT("All standard cases executed"),Cases,243);
	AddInfo(FString::Printf(TEXT("R03 maxima line=%.6fm time=%.3fs"),MaxLine,MaxTime)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSpatialWaterTest,"TipRun.M105C.WaterWeightsAndDemand",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRSpatialWaterTest::RunTest(const FString& Parameters)
{
	float LastDepth=0;
	for(const TCHAR* Id:{TEXT("Egi_3"),TEXT("Egi_3_5"),TEXT("Egi_4")})
	{
		FSpatialCase C; C.F.Area->Settings.WindMps={};
		if(!C.Start(*this,Id,TEXT("Sinker_None"),{},100)){return false;}
		const auto Initial=C.Get();
		for(int32 I=0;I<120;++I){C.Step(false);}
		const auto S=C.Get(); TestTrue(TEXT("Weight increases sinking"),S.DepthM>LastDepth); LastDepth=S.DepthM;
		TestTrue(TEXT("Still water vertical"),S.PositionXYM.Equals(Initial.PositionXYM,0));
		TestEqual(TEXT("Existing slack pays no line"),S.LineLengthM,100.f); C.Geometry(*this);
	}
	FSpatialCase Heavy; if(!Heavy.Start(*this,TEXT("Egi_4"),TEXT("Sinker_50"),{},100)){return false;}
	for(int32 I=0;I<120;++I){Heavy.Step(false);} TestTrue(TEXT("Added sinker increases sinking"),Heavy.Get().DepthM>LastDepth);
	double PreviousX=-1;
	for(double Knots:{.4,.7,1.0})
	{
		FSpatialCase C; TREnvironmentUnits::TryKnotsToMps(Knots,C.F.Area->Settings.CurrentMps.X);
		if(!C.Start(*this)){return false;} const double X=C.Get().WorldPositionM.X;
		for(int32 I=0;I<120;++I){C.Step(false);}
		const double Travel=C.Get().WorldPositionM.X-X;
		TestTrue(TEXT("Representative currents yield increasing horizontal response"),Travel>PreviousX); PreviousX=Travel;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSpatialStayTest,"TipRun.M105C.StayRangeBalance",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRSpatialStayTest::RunTest(const FString& Parameters)
{
	int32 Index=0;
	for(const TCHAR* Id:{TEXT("Egi_3"),TEXT("Egi_3_5"),TEXT("Egi_4")})
	{
		FSpatialCase C;
		// Shared controlled R04 geometry and response for every weight; no boundary contact.
		C.F.Tuning->Parameters.VerticalResponsePerS=40; C.F.Tuning->Parameters.LineDragKgPerMS=0;
		for(auto& Profile:C.F.Tuning->Profiles)
		{ auto* Curve=Profile.HorizontalResponseByTotalMass.GetRichCurve(); Curve->Reset(); Curve->AddKey(30,40); Curve->AddKey(90,40); }
		// Heading north rotates rod offset to (-1,2,1.5). Egi 10m behind and 10m below it.
		if(!C.Start(*this,Id,TEXT("Sinker_None"),FVector(-11,2,-8.5),float(FMath::Sqrt(200.0)),false)){return false;}
		C.Action.FishingState=ETRFishingState::Stay; C.Action.LineMode=ETRLineMode::Locked;
		const float Start=C.Get().DepthM; float Min=Start,Max=Start;
		for(int32 I=0;I<600;++I)
		{
			if(!TestTrue(TEXT("Stay step valid"),C.Step(false,FVector(.7,0,0))!=ETREgiStepEvent::EnvironmentInvalid)){return false;}
			const auto S=C.Get(); Min=FMath::Min(Min,S.DepthM);Max=FMath::Max(Max,S.DepthM);
			if(!TestFalse(TEXT("R04 never boundary stabilization"),S.bBottomContact||S.bSurfaceContact)){return false;}
		}
		const float Change=C.Get().DepthM-Start;
		AddInfo(FString::Printf(TEXT("R04 mass=%.0f change=%.6f width=%.6f"),C.Equipment.TotalMassG,Change,Max-Min));
		if(Index==0){TestTrue(TEXT("Light rises at least 0.2m"),Change<=-.2f);}
		if(Index==1){TestTrue(TEXT("Middle full observation width <=0.1m"),Max-Min<=.1f);}
		if(Index==2){TestTrue(TEXT("Heavy sinks at least 0.2m"),Change>=.2f);}
		C.Geometry(*this); ++Index;
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSpatialFieldTest,"TipRun.M105C.DepthFieldLineDragAndBottom",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRSpatialFieldTest::RunTest(const FString& Parameters)
{
	FVector Velocities[2];
	for(int32 Case=0;Case<2;++Case)
	{
		FSpatialCase C; C.F.Area->Settings.CurrentMode=ETRCurrentFieldMode::DepthProfile;
		FTRCurrentDepthKey Top,Bottom; Top.DepthM=0;Top.CurrentMps=FVector(.4,0,0);Bottom.DepthM=30;Bottom.CurrentMps=FVector(-.4,0,0);
		C.F.Area->Settings.CurrentDepthProfile={Top,Bottom};
		if(!C.Start(*this,TEXT("Egi_3_5"),TEXT("Sinker_None"),FVector(-1,2,Case==0?-2:-25),100,false)){return false;}
		for(int32 I=0;I<60;++I){C.Step(false);} Velocities[Case]=C.Get().VelocityMps;
		TestTrue(TEXT("Snapshot current is evaluated at final depth"),FMath::Abs(C.Get().CurrentAtEgiDepthMps.X-(.4-.8*C.Get().DepthM/30))<1.e-6);
	}
	TestTrue(TEXT("Deep/shallow currents reverse horizontal movement"),Velocities[0].X>0&&Velocities[1].X<0);
	FVector DragPositions[2];
	for(int32 Case=0;Case<2;++Case)
	{
		FSpatialCase C; C.F.Tuning->Parameters.LineDragKgPerMS=Case==0?0:.003;
		C.F.Tuning->Parameters.TautLineTransfer01=1; C.F.Tuning->Parameters.SlackLineTransfer01=1;
		if(!C.Start(*this,TEXT("Egi_3_5"),TEXT("Sinker_None"),FVector(-1,2,-10),100,false)){return false;}
		int32 WetQueries=0;
		C.Override=[&](const FTROceanQuery& Q)
		{
			auto S=C.F.World->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(Q);
			S.CurrentMps=Q.DepthM<8?FVector(.5,0,0):FVector::ZeroVector;
			if(Q.DepthM>0&&Q.DepthM<8){++WetQueries;} return S;
		};
		for(int32 I=0;I<60;++I){C.Step(false);} DragPositions[Case]=C.Get().WorldPositionM;
		if(Case==1){TestTrue(TEXT("Submerged line queried independently"),WetQueries>0);}
	}
	TestTrue(TEXT("Line drag acts even when Egi endpoint current is zero"),DragPositions[1].X>DragPositions[0].X+.01);
	for(float Depth:{.5f,3.f,30.f})
	{
		FSpatialCase C; C.F.Area->Settings.FlatDepthM=Depth;
		if(!C.Start(*this)){return false;} int32 Hits=0;
		for(int32 I=0;I<5000;++I){auto E=C.Step(false);if(E==ETREgiStepEvent::ReachedBottom){++Hits;break;}}
		C.Action.FishingState=ETRFishingState::BottomContact; C.Action.LineMode=ETRLineMode::Locked;
		for(int32 I=0;I<30;++I){if(C.Step(false)==ETREgiStepEvent::ReachedBottom){++Hits;}}
		TestEqual(TEXT("Single bottom event"),Hits,1); TestEqual(TEXT("World bottom exact"),C.Get().WorldPositionM.Z,-double(Depth)); C.Geometry(*this);
	}
	// Value-provider seam for a future slope: no production terrain mode is added.
	FSpatialCase Slope; Slope.F.Area->Settings.CurrentMps=FVector(.4,0,0);
	if(!Slope.Start(*this)){return false;}
	int32 Queries=0;
	Slope.Override=[&](const FTROceanQuery& Q)
	{
		auto S=Slope.F.World->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(Q);
		S.BottomDepthM=float(3-.05*Q.PositionXYM.X); ++Queries; return S;
	};
	for(int32 I=0;I<1000;++I)
	{
		if(!TestTrue(TEXT("Changing bottom remains valid"),Slope.Step(false)!=ETREgiStepEvent::EnvironmentInvalid)){return false;}
		if(!TestTrue(TEXT("Destination depth prevents penetration"),Slope.Get().DepthM<=3-.05*Slope.Get().WorldPositionM.X+1.e-5)){return false;}
	}
	TestTrue(TEXT("Moving destination queried"),Queries>1000); Slope.Geometry(*this); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSpatialSafetyTest,"TipRun.M105C.SafetyAndSnapshotContract",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRSpatialSafetyTest::RunTest(const FString& Parameters)
{
	for(double Dt:{1.0/60,1.0/30,.25,1.0})
	{
		FSpatialCase C; if(!C.Start(*this)){return false;} C.Time.StepSeconds=Dt;
		for(int32 I=0;I<10;++I){if(!TestTrue(TEXT("Large step finite"),C.Step(false)!=ETREgiStepEvent::EnvironmentInvalid)){return false;} C.Geometry(*this);}
	}
	for(int32 Bad=0;Bad<4;++Bad)
	{
		FSpatialCase C; if(!C.Start(*this)){return false;} const auto Before=C.Get();
		if(Bad==0){C.Time.StepSeconds=1000;}
		if(Bad==1){C.Boat.RodTipM.X=100000;}
		if(Bad>=2){C.Override=[&](const FTROceanQuery& Q){auto O=C.F.World->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(Q);if(Bad==2){O.bValid=false;}else{O.CurrentMps.X=std::numeric_limits<double>::infinity();}return O;};}
		TestTrue(TEXT("Unsafe candidate rejected"),C.Step(false)==ETREgiStepEvent::EnvironmentInvalid);
		TestTrue(TEXT("Failure never commits warp or NaN"),C.Get().WorldPositionM.Equals(Before.WorldPositionM,0));
	}
	FSpatialCase C; if(!C.Start(*this)){return false;}
	// World-coordinate callers need not maintain legacy XY/depth input copies.
	TStrongObjectPtr<UTREgiSimulationComponent> NewEgi{NewObject<UTREgiSimulationComponent>()};
	FTREgiSnapshot Initial; Initial.CastId=FTRCastId(1); Initial.bWorldPositionValid=true;
	Initial.WorldPositionM=C.Get().WorldPositionM; Initial.LineLengthM=10;
	Initial.DepthM=std::numeric_limits<float>::quiet_NaN(); Initial.PositionXYM=FVector2D(999,999);
	FTROceanQuery InitialQ; InitialQ.PositionXYM=C.Get().PositionXYM; TArray<FText> InitialErrors;
	TestTrue(TEXT("World position overrides legacy input copies"),NewEgi->InitializeCast(Initial,C.Equipment,C.Sea(InitialQ),InitialErrors,&C.Boat));
	TestEqual(TEXT("Initial derived surface contact"),NewEgi->BuildSnapshot(C.Action.FishingState).DepthM,0.f);
	for(int32 Bad=0;Bad<5;++Bad)
	{
		auto P=C.Equipment.Parameters;
		if(Bad==0){P.VerticalResponsePerS=0;} if(Bad==1){P.LineSlackAllowanceM=-1;}
		if(Bad==2){P.LineDragKgPerMS=std::numeric_limits<double>::quiet_NaN();}
		if(Bad==3){P.SlackLineTransfer01=2;} if(Bad==4){P.EgiModelRevision=1;}
		TArray<FText> Errors; TestFalse(TEXT("Bad spatial parameters rejected"),P.Validate(Errors));
	}
	TArray<uint8> Bytes; FObjectWriter Writer(C.F.Tuning.Get(),Bytes);
	TStrongObjectPtr<UTRFishingTuningDataAsset> Copy{NewObject<UTRFishingTuningDataAsset>()}; FObjectReader Reader(Copy.Get(),Bytes);
	TestEqual(TEXT("Revision round trip"),Copy->Parameters.EgiModelRevision,2);
	TestEqual(TEXT("Line drag round trip"),Copy->Parameters.LineDragKgPerMS,C.Equipment.Parameters.LineDragKgPerMS);
	C.F.Tuning->Parameters.VerticalResponsePerS=0; C.Step(false); TestTrue(TEXT("Frozen parameters unaffected"),C.Get().DepthM>0);
	const auto Before=C.Get(); ++C.Time.TickIndex; C.Boat.Tick=C.Time.TickIndex;
	FTROceanQuery Q;Q.SimTick=C.Time.TickIndex;
	TestTrue(TEXT("Old cast ignored"),C.Egi->StepEgi(FTRCastId(999),C.Time,C.Sea(Q),C.Boat,C.Action,[&](const FTROceanQuery& D){return C.Sea(D);})==ETREgiStepEvent::None);
	TestEqual(TEXT("Snapshot read/old cast leave tick"),C.Get().Tick,Before.Tick);
	C.Egi->Reset(); TestTrue(TEXT("Destroyed cast ignored"),C.Egi->StepEgi(Before.CastId,C.Time,C.Sea(Q),C.Boat,C.Action,[&](const FTROceanQuery& D){return C.Sea(D);})==ETREgiStepEvent::None);
	FSpatialCase Limited; Limited.F.Tuning->Parameters.PayoutMps=.001f;
	if(!Limited.Start(*this)){return false;}
	for(int32 I=0;I<120;++I)
	{
		if(!TestTrue(TEXT("Limited payout solves taut constraint"),Limited.Step(false)!=ETREgiStepEvent::EnvironmentInvalid)){return false;}
		Limited.Geometry(*this);
	}
	TestTrue(TEXT("Demand never exceeds spool rate"),Limited.Get().LineLengthM<=1.5021f);
	TestTrue(TEXT("Insufficient payout limits sink rather than inventing line"),Limited.Get().DepthM<.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSpatialSessionTest,"TipRun.M105C.FixedFramesPauseSessionAndVisual",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRSpatialSessionTest::RunTest(const FString& Parameters)
{
	FTREgiSnapshot Reference;
	for(int32 Fps:{30,60,120})
	{
		FTRTestSession F; SpatialTestTuning(F); F.Area->Settings.CurrentMps=FVector(.3,0,0);
		if(!F.Start(*this)){return false;} F.Deploy();
		for(int32 I=0;I<Fps*3;++I){F.Sim()->AdvanceFrame(1.0/Fps);}
		const auto S=F.Session->Fishing->GetSnapshot();
		TestTrue(TEXT("Session uses revision 2"),S.EgiModelRevision==2&&S.DepthM>0);
		if(Fps==30){Reference=S;}else{TestTrue(TEXT("Fixed frames same space and line"),S.WorldPositionM.Equals(Reference.WorldPositionM,0)&&S.LineLengthM==Reference.LineLengthM&&S.Tick==Reference.Tick);}
		TestTrue(TEXT("Visual uses world meters at cm boundary"),F.Session->GetEgiActor()->GetActorLocation().Equals(TRUnits::MetersToCentimeters(S.WorldPositionM),1.e-6));
		F.Sim()->SetSimulationPaused(true); F.Sim()->AdvanceFrame(1);
		TestTrue(TEXT("Pause freezes position"),F.Session->Fishing->GetSnapshot().WorldPositionM.Equals(S.WorldPositionM,0)); F.Sim()->SetSimulationPaused(false);
		F.Session->SubmitCommand(ETRFishingCommandType::Jerk,FTRCastId(S.CastId.Value+100)); F.Step();
		TestEqual(TEXT("Old cast input no operation"),F.Session->Fishing->GetSnapshot().JerkCount,int64(0));
		TWeakObjectPtr<UTREgiSimulationComponent> Weak=F.Session->EgiSimulation;
		const auto Id=F.Session->GetRegistrationId(); F.Session->DispatchBeginPlay();F.Session->Destroy();F.Step();
		TestFalse(TEXT("Session removed"),F.Sim()->IsRegistered(Id));
		F.Shutdown();CollectGarbage(RF_NoFlags); TestFalse(TEXT("No retained simulation after world GC"),Weak.IsValid());
	}
	return true;
}
#endif
