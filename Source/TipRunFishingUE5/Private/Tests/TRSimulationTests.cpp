#include "Game/TRSimulationWorldSubsystem.h"
#include "Data/TRSessionConfigDataAsset.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Data/TROceanAreaDataAsset.h"
#include "Ocean/TROceanWorldSubsystem.h"
#include "Ocean/TRSeabedProviderActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace
{
	struct FTRTestSimulation
	{
		UWorld* World = nullptr;
		TStrongObjectPtr<UTRSessionConfigDataAsset> Config{NewObject<UTRSessionConfigDataAsset>()};
		FTRTestSimulation()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("TR_Simulation_TestWorld")),
				nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			Config->StepSeconds = 1.0 / 60.0;
			Config->MaxCatchUpSteps = 8;
			Config->SessionSeed = 12345; // Test-only reproducible setup.
		}
		~FTRTestSimulation() { Shutdown(); }
		void Shutdown()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
				World = nullptr;
			}
		}
		FTRTestSimulation(const FTRTestSimulation&) = delete;
		FTRTestSimulation& operator=(const FTRTestSimulation&) = delete;
		UTRSimulationWorldSubsystem* Sim() const { return World ? World->GetSubsystem<UTRSimulationWorldSubsystem>() : nullptr; }
		bool Start(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Game coordinator"), Sim())) { return false; }
			TArray<FText> Errors;
			const bool bSuccess = Sim()->Configure(Config.Get(), Errors);
			for (const FText& Error : Errors) { Test.AddError(Error.ToString()); }
			return Test.TestTrue(TEXT("Clock configured"), bSuccess);
		}
		AActor* Actor() const { return World->SpawnActor<AActor>(); }
		FTRActorSimId Session(FTRSimulationStep Step = FTRSimulationStep::CreateLambda([](ETRSimulationPhase, const FTRSimTime&) {}),
			FTRSimulationCommand Command = FTRSimulationCommand::CreateLambda([](const FTRFishingCommand&) {})) const
		{
			return Sim()->RegisterSession(Actor(), MoveTemp(Step), MoveTemp(Command));
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSimulationOrderTest, "TipRun.M04.TickOrderAndOcean",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSimulationOrderTest::RunTest(const FString& Parameters)
{
	FTRTestSimulation F;
	if (!F.Start(*this)) { return false; }
	TStrongObjectPtr<UTROceanAreaDataAsset> Area(NewObject<UTROceanAreaDataAsset>());
	Area->Settings.AreaId = FName(TEXT("TestM04Ocean"));
	Area->Settings.BoundsMinXYM = FVector2D(-10.0, -10.0);
	Area->Settings.BoundsMaxXYM = FVector2D(10.0, 10.0);
	Area->Settings.FlatDepthM = 30.0f;
	UTROceanWorldSubsystem* Ocean = F.World->GetSubsystem<UTROceanWorldSubsystem>();
	TArray<FText> Errors;
	if (!TestTrue(TEXT("M03 ocean initialized"), Ocean->InitializeArea(Area.Get(), F.World->SpawnActor<ATRSeabedProviderActor>(), Errors))) { return false; }
	F.Sim()->AdvanceFrame(1.0 / 60.0); // Next update is tick 1; makes the time assertion nonzero.
	FString Trace;
	F.Session(FTRSimulationStep::CreateLambda([&](ETRSimulationPhase Phase, const FTRSimTime& Time)
	{
		Trace += FString::Printf(TEXT("S%d,"), int32(Phase));
		TestEqual(TEXT("All callbacks use current tick"), Time.TickIndex, int64(1));
		if (Phase == ETRSimulationPhase::Boat)
		{
			TestEqual(TEXT("Ocean advanced before boat phase"), Ocean->GetEnvironmentTimeS(), 1.0 / 60.0);
		}
	}));
	const auto Squid = [&](const TCHAR* Name)
	{
		return F.Sim()->RegisterSquid(F.Actor(), FTRSimulationStep::CreateLambda([&, Name](ETRSimulationPhase Phase, const FTRSimTime&)
		{
			Trace += FString::Printf(TEXT("%s%d,"), Name, int32(Phase));
		}));
	};
	const FTRActorSimId A = Squid(TEXT("A"));
	const FTRActorSimId B = Squid(TEXT("B"));
	TestTrue(TEXT("Registration-order IDs"), A.Value < B.Value);
	const FTRActorSimId Input = F.Session(FTRSimulationStep::CreateLambda([](ETRSimulationPhase, const FTRSimTime&) {}),
		FTRSimulationCommand::CreateLambda([&](const FTRFishingCommand&) { Trace += TEXT("C,"); }));
	TestTrue(TEXT("Hook queued"), F.Sim()->EnqueueCommand(Input, ETRFishingCommandType::Hook));
	F.Sim()->AdvanceFrame(1.0 / 60.0);
	TestEqual(TEXT("Input, timers, boat, fishing, ordered squid, resolution, fight, publication"), Trace,
		FString(TEXT("C,S0,A0,B0,S1,S2,A3,B3,S4,S5,S6,")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSimulationCommandsTest, "TipRun.M04.CommandQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSimulationCommandsTest::RunTest(const FString& Parameters)
{
	FTRTestSimulation F;
	if (!F.Start(*this)) { return false; }
	TArray<int64> Sequences;
	TArray<int64> Ticks;
	FTRActorSimId Id;
	Id = F.Session(FTRSimulationStep::CreateLambda([&](ETRSimulationPhase Phase, const FTRSimTime& Time)
	{
		if (Phase == ETRSimulationPhase::Publish && Time.TickIndex == 0)
		{
			TestTrue(TEXT("Callback input queued for next tick"), F.Sim()->EnqueueCommand(Id, ETRFishingCommandType::Hook));
			TestFalse(TEXT("Cannot inject into current step"), F.Sim()->EnqueueCommand(Id, ETRFishingCommandType::Hook, 0.0f, 0));
			F.Sim()->AdvanceFrame(1.0); // Reentrant advance must do nothing.
		}
	}), FTRSimulationCommand::CreateLambda([&](const FTRFishingCommand& Command)
	{
		Sequences.Add(Command.Sequence); Ticks.Add(Command.TargetTick);
	}));
	TestTrue(TEXT("Future first"), F.Sim()->EnqueueCommand(Id, ETRFishingCommandType::Fall, 0.0f, 2));
	TestTrue(TEXT("Live second"), F.Sim()->EnqueueCommand(Id, ETRFishingCommandType::Stay));
	TestTrue(TEXT("Live third"), F.Sim()->EnqueueCommand(Id, ETRFishingCommandType::Jerk));
	F.Sim()->AdvanceFrame(1.0 / 60.0);
	TestTrue(TEXT("Same-tick sequence before future input"), Sequences == TArray<int64>({2, 3}));
	TestEqual(TEXT("No recursive advancement"), F.Sim()->GetSimulationTime().TickIndex, int64(1));
	F.Sim()->AdvanceFrame(2.0 / 60.0);
	TestTrue(TEXT("TargetTick then Sequence"), Sequences == TArray<int64>({2, 3, 4, 1}));
	TestTrue(TEXT("Publication command executes on next tick"), Ticks == TArray<int64>({0, 0, 1, 2}));
	TestFalse(TEXT("Past input rejected"), F.Sim()->EnqueueCommand(Id, ETRFishingCommandType::Hook, 0.0f, 1));
	TestFalse(TEXT("Nonfinite axis rejected"), F.Sim()->EnqueueCommand(Id, ETRFishingCommandType::Hook, std::numeric_limits<float>::quiet_NaN()));
	TestFalse(TEXT("Unknown type rejected"), F.Sim()->EnqueueCommand(Id, static_cast<ETRFishingCommandType>(255)));
	F.Sim()->EnqueueCommand(Id, ETRFishingCommandType::Hook);
	F.Sim()->ClearCommands();
	F.Sim()->AdvanceFrame(1.0 / 60.0);
	TestEqual(TEXT("Cleared input not replayed"), Sequences.Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSimulationPauseTest, "TipRun.M04.PauseAndCatchUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSimulationPauseTest::RunTest(const FString& Parameters)
{
	FTRTestSimulation F;
	if (!F.Start(*this)) { return false; }
	int32 Commands = 0;
	const FTRActorSimId Id = F.Session(FTRSimulationStep::CreateLambda([](ETRSimulationPhase, const FTRSimTime&) {}),
		FTRSimulationCommand::CreateLambda([&](const FTRFishingCommand&) { ++Commands; }));
	F.Sim()->AdvanceFrame(0.5 / 60.0);
	TestEqual(TEXT("Fraction does not advance"), F.Sim()->GetSimulationTime().TickIndex, int64(0));
	F.Sim()->EnqueueCommand(Id, ETRFishingCommandType::Hook);
	F.Sim()->SetSimulationPaused(true);
	F.Sim()->AdvanceFrame(1000.0);
	TestFalse(TEXT("Paused input rejected"), F.Sim()->EnqueueCommand(Id, ETRFishingCommandType::Hook));
	TestEqual(TEXT("Pause stops clock"), F.Sim()->GetSimulationTime().TickIndex, int64(0));
	F.Sim()->SetSimulationPaused(false);
	F.Sim()->AdvanceFrame(1.0 / 60.0);
	TestEqual(TEXT("No paused command replay"), Commands, 0);
	F.Sim()->EnqueueCommand(Id, ETRFishingCommandType::Hook);
	F.World->GetWorldSettings()->SetPauserPlayerState(F.World->SpawnActor<APlayerState>());
	TestTrue(TEXT("Engine pause recognized"), F.Sim()->IsSimulationPaused());
	F.Sim()->Tick(10.0f);
	F.World->GetWorldSettings()->SetPauserPlayerState(nullptr);
	TestEqual(TEXT("Engine pause stops clock"), F.Sim()->GetSimulationTime().TickIndex, int64(1));
	F.Sim()->AdvanceFrame(1000.0);
	TestEqual(TEXT("Catch-up limited to eight"), F.Sim()->GetSimulationTime().TickIndex, int64(9));
	TestEqual(TEXT("Overload diagnostic"), F.Sim()->GetCatchUpDropCount(), int64(1));
	F.Sim()->AdvanceFrame(0.0);
	TestEqual(TEXT("Excess not carried forward"), F.Sim()->GetSimulationTime().TickIndex, int64(9));
	F.Sim()->AdvanceFrame(1.0 / 60.0);
	TestEqual(TEXT("Normal resume"), F.Sim()->GetSimulationTime().TickIndex, int64(10));
	TestEqual(TEXT("No engine-paused command replay"), Commands, 0);
	F.Sim()->AdvanceFrame(-1.0);
	F.Sim()->AdvanceFrame(std::numeric_limits<double>::infinity());
	F.Sim()->AdvanceFrame(std::numeric_limits<double>::quiet_NaN());
	TestEqual(TEXT("Invalid delta does not change time"), F.Sim()->GetSimulationTime().TickIndex, int64(10));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSimulationLifetimeTest, "TipRun.M04.RegistrationLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSimulationLifetimeTest::RunTest(const FString& Parameters)
{
	FTRTestSimulation F;
	if (!F.Start(*this)) { return false; }
	int32 RemovedCalls = 0;
	int32 NewCalls = 0;
	FTRActorSimId Victim, Added;
	const FTRActorSimId First = F.Session(FTRSimulationStep::CreateLambda([&](ETRSimulationPhase Phase, const FTRSimTime& Time)
	{
		if (Phase == ETRSimulationPhase::Timers && Time.TickIndex == 0)
		{
			F.Sim()->Unregister(Victim);
			F.Sim()->Unregister(Victim);
			Added = F.Session(FTRSimulationStep::CreateLambda([&](ETRSimulationPhase, const FTRSimTime&) { ++NewCalls; }));
		}
	}));
	Victim = F.Session(FTRSimulationStep::CreateLambda([&](ETRSimulationPhase, const FTRSimTime&) { ++RemovedCalls; }),
		FTRSimulationCommand::CreateLambda([&](const FTRFishingCommand&) { ++RemovedCalls; }));
	F.Sim()->EnqueueCommand(Victim, ETRFishingCommandType::Hook, 0.0f, 1);
	F.Sim()->AdvanceFrame(1.0 / 60.0);
	TestEqual(TEXT("Removal during iteration cancels later callbacks"), RemovedCalls, 0);
	TestEqual(TEXT("Registration during iteration waits for next step"), NewCalls, 0);
	TestEqual(TEXT("Removed future command deleted"), F.Sim()->GetQueuedCommandCount(), 0);
	TestTrue(TEXT("ID never reused"), Added.Value > Victim.Value && Victim.Value > First.Value);
	F.Sim()->AdvanceFrame(1.0 / 60.0);
	TestEqual(TEXT("New session participates next step"), NewCalls, 6);
	AActor* Destroyed = F.Actor();
	const FTRActorSimId DeadId = F.Sim()->RegisterSession(Destroyed,
		FTRSimulationStep::CreateLambda([&](ETRSimulationPhase, const FTRSimTime&) { ++RemovedCalls; }),
		FTRSimulationCommand::CreateLambda([&](const FTRFishingCommand&) { ++RemovedCalls; }));
	TestFalse(TEXT("Duplicate owner refused"), F.Sim()->RegisterSquid(Destroyed,
		FTRSimulationStep::CreateLambda([](ETRSimulationPhase, const FTRSimTime&) {})).IsValid());
	F.Sim()->EnqueueCommand(DeadId, ETRFishingCommandType::Hook);
	TestTrue(TEXT("Test actor destroyed"), F.World->DestroyActor(Destroyed));
	F.Sim()->AdvanceFrame(1.0 / 60.0);
	TestFalse(TEXT("Destroyed actor unavailable"), F.Sim()->IsRegistered(DeadId));
	TestEqual(TEXT("No callback into dead or removed owner"), RemovedCalls, 0);
	TestEqual(TEXT("Destroyed actor command removed"), F.Sim()->GetQueuedCommandCount(), 0);
	F.Sim()->Unregister(First); F.Sim()->Unregister(Added);
	TestEqual(TEXT("Unregistration does not reset world time"), F.Sim()->GetSimulationTime().TickIndex, int64(3));
	const FTRActorSimId Last = F.Session();
	F.Sim()->EnqueueCommand(Last, ETRFishingCommandType::Hook);
	TStrongObjectPtr<UTRSimulationWorldSubsystem> Retained(F.Sim());
	F.Shutdown(); // World owns subsystem deinitialization; never call it a second time.
	TestFalse(TEXT("World shutdown clears registration"), Retained->IsRegistered(Last));
	TestEqual(TEXT("World shutdown clears queued input"), Retained->GetQueuedCommandCount(), 0);
	Retained->AdvanceFrame(1.0);
	TestEqual(TEXT("Deinitialized clock stopped"), Retained->GetSimulationTime().TickIndex, int64(3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSimulationReplayTest, "TipRun.M04.FrameRateAndSeedReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSimulationReplayTest::RunTest(const FString& Parameters)
{
	TArray<uint32> Reference;
	for (int32 FPS : {30, 60, 120})
	{
		FTRTestSimulation F;
		if (!F.Start(*this)) { return false; }
		TArray<uint32> Trace;
		FRandomStream Stream;
		const FTRActorSimId Id = F.Session(FTRSimulationStep::CreateLambda([&](ETRSimulationPhase Phase, const FTRSimTime& Time)
		{
			if (Phase == ETRSimulationPhase::Publish) { Trace.Add(uint32(Time.TickIndex)); Trace.Add(Stream.GetUnsignedInt()); }
		}), FTRSimulationCommand::CreateLambda([&](const FTRFishingCommand& Command) { Trace.Add(0xffffffffU); Trace.Add(uint32(Command.TargetTick)); }));
		TestTrue(TEXT("Create purpose stream"), F.Sim()->CreateRandomStream(Id, 1, Stream));
		FRandomStream Same, OtherPurpose, OtherActor;
		F.Sim()->CreateRandomStream(Id, 1, Same);
		F.Sim()->CreateRandomStream(Id, 2, OtherPurpose);
		F.Sim()->CreateRandomStream(F.Session(), 1, OtherActor);
		TestEqual(TEXT("Same identity seed"), Same.GetInitialSeed(), Stream.GetInitialSeed());
		TestNotEqual(TEXT("Purpose separation"), OtherPurpose.GetInitialSeed(), Stream.GetInitialSeed());
		TestNotEqual(TEXT("Participant separation"), OtherActor.GetInitialSeed(), Stream.GetInitialSeed());
		for (int32 Draw = 0; Draw < 100; ++Draw) { OtherPurpose.GetUnsignedInt(); }
		TestEqual(TEXT("Other purpose draws do not change stream"), Same.GetUnsignedInt(), Stream.GetUnsignedInt());
		TestFalse(TEXT("Invalid ID has no stream"), F.Sim()->CreateRandomStream({}, 1, Same));
		for (int64 Tick : {int64(0), int64(3), int64(17), int64(59)})
		{
			TestTrue(TEXT("Recorded input queued"), F.Sim()->EnqueueCommand(Id, ETRFishingCommandType::Jerk, 0.0f, Tick));
		}
		for (int32 Frame = 0; Frame < FPS; ++Frame) { F.Sim()->Tick(1.0f / float(FPS)); }
		TestEqual(TEXT("One second has 60 simulation ticks"), F.Sim()->GetSimulationTime().TickIndex, int64(60));
		if (FPS == 30) { Reference = Trace; }
		else { TestTrue(TEXT("30/60/120 fps event and random sequences match"), Trace == Reference); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRSimulationConfigTest, "TipRun.M04.ConfigurationAndWorldScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRSimulationConfigTest::RunTest(const FString& Parameters)
{
	FTRTestSimulation F;
	TArray<FText> Errors;
	F.Sim()->AdvanceFrame(1.0);
	TestEqual(TEXT("Unconfigured clock cannot advance"), F.Sim()->GetSimulationTime().TickIndex, int64(0));
	F.Config->StepSeconds = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("Reject NaN step"), F.Sim()->Configure(F.Config.Get(), Errors));
	F.Config->StepSeconds = 0.0;
	TestFalse(TEXT("Reject zero step"), F.Config->Validate(Errors));
	F.Config->StepSeconds = -1.0;
	TestFalse(TEXT("Reject negative step"), F.Config->Validate(Errors));
	F.Config->StepSeconds = std::numeric_limits<double>::max();
	TestFalse(TEXT("Reject overflow budget"), F.Config->Validate(Errors));
	F.Config->StepSeconds = 1.0 / 60.0;
	F.Config->MaxCatchUpSteps = 0;
	FDataValidationContext Context;
	TestTrue(TEXT("Editor validation rejects zero catch-up"), F.Config->IsDataValid(Context) == EDataValidationResult::Invalid);
	F.Config->MaxCatchUpSteps = 8;
	if (!F.Start(*this)) { return false; }
	F.Config->StepSeconds = 1.0;
	F.Config->MaxCatchUpSteps = 1;
	F.Config->SessionSeed = 2;
	F.Sim()->AdvanceFrame(2.0 / 60.0);
	TestEqual(TEXT("Config copied, not live-edited"), F.Sim()->GetSimulationTime().TickIndex, int64(2));
	TestFalse(TEXT("Cannot reset world by reconfiguration"), F.Sim()->Configure(F.Config.Get(), Errors));
	TStrongObjectPtr<UWorld> ScopeWorld(NewObject<UWorld>());
	const UTRSimulationWorldSubsystem* Defaults = GetDefault<UTRSimulationWorldSubsystem>();
	for (EWorldType::Type Type : {EWorldType::Game, EWorldType::PIE, EWorldType::Editor, EWorldType::EditorPreview, EWorldType::GamePreview})
	{
		ScopeWorld->WorldType = Type;
		TestEqual(TEXT("Only Game/PIE create coordinator"), Defaults->ShouldCreateSubsystem(ScopeWorld.Get()),
			Type == EWorldType::Game || Type == EWorldType::PIE);
	}
	return true;
}
#endif
