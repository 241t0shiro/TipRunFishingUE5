#include "Boat/TRBoatPawn.h"
#include "Boat/TRBoatDriftComponent.h"
#include "Data/TREnvironmentUnits.h"
#include "Data/TRBoatTuningDataAsset.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "TRSessionTestFixture.h"
#include "Components/SceneComponent.h"
#include "Misc/DataValidation.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include <limits>
#include <cmath>

namespace
{
	FTRBoatParameters TestWindTuning()
	{
		FTRBoatParameters P;
		P.ModelRevision=2; P.WindResponseKgPerS=.1; P.CurrentResponseKgPerS=1;
		P.DragKgPerS=1; P.InertiaKg=10;
		P.BowWindScale=1; P.SternWindScale=.5; P.SideWindScale=2;
		P.MaxDriftSpeedMps=1; P.HullHeightOffsetM=.5; P.RodAnchorOffsetM=FVector(2,1,1);
		return P;
	}
	void SetupWindTest(FTRTestSession& F)
	{
		F.BoatTuning->Parameters=TestWindTuning();
		F.Area->Settings.FieldRevision=2;
		F.Area->Settings.WindMps=FVector2D(2,0);
		TREnvironmentUnits::TryKnotsToMps(.7,F.Area->Settings.CurrentMps.X);
	}
	FTROceanSample SampleWind(FVector2D Wind, FVector Current, int64 Tick=0)
	{
		FTROceanSample S; S.bValid=true; S.InvalidReason=ETRSampleError::None;
		S.SampleTick=Tick; S.BottomDepthM=30; S.FieldRevision=2;
		S.WindMps=Wind; S.SurfaceCurrentMps=Current;
		S.CurrentMps=FVector(9,8,0); // Deliberately different depth current; Boat must not use it.
		return S;
	}
	bool StartDirect(FAutomationTestBase& Test,FTRTestSession& F,const FTROceanSample& S,float Heading=0)
	{
		TArray<FText> Errors;
		const bool Ok=F.Boat->InitializeBoat({},Heading,S,Errors);
		for (const auto& E:Errors) { Test.AddError(E.ToString()); }
		return Test.TestTrue(TEXT("Revision 2 initialization"),Ok);
	}
	void DirectStep(FTRTestSession& F,FTROceanSample S,double Dt,int64 Tick)
	{
		S.SampleTick=Tick; FTRSimTime Time; Time.TickIndex=Tick; Time.StepSeconds=Dt;
		F.Boat->StepBoat(Time,S,[](const FVector2D&) { return true; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRWindAnalyticTest,"TipRun.M105B.AnalyticResponseAndContributions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRWindAnalyticTest::RunTest(const FString& Parameters)
{
	FTRTestSession F; SetupWindTest(F);
	auto& P=F.BoatTuning->Parameters;
	P.WindResponseKgPerS=.2; P.CurrentResponseKgPerS=.8; P.DragKgPerS=1; P.InertiaKg=4;
	P.BowWindScale=P.SternWindScale=P.SideWindScale=1;
	const auto S=SampleWind(FVector2D(2,0),FVector(.5,0,0));
	if (!StartDirect(*this,F,S)) { return false; }
	FVector Previous=FVector::ZeroVector;
	for(int32 I=0;I<120;++I)
	{
		DirectStep(F,S,1.0/60,I);
		const auto B=F.Boat->GetBoatSnapshot();
		TestTrue(TEXT("Integrated contributions reconstruct velocity change"),
			(Previous+B.WindDeltaVelocityMps+B.CurrentDeltaVelocityMps+B.DragDeltaVelocityMps).Equals(B.VelocityMps,1e-10));
		Previous=B.VelocityMps;
	}
	const auto B=F.Boat->GetBoatSnapshot();
	// Independent solution: target .4 m/s, tau 2 seconds, elapsed 2 seconds.
	TestTrue(TEXT("Analytic velocity"),B.VelocityMps.Equals(FVector(.4*(1-std::exp(-1.0)),0,0),1e-10));
	TestTrue(TEXT("Analytic displacement"),FMath::Abs(B.PositionM.X-(.8-.8*(1-std::exp(-1.0))))<1e-10);
	TestTrue(TEXT("Independent surface current retained"),B.SurfaceCurrentMps.Equals(FVector(.5,0,0)));
	TestTrue(TEXT("Independent wind retained"),B.WindMps.Equals(FVector2D(2,0)));
	TestEqual(TEXT("Speed derived from velocity"),B.SpeedMps,B.VelocityMps.Size2D());
	TestTrue(TEXT("Not surface current copy"),B.SpeedMps<.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRWindDirectionsTest,"TipRun.M105B.WindCurrentHeadingAndRepresentativeSpeeds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRWindDirectionsTest::RunTest(const FString& Parameters)
{
	FTRTestSession F; SetupWindTest(F);
	if (!StartDirect(*this,F,SampleWind({},{}))) { return false; }
	DirectStep(F,SampleWind({},{}),10,0);
	TestTrue(TEXT("No wind/current creates no motion"),F.Boat->GetBoatSnapshot().VelocityMps.IsZero() && F.Boat->GetBoatSnapshot().PositionM.Size2D()==0);
	for (double Knots : {.4,.7,1.0})
	{
		double U=0; TREnvironmentUnits::TryKnotsToMps(Knots,U);
		FVector Results[3]; int32 Case=0;
		for (FVector Direction : {FVector(1,0,0),FVector(-1,0,0),FVector(0,-1,0)})
		{
			const auto S=SampleWind(FVector2D(2,0),Direction*U);
			if (!StartDirect(*this,F,S,float(UE_DOUBLE_PI/2))) { return false; }
			for(int32 I=0;I<600;++I)
			{
				DirectStep(F,S,1.0/60,I);
				const auto B=F.Boat->GetBoatSnapshot();
				TestTrue(TEXT("Representative test speed below .5 m/s without guard"),B.SpeedMps<.5 && F.Boat->GetBoatMode()==ETRBoatMode::DriftOnly);
			}
			const auto B=F.Boat->GetBoatSnapshot(); Results[Case++]=B.VelocityMps;
			TestTrue(TEXT("Heading remains north"),B.ForwardVector.Equals(FVector(0,1,0),1e-6));
		}
		TestFalse(TEXT("Opposed differs from aligned"),Results[0].Equals(Results[1],1e-4));
		TestTrue(TEXT("Orthogonal wind and tide produce southeast drift"),Results[2].X>0 && Results[2].Y<0);
		TestFalse(TEXT("Orthogonal differs from aligned"),Results[0].Equals(Results[2],1e-4));
		const auto S=SampleWind({},FVector(0,U,0));
		if (!StartDirect(*this,F,S)) { return false; }
		DirectStep(F,S,1.0/60,0);
		TestTrue(TEXT("Current-only finite response, not instant matching"),F.Boat->GetBoatSnapshot().VelocityMps.Y>0 && F.Boat->GetBoatSnapshot().VelocityMps.Y<U);
	}
	double Speeds[3]; int32 Case=0;
	for(float Heading : {0.f,float(UE_DOUBLE_PI/2),float(UE_DOUBLE_PI)})
	{
		const auto S=SampleWind(FVector2D(2,0),{});
		if (!StartDirect(*this,F,S,Heading)) { return false; }
		DirectStep(F,S,1,0); Speeds[Case++]=F.Boat->GetBoatSnapshot().SpeedMps;
	}
	TestTrue(TEXT("Broadside wind response exceeds stern and bow test response"),Speeds[1]>Speeds[2] && Speeds[2]>Speeds[0] && Speeds[0]>0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRWindInertiaTest,"TipRun.M105B.InertiaChangesAndNumericalGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRWindInertiaTest::RunTest(const FString& Parameters)
{
	FTRTestSession F; SetupWindTest(F);
	const auto On=SampleWind(FVector2D(2,0),FVector(.3,0,0));
	if (!StartDirect(*this,F,On)) { return false; }
	DirectStep(F,On,1,0); const double Light=F.Boat->GetBoatSnapshot().SpeedMps;
	F.BoatTuning->Parameters.InertiaKg=20;
	if (!StartDirect(*this,F,On)) { return false; }
	DirectStep(F,On,1,0); const double Heavy=F.Boat->GetBoatSnapshot().SpeedMps;
	TestTrue(TEXT("Higher inertia slows acceleration"),Heavy>0 && Heavy<Light);
	DirectStep(F,SampleWind({},{}),1.0/60,1);
	TestTrue(TEXT("Stopping inputs decays speed without zero jump"),F.Boat->GetBoatSnapshot().SpeedMps>0 && F.Boat->GetBoatSnapshot().SpeedMps<Heavy);
	const auto Before=F.Boat->GetBoatSnapshot();
	DirectStep(F,SampleWind(FVector2D(-2,0),FVector(-.3,0,0)),1.0/60,2);
	TestTrue(TEXT("Reversal preserves inertia"),F.Boat->GetBoatSnapshot().VelocityMps.X>0 && F.Boat->GetBoatSnapshot().SpeedMps<Before.SpeedMps);
	for(double Dt : {1e-10,1.0/60,.25,1.0,1000.0})
	{
		if (!StartDirect(*this,F,On)) { return false; }
		DirectStep(F,On,Dt,0);
		TestTrue(TEXT("Small/large steps remain finite and active"),F.Boat->GetBoatMode()==ETRBoatMode::DriftOnly && !F.Boat->GetBoatSnapshot().PositionM.ContainsNaN());
	}
	F.BoatTuning->Parameters.MaxDriftSpeedMps=.00001;
	if (!StartDirect(*this,F,On)) { return false; }
	const auto Initial=F.Boat->GetBoatSnapshot(); int32 Failures=0;
	F.Boat->OnEnvironmentInvalid.AddLambda([&](ETRSampleError){++Failures;});
	DirectStep(F,On,1,0); DirectStep(F,On,1,1);
	TestTrue(TEXT("Speed guard disables instead of clamping"),F.Boat->GetBoatMode()==ETRBoatMode::Disabled);
	TestTrue(TEXT("Failed candidate not committed"),F.Boat->GetBoatSnapshot().PositionM.Equals(Initial.PositionM,0));
	TestEqual(TEXT("Guard notification once"),Failures,1);
	F.Boat->OnEnvironmentInvalid.Clear();
	F.BoatTuning->Parameters=TestWindTuning();
	if (!StartDirect(*this,F,On)) { return false; }
	FTRSimTime Time; Time.StepSeconds=1.0/60;
	const auto BeforeStop=F.Boat->GetBoatSnapshot();
	TestFalse(TEXT("Stop during destination validation prevents commit"),F.Boat->DriftComponent->StepDrift(Time,On,[&](const FVector2D&)
	{
		F.Boat->DriftComponent->StopMotion(); return true;
	}));
	TestTrue(TEXT("No post-stop displacement"),F.Boat->GetBoatSnapshot().PositionM.Equals(BeforeStop.PositionM,0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRWindValidationTest,"TipRun.M105B.ValidationSerializationAndFrozenTuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRWindValidationTest::RunTest(const FString& Parameters)
{
	FTRTestSession F; SetupWindTest(F); const auto Good=F.BoatTuning->Parameters;
	for(int32 Case=0;Case<10;++Case)
	{
		F.BoatTuning->Parameters=Good; auto& P=F.BoatTuning->Parameters;
		switch(Case)
		{
		case 0:P.InertiaKg=0;break;
		case 1:P.WindResponseKgPerS=-1;break;
		case 2:P.CurrentResponseKgPerS=std::numeric_limits<double>::infinity();break;
		case 3:P.DragKgPerS=std::numeric_limits<double>::quiet_NaN();break;
		case 4:P.SideWindScale=-1;break;
		case 5:P.WindResponseKgPerS=std::numeric_limits<double>::max();P.SideWindScale=10;break;
		case 6:P.ModelRevision=99;break;
		case 7:P.CurrentResponse01=.5;break;
		case 8:P.WindResponseKgPerS=P.CurrentResponseKgPerS=P.DragKgPerS=0;break;
		case 9:P.MaxDriftSpeedMps=0;break;
		}
		TArray<FText> Errors; TestFalse(TEXT("Invalid tuning rejected"),P.Validate(Errors));
		FDataValidationContext Context;
		TestTrue(TEXT("Editor validator agrees"),F.BoatTuning->IsDataValid(Context)==EDataValidationResult::Invalid);
	}
	F.BoatTuning->Parameters=Good;
	TArray<uint8> Bytes; FObjectWriter Writer(F.BoatTuning.Get(),Bytes);
	TStrongObjectPtr<UTRBoatTuningDataAsset> Copy(NewObject<UTRBoatTuningDataAsset>());
	FObjectReader Reader(Copy.Get(),Bytes);
	TestEqual(TEXT("Model revision serialized"),Copy->Parameters.ModelRevision,2);
	TestEqual(TEXT("Side scale serialized"),Copy->Parameters.SideWindScale,2.0);
	const auto S=SampleWind(FVector2D(2,0),{});
	if (!StartDirect(*this,F,S)) { return false; }
	F.BoatTuning->Parameters.WindResponseKgPerS=0; F.BoatTuning->Parameters.RodAnchorOffsetM=FVector(100);
	DirectStep(F,S,1,0);
	TestTrue(TEXT("Frozen wind response still acts"),F.Boat->GetBoatSnapshot().SpeedMps>0);
	TestTrue(TEXT("Frozen rod offset"),(F.Boat->GetBoatSnapshot().RodTipM-F.Boat->GetBoatSnapshot().PositionM).Equals(Good.RodAnchorOffsetM));
	for(int32 Case=0;Case<3;++Case)
	{
		F.BoatTuning->Parameters=Good; if(!StartDirect(*this,F,S)){return false;}
		auto Bad=S;
		if(Case==0){Bad.WindMps.X=std::numeric_limits<double>::quiet_NaN();}
		if(Case==1){Bad.SurfaceCurrentMps.Y=std::numeric_limits<double>::infinity();}
		if(Case==2){Bad.SurfaceCurrentMps.Z=1;}
		DirectStep(F,Bad,1,0);
		TestTrue(TEXT("Invalid environmental input disables motion"),F.Boat->GetBoatMode()==ETRBoatMode::Disabled);
		TestFalse(TEXT("No nonfinite committed state"),F.Boat->GetBoatSnapshot().VelocityMps.ContainsNaN());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRWindIntegrationTest,"TipRun.M105B.FixedFramesRodAndSessionLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRWindIntegrationTest::RunTest(const FString& Parameters)
{
	FTRBoatSnapshot Reference;
	for(int32 Fps : {30,60,120})
	{
		FTRTestSession F; SetupWindTest(F);
		if(!F.Start(*this)){return false;}
		F.Deploy();
		for(int32 Frame=0;Frame<Fps*2;++Frame){F.Sim()->AdvanceFrame(1.0/Fps);}
		const auto B=F.Boat->GetBoatSnapshot();
		TestTrue(TEXT("A field supplies wind and surface independently"),B.WindMps.Equals(FVector2D(2,0)) && B.SurfaceCurrentMps.Equals(F.Area->Settings.CurrentMps));
		TestTrue(TEXT("Rod follows boat"),(B.RodTipM-B.PositionM).Equals(TestWindTuning().RodAnchorOffsetM,1e-6));
		TestTrue(TEXT("Actor and numerical boat position agree"),F.Boat->GetActorLocation().Equals(TRUnits::MetersToCentimeters(B.PositionM),1e-5));
		TestTrue(TEXT("Scene rod and numerical rod agree"),F.Boat->RodAnchor->GetComponentLocation().Equals(TRUnits::MetersToCentimeters(B.RodTipM),1e-5));
		const auto E=F.Session->Fishing->GetSnapshot();
		TestEqual(TEXT("Existing M08 consumes boat in same tick"),E.Tick,B.Tick);
		TestTrue(TEXT("Existing line remains valid"),FVector(E.PositionXYM.X-B.RodTipM.X,E.PositionXYM.Y-B.RodTipM.Y,
			F.Area->Settings.SurfaceZ_M-E.DepthM-B.RodTipM.Z).Size()<=E.LineLengthM+1e-3);
		if(Fps==30){Reference=B;}else
		{
			TestTrue(TEXT("Identical fixed positions"),B.PositionM.Equals(Reference.PositionM,0));
			TestTrue(TEXT("Identical fixed velocities"),B.VelocityMps.Equals(Reference.VelocityMps,0));
			TestTrue(TEXT("Identical wind contributions"),B.WindDeltaVelocityMps.Equals(Reference.WindDeltaVelocityMps,0));
		}
		F.Sim()->SetSimulationPaused(true);F.Sim()->AdvanceFrame(1);
		TestTrue(TEXT("Paused boat unchanged"),F.Boat->GetBoatSnapshot().PositionM.Equals(B.PositionM,0));
		F.Sim()->SetSimulationPaused(false);
		const auto SessionId=F.Session->GetRegistrationId();
		F.Session->DispatchBeginPlay();F.Session->Destroy();F.Step();
		TestFalse(TEXT("Destroyed session removed"),F.Sim()->IsRegistered(SessionId));
		// Boat is world-owned: ending fishing must not reset environmental drift.
		TestTrue(TEXT("World boat independent from destroyed session"),F.Boat->GetBoatSnapshot().Tick>B.Tick);
		F.Sim()->Unregister(F.BoatId); const auto Stopped=F.Boat->GetBoatSnapshot();F.Step(3);
		TestEqual(TEXT("Unregistered boat not updated"),F.Boat->GetBoatSnapshot().Tick,Stopped.Tick);
		FTRBoatSnapshot Out;TestFalse(TEXT("Unregistered boat not exposed"),F.Sim()->GetBoatSnapshot(F.BoatId,Out));
		F.Boat->DispatchBeginPlay();TWeakObjectPtr<ATRBoatPawn> WeakBoat=F.Boat;
		F.Shutdown();
		// DestroyWorld releases ownership; UObject reclamation occurs on garbage collection.
		CollectGarbage(RF_NoFlags);
		TestFalse(TEXT("World end leaves no retained boat after GC"),WeakBoat.IsValid());
	}
	return true;
}
#endif
