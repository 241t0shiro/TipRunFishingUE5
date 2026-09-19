#include "TRSessionTestFixture.h"
#include "Game/TRPlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
 bool StartNavigation(FTRTestSession& F, FAutomationTestBase& Test)
 {
  if (!F.Initialize(Test)) { return false; }
  TArray<FText> Errors;
  return Test.TestTrue(TEXT("Area session started"), F.Session->StartFishing(Errors) == ETRCommandResult::Accepted);
 }
 bool Request(FTRTestSession& F, ETRPlayerMode Target, int64 Tick = -1)
 {
  return F.Session->SubmitModeChange(Target, F.Session->GetPlayerModeSnapshot().ModeEpoch, F.Session->GetRegistrationId(), Tick);
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRModeTransitions, "TipRun.M105R1.InitialAndTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRModeTransitions::RunTest(const FString&)
{
 FTRTestSession F; if (!StartNavigation(F,*this)) { return false; }
 const auto Initial = F.Session->GetPlayerModeSnapshot();
 TestTrue(TEXT("Navigation, no side, Ready, no cast"), Initial.bValid && Initial.Mode == ETRPlayerMode::Navigation &&
  Initial.FishingSide == ETRFishingSide::Unselected && !F.Session->GetCastId().IsValid() && F.Session->IsEgiOnboard());
 TestTrue(TEXT("Navigation permission is authoritative"), F.Session->IsInputModeAllowed(ETRPlayerMode::Navigation));
 F.Session->SubmitCommand(ETRFishingCommandType::Deploy, {}); F.Step();
 TestFalse(TEXT("Navigation cannot deploy even via session bypass"), F.Session->GetCastId().IsValid());
 TestTrue(TEXT("Fishing start queued"), Request(F, ETRPlayerMode::Fishing));
 TestTrue(TEXT("No immediate transition"), F.Session->GetPlayerModeSnapshot().Mode == ETRPlayerMode::Navigation);
 FTRBoatSnapshot Before = F.Boat->GetBoatSnapshot();
 bool bObserved = false;
 auto Handle = F.Session->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C, ETRCommandResult Result)
 {
  if (C.Type != ETRFishingCommandType::StartFishingMode) { return; }
  bObserved = true;
  TestTrue(TEXT("Accepted before boat phase"), Result == ETRCommandResult::Accepted);
  auto Boat = F.Boat->GetBoatSnapshot();
  TestTrue(TEXT("Mode transition clears velocity, preserves position/heading"), Boat.VelocityMps == FVector::ZeroVector && Boat.PositionM == Before.PositionM && Boat.HeadingRad == Before.HeadingRad);
 });
 F.Step(); F.Session->OnCommandProcessed.Remove(Handle);
 auto Mode = F.Session->GetPlayerModeSnapshot();
 TestTrue(TEXT("Fishing with stop thrust and hold heading policy"), bObserved && Mode.Mode == ETRPlayerMode::Fishing && Mode.bStopNavigationThrustRequested && Mode.bHoldBoatHeading && !Mode.bNavigationInputAllowed);
 TestFalse(TEXT("Navigation control rejected in simulation policy"), F.Session->IsInputModeAllowed(ETRPlayerMode::Navigation));
 TestEqual(TEXT("Epoch advances"), Mode.ModeEpoch, Initial.ModeEpoch+1);
 auto Registration = F.Session->GetRegistrationId();
 Request(F, ETRPlayerMode::Navigation); F.Step();
 TestTrue(TEXT("Safe return keeps session registered and equipment"), F.Session->GetPlayerModeSnapshot().Mode == ETRPlayerMode::Navigation && F.Session->GetRegistrationId() == Registration && F.Session->CanChangeEquipment());
 const auto Tick = F.Sim()->GetSimulationTime().TickIndex;
 for (int I=0;I<100;++I) { auto Copy=F.Session->GetHUDSnapshot(); Copy.PlayerMode.Mode=ETRPlayerMode::Fishing; }
 TestTrue(TEXT("Snapshot read/copy cannot mutate"), F.Session->GetPlayerModeSnapshot().Mode == ETRPlayerMode::Navigation && F.Sim()->GetSimulationTime().TickIndex == Tick);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRModeGuards, "TipRun.M105R1.CastGuardsAndReturns", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRModeGuards::RunTest(const FString&)
{
 FTRTestSession F;
 F.Tuning->Parameters.QuickRetrieveDurationS = 1.5; // Explicit test setting; legacy asset has Quick disabled.
 if (!F.Start(*this)) { return false; }
 F.Deploy();
 auto Reject = [&]()
 {
  const auto Cast = F.Session->GetCastId();
  Request(F, ETRPlayerMode::Navigation); F.Step();
  TestTrue(TEXT("Active cast rejects Navigation and preserves cast"), F.Session->GetPlayerModeSnapshot().Mode == ETRPlayerMode::Fishing &&
   F.Session->GetPlayerModeSnapshot().LastRejectedReason == ETRModeChangeRejection::ActiveCast && F.Session->GetCastId() == Cast && F.Session->IsEquipmentLocked());
 };
 TestTrue(TEXT("FreeFall setup"), F.Session->Fishing->GetState() == ETRFishingState::FreeFall); Reject();
 F.Step(120);
 F.Session->SubmitCommand(ETRFishingCommandType::TensionFall,F.Session->GetCastId()); F.Step(120);
 TestTrue(TEXT("Stay setup"), F.Session->Fishing->GetState() == ETRFishingState::Stay); Reject();
 F.Session->SubmitCommand(ETRFishingCommandType::RetrieveStarted,F.Session->GetCastId()); F.Step();
 TestTrue(TEXT("Retrieving setup"), F.Session->Fishing->GetState() == ETRFishingState::Retrieving); Reject();
 F.Session->SubmitCommand(ETRFishingCommandType::QuickRetrieve,F.Session->GetCastId()); F.Step(); Reject();
 TestTrue(TEXT("Quick command enabled by test tuning"), F.Session->Fishing->GetState() == ETRFishingState::QuickRetrieving);
 for (int I=0; I<300 && F.Session->Fishing->GetState()!=ETRFishingState::Ready; ++I) { F.Step(); }
 TestTrue(TEXT("Quick Ready and onboard"), F.Session->IsEgiOnboard() && F.Session->CanChangeEquipment());
 const auto OldCast=F.Session->GetCastId();
 Request(F,ETRPlayerMode::Navigation); F.Step();
 TestTrue(TEXT("Quick allows Navigation without NextCast"),F.Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Navigation);
 Request(F,ETRPlayerMode::Fishing); F.Step(); F.Deploy();
 F.Session->SubmitCommand(ETRFishingCommandType::ReturnNavigationMode,OldCast); F.Step();
 TestTrue(TEXT("Old CastId rejected"),F.Session->GetLastCommandResult()==ETRCommandResult::RejectedInvalidState);
 F.Session->SubmitCommand(ETRFishingCommandType::RetrieveStarted,F.Session->GetCastId());
 for (int I=0;I<12000 && F.Session->Fishing->GetState()!=ETRFishingState::Result;++I) { F.Step(); }
 TestTrue(TEXT("Normal completion result"),F.Session->GetSessionPhase()==ETRSessionPhase::Result);
 Request(F,ETRPlayerMode::Navigation); F.Step();
 TestTrue(TEXT("Result blocked until NextCast"),F.Session->GetPlayerModeSnapshot().LastRejectedReason==ETRModeChangeRejection::NotReady);
 F.Session->SubmitCommand(ETRFishingCommandType::NextCast,F.Session->GetCastId()); F.Step();
 Request(F,ETRPlayerMode::Navigation); F.Step();
 TestTrue(TEXT("Normal next preparation permits return"),F.Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Navigation);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRModeOrdering, "TipRun.M105R1.OrderEpochAndFrameRates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRModeOrdering::RunTest(const FString&)
{
 TArray<FString> Reference;
 for(int FPS : {30,60,120})
 {
  FTRTestSession F; if(!StartNavigation(F,*this)){return false;}
  TArray<FString> Events;
  auto Handle=F.Session->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C, ETRCommandResult R)
  { Events.Add(FString::Printf(TEXT("%lld:%lld:%d:%d:%lld"),C.TargetTick,C.Sequence,int(C.Type),int(R),F.Session->GetPlayerModeSnapshot().ModeEpoch)); });
  // An earlier sequence at a future tick must not suppress the earlier-tick command.
  Request(F,ETRPlayerMode::Fishing,8);
  Request(F,ETRPlayerMode::Fishing,2);
  Request(F,ETRPlayerMode::Navigation,2);
  F.Session->SubmitCommand(ETRFishingCommandType::Deploy,{},2);
  for(int I=0;I<FPS;++I){F.Sim()->AdvanceFrame(1.0/FPS);}
  TestEqual(TEXT("All four due inputs observed"),Events.Num(),4);
  TestTrue(TEXT("First mode change wins; old same/future epoch rejected"),F.Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Fishing && !F.Session->GetCastId().IsValid());
  TestEqual(TEXT("Mode changed at requested tick"),F.Session->GetPlayerModeSnapshot().ChangedAtTick,int64(2));
  TestTrue(TEXT("Stale epoch visible"),F.Session->GetPlayerModeSnapshot().LastRejectedReason==ETRModeChangeRejection::StaleInput);
  if(FPS==30){Reference=Events;} else {TestTrue(TEXT("Exact Tick/Sequence/result replay"),Events==Reference);}
  F.Session->OnCommandProcessed.Remove(Handle);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRModeLifetime, "TipRun.M105R1.PauseFocusAndLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRModeLifetime::RunTest(const FString&)
{
 FTRTestSession F; if(!StartNavigation(F,*this)){return false;}
 auto* PC=F.World->SpawnActor<ATRPlayerController>();
 TestTrue(TEXT("Controller bound in Navigation"),PC->BindSession(F.Session.Get()));
 TestTrue(TEXT("Controller submits mode through queue"),PC->RequestPlayerMode(ETRPlayerMode::Fishing));
 PC->SetPauseRequested(true); F.Step(60);
 TestTrue(TEXT("Paused transition discarded"),F.Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Navigation);
 TestFalse(TEXT("Paused input rejected"),PC->RequestPlayerMode(ETRPlayerMode::Fishing));
 PC->SetPauseRequested(false); F.Step();
 TestTrue(TEXT("No delayed transition on resume"),F.Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Navigation);
 PC->RequestPlayerMode(ETRPlayerMode::Fishing); PC->SetInputFocus(false); F.Step();
 TestTrue(TEXT("Focus clears pending mode request"),F.Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Navigation);
 PC->SetInputFocus(true); PC->RequestPlayerMode(ETRPlayerMode::Fishing); F.Step();
 TestTrue(TEXT("Explicit fresh request works"),F.Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Fishing);
 TestTrue(TEXT("Held bookkeeping starts"),PC->ActionStarted(ETRPlayerAction::Retrieve));
 PC->RequestPlayerMode(ETRPlayerMode::Navigation); F.Step();
 TestFalse(TEXT("Mode transition clears held input"),PC->IsRetrieveHeld());
 const auto OldEpoch=F.Session->GetPlayerModeSnapshot().ModeEpoch;
 const auto OldRegistration=F.Session->GetRegistrationId();
 F.Session->EndFishing();
 TestFalse(TEXT("Stopped snapshot invalid"),F.Session->GetPlayerModeSnapshot().bValid);
 TestFalse(TEXT("Stopped mode request rejected"),Request(F,ETRPlayerMode::Fishing));
 TArray<FText> Errors; F.Session->StartFishing(Errors);
 TestFalse(TEXT("Old registration/epoch rejected after restart"),F.Session->SubmitModeChange(ETRPlayerMode::Fishing,OldEpoch,OldRegistration));
 Request(F,ETRPlayerMode::Fishing,F.Sim()->GetSimulationTime().TickIndex+10);
 auto Id=F.Session->GetRegistrationId(); F.World->DestroyActor(F.Session.Get());
 TestFalse(TEXT("Destroyed session unregistered"),F.Sim()->IsRegistered(Id));
 TestEqual(TEXT("Destroyed queued requests removed"),F.Sim()->GetQueuedCommandCount(),0);
 F.Step(20); PC->UnbindSession();
 return true;
}
#endif
