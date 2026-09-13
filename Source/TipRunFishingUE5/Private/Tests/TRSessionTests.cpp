#include "TRSessionTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSessionLockTest, "TipRun.M06.F16EquipmentLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSessionLockTest::RunTest(const FString& Parameters)
{
	FTRTestSession F; if (!F.Start(*this)) { return false; }
	TArray<FText> Errors;
	TestFalse(TEXT("Start does not lock equipment"), F.Session->IsEquipmentLocked());
	TestTrue(TEXT("Duplicate start rejected"), F.Session->StartFishing(Errors) == ETRCommandResult::RejectedInvalidState);
	TestTrue(TEXT("Initial Ready allows change"), F.Session->TrySetEquipment(TEXT("Egi_4"), TEXT("Sinker_5"), Errors) == ETRCommandResult::Accepted);
	F.Deploy(); TestTrue(TEXT("Deploy locks"), F.Session->IsEquipmentLocked());
	TestTrue(TEXT("Cast change rejected"), F.Session->TrySetEquipment(TEXT("Egi_3"), TREquipment::NoSinkerId(), Errors) == ETRCommandResult::RejectedBusy);
	const auto First = F.Session->GetCastId();
	if (!TestTrue(TEXT("Retrieve unlocks next preparation"), F.ReturnToReady())) { return false; }
	TestTrue(TEXT("Can change next equipment"), F.Session->CanChangeEquipment());
	TestTrue(TEXT("Change next equipment"), F.Session->TrySetEquipment(TEXT("Egi_3"), TREquipment::NoSinkerId(), Errors) == ETRCommandResult::Accepted);
	TestEqual(TEXT("Previous result equipment preserved"), F.Session->GetLastResultEquipment().TotalMassG, 45.0f);
	F.Deploy(); TestTrue(TEXT("New cast identity and lock"), F.Session->GetCastId().Value > First.Value && F.Session->IsEquipmentLocked());
	TestEqual(TEXT("New weight"), F.Session->GetEquipmentSnapshot().TotalMassG, 30.0f);
	F.Session->AbortCast(F.Session->GetCastId()); F.Session->ResetCast();
	TestFalse(TEXT("Abort Ready is not onboard preparation"), F.Session->CanChangeEquipment());
	F.Session->EndFishing(); TestFalse(TEXT("End cannot invent onboard return"), F.Session->CanChangeEquipment());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSessionDeployTest, "TipRun.M06.DeploymentAndCastIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSessionDeployTest::RunTest(const FString& Parameters)
{
	FTRTestSession F;
	if (!F.Start(*this)) { return false; }
	TestFalse(TEXT("Session no independent tick"), F.Session->PrimaryActorTick.bCanEverTick);
	TestFalse(TEXT("Fishing no independent tick"), F.Session->Fishing->PrimaryComponentTick.bCanEverTick);
	F.Step(10);
	const FVector OldRod = F.Boat->GetRodAnchorWorldM();
	F.Session->SubmitCommand(ETRFishingCommandType::Deploy, {});
	TestTrue(TEXT("Queued input has no immediate effect"), F.Session->Fishing->GetState() == ETRFishingState::Ready);
	F.Step();
	const FTREgiSnapshot Egi = F.Session->Fishing->GetSnapshot();
	const FTRBoatSnapshot Boat = F.Boat->GetBoatSnapshot();
	TestTrue(TEXT("Boat moved before placement"), Boat.RodTipM.X > OldRod.X);
	TestTrue(TEXT("No ballistic cast: rod XY"), Egi.PositionXYM.Equals(FVector2D(Boat.RodTipM.X, Boat.RodTipM.Y), 0.0));
	TestEqual(TEXT("Placed on sea surface"), Egi.DepthM, 0.0f);
	TestEqual(TEXT("Initial line reaches sea from rod"), Egi.LineLengthM, 1.5f);
	TestEqual(TEXT("Same tick as boat"), Egi.Tick, Boat.Tick);
	TestTrue(TEXT("FreeFall ready for M07"), Egi.FishingState == ETRFishingState::FreeFall && F.Session->GetSessionPhase() == ETRSessionPhase::Fishing);
	TestTrue(TEXT("Automatic payout action selected"), F.Session->Fishing->GetAction().LineMode == ETRLineMode::Payout);
	F.Step(60);
	TestTrue(TEXT("M07 advances depth after the unchanged M06 deployment contract"), F.Session->Fishing->GetSnapshot().DepthM > 0.0f);
	F.Session->SubmitCommand(ETRFishingCommandType::EndFishing, {}); F.Step();
	TestTrue(TEXT("Stale cast input rejected"), F.Session->GetLastCommandResult() == ETRCommandResult::RejectedInvalidState && F.Session->IsEquipmentLocked());
	TestTrue(TEXT("Abort current identity"), F.Session->AbortCast(Egi.CastId) == ETRCommandResult::Accepted);
	const FTRCatchResult Result = F.Session->GetLastResult();
	TestTrue(TEXT("Abort is no catch"), Result.Outcome == ETRCastOutcome::Aborted && Result.WeightKg == 0.0f && !Result.SimId.IsValid());
	TestTrue(TEXT("Second abort rejected"), F.Session->AbortCast(Egi.CastId) == ETRCommandResult::RejectedInvalidState);
	TestEqual(TEXT("Result not rewritten"), F.Session->GetLastResult().ElapsedSimSeconds, Result.ElapsedSimSeconds);
	F.Session->EndFishing();
	TArray<FText> Errors;
	TestTrue(TEXT("Restart without onboard return rejected"), F.Session->StartFishing(Errors) == ETRCommandResult::RejectedInvalidState);
	F.Deploy();
	TestEqual(TEXT("Rejected restart preserves last identity"), F.Session->GetCastId().Value, Egi.CastId.Value);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSessionFrozenTest, "TipRun.M06.FrozenEquipmentAndQueueBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSessionFrozenTest::RunTest(const FString& Parameters)
{
	FTRTestSession F; if (!F.Start(*this)) { return false; }
	const double Original = F.Session->GetEquipmentSnapshot().Parameters.JerkDurationS;
	F.Tuning->Parameters.JerkDurationS = 2.0; F.Deploy();
	TestEqual(TEXT("Prepared candidate frozen through Deploy"), F.Session->GetEquipmentSnapshot().Parameters.JerkDurationS, Original);
	F.Session->SubmitCommand(ETRFishingCommandType::EndFishing, F.Session->GetCastId(), F.Sim()->GetSimulationTime().TickIndex + 10000);
	if (!TestTrue(TEXT("Retrieve and prepare"), F.ReturnToReady())) { return false; }
	TestEqual(TEXT("Cast end clears old queue"), F.Sim()->GetQueuedCommandCount(), 0);
	TArray<FText> Errors;
	F.Session->TrySetEquipment(TREquipment::InitialEgiId(), TREquipment::NoSinkerId(), Errors);
	TestEqual(TEXT("Explicit candidate preparation reads edited tuning"), F.Session->GetEquipmentSnapshot().Parameters.JerkDurationS, 2.0);
	F.Session->SubmitCommand(ETRFishingCommandType::Deploy, F.Session->GetCastId());
	F.Sim()->SetSimulationPaused(true); F.Step(60); F.Sim()->SetSimulationPaused(false); F.Step();
	TestTrue(TEXT("Paused deploy discarded"), F.Session->Fishing->GetState() == ETRFishingState::Ready);
	F.Deploy(); const int64 Now = F.Sim()->GetSimulationTime().TickIndex;
	F.Session->SubmitCommand(ETRFishingCommandType::EndFishing, F.Session->GetCastId(), Now + 2);
	F.Session->SubmitCommand(ETRFishingCommandType::Jerk, F.Session->GetCastId(), Now); F.Step(3);
	TestTrue(TEXT("Earlier sequence at future tick processed"), !F.Session->IsAcceptingPlayerInput() && F.Session->GetLastCommandResult() == ETRCommandResult::Accepted);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSessionInvalidTest, "TipRun.M06.InvalidInputsAndDependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSessionInvalidTest::RunTest(const FString& Parameters)
{
	FTRTestSession F;
	TArray<FText> Errors;
	TestFalse(TEXT("Cannot submit before initialization"), F.Session->SubmitCommand(ETRFishingCommandType::Deploy, {}));
	if (!F.Environment(*this)) { return false; }
	TestFalse(TEXT("Missing initial sinker is not silently zero grams"), F.Session->Initialize(F.Sim(), F.BoatId, F.Egis.Get(), F.Sinkers.Get(), F.Tuning.Get(), NAME_None, Errors));
	TestFalse(TEXT("Missing table rejected"), F.Session->Initialize(F.Sim(), F.BoatId, nullptr, F.Sinkers.Get(), F.Tuning.Get(), TREquipment::NoSinkerId(), Errors));
	TestFalse(TEXT("Missing boat rejected"), F.Session->Initialize(F.Sim(), {}, F.Egis.Get(), F.Sinkers.Get(), F.Tuning.Get(), TREquipment::NoSinkerId(), Errors));
	TestTrue(TEXT("Valid retry works"), F.Session->Initialize(F.Sim(), F.BoatId, F.Egis.Get(), F.Sinkers.Get(), F.Tuning.Get(), TREquipment::NoSinkerId(), Errors));
	TestTrue(TEXT("Invalid equipment change rejected"), F.Session->TrySetEquipment(TEXT("MissingEgi"), TREquipment::NoSinkerId(), Errors) == ETRCommandResult::RejectedMissingData);
	TestEqual(TEXT("Rejected selection preserves initial equipment"), F.Session->GetEquipmentSnapshot().TotalMassG, 35.0f);
	TestTrue(TEXT("Start"), F.Session->StartFishing(Errors) == ETRCommandResult::Accepted);
	for (ETRFishingCommandType Type : {ETRFishingCommandType::Jerk, ETRFishingCommandType::Hook, ETRFishingCommandType::NextCast})
	{
		F.Session->SubmitCommand(Type, F.Session->GetCastId()); F.Step();
		TestTrue(TEXT("Unsupported/invalid-state command rejected"), F.Session->GetLastCommandResult() == ETRCommandResult::RejectedInvalidState);
	}
	TestFalse(TEXT("Past queued input rejected"), F.Session->SubmitCommand(ETRFishingCommandType::Deploy, {}, 0));
	F.Deploy();
	const FTRCastId First = F.Session->GetCastId();
	F.Session->SubmitCommand(ETRFishingCommandType::Deploy, First); F.Step();
	TestTrue(TEXT("Duplicate deploy cannot create another cast"), F.Session->GetCastId() == First && F.Session->GetLastCommandResult() == ETRCommandResult::RejectedInvalidState);
	F.World->DestroyActor(F.Provider.Get()); F.Step();
	TestTrue(TEXT("Invalid ocean aborts safely"), F.Session->GetSessionPhase() == ETRSessionPhase::Result && F.Session->GetLastResult().Outcome == ETRCastOutcome::Aborted);
	TestTrue(TEXT("Environment failure cannot unlock equipment"), F.Session->IsEquipmentLocked());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSessionLifetimeTest, "TipRun.M06.SessionLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSessionLifetimeTest::RunTest(const FString& Parameters)
{
	FTRTestSession F;
	if (!F.Start(*this)) { return false; }
	F.World->InitializeActorsForPlay(FURL());
	TestTrue(TEXT("Actor lifecycle initialized"), F.Session->IsActorInitialized());
	F.Session->DispatchBeginPlay();
	const FTRActorSimId Registration = F.Session->GetRegistrationId();
	F.Session->SubmitCommand(ETRFishingCommandType::Deploy, {}, 10);
	TestTrue(TEXT("Destroy session"), F.World->DestroyActor(F.Session.Get()));
	TestFalse(TEXT("EndPlay removed registration"), F.Sim()->IsRegistered(Registration));
	TestEqual(TEXT("EndPlay removed queued command"), F.Sim()->GetQueuedCommandCount(), 0);
	F.Step(20);
	TestTrue(TEXT("Boat continues independently after session end"), F.Boat->GetBoatSnapshot().PositionM.X > 10.0);
	F.Sim()->Unregister(Registration);
	FTRTestSession BeforeBeginPlay;
	if (!BeforeBeginPlay.Start(*this)) { return false; }
	BeforeBeginPlay.Session->SubmitCommand(ETRFishingCommandType::Deploy, {}, 10);
	BeforeBeginPlay.World->DestroyActor(BeforeBeginPlay.Session.Get());
	TestEqual(TEXT("Destruction before BeginPlay also clears input immediately"), BeforeBeginPlay.Sim()->GetQueuedCommandCount(), 0);
	return true;
}
#endif
