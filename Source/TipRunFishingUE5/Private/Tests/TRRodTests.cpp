#include "TRSessionTestFixture.h"
#include "Fishing/TRRodControlComponent.h"
#include "Game/TRPlayerController.h"
#include "EnhancedInputComponent.h"
#include "Misc/DataValidation.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
	FTRRodParameters RodTestParameters()
	{
		FTRRodParameters P; P.MinPitchRad=0;P.MaxPitchRad=1.2;P.MinYawRad=-.6;P.MaxYawRad=.6;
		P.InitialPitchRad=.1;P.InitialYawRad=0;P.SensitivityXRad=.01;P.SensitivityYRad=.02;
		P.MaxMouseDelta=50;P.MaxAimRateRadPerS=1.2;P.LengthM=2;P.MountOffsetM=FVector(0,0,1);
		P.ShakuriAmplitudeRad=.3;P.ShakuriUpSeconds=.15;P.ShakuriReturnSeconds=.25;return P;
	}
	struct FRodSession
	{
		FTRTestSession F;
		TStrongObjectPtr<UTRRodTuningDataAsset> Tuning{NewObject<UTRRodTuningDataAsset>()};
		TWeakObjectPtr<ATRPlayerController> PC;
		FRodSession()
		{
			Tuning->Parameters=RodTestParameters();F.Session->RodTuning=Tuning.Get();
			auto& P=F.Tuning->Parameters;P.EgiModelRevision=2;P.VerticalResponsePerS=2;P.LineDragKgPerMS=.0001;
			P.TautLineTransfer01=.05;P.SlackLineTransfer01=0;P.LineSlackAllowanceM=.1;P.MaxStepTravelM=20;
			P.MaxEgiSpeedMps=10;P.PayoutMps=4;P.MaxLineLengthM=200;
			F.Area->Settings.FieldRevision=2;F.Area->Settings.CurrentMps={};F.Area->Settings.WindMps={};
			auto& B=F.BoatTuning->Parameters;B={};B.ModelRevision=2;B.WindResponseKgPerS=.1;B.CurrentResponseKgPerS=1;
			B.DragKgPerS=1;B.InertiaKg=10;B.BowWindScale=1;B.SternWindScale=.5;B.SideWindScale=2;
			B.MaxDriftSpeedMps=1;B.HullHeightOffsetM=.5;B.RodAnchorOffsetM=FVector(2,1,1);
		}
		bool Start(FAutomationTestBase& Test,bool Deploy=true)
		{
			if(!F.Start(Test)){return false;}
			if(Deploy){F.Deploy();F.Step(240);F.Session->SubmitCommand(ETRFishingCommandType::TensionFall,F.Session->GetCastId());F.Step();}
			PC=F.World->SpawnActor<ATRPlayerController>();PC->InputConfig=UTRInputConfigDataAsset::CreateMouseRodPrototype(PC.Get());
			return Test.TestTrue(TEXT("Mouse controller bound"),PC->BindSession(F.Session.Get()));
		}
		FTRRodSnapshot Rod()const{return F.Session->RodControl->GetSnapshot();}
		bool Geometry(FAutomationTestBase& Test)const
		{
			const auto R=Rod(); const auto E=F.Session->Fishing->GetSnapshot();
			return Test.TestTrue(TEXT("Rod feeds same-tick finite line geometry"),R.bValid&&R.Tick==E.Tick&&
				(E.WorldPositionM-R.TipWorldPositionM).Size()<=E.LineLengthM+1.e-5 && !E.WorldPositionM.ContainsNaN() &&
				R.TipWorldRotation.GetForwardVector().Equals(R.TipDirection,1.e-8));
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRodAimTest,"TipRun.M105D.AimQueueClampAndTransform",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRodAimTest::RunTest(const FString& Parameters)
{
	FRodSession R;if(!R.Start(*this)){return false;}
	const auto Before=R.Rod();TestTrue(TEXT("Mouse event queued"),R.PC->SubmitMouseDelta(FVector2D(1,1)));
	TestEqual(TEXT("No immediate simulation mutation"),R.Rod().BaseYawRad,Before.BaseYawRad);
	R.F.Step();TestTrue(TEXT("Mouse X sensitivity yaw"),FMath::IsNearlyEqual(R.Rod().BaseYawRad,Before.BaseYawRad+.01,1.e-9));
	TestTrue(TEXT("Mouse Y sensitivity pitch"),FMath::IsNearlyEqual(R.Rod().BasePitchRad,Before.BasePitchRad+.02,1.e-9));
	R.Tuning->Parameters.SensitivityXRad=999;R.PC->SubmitMouseDelta(FVector2D(1,0));R.F.Step();
	TestTrue(TEXT("Frozen tuning"),FMath::IsNearlyEqual(R.Rod().BaseYawRad,Before.BaseYawRad+.02,1.e-9));
	for(int32 I=0;I<100;++I){R.PC->SubmitMouseDelta(FVector2D(1.e10,1.e10));R.F.Step();}
	TestTrue(TEXT("Base angles clamped"),R.Rod().BaseYawRad<=.6&&R.Rod().BasePitchRad<=1.2);R.Geometry(*this);
	TestFalse(TEXT("Nonfinite input rejected"),R.PC->SubmitMouseDelta(FVector2D(std::numeric_limits<double>::infinity(),0)));
	// Isolated transform with moved and rotated B snapshot.
	TStrongObjectPtr<UTRRodControlComponent> Rod{NewObject<UTRRodControlComponent>()};TArray<FText> Errors;
	TestTrue(TEXT("Rod parameters initialize"),Rod->Initialize(RodTestParameters(),1.0/60,Errors));
	FTRSimTime T;T.StepSeconds=1.0/60;FTRBoatSnapshot Boat;Boat.PositionM=FVector(5,7,2);Boat.HeadingRad=float(UE_DOUBLE_PI/2);
	FTREgiSnapshot E;TestTrue(TEXT("World transform valid"),Rod->Step(T,Boat,E,0));
	const auto S=Rod->GetSnapshot();const FVector Expected=Boat.PositionM+FVector(0,0,1)+FVector(FMath::Cos(double(Boat.HeadingRad))*FMath::Cos(.1),FMath::Sin(double(Boat.HeadingRad))*FMath::Cos(.1),FMath::Sin(.1))*2;
	TestTrue(TEXT("Boat heading/position and rod length compose"),S.TipWorldPositionM.Equals(Expected,1.e-9));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRodJerkTest,"TipRun.M105D.ClickProfileQueueAndLine",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRodJerkTest::RunTest(const FString& Parameters)
{
	for(int32 Clicks:{1,3,11,20})
	{
		FRodSession R;if(!R.Start(*this)){return false;}
		const auto Initial=R.F.Session->Fishing->GetSnapshot();const auto Base=R.Rod();
		TestTrue(TEXT("Initial Stay"),Initial.FishingState==ETRFishingState::Stay);
		for(int32 I=0;I<Clicks;++I)
		{
			TestTrue(TEXT("RMB Started uses Jerk command"),R.PC->ActionStarted(ETRPlayerAction::Jerk));
			TestFalse(TEXT("Held/repeat cannot duplicate"),R.PC->ActionStarted(ETRPlayerAction::Jerk));R.PC->ActionReleased(ETRPlayerAction::Jerk);
		}
		int32 TFTransitions=0;
		R.F.Session->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState From,ETRFishingState To){if(From==ETRFishingState::Jerking&&To==ETRFishingState::TensionFall){++TFTransitions;}});
		bool Moved=false,Lifted=false,SawTF=false;int64 MaxPending=0;
		for(int32 I=0;I<Clicks*30+60;++I)
		{
			R.F.Step();const auto S=R.F.Session->Fishing->GetSnapshot(); const auto Rod=R.Rod();
			if(!TestFalse(TEXT("Rod motion must not abort C simulation"),R.F.Session->HasResult())){return false;}
			Moved |= Rod.FinalPitchRad>Rod.BasePitchRad+.01;Lifted |= S.DepthM<Initial.DepthM-.01;SawTF |= S.FishingState==ETRFishingState::TensionFall;
			MaxPending=FMath::Max(MaxPending,S.PendingJerkCount);if(!R.Geometry(*this)){return false;}
			TestEqual(TEXT("Shakuri does not reel line"),S.LineLengthM,Initial.LineLengthM);
		}
		const auto S=R.F.Session->Fishing->GetSnapshot();TestEqual(TEXT("Clicks equal actual actions"),S.JerkCount,int64(Clicks));
		TestEqual(TEXT("Existing pending queue used"),MaxPending,int64(Clicks-1));
		TestTrue(TEXT("Rod physically moves and pulls Egi"),Moved&&Lifted);
		TestTrue(TEXT("Returns to Stay"),S.FishingState==ETRFishingState::Stay);
		TestEqual(TEXT("Every operation passes through TensionFall"),TFTransitions,Clicks);
		TestTrue(TEXT("Returns to undamaged base pose"),R.Rod().BasePitchRad==Base.BasePitchRad&&R.Rod().FinalPitchRad==Base.BasePitchRad);
		// TF can complete within its first fixed update, as required; it is not a timed waiting state.
		(void)SawTF;
		R.F.Session->Fishing->OnFishingStateChanged.Clear();
	}
	FRodSession Bottom;if(!Bottom.Start(*this)){return false;}
	Bottom.PC->ActionStarted(ETRPlayerAction::Fall);Bottom.PC->ActionReleased(ETRPlayerAction::Fall);
	for(int32 I=0;I<6000&&Bottom.F.Session->Fishing->GetState()!=ETRFishingState::BottomContact;++I){Bottom.F.Step();}
	TestTrue(TEXT("Reached Bottom for input test"),Bottom.F.Session->Fishing->GetState()==ETRFishingState::BottomContact);
	const float Depth=Bottom.F.Session->Fishing->GetSnapshot().DepthM;
	Bottom.PC->ActionStarted(ETRPlayerAction::Jerk);Bottom.PC->ActionReleased(ETRPlayerAction::Jerk);
	bool Lifted=false;
	for(int32 I=0;I<30;++I){Bottom.F.Step();Lifted|=Bottom.F.Session->Fishing->GetSnapshot().DepthM<Depth-.01;Bottom.Geometry(*this);}
	TestTrue(TEXT("Bottom Shakuri pulls up through line"),Lifted);
	TestEqual(TEXT("Bottom click counted once"),Bottom.F.Session->Fishing->GetSnapshot().JerkCount,int64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRodLifetimeTest,"TipRun.M105D.PauseFocusCastAndLifetime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRodLifetimeTest::RunTest(const FString& Parameters)
{
	FRodSession R;if(!R.Start(*this,false)){return false;}
	R.PC->SubmitMouseDelta(FVector2D(1,1));R.F.Step();const auto Ready=R.Rod();
	R.F.Session->SubmitCommand(ETRFishingCommandType::Deploy,FTRCastId(0));R.PC->SubmitMouseDelta(FVector2D(10,10));R.F.Step();
	TestEqual(TEXT("Pre-deploy accepted base preserved; old cast delta discarded"),R.Rod().BaseYawRad,Ready.BaseYawRad);
	R.F.Step(120);R.PC->SubmitMouseDelta(FVector2D(1,0));R.F.Step();
	R.PC->ActionStarted(ETRPlayerAction::Jerk);R.F.Step();R.PC->ActionReleased(ETRPlayerAction::Jerk);
	R.PC->ActionStarted(ETRPlayerAction::Jerk);R.F.Step(); // A queued second action, still held.
	R.PC->SetPauseRequested(true);const auto Paused=R.Rod();R.F.Sim()->AdvanceFrame(1);
	TestTrue(TEXT("Pause no rod progression"),R.Rod().TipWorldPositionM.Equals(Paused.TipWorldPositionM,0));
	TestFalse(TEXT("Pause rejects mouse delta"),R.PC->SubmitMouseDelta(FVector2D(999,999)));
	R.PC->SetPauseRequested(false);R.PC->SetInputFocus(false);R.PC->SetInputFocus(true);
	TestFalse(TEXT("Held RMB cannot replay after focus"),R.PC->ActionStarted(ETRPlayerAction::Jerk));
	R.F.Step(90);TestEqual(TEXT("Pending repeat canceled at next fixed update"),R.F.Session->Fishing->GetSnapshot().JerkCount,int64(1));
	const auto Id=R.F.Session->GetRegistrationId();TestFalse(TEXT("Old registration rejected"),R.F.Session->SubmitRodAim(FVector2D(1),R.F.Session->GetCastId(),FTRActorSimId(Id.Value+1)));
	TestFalse(TEXT("Old cast rejected"),R.F.Session->SubmitRodAim(FVector2D(1),FTRCastId(0),Id));
	TWeakObjectPtr<UTRRodControlComponent> Weak=R.F.Session->RodControl;
	R.F.Session->DispatchBeginPlay();R.F.Session->Destroy();R.F.Step();TestFalse(TEXT("Destroyed target rejects input"),R.PC->SubmitMouseDelta(FVector2D(1)));
	R.F.Shutdown();CollectGarbage(RF_NoFlags);TestFalse(TEXT("Rod reclaimed with world"),Weak.IsValid());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRodFrameTest,"TipRun.M105D.FixedFramesAndSequence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRodFrameTest::RunTest(const FString& Parameters)
{
	FTRRodSnapshot ReferenceRod;FTREgiSnapshot ReferenceEgi;
	for(int32 FPS:{30,60,120})
	{
		FRodSession R;if(!R.Start(*this)){return false;}const int64 Start=R.F.Sim()->GetSimulationTime().TickIndex;
		R.PC->SubmitMouseDelta(FVector2D(1,1),Start);R.PC->ActionStarted(ETRPlayerAction::Jerk,Start);R.PC->ActionReleased(ETRPlayerAction::Jerk);
		R.PC->SubmitMouseDelta(FVector2D(-1,-.5),Start); // After the click: start-base capture must differ from final base.
		for(int32 I=0;I<FPS*2;++I){R.F.Sim()->AdvanceFrame(1.0/FPS);}
		const auto Rod=R.Rod(); const auto Egi=R.F.Session->Fishing->GetSnapshot();
		TestTrue(TEXT("Sequence captures click's base before subsequent delta"),Rod.ShakuriStartBaseYawRad>Rod.BaseYawRad);
		TestTrue(TEXT("Return follows latest mouse base"),Rod.FinalPitchRad==Rod.BasePitchRad);
		if(FPS==30){ReferenceRod=Rod;ReferenceEgi=Egi;}else
		{
			TestTrue(TEXT("Identical fixed rod and Egi"),Rod.TipWorldPositionM.Equals(ReferenceRod.TipWorldPositionM,0)&&Egi.WorldPositionM.Equals(ReferenceEgi.WorldPositionM,0)&&Egi.LineLengthM==ReferenceEgi.LineLengthM&&Egi.JerkCount==ReferenceEgi.JerkCount);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRodConfigTest,"TipRun.M105D.ConfigurationAndEnhancedMapping",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRodConfigTest::RunTest(const FString& Parameters)
{
	FRodSession R;if(!R.Start(*this)){return false;}TArray<FText> Errors;
	TestTrue(TEXT("Mixed semantic Boolean/Axis2D config validates"),R.PC->InputConfig->Validate(Errors));
	bool Right=false,Mouse=false,Left=false;
	for(const auto& M:R.PC->InputConfig->FishingContext->GetMappings()){Right|=M.Key==EKeys::RightMouseButton;Mouse|=M.Key==EKeys::Mouse2D;Left|=M.Key==EKeys::LeftMouseButton;}
	TestTrue(TEXT("D mappings only: RMB and mouse axes, no LMB"),Right&&Mouse&&!Left);
	auto* Input=NewObject<UEnhancedInputComponent>(R.PC.Get());TestTrue(TEXT("Enhanced bindings installed"),R.PC->InstallInputBindings(Input));
	for(int32 Bad=0;Bad<4;++Bad)
	{
		auto P=RodTestParameters();if(Bad==0){P.LengthM=0;}if(Bad==1){P.MaxPitchRad=P.MinPitchRad;}
		if(Bad==2){P.ShakuriUpSeconds=-1;}if(Bad==3){P.SensitivityXRad=std::numeric_limits<double>::quiet_NaN();}
		Errors.Empty();TestFalse(TEXT("Invalid rod settings rejected"),P.Validate(Errors));
	}
	TArray<uint8> Bytes;FObjectWriter Writer(R.Tuning.Get(),Bytes);TStrongObjectPtr<UTRRodTuningDataAsset> Copy{NewObject<UTRRodTuningDataAsset>()};FObjectReader Reader(Copy.Get(),Bytes);
	TestEqual(TEXT("Rod tuning round trip"),Copy->Parameters.ShakuriAmplitudeRad,.3);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRShakuriPulseGTest,"TipRun.M105G.ShakuriRepeatedPulse",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRShakuriPulseGTest::RunTest(const FString& Parameters)
{
 for(int32 Clicks:{1,2,3,5})
 {
  for(bool Pulse:{false,true})
  {
   FRodSession R;
   if(Pulse){R.Tuning->Parameters.ShakuriReelSpeedMps=8;R.Tuning->Parameters.ShakuriReelSeconds=.25;}
   if(!R.Start(*this,false)){return false;}
   R.F.Deploy();R.F.Step(1800);
   R.F.Session->SubmitCommand(ETRFishingCommandType::TensionFall,R.F.Session->GetCastId());R.F.Step();
   for(int32 I=0;I<Clicks;++I){TestTrue(TEXT("One click accepted"),R.PC->ActionStarted(ETRPlayerAction::Jerk));TestFalse(TEXT("Held not repeated"),R.PC->ActionStarted(ETRPlayerAction::Jerk));R.PC->ActionReleased(ETRPlayerAction::Jerk);}
   const auto Initial=R.F.Session->Fishing->GetSnapshot();
   double StartDepth=Initial.DepthM,MinDepth=StartDepth,StartLine=Initial.LineLengthM,PeakTension=0;
   int64 Count=0;
   auto Check=[&]()
   {
    const auto S=R.F.Session->Fishing->GetSnapshot();
    const double Lift=StartDepth-MinDepth;
    AddInfo(FString::Printf(TEXT("pulse=%d clicks=%d action=%lld lift=%.6fm line=%.6fm slack=%.6fm"),Pulse,Clicks,Count,Lift,double(S.LineLengthM),S.SlackM));
    if(Pulse){TestTrue(TEXT("Every action lifts via constraint"),Lift>.05);TestTrue(TEXT("Each pulse takes up line"),StartLine-S.LineLengthM>1.1);TestTrue(TEXT("Every action engages line, not just previous momentum"),PeakTension>0);}
   };
   for(int32 I=0;I<Clicks*24+1;++I)
   {
    const auto Before=R.F.Session->Fishing->GetSnapshot();
    R.F.Step();const auto S=R.F.Session->Fishing->GetSnapshot();
    if(!TestFalse(TEXT("Action never aborts"),R.F.Session->HasResult())){return false;}
    if(S.JerkCount!=Count)
    {
     if(Count>0){Check();}
     Count=S.JerkCount;StartDepth=Before.DepthM;MinDepth=StartDepth;StartLine=Before.LineLengthM;PeakTension=0;
    }
    MinDepth=FMath::Min(MinDepth,double(S.DepthM));
    PeakTension=FMath::Max(PeakTension,double(S.Tension01));
    if(!R.Geometry(*this)){return false;}
    TestTrue(TEXT("Finite nonnegative line/slack and bounded proxy"),S.LineLengthM>=0 && FMath::IsFinite(S.LineLengthM) && S.SlackM>=0 && FMath::IsFinite(S.SlackM) && S.Tension01>=0 && S.Tension01<=1);
   }
   Check();TestEqual(TEXT("Exact action count"),Count,int64(Clicks));
   if(!Pulse){TestEqual(TEXT("Disabled pulse retains baseline line"),R.F.Session->Fishing->GetSnapshot().LineLengthM,Initial.LineLengthM);}
  }
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRShakuriSequenceGTest,"TipRun.M105G.ShakuriFramesRetrieveAndSafety",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRShakuriSequenceGTest::RunTest(const FString& Parameters)
{
 FTREgiSnapshot Reference;
 for(int32 FPS:{30,60,120})
 {
  FRodSession R;R.Tuning->Parameters.ShakuriReelSpeedMps=8;R.Tuning->Parameters.ShakuriReelSeconds=.25;
  if(!R.Start(*this,false)){return false;}R.F.Deploy();R.F.Step(1800);
  for(int32 I=0;I<5;++I){R.PC->ActionStarted(ETRPlayerAction::Jerk);R.PC->ActionReleased(ETRPlayerAction::Jerk);}
  for(int32 I=0;I<FPS*3;++I){R.F.Sim()->AdvanceFrame(1.0/FPS);}
  auto S=R.F.Session->Fishing->GetSnapshot();
  if(FPS==30){Reference=S;}else{TestTrue(TEXT("30/60/120 exact pulse result"),S.WorldPositionM.Equals(Reference.WorldPositionM,0)&&S.LineLengthM==Reference.LineLengthM&&S.JerkCount==Reference.JerkCount);}
  R.PC->ActionStarted(ETRPlayerAction::Retrieve);R.F.Step(3);
  R.PC->ActionStarted(ETRPlayerAction::Jerk);R.PC->ActionReleased(ETRPlayerAction::Jerk);R.F.Step();
  TestTrue(TEXT("Shakuri replaces normal retrieve"),R.F.Session->Fishing->GetState()==ETRFishingState::Jerking);
  R.PC->ActionReleased(ETRPlayerAction::Retrieve);R.F.Step(30);
  TestTrue(TEXT("Release does not cancel pulse, then Stay"),R.F.Session->Fishing->GetState()==ETRFishingState::Stay);
  R.PC->ActionStarted(ETRPlayerAction::Jerk);R.PC->ActionReleased(ETRPlayerAction::Jerk);R.PC->ActionStarted(ETRPlayerAction::Retrieve);R.F.Step();
  TestTrue(TEXT("Sequence determines conflicting action"),R.F.Session->Fishing->GetState()==ETRFishingState::Retrieving);
  R.PC->ActionReleased(ETRPlayerAction::Retrieve);R.F.Step();
  R.PC->ActionStarted(ETRPlayerAction::Jerk);R.F.Step(4);S=R.F.Session->Fishing->GetSnapshot();
  R.PC->SetPauseRequested(true);R.F.Step(30);TestEqual(TEXT("Pause does not reel"),R.F.Session->Fishing->GetSnapshot().LineLengthM,S.LineLengthM);
  R.PC->SetPauseRequested(false);R.PC->SetInputFocus(false);R.PC->SetInputFocus(true);R.F.Step(60);
  TestEqual(TEXT("Focus does not repeat held jerk"),R.F.Session->Fishing->GetSnapshot().JerkCount,S.JerkCount);
 }
 auto P=RodTestParameters();P.ShakuriReelSpeedMps=1;TArray<FText> Errors;TestFalse(TEXT("Partial pulse config rejected"),P.Validate(Errors));
 P.ShakuriReelSeconds=.5;Errors.Reset();TestFalse(TEXT("Pulse cannot outlive action"),P.Validate(Errors));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRShakuriBoundaryGTest,"TipRun.M105G.ShakuriBoundaryAndNoDirectLift",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRShakuriBoundaryGTest::RunTest(const FString& Parameters)
{
 for(int32 FallTicks:{0,240,3000})
 {
  FRodSession R;R.Tuning->Parameters.ShakuriReelSpeedMps=8;R.Tuning->Parameters.ShakuriReelSeconds=.25;
  R.F.Area->Settings.CurrentMps=FVector(.36,0,0);R.F.Area->Settings.WindMps=FVector2D(0,2);
  if(!R.Start(*this,false)){return false;}R.F.Deploy();R.F.Step(FallTicks);
  for(int32 I=0;I<5;++I){R.PC->ActionStarted(ETRPlayerAction::Jerk);R.PC->ActionReleased(ETRPlayerAction::Jerk);}
  for(int32 I=0;I<150;++I)
  {
   R.F.Step();if(!TestFalse(TEXT("Surface/shallow/bottom pulse safe"),R.F.Session->HasResult())){return false;}
   if(!R.Geometry(*this)){return false;}
  }
  TestEqual(TEXT("Boundary still counts all clicks"),R.F.Session->Fishing->GetSnapshot().JerkCount,int64(5));
 }
 FTREgiSnapshot Reference;
 for(bool ExtremeLegacy:{false,true})
 {
  FRodSession R;R.Tuning->Parameters.ShakuriReelSpeedMps=8;R.Tuning->Parameters.ShakuriReelSeconds=.25;
  if(ExtremeLegacy){R.F.Tuning->Parameters.JerkLiftMps=1000;R.F.Tuning->Parameters.JerkReelMps=1000;}
  if(!R.Start(*this,false)){return false;}R.F.Deploy();R.F.Step(1200);
  R.PC->ActionStarted(ETRPlayerAction::Jerk);R.PC->ActionReleased(ETRPlayerAction::Jerk);R.F.Step(24);
  const auto S=R.F.Session->Fishing->GetSnapshot();
  if(!ExtremeLegacy){Reference=S;}else{TestTrue(TEXT("Legacy direct lift/reel cannot affect rod action"),S.WorldPositionM.Equals(Reference.WorldPositionM,0)&&S.VelocityMps.Equals(Reference.VelocityMps,0)&&S.LineLengthM==Reference.LineLengthM);}
 }
 return true;
}
#endif
