#include "TRSessionTestFixture.h"
#include "Fishing/TRRodControlComponent.h"
#include "Fishing/TREgiSimulationComponent.h"
#include "Fishing/TREgiActor.h"
#include "Game/TRPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
	struct FRetrieveSession
	{
		FTRTestSession F;
		TStrongObjectPtr<UTRRodTuningDataAsset> Rod{NewObject<UTRRodTuningDataAsset>()};
		TWeakObjectPtr<ATRPlayerController> PC;
		TStrongObjectPtr<UEnhancedInputComponent> Input;
		TStrongObjectPtr<UEnhancedPlayerInput> PlayerInput;
		FRetrieveSession()
		{
			auto& R = Rod->Parameters;
			R.MinPitchRad=0; R.MaxPitchRad=1.2; R.MinYawRad=-.6; R.MaxYawRad=.6; R.InitialPitchRad=.1;
			R.SensitivityXRad=.01; R.SensitivityYRad=.02; R.MaxMouseDelta=50; R.MaxAimRateRadPerS=1.2;
			R.LengthM=2; R.MountOffsetM=FVector(0,0,1); R.ShakuriAmplitudeRad=.3; R.ShakuriUpSeconds=.15; R.ShakuriReturnSeconds=.25;
			F.Session->RodTuning=Rod.Get();
			auto& P=F.Tuning->Parameters;
			P.EgiModelRevision=2; P.VerticalResponsePerS=2; P.LineDragKgPerMS=.0001; P.TautLineTransfer01=.05;
			P.SlackLineTransfer01=0; P.LineSlackAllowanceM=.1; P.MaxStepTravelM=20; P.MaxEgiSpeedMps=10;
			P.PayoutMps=4; P.MaxLineLengthM=200; P.ReelMps=1; P.QuickRetrieveDurationS=1.5;
			F.Area->Settings.FieldRevision=2; F.Area->Settings.CurrentMps=FVector(.2,0,0); F.Area->Settings.WindMps=FVector2D(0,2);
			auto& B=F.BoatTuning->Parameters;
			B={}; B.ModelRevision=2; B.WindResponseKgPerS=.1; B.CurrentResponseKgPerS=1; B.DragKgPerS=1;
			B.InertiaKg=10; B.BowWindScale=1; B.SternWindScale=.5; B.SideWindScale=2;
			B.MaxDriftSpeedMps=1; B.HullHeightOffsetM=.5; B.RodAnchorOffsetM=FVector(2,1,1);
		}
		bool Start(FAutomationTestBase& Test, bool bDeploy=true)
		{
			if (!F.Start(Test)) { return false; }
			if (bDeploy) { F.Deploy(); F.Step(240); Send(ETRFishingCommandType::TensionFall); F.Step(); }
			PC=F.World->SpawnActor<ATRPlayerController>();
			PC->InputConfig=UTRInputConfigDataAsset::CreateMouseRetrievePrototype(PC.Get());
			Input.Reset(NewObject<UEnhancedInputComponent>(PC.Get())); PC->InputComponent=Input.Get();
			PlayerInput.Reset(NewObject<UEnhancedPlayerInput>(PC.Get())); PC->PlayerInput=PlayerInput.Get();
			return Test.TestTrue(TEXT("E input installed"),PC->InstallInputBindings(Input.Get())) &&
				Test.TestTrue(TEXT("E session bound"),PC->BindSession(F.Session.Get()));
		}
		void Send(ETRFishingCommandType Type, int64 Tick=-1) { F.Session->SubmitCommand(Type,F.Session->GetCastId(),Tick); }
		void Inject(FKey Key, bool bPressed)
		{
			// Resolve the actual configured device mapping, then exercise Enhanced action events.
			for (const auto& M:PC->InputConfig->FishingContext->GetMappings())
			{
				if (M.Key==Key)
				{
					PlayerInput->InjectInputForAction(M.Action,FInputActionValue(bPressed));
					PlayerInput->ProcessInputStack({Input.Get()},1.0f/60,false); return;
				}
			}
		}
		FTREgiSnapshot Egi() const { return F.Session->Fishing->GetSnapshot(); }
		bool Geometry(FAutomationTestBase& Test) const
		{
			const auto E=Egi(); const auto Tip=F.Session->RodControl->GetSnapshot().TipWorldPositionM;
			return Test.TestTrue(TEXT("Finite constrained world position"),!E.WorldPositionM.ContainsNaN() && !E.VelocityMps.ContainsNaN() &&
				FMath::IsFinite(E.LineLengthM) && (E.WorldPositionM-Tip).Size()<=E.LineLengthM+1.e-4 && E.DepthM>=0 && E.DepthM<=30);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNormalRetrieveTest,"TipRun.M105E.NormalInputReleaseContinuity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNormalRetrieveTest::RunTest(const FString& Parameters)
{
	FRetrieveSession R; if(!R.Start(*this)){return false;}
	const auto Initial=R.Egi(); int32 Starts=0, Stops=0;
	R.F.Session->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result)
	{
		if(C.Type==ETRFishingCommandType::RetrieveStarted){++Starts; TestTrue(TEXT("Start accepted"),Result==ETRCommandResult::Accepted);}
		if(C.Type==ETRFishingCommandType::RetrieveStopped){++Stops;}
	});
	R.Inject(EKeys::LeftMouseButton,true);
	TestEqual(TEXT("Controller never changes line immediately"),R.Egi().LineLengthM,Initial.LineLengthM);
	for(int32 I=0;I<30;++I){R.Inject(EKeys::LeftMouseButton,true);R.F.Step();R.Geometry(*this);}
	TestEqual(TEXT("Hold sends only one start"),Starts,1);
	TestTrue(TEXT("Normal state and configured speed"),R.Egi().FishingState==ETRFishingState::Retrieving &&
		R.F.Session->GetHUDSnapshot().Retrieval.RequestedRetrieveSpeedMps==1 &&
		FMath::IsNearlyEqual(R.F.Session->GetHUDSnapshot().Retrieval.RetrieveSpeedMps,1.f,.0001f));
	TestTrue(TEXT("Half second reels half a metre"),FMath::IsNearlyEqual(Initial.LineLengthM-R.Egi().LineLengthM,.5f,.0001f));
	TestTrue(TEXT("Offset not snapped under boat"),R.Egi().HorizontalDistanceFromRodTipM>.01);
	const auto Before=R.Egi();
	bool bReleaseSeen=false;
	auto Handle=R.F.Session->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result)
	{
		if(C.Type!=ETRFishingCommandType::RetrieveStopped){return;}
		bReleaseSeen=true; const auto After=R.Egi();
		TestTrue(TEXT("Release accepted to Stay"),Result==ETRCommandResult::Accepted && After.FishingState==ETRFishingState::Stay);
		TestTrue(TEXT("Command boundary preserves position and velocity"),Before.WorldPositionM.Equals(After.WorldPositionM,0) && Before.VelocityMps.Equals(After.VelocityMps,0));
		TestEqual(TEXT("Command boundary preserves length"),After.LineLengthM,Before.LineLengthM);
		TestEqual(TEXT("Command boundary preserves angle"),After.LineAngleRad,Before.LineAngleRad);
		TestTrue(TEXT("Command boundary preserves direction/current/offset"),After.LineDirection.Equals(Before.LineDirection,0) &&
			After.CurrentAtEgiDepthMps.Equals(Before.CurrentAtEgiDepthMps,0) && After.HorizontalOffsetFromBoatM.Equals(Before.HorizontalOffsetFromBoatM,0));
	});
	R.Inject(EKeys::LeftMouseButton,false); R.F.Step();
	R.F.Session->OnCommandProcessed.Remove(Handle);
	TestTrue(TEXT("Enhanced release delivered"),bReleaseSeen && Stops==1 && !R.PC->IsRetrieveHeld());
	R.F.Step(60); R.Geometry(*this);
	TestEqual(TEXT("Stay has no payout or reel"),R.Egi().LineLengthM,Before.LineLengthM);
	TestTrue(TEXT("Space simulation continues after release"),!R.Egi().WorldPositionM.Equals(Before.WorldPositionM,.0001) && R.Egi().Tick>Before.Tick);
	TestTrue(TEXT("Cast remains locked and unfinished"),R.F.Session->IsEquipmentLocked() && !R.F.Session->HasResult());
	R.Inject(EKeys::RightMouseButton,true); R.F.Step();
	TestTrue(TEXT("Release allows Shakuri"),R.Egi().FishingState==ETRFishingState::Jerking);
	R.Inject(EKeys::RightMouseButton,false); R.F.Step(60);
	R.Send(ETRFishingCommandType::Fall); R.F.Step(); TestTrue(TEXT("Release allows Re-Fall"),R.Egi().FishingState==ETRFishingState::FreeFall);
	R.Inject(EKeys::LeftMouseButton,true); R.F.Step(); TestTrue(TEXT("Release allows Retrieve again"),R.Egi().FishingState==ETRFishingState::Retrieving);
	R.F.Session->OnCommandProcessed.Clear(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNormalCompleteTest,"TipRun.M105E.NormalCompletionResultLock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNormalCompleteTest::RunTest(const FString& Parameters)
{
	for(bool bMoving:{false,true})
	{
		FRetrieveSession R;
		if(!bMoving){R.F.Area->Settings.CurrentMps={};R.F.Area->Settings.WindMps={};}
		if(!R.Start(*this)){return false;} int32 Results=0; TArray<FText> Errors;
		R.F.Session->OnCastCompleted.AddLambda([&](const FTRCatchResult& C){++Results;TestFalse(TEXT("Normal result is not Quick"),C.bQuickRetrieved);});
		TestTrue(TEXT("Cast rejects equipment"),R.F.Session->TrySetEquipment(TEXT("Egi_4"),TEXT("Sinker_5"),Errors)==ETRCommandResult::RejectedBusy);
		R.Inject(EKeys::LeftMouseButton,true); R.F.Step();
		TestTrue(TEXT("Normal Retrieve rejects equipment changes"),R.F.Session->TrySetEquipment(TEXT("Egi_4"),TEXT("Sinker_5"),Errors)==ETRCommandResult::RejectedBusy);
		for(int32 I=0;I<2400 && !R.F.Session->HasResult();++I){R.F.Step();if(!R.F.Session->HasResult()){R.Geometry(*this);}}
		R.F.Step();
		AddInfo(FString::Printf(TEXT("Normal completion moving=%d outcome=%d depth=%.9f line=%.9f rodDistance=%.9f horizontal=%.9f speed=%.9f"),
			bMoving,int32(R.F.Session->GetLastResult().Outcome),R.Egi().DepthM,R.Egi().LineLengthM,R.Egi().RodToEgiDistanceM,
			R.Egi().HorizontalDistanceFromRodTipM,R.Egi().VelocityMps.Size()));
		TestTrue(TEXT("Normal return reaches Result and onboard"),R.F.Session->HasResult() && R.F.Session->IsEgiOnboard() &&
			R.F.Session->GetSessionPhase()==ETRSessionPhase::Result && R.Egi().FishingState==ETRFishingState::Result);
		TestTrue(TEXT("Result remains equipment locked"),R.F.Session->IsEquipmentLocked() && !R.F.Session->CanChangeEquipment());
		TestTrue(TEXT("Normal Result rejects changes"),R.F.Session->TrySetEquipment(TEXT("Egi_4"),TEXT("Sinker_5"),Errors)==ETRCommandResult::RejectedBusy);
		TestEqual(TEXT("One terminal event"),Results,1); TestNull(TEXT("Normal visual released"),R.F.Session->GetEgiActor());
		R.Send(ETRFishingCommandType::QuickRetrieve);R.F.Step();
		TestTrue(TEXT("Normal Result refuses Quick"),R.F.Session->GetLastCommandResult()==ETRCommandResult::RejectedInvalidState);
		R.Send(ETRFishingCommandType::NextCast);R.F.Step();
		TestTrue(TEXT("NextCast unlocks Ready"),R.F.Session->CanChangeEquipment());
		R.F.Session->OnCastCompleted.Clear();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRQuickTest,"TipRun.M105E.QuickDurationRestrictionsAndUnlock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRQuickTest::RunTest(const FString& Parameters)
{
	for(float Length:{2.f,30.f,80.f,100.f})
	{
		FRetrieveSession R;R.F.Tuning->Parameters.MinLineM=Length;
		if(!R.Start(*this,false)){return false;}
		R.Send(ETRFishingCommandType::QuickRetrieve);R.F.Step();
		TestTrue(TEXT("Ready refuses Quick"),R.F.Session->GetLastCommandResult()==ETRCommandResult::RejectedInvalidState);
		R.F.Deploy();R.F.Step(60); // Valid long slack at shallow depth; not an abnormal FreeFall payout scenario.
		R.PC->SubmitMouseDelta(FVector2D::ZeroVector);R.F.Step(); // Observe new Cast before test queueing.
		const auto Start=R.Egi(); const auto Cast=R.F.Session->GetCastId(); const auto Registration=R.F.Session->GetRegistrationId();
		TWeakObjectPtr<ATREgiActor> OldVisual=R.F.Session->GetEgiActor();
		int32 Results=0; int32 Busy=0;
		R.F.Session->OnCastCompleted.AddLambda([&](const FTRCatchResult& C){++Results;TestTrue(TEXT("Quick result marks method"),C.bQuickRetrieved && C.Outcome==ETRCastOutcome::Retrieved);});
		R.F.Session->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result)
		{if(Result==ETRCommandResult::RejectedBusy){++Busy;}});
		const int64 Begin=R.F.Sim()->GetSimulationTime().TickIndex;
		R.Inject(EKeys::Q,true);R.F.Step();
		TestTrue(TEXT("Q starts Quick"),R.Egi().FishingState==ETRFishingState::QuickRetrieving);
		TestFalse(TEXT("Quick is not a water evaluation target"),R.F.Session->GetHUDSnapshot().Retrieval.bUnderwaterSimulationActive);
		R.F.Tuning->Parameters.QuickRetrieveDurationS=9; // Cast-frozen duration remains 90 ticks.
		for(auto C:{ETRFishingCommandType::QuickRetrieve,ETRFishingCommandType::Jerk,ETRFishingCommandType::Fall,
			ETRFishingCommandType::RetrieveStarted,ETRFishingCommandType::RetrieveStopped,ETRFishingCommandType::Hook,
			ETRFishingCommandType::RodAim,ETRFishingCommandType::NextCast,ETRFishingCommandType::Deploy}){R.Send(C);}
		R.F.Step(89);
		TestEqual(TEXT("Other fishing commands explicitly rejected"),Busy,9);
		TestFalse(TEXT("One tick before deadline not completed"),R.F.Session->HasResult());
		TestTrue(TEXT("Quick keeps last physical sample"),R.Egi().WorldPositionM.Equals(Start.WorldPositionM,0) && R.Egi().VelocityMps.Equals(Start.VelocityMps,0));
		TestEqual(TEXT("Quick does not simulate line"),R.Egi().LineLengthM,Start.LineLengthM);
		TestTrue(TEXT("Fixed progress reflects 89/90"),FMath::IsNearlyEqual(R.F.Session->GetHUDSnapshot().Retrieval.QuickRetrieveProgress01,89.0/90,1.e-12));
		R.F.Step();
		TestEqual(TEXT("Independent of line length: exactly 90 ticks"),R.F.Sim()->GetSimulationTime().TickIndex-1-Begin,int64(90));
		TestTrue(TEXT("Quick directly Ready and editable, no NextCast"),R.Egi().FishingState==ETRFishingState::Ready &&
			R.F.Session->GetSessionPhase()==ETRSessionPhase::Ready && R.F.Session->IsEgiOnboard() && R.F.Session->CanChangeEquipment() && !R.F.Session->IsEquipmentLocked());
		TestNull(TEXT("Quick visual released"),R.F.Session->GetEgiActor());
		TestTrue(TEXT("Old visual destroyed"),!OldVisual.IsValid() || OldVisual->IsActorBeingDestroyed());
		TestFalse(TEXT("Old queue registration invalidated"),R.F.Sim()->IsRegistered(Registration));
		R.F.Step(10);TestEqual(TEXT("Completion published once"),Results,1);
		TArray<FText> Errors;
		TestTrue(TEXT("Immediate Ready equipment change"),R.F.Session->TrySetEquipment(TEXT("Egi_4"),TEXT("Sinker_5"),Errors)==ETRCommandResult::Accepted);
		TestEqual(TEXT("Previous cast equipment frozen"),R.F.Session->GetLastResultEquipment().TotalMassG,35.f);
		R.F.Deploy(); TestTrue(TEXT("Next deploy relocks new equipment"),R.F.Session->IsEquipmentLocked() && R.F.Session->GetCastId()!=Cast && R.F.Session->GetEquipmentSnapshot().TotalMassG==45.f);
		R.F.Session->SubmitCommand(ETRFishingCommandType::QuickRetrieve,Cast);R.F.Step();
		TestTrue(TEXT("Old Cast Quick rejected"),R.F.Session->GetLastCommandResult()==ETRCommandResult::RejectedInvalidState);
		R.F.Session->OnCastCompleted.Clear();R.F.Session->OnCommandProcessed.Clear();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRetrievePauseTest,"TipRun.M105E.PauseFocusAndDestruction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRetrievePauseTest::RunTest(const FString& Parameters)
{
	FRetrieveSession R;if(!R.Start(*this)){return false;}
	for(bool bPause:{true,false})
	{
		R.PC->ActionReleased(ETRPlayerAction::Retrieve);R.PC->ActionStarted(ETRPlayerAction::Retrieve);R.F.Step(5);
		if(bPause){R.PC->SetPauseRequested(true);}else{R.PC->SetInputFocus(false);}
		const auto Before=R.Egi();R.F.Step(120);
		TestFalse(TEXT("Safety clears held input"),R.PC->IsRetrieveHeld());
		if(bPause)
		{
			TestTrue(TEXT("Pause freezes physical state"),R.Egi().WorldPositionM.Equals(Before.WorldPositionM,0));
			R.PC->SetPauseRequested(false);
		}
		else{R.PC->SetInputFocus(true);}
		R.F.Step();const float Line=R.Egi().LineLengthM;R.F.Step(30);
		TestEqual(TEXT("No automatic reel restart"),R.Egi().LineLengthM,Line);
		TestTrue(TEXT("Safety stop returns Stay"),R.Egi().FishingState==ETRFishingState::Stay);
		TestFalse(TEXT("Held input blocked until release"),R.PC->ActionStarted(ETRPlayerAction::Retrieve));
	}
	R.PC->ActionReleased(ETRPlayerAction::Retrieve);R.PC->ActionStarted(ETRPlayerAction::Retrieve);R.F.Step();
	R.PC->ActionStarted(ETRPlayerAction::QuickRetrieve);R.F.Step();
	TestFalse(TEXT("Accepted Quick clears normal hold synchronously"),R.PC->IsRetrieveHeld());
	R.PC->SetPauseRequested(true);const auto Progress=R.F.Session->GetHUDSnapshot().Retrieval.QuickRetrieveProgress01;
	R.F.Step(300);TestEqual(TEXT("Quick deadline paused"),R.F.Session->GetHUDSnapshot().Retrieval.QuickRetrieveProgress01,Progress);
	R.PC->SetPauseRequested(false);R.PC->SetInputFocus(false);R.F.Step(5);
	TestTrue(TEXT("Focus loss does not cancel Quick"),R.Egi().FishingState==ETRFishingState::QuickRetrieving);
	int32 Returns=0;R.F.Session->OnCastCompleted.AddLambda([&](const FTRCatchResult& C){Returns+=C.bQuickRetrieved;});
	TWeakObjectPtr<UTREgiSimulationComponent> Numeric=R.F.Session->EgiSimulation;
	R.F.World->DestroyActor(R.F.Session.Get());R.F.Step(180);
	TestEqual(TEXT("Destroyed Session cannot return later"),Returns,0);
	TestFalse(TEXT("Destroyed Session rejects input"),R.PC->ActionStarted(ETRPlayerAction::QuickRetrieve));
	R.F.Shutdown();CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Numeric component released with world"),Numeric.IsValid());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRetrieveStatesTest,"TipRun.M105E.QuickEntryStatesAndAbort",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRetrieveStatesTest::RunTest(const FString& Parameters)
{
	for(auto State:{ETRFishingState::FreeFall,ETRFishingState::BottomContact,ETRFishingState::Jerking,
		ETRFishingState::TensionFall,ETRFishingState::Stay,ETRFishingState::Retrieving})
	{
		FRetrieveSession R;if(State==ETRFishingState::BottomContact){R.F.Area->Settings.CurrentMps={};R.F.Area->Settings.WindMps={};}
		if(!R.Start(*this)){return false;}
		if(State==ETRFishingState::FreeFall || State==ETRFishingState::BottomContact){R.Send(ETRFishingCommandType::Fall);R.F.Step();}
		if(State==ETRFishingState::BottomContact)
		{
			for(int32 I=0;I<6000 && R.Egi().FishingState!=State;++I){R.F.Step();}
			TestTrue(TEXT("Bottom setup reached"),R.Egi().FishingState==State);
			// Releasing on the bottom retains contact, never starts payout.
			R.Send(ETRFishingCommandType::RetrieveStarted);R.Send(ETRFishingCommandType::RetrieveStopped);R.F.Step();
			TestTrue(TEXT("Bottom release preserves contact state"),R.Egi().FishingState==ETRFishingState::BottomContact);
		}
		if(State==ETRFishingState::Jerking){R.Send(ETRFishingCommandType::Jerk);R.Send(ETRFishingCommandType::Jerk);R.F.Step();}
		if(State==ETRFishingState::Retrieving){R.Send(ETRFishingCommandType::RetrieveStarted);R.F.Step();}
		if(State==ETRFishingState::TensionFall)
		{
			R.Send(ETRFishingCommandType::Fall);R.F.Step();
			R.Send(ETRFishingCommandType::TensionFall); // Same-tick Quick must see TF before immediate transient completion.
		}
		R.Send(ETRFishingCommandType::QuickRetrieve);R.F.Step();
		TestTrue(TEXT("Quick entry accepted"),R.Egi().FishingState==ETRFishingState::QuickRetrieving);
		TestEqual(TEXT("Quick cancels jerk reservations"),R.Egi().PendingJerkCount,int64(0));
		R.Send(ETRFishingCommandType::EndFishing);R.F.Step(120);
		TestFalse(TEXT("Explicit Session end never fabricates onboard return"),R.F.Session->IsEgiOnboard());
		TestTrue(TEXT("Explicit end is Aborted"),R.F.Session->GetLastResult().Outcome==ETRCastOutcome::Aborted && !R.F.Session->GetLastResult().bQuickRetrieved);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRetrieveDeterminismTest,"TipRun.M105E.FixedFramesAndReadOnlySnapshot",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRetrieveDeterminismTest::RunTest(const FString& Parameters)
{
	TArray<FString> Reference;TArray<FTREgiSnapshot> ReferenceEgi;double ReferenceTime=0;
	for(int32 FPS:{30,60,120})
	{
		FRetrieveSession R;if(!R.Start(*this)){return false;}TArray<FString> Events;TArray<FTREgiSnapshot> Samples;
		R.F.Session->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState From,ETRFishingState To)
		{Events.Add(FString::Printf(TEXT("%lld:%d:%d"),R.F.Sim()->GetSimulationTime().TickIndex,int32(From),int32(To)));});
		R.F.Session->OnCommandProcessed.AddLambda([&](const FTRFishingCommand&,ETRCommandResult){Samples.Add(R.Egi());});
		const int64 T=R.F.Sim()->GetSimulationTime().TickIndex;
		R.Send(ETRFishingCommandType::RetrieveStarted,T);
		R.Send(ETRFishingCommandType::RetrieveStopped,T+20);
		R.Send(ETRFishingCommandType::Jerk,T+20);
		R.Send(ETRFishingCommandType::Fall,T+60);
		R.Send(ETRFishingCommandType::RetrieveStarted,T+80);
		R.Send(ETRFishingCommandType::QuickRetrieve,T+100);
		R.Send(ETRFishingCommandType::Fall,T+100);
		const auto Before=R.Egi();const int32 Queue=R.F.Sim()->GetQueuedCommandCount();
		for(int32 I=0;I<100;++I){auto S=R.F.Session->GetHUDSnapshot();S.Retrieval.QuickRetrieveProgress01=999;S.Egi.WorldPositionM={};}
		TestEqual(TEXT("Reads preserve queue"),R.F.Sim()->GetQueuedCommandCount(),Queue);
		TestTrue(TEXT("Reads preserve position"),R.Egi().WorldPositionM.Equals(Before.WorldPositionM,0));
		for(int32 I=0;I<FPS*4;++I){R.F.Sim()->Tick(1.f/FPS);}
		TestTrue(TEXT("Replay returns Ready"),R.F.Session->CanChangeEquipment() && R.F.Session->GetLastResult().bQuickRetrieved);
		if(FPS==30){Reference=Events;ReferenceEgi=Samples;ReferenceTime=R.F.Session->GetLastResult().ElapsedSimSeconds;}
		else
		{
			TestTrue(TEXT("Identical state/tick sequence"),Events==Reference);
			TestEqual(TEXT("Identical end time"),R.F.Session->GetLastResult().ElapsedSimSeconds,ReferenceTime);
			if(TestEqual(TEXT("Same command observations"),Samples.Num(),ReferenceEgi.Num()))
			{for(int32 I=0;I<Samples.Num();++I){TestTrue(TEXT("Identical world/line results"),Samples[I].WorldPositionM.Equals(ReferenceEgi[I].WorldPositionM,1.e-9) && Samples[I].LineLengthM==ReferenceEgi[I].LineLengthM);}}
		}
		R.F.Session->OnCommandProcessed.Clear();R.F.Session->Fishing->OnFishingStateChanged.Clear();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRetrieveConfigTest,"TipRun.M105E.ConfigurationAndSafety",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRetrieveConfigTest::RunTest(const FString& Parameters)
{
	FRetrieveSession R;TArray<FText> Errors;
	TestTrue(TEXT("Explicit tuning valid"),R.F.Tuning->Validate(Errors));
	for(double Invalid:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
	{
		auto P=R.F.Tuning->Parameters;P.QuickRetrieveDurationS=Invalid;TestFalse(TEXT("Nonfinite/negative duration rejected"),P.Validate(Errors));
	}
	FTREquipmentSnapshot E;
	R.F.Tuning->Parameters.QuickRetrieveDurationS=std::numeric_limits<double>::max();
	TestFalse(TEXT("Unrepresentable duration rejected before cast"),TREquipment::TryBuildSnapshot(R.F.Egis.Get(),R.F.Sinkers.Get(),R.F.Tuning.Get(),TREquipment::InitialEgiId(),TREquipment::NoSinkerId(),1.0/60,E,Errors));
	R.F.Tuning->Parameters.QuickRetrieveDurationS=1.5;
	TArray<uint8> Bytes;FObjectWriter Writer(R.F.Tuning.Get(),Bytes);
	TStrongObjectPtr<UTRFishingTuningDataAsset> Copy{NewObject<UTRFishingTuningDataAsset>()};FObjectReader Reader(Copy.Get(),Bytes);
	TestEqual(TEXT("Duration survives serialization"),Copy->Parameters.QuickRetrieveDurationS,1.5);
	R.F.Tuning->Parameters.QuickRetrieveDurationS=0;
	if(!R.Start(*this)){return false;}
	R.Send(ETRFishingCommandType::QuickRetrieve);R.F.Step();
	TestTrue(TEXT("Legacy zero duration refuses rather than invents a default"),R.F.Session->GetLastCommandResult()==ETRCommandResult::RejectedMissingData);
	TestTrue(TEXT("New input config valid"),R.PC->InputConfig->Validate(Errors));
	const UInputAction* Mouse=nullptr;const UInputAction* Keyboard=nullptr;bool Quick=false;
	for(const auto& M:R.PC->InputConfig->FishingContext->GetMappings())
	{if(M.Key==EKeys::LeftMouseButton){Mouse=M.Action;}if(M.Key==EKeys::R){Keyboard=M.Action;}if(M.Key==EKeys::Q){Quick=true;}}
	TestTrue(TEXT("Left and R share semantic action, Q separately mapped"),Mouse && Mouse==Keyboard && Quick);
	return true;
}
#endif
