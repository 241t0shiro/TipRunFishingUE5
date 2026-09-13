#include "TRSessionTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSessionLockTest, "TipRun.M06.F16EquipmentLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSessionLockTest::RunTest(const FString& Parameters)
{
	FTRTestSession F;
	if (!F.Initialize(*this)) { return false; }
	TArray<FText> Errors;
	TestTrue(TEXT("Initial 3.5 egi"), F.Session->GetEquipmentSnapshot().EgiId == TREquipment::InitialEgiId());
	TestEqual(TEXT("Initial 35 g with explicit no-sinker test selection"), F.Session->GetEquipmentSnapshot().TotalMassG, 35.0f);
	TestTrue(TEXT("Before start may change"), F.Session->TrySetEquipment(TEXT("Egi_4"), TEXT("Sinker_5"), Errors) == ETRCommandResult::Accepted);
	TestTrue(TEXT("Start"), F.Session->StartFishing(Errors) == ETRCommandResult::Accepted);
	TestTrue(TEXT("Duplicate start refused"), F.Session->StartFishing(Errors) == ETRCommandResult::RejectedInvalidState);
	TestTrue(TEXT("Ready after start stays locked"), F.Session->TrySetEquipment(TEXT("Egi_3"), TREquipment::NoSinkerId(), Errors) == ETRCommandResult::RejectedBusy);
	F.Deploy();
	TestTrue(TEXT("During cast stays locked"), F.Session->TrySetEquipment(TEXT("Egi_3"), TREquipment::NoSinkerId(), Errors) == ETRCommandResult::RejectedBusy);
	const FTRCastId First = F.Session->GetCastId();
	TestTrue(TEXT("Explicit abort allows next-cast preparation without implementing retrieval"), F.Session->AbortCast(First) == ETRCommandResult::Accepted);
	TestTrue(TEXT("Result stays locked"), F.Session->IsEquipmentLocked());
	F.Session->SubmitCommand(ETRFishingCommandType::NextCast, First); F.Step();
	TestTrue(TEXT("Next cast Ready"), F.Session->Fishing->GetState() == ETRFishingState::Ready);
	TestTrue(TEXT("Next Ready still locked"), F.Session->TrySetEquipment(TEXT("Egi_3"), TREquipment::NoSinkerId(), Errors) == ETRCommandResult::RejectedBusy);
	F.Deploy();
	TestTrue(TEXT("Cast IDs increase"), F.Session->GetCastId().Value > First.Value);
	TestEqual(TEXT("Locked equipment carried to next cast"), F.Session->GetEquipmentSnapshot().TotalMassG, 45.0f);
	F.Session->EndFishing();
	TestFalse(TEXT("End releases lock"), F.Session->IsEquipmentLocked());
	TestTrue(TEXT("End allows equipment changes"), F.Session->TrySetEquipment(TEXT("Egi_3"), TREquipment::NoSinkerId(), Errors) == ETRCommandResult::Accepted);
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
	TestTrue(TEXT("Restart fishing"), F.Session->StartFishing(Errors) == ETRCommandResult::Accepted);
	F.Deploy();
	TestTrue(TEXT("End/start does not reuse CastId"), F.Session->GetCastId().Value > Egi.CastId.Value);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSessionFrozenTest, "TipRun.M06.FrozenEquipmentAndQueueBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSessionFrozenTest::RunTest(const FString& Parameters)
{
	FTRTestSession F;
	if (!F.Start(*this)) { return false; }
	const double Original = F.Session->GetEquipmentSnapshot().Parameters.AutoStayDelayS;
	F.Tuning->Parameters.AutoStayDelayS = 2.0;
	F.Deploy();
	TestEqual(TEXT("Editor change cannot alter locked tuning"), F.Session->GetEquipmentSnapshot().Parameters.AutoStayDelayS, Original);
	F.Session->SubmitCommand(ETRFishingCommandType::EndFishing, F.Session->GetCastId(), F.Sim()->GetSimulationTime().TickIndex + 1);
	F.Session->AbortCast(F.Session->GetCastId());
	TestEqual(TEXT("Abort clears old registration queue"), F.Sim()->GetQueuedCommandCount(), 0);
	F.Session->ResetCast(); F.Deploy(); F.Step(2);
	TestTrue(TEXT("Old queued end cannot end new cast"), F.Session->IsEquipmentLocked());
	TestEqual(TEXT("Next cast keeps frozen tuning"), F.Session->GetEquipmentSnapshot().Parameters.AutoStayDelayS, Original);
	F.Session->EndFishing();
	TArray<FText> Errors;
	TestTrue(TEXT("Restart validates new settings"), F.Session->StartFishing(Errors) == ETRCommandResult::Accepted);
	TestEqual(TEXT("New fishing session uses edited settings"), F.Session->GetEquipmentSnapshot().Parameters.AutoStayDelayS, 2.0);
	F.Session->SubmitCommand(ETRFishingCommandType::Deploy, F.Session->GetCastId());
	F.Sim()->SetSimulationPaused(true); F.Step(60); F.Sim()->SetSimulationPaused(false); F.Step();
	TestTrue(TEXT("Paused input does not replay deployment"), F.Session->Fishing->GetState() == ETRFishingState::Ready);
	F.Deploy();
	const int64 Now = F.Sim()->GetSimulationTime().TickIndex;
	F.Session->SubmitCommand(ETRFishingCommandType::EndFishing, F.Session->GetCastId(), Now + 2);
	F.Session->SubmitCommand(ETRFishingCommandType::Jerk, F.Session->GetCastId(), Now);
	F.Step(3);
	TestTrue(TEXT("Future tick with earlier sequence is processed in Tick/Sequence order"),
		!F.Session->IsEquipmentLocked() && F.Session->GetLastCommandResult() == ETRCommandResult::Accepted);
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
