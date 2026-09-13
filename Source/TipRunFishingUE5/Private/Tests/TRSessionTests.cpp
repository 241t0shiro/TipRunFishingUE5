#include "Game/TRFishingSessionActor.h"
#include "Fishing/TRFishingComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Boat/TRBoatPawn.h"
#include "Data/TROceanAreaDataAsset.h"
#include "Data/TRSessionConfigDataAsset.h"
#include "Ocean/TROceanWorldSubsystem.h"
#include "Ocean/TRSeabedProviderActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/AutomationTest.h"

namespace
{
	struct FTRTestSession
	{
		UWorld* World = nullptr;
		TStrongObjectPtr<UDataTable> Egis{LoadObject<UDataTable>(nullptr, TEXT("/Game/TipRun/Prototype/Data/DT_TR_Egi_Prototype"))};
		TStrongObjectPtr<UDataTable> Sinkers{LoadObject<UDataTable>(nullptr, TEXT("/Game/TipRun/Prototype/Data/DT_TR_Sinker_Prototype"))};
		TStrongObjectPtr<UTRFishingTuningDataAsset> Tuning;
		TStrongObjectPtr<UTROceanAreaDataAsset> Area{NewObject<UTROceanAreaDataAsset>()};
		TStrongObjectPtr<UTRBoatTuningDataAsset> BoatTuning{NewObject<UTRBoatTuningDataAsset>()};
		TStrongObjectPtr<UTRSessionConfigDataAsset> Clock{NewObject<UTRSessionConfigDataAsset>()};
		TWeakObjectPtr<ATRBoatPawn> Boat;
		TWeakObjectPtr<ATRSeabedProviderActor> Provider;
		TWeakObjectPtr<ATRFishingSessionActor> Session;
		FTRActorSimId BoatId;
		FTRTestSession()
		{
			if (auto* Source = LoadObject<UTRFishingTuningDataAsset>(nullptr, TEXT("/Game/TipRun/Prototype/Data/DA_TR_FishingTuning_Prototype")))
			{
				Tuning.Reset(DuplicateObject<UTRFishingTuningDataAsset>(Source, GetTransientPackage()));
			}
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("TR_Session_TestWorld")), nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			// Temporary test world/coefficients; no saved content or product defaults.
			Area->Settings.AreaId = TEXT("TestM06Ocean");
			Area->Settings.BoundsMinXYM = FVector2D(-1000.0, -1000.0);
			Area->Settings.BoundsMaxXYM = FVector2D(1000.0, 1000.0);
			Area->Settings.FlatDepthM = 30.0f;
			Area->Settings.SurfaceZ_M = 2.0f;
			Area->Settings.CurrentMps = FVector(1.0, 0.0, 0.0);
			BoatTuning->Parameters.CurrentResponse01 = 0.5;
			BoatTuning->Parameters.VelocityResponsePerS = 2.0;
			BoatTuning->Parameters.MaxDriftSpeedMps = 3.0;
			BoatTuning->Parameters.HullHeightOffsetM = 0.5;
			BoatTuning->Parameters.RodAnchorOffsetM = FVector(2.0, 1.0, 1.0);
			Clock->StepSeconds = 1.0 / 60.0; Clock->MaxCatchUpSteps = 8;
			if (World)
			{
				Provider = World->SpawnActor<ATRSeabedProviderActor>();
				Boat = World->SpawnActor<ATRBoatPawn>(); Boat->Tuning = BoatTuning.Get();
				Session = World->SpawnActor<ATRFishingSessionActor>();
			}
		}
		~FTRTestSession() { Shutdown(); }
		void Shutdown()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } World = nullptr; }
		}
		FTRTestSession(const FTRTestSession&) = delete;
		FTRTestSession& operator=(const FTRTestSession&) = delete;
		UTRSimulationWorldSubsystem* Sim() const { return World->GetSubsystem<UTRSimulationWorldSubsystem>(); }
		bool Environment(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Test tuning loaded"), Tuning.Get()) || !Test.TestNotNull(TEXT("Session spawned"), Session.Get())) { return false; }
			TArray<FText> Errors;
			bool bValid = Sim()->Configure(Clock.Get(), Errors) && World->GetSubsystem<UTROceanWorldSubsystem>()->InitializeArea(Area.Get(), Provider.Get(), Errors);
			if (bValid) { BoatId = Sim()->RegisterBoat(Boat.Get(), FVector2D(10.0, 20.0), 0.0f, Errors); bValid = BoatId.IsValid(); }
			for (const FText& Error : Errors) { Test.AddError(Error.ToString()); }
			return Test.TestTrue(TEXT("Environment initialized"), bValid);
		}
		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Environment(Test)) { return false; }
			TArray<FText> Errors;
			const bool bValid = Session->Initialize(Sim(), BoatId, Egis.Get(), Sinkers.Get(), Tuning.Get(), TREquipment::NoSinkerId(), Errors);
			for (const FText& Error : Errors) { Test.AddError(Error.ToString()); }
			return Test.TestTrue(TEXT("Session initialized"), bValid);
		}
		bool Start(FAutomationTestBase& Test)
		{
			if (!Initialize(Test)) { return false; }
			TArray<FText> Errors;
			const ETRCommandResult Result = Session->StartFishing(Errors);
			for (const FText& Error : Errors) { Test.AddError(Error.ToString()); }
			return Test.TestTrue(TEXT("Fishing started"), Result == ETRCommandResult::Accepted);
		}
		void Step(int32 Count = 1) { for (int32 I = 0; I < Count; ++I) { Sim()->AdvanceFrame(1.0 / 60.0); } }
		void Deploy() { Session->SubmitCommand(ETRFishingCommandType::Deploy, Session->GetCastId()); Step(); }
	};
}

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
	TestEqual(TEXT("M06 does not implement depth integration"), F.Session->Fishing->GetSnapshot().DepthM, 0.0f);
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
