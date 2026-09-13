#include "TRSessionTestFixture.h"
#include "Game/TRPlayerController.h"
#include "Game/TRGameModeBase.h"
#include "UI/TRFishingHUDWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"

namespace
{
	struct FTRTestInput
	{
		FTRTestSession F;
		TWeakObjectPtr<ATRPlayerController> PC;
		TStrongObjectPtr<UEnhancedInputComponent> Input;
		TStrongObjectPtr<UEnhancedPlayerInput> PlayerInput;
		bool Start(FAutomationTestBase& Test)
		{
			if (!F.Start(Test)) { return false; }
			PC = F.World->SpawnActor<ATRPlayerController>();
			PC->InputConfig = UTRInputConfigDataAsset::CreatePrototype(PC.Get());
			Input.Reset(NewObject<UEnhancedInputComponent>(PC.Get())); PC->InputComponent = Input.Get();
			PlayerInput.Reset(NewObject<UEnhancedPlayerInput>(PC.Get())); PC->PlayerInput = PlayerInput.Get();
			return Test.TestTrue(TEXT("Enhanced bindings installed"), PC->InstallInputBindings(Input.Get())) &&
				Test.TestTrue(TEXT("Session connected"), PC->BindSession(F.Session.Get()));
		}
		void Inject(ETRPlayerAction Action, bool bPressed)
		{
			const UInputAction* IA = PC->InputConfig->Bindings[int32(Action)].Action;
			PlayerInput->InjectInputForAction(IA, FInputActionValue(bPressed));
			PlayerInput->ProcessInputStack({ Input.Get() }, 1.0f / 60.0f, false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRInputEnhancedTest, "TipRun.M09.U02EnhancedPressRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRInputEnhancedTest::RunTest(const FString& Parameters)
{
	FTRTestInput R; if (!R.Start(*this)) { return false; }
	R.F.Deploy();
	TArray<ETRFishingCommandType> Commands;
	R.F.Session->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C, ETRCommandResult Result)
	{
		Commands.Add(C.Type); TestTrue(TEXT("Future game logic is still rejected"), Result == ETRCommandResult::RejectedInvalidState);
	});
	R.PC->InstallInputBindings(R.Input.Get()); // Rebinding must not duplicate handlers.
	TestEqual(TEXT("Started/Completed/Canceled, never Triggered"), R.Input->GetActionEventBindings().Num(), 27);
	for (int32 I = 0; I < 30; ++I) { R.Inject(ETRPlayerAction::Hook, true); }
	R.F.Step(); TestEqual(TEXT("Holding Hook queues once"), Commands.Num(), 1);
	R.Inject(ETRPlayerAction::Hook, false); R.Inject(ETRPlayerAction::Hook, true); R.F.Step();
	TestEqual(TEXT("New press queues once more"), Commands.Num(), 2);
	R.Inject(ETRPlayerAction::Hook, false);
	R.Inject(ETRPlayerAction::Retrieve, true); TestTrue(TEXT("Retrieve held at adapter"), R.PC->IsRetrieveHeld());
	R.Inject(ETRPlayerAction::Retrieve, false); TestFalse(TEXT("Release clears held state"), R.PC->IsRetrieveHeld());
	R.F.Step();
	TestEqual(TEXT("Retrieve start and stop both delivered"), Commands.Num(), 4);
	TestTrue(TEXT("Retrieve order"), Commands.Num() == 4 && Commands[2] == ETRFishingCommandType::RetrieveStarted && Commands[3] == ETRFishingCommandType::RetrieveStopped);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRInputOrderTest, "TipRun.M09.FixedOrderAndFrameRates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRInputOrderTest::RunTest(const FString& Parameters)
{
	TArray<FTRFishingCommand> Reference; FTREgiSnapshot ReferenceEgi;
	for (int32 FPS : {30, 60, 120})
	{
		FTRTestInput R; if (!R.Start(*this)) { return false; } R.F.Deploy();
		TArray<FTRFishingCommand> Seen;
		R.F.Session->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C, ETRCommandResult) { Seen.Add(C); });
		const int64 T = R.F.Sim()->GetSimulationTime().TickIndex;
		TestTrue(TEXT("Queue Hook at T+2"), R.PC->ActionStarted(ETRPlayerAction::Hook, T + 2));
		TestTrue(TEXT("Queue Jerk at earlier T+1"), R.PC->ActionStarted(ETRPlayerAction::Jerk, T + 1));
		TestTrue(TEXT("Queue Fall at same T+2"), R.PC->ActionStarted(ETRPlayerAction::Fall, T + 2));
		TestEqual(TEXT("No immediate game input execution"), Seen.Num(), 0);
		for (int32 I = 0; I < FPS; ++I) { R.F.Sim()->Tick(1.0f / FPS); }
		if (!TestEqual(TEXT("Three commands delivered"), Seen.Num(), 3)) { return false; }
		TestTrue(TEXT("TargetTick then Sequence"), Seen[0].Type == ETRFishingCommandType::Jerk && Seen[1].Type == ETRFishingCommandType::Hook &&
			Seen[2].Type == ETRFishingCommandType::Fall && Seen[1].Sequence < Seen[2].Sequence);
		const auto Egi = R.F.Session->Fishing->GetSnapshot();
		if (FPS == 30) { Reference = Seen; ReferenceEgi = Egi; }
		else
		{
			for (int32 I = 0; I < Seen.Num(); ++I) { TestTrue(TEXT("Identical input replay"), Seen[I].Type == Reference[I].Type && Seen[I].TargetTick == Reference[I].TargetTick && Seen[I].Sequence == Reference[I].Sequence); }
			TestTrue(TEXT("Identical simulation result"), Egi.Tick == ReferenceEgi.Tick && Egi.DepthM == ReferenceEgi.DepthM && Egi.PositionXYM.Equals(ReferenceEgi.PositionXYM, 0.0));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRInputLifetimeTest, "TipRun.M09.CastSessionAndControllerLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRInputLifetimeTest::RunTest(const FString& Parameters)
{
	FTRTestInput R; if (!R.Start(*this)) { return false; } R.F.Deploy();
	const auto Old = R.F.Session->GetCastId();
	R.F.Session->AbortCast(Old); R.F.Session->ResetCast(); R.F.Deploy();
	TestFalse(TEXT("Controller rejects stale CastId"), R.PC->SubmitFishingCommand(ETRFishingCommandType::EndFishing, Old));
	R.F.Session->SubmitCommand(ETRFishingCommandType::EndFishing, Old); R.F.Step();
	TestTrue(TEXT("Session also rejects stale queued input"), R.F.Session->IsAcceptingPlayerInput() && R.F.Session->GetLastCommandResult() == ETRCommandResult::RejectedInvalidState);
	TestTrue(TEXT("Cancel queues existing EndFishing"), R.PC->ActionStarted(ETRPlayerAction::Cancel)); R.F.Step();
	TestTrue(TEXT("Cancel aborts on fixed update"), R.F.Session->HasResult() && R.F.Session->GetLastResult().Outcome == ETRCastOutcome::Aborted);
	TestFalse(TEXT("Ended session rejects input"), R.PC->ActionStarted(ETRPlayerAction::Fall));
	R.F.World->DestroyActor(R.F.Session.Get());
	TestFalse(TEXT("Destroyed session rejects input"), R.PC->ActionStarted(ETRPlayerAction::Hook));
	R.F.World->DestroyActor(R.PC.Get());
	TestEqual(TEXT("Controller destruction removes Enhanced bindings"), R.Input->GetActionEventBindings().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRInputPauseFocusTest, "TipRun.M09.PauseFocusAndHeldInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRInputPauseFocusTest::RunTest(const FString& Parameters)
{
	FTRTestInput R; if (!R.Start(*this)) { return false; } R.F.Deploy();
	R.PC->ActionStarted(ETRPlayerAction::Retrieve); R.PC->ActionStarted(ETRPlayerAction::Hook);
	R.PC->SetPauseRequested(true);
	const auto Before = R.F.Session->Fishing->GetSnapshot();
	TestFalse(TEXT("Pause releases reel hold"), R.PC->IsRetrieveHeld());
	TestEqual(TEXT("Pause discards queued actions"), R.F.Sim()->GetQueuedCommandCount(), 0);
	TestFalse(TEXT("Paused press rejected"), R.PC->ActionStarted(ETRPlayerAction::Jerk));
	R.F.Step(120); TestEqual(TEXT("Pause preserves simulation tick"), R.F.Session->Fishing->GetSnapshot().Tick, Before.Tick);
	R.PC->SetPauseRequested(false);
	TestFalse(TEXT("Held key not replayed on resume"), R.PC->ActionStarted(ETRPlayerAction::Retrieve));
	R.F.Step(); TestTrue(TEXT("Resume delivers only safety stop"), R.F.Session->GetLastCommandResult() == ETRCommandResult::RejectedInvalidState);
	R.PC->ActionReleased(ETRPlayerAction::Retrieve); TestTrue(TEXT("Release then new press rearms"), R.PC->ActionStarted(ETRPlayerAction::Retrieve));
	R.PC->SetInputFocus(false);
	TestFalse(TEXT("Focus loss releases hold"), R.PC->IsRetrieveHeld());
	TestFalse(TEXT("Unfocused input rejected"), R.PC->ActionStarted(ETRPlayerAction::Fall));
	R.PC->SetInputFocus(true);
	TestFalse(TEXT("Focus regain does not replay reel"), R.PC->ActionStarted(ETRPlayerAction::Retrieve));
	R.PC->ActionReleased(ETRPlayerAction::Retrieve); R.PC->ActionStarted(ETRPlayerAction::Retrieve);
	R.PC->FlushPressedKeys(); TestFalse(TEXT("Viewport focus flush clears hold"), R.PC->IsRetrieveHeld());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRHUDReadTest, "TipRun.M09.HUDReadOnlySnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRHUDReadTest::RunTest(const FString& Parameters)
{
	FTRTestInput R; if (!R.Start(*this)) { return false; } R.F.Deploy(); R.F.Step(60);
	const auto Before = R.F.Session->Fishing->GetSnapshot(); const auto Time = R.F.Sim()->GetSimulationTime();
	const int32 Queue = R.F.Sim()->GetQueuedCommandCount();
	for (int32 I = 0; I < 100; ++I)
	{
		auto S = R.PC->GetDebugSnapshot();
		TestTrue(TEXT("HUD valid fixed snapshot"), S.bEgiValid && S.bEnvironmentValid && S.Egi.Tick == Before.Tick && S.Ocean.SampleTick == Before.Tick && S.Boat.Tick == Before.Tick);
		TestEqual(TEXT("HUD depth matches"), S.Egi.DepthM, Before.DepthM);
		TestEqual(TEXT("Water depth"), S.Ocean.BottomDepthM, 30.0f);
		TestEqual(TEXT("Egi mass"), S.Equipment.BaseMassG, 35.0f); TestEqual(TEXT("No sinker valid"), S.Equipment.SinkerMassG, 0.0f);
		const FString Text = UTRFishingHUDWidget::FormatSnapshot(S).ToString();
		TestTrue(TEXT("HUD shows units and vertical sign"), Text.Contains(TEXT("world Z, up +")) && Text.Contains(TEXT("Boat drift")) && Text.Contains(TEXT("Tension proxy")));
		S.Egi.DepthM = 999.0f; S.Equipment.BaseMassG = 999.0f;
	}
	TestEqual(TEXT("Reads never advance clock"), R.F.Sim()->GetSimulationTime().TickIndex, Time.TickIndex);
	TestEqual(TEXT("Reads never alter queue"), R.F.Sim()->GetQueuedCommandCount(), Queue);
	TestEqual(TEXT("Returned copy cannot change depth"), R.F.Session->Fishing->GetSnapshot().DepthM, Before.DepthM);
	TestEqual(TEXT("Returned copy cannot change equipment"), R.F.Session->GetEquipmentSnapshot().BaseMassG, 35.0f);
	R.F.Session->EndFishing(); TestFalse(TEXT("Ended cast has no live egi HUD"), R.PC->GetDebugSnapshot().bEgiValid);
	R.F.World->DestroyActor(R.F.Session.Get()); TestFalse(TEXT("Destroyed session clears HUD validity"), R.PC->GetDebugSnapshot().bSessionValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRInputConfigTest, "TipRun.M09.InputConfigAndStartup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRInputConfigTest::RunTest(const FString& Parameters)
{
	FTRTestSession F;
	TStrongObjectPtr<UTRInputConfigDataAsset> Input(UTRInputConfigDataAsset::CreatePrototype(GetTransientPackage()));
	TArray<FText> Errors; TestTrue(TEXT("Prototype config valid"), Input->Validate(Errors));
	TestEqual(TEXT("Keyboard and gamepad mappings"), Input->FishingContext->GetMappings().Num(), 18);
	Input->Bindings[1].Action = Input->Bindings[0].Action;
	TestFalse(TEXT("Duplicate action rejected"), Input->Validate(Errors));
	F.Clock->Ocean = F.Area.Get(); F.Clock->Boat = F.BoatTuning.Get(); F.Clock->Fishing = F.Tuning.Get();
	F.Clock->Egis = F.Egis.Get(); F.Clock->Sinkers = F.Sinkers.Get(); F.Clock->InitialSinkerId = TREquipment::NoSinkerId();
	F.Clock->Input = UTRInputConfigDataAsset::CreatePrototype(F.Clock.Get());
	F.Clock->InitialBoatXYM = FVector2D(10.0, 20.0);
	auto* Mode = F.World->SpawnActor<ATRGameModeBase>(); Mode->SessionConfig = F.Clock.Get();
	Errors.Reset(); TestTrue(TEXT("M09 startup wires existing systems"), Mode->InitializeSession(Errors));
	for (const auto& E : Errors) { AddError(E.ToString()); }
	TestNotNull(TEXT("Startup produces Session"), Mode->GetSession());
	{
		FTRTestSession Bad;
		Bad.Clock->Ocean = Bad.Area.Get(); Bad.Clock->Boat = Bad.BoatTuning.Get(); Bad.Clock->Fishing = Bad.Tuning.Get();
		Bad.Clock->Egis = Bad.Egis.Get(); Bad.Clock->Sinkers = Bad.Sinkers.Get(); Bad.Clock->InitialSinkerId = TREquipment::NoSinkerId();
		Bad.Clock->Input = UTRInputConfigDataAsset::CreatePrototype(Bad.Clock.Get());
		Bad.Clock->InitialBoatXYM = FVector2D(10000.0, 10000.0); // Valid number, outside configured sea.
		auto* BadMode = Bad.World->SpawnActor<ATRGameModeBase>(); BadMode->SessionConfig = Bad.Clock.Get();
		Errors.Reset(); TestFalse(TEXT("Invalid startup environment rejected"), BadMode->InitializeSession(Errors));
		TestNull(TEXT("Failed startup keeps no session"), BadMode->GetSession());
		TestTrue(TEXT("Failed startup stops clock"), Bad.Sim()->IsSimulationPaused());
		TestTrue(TEXT("Failed startup has diagnostic"), !Errors.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRInputPrototypeAssetsTest, "TipRun.M09.PrototypeAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRInputPrototypeAssetsTest::RunTest(const FString& Parameters)
{
	const FString ConfigPath = TEXT("/Game/TipRun/Prototype/Data/DA_TR_M09Session_Prototype");
	const FString ModePath = TEXT("/Game/TipRun/Prototype/BP_TR_M09GameMode_Prototype");
	if (FParse::Param(FCommandLine::Get(), TEXT("TRCreateM09Prototype")))
	{
		// UE serializers only, explicit creation switch, no overwriting existing user assets.
		for (const FString& Path : {ConfigPath, ModePath})
		{
			if (FPaths::FileExists(FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension())))
			{
				AddError(TEXT("Prototype creation refuses to overwrite existing assets")); return false;
			}
		}
		auto Save = [](UObject* Asset)
		{
			const FString Filename = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
			return UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args);
		};
		FTRTestSession F;
		UPackage* Package = CreatePackage(*ConfigPath);
		auto* Config = DuplicateObject<UTRSessionConfigDataAsset>(F.Clock.Get(), Package, *FPackageName::GetLongPackageAssetName(ConfigPath));
		Config->SetFlags(RF_Public | RF_Standalone);
		// Nested assets are owned and serialized by this explicitly named Prototype configuration.
		Config->Ocean = DuplicateObject<UTROceanAreaDataAsset>(F.Area.Get(), Config, TEXT("Ocean_Prototype"));
		Config->Boat = DuplicateObject<UTRBoatTuningDataAsset>(F.BoatTuning.Get(), Config, TEXT("Boat_Prototype"));
		Config->Input = UTRInputConfigDataAsset::CreatePrototype(Config);
		Config->Fishing = LoadObject<UTRFishingTuningDataAsset>(nullptr, TEXT("/Game/TipRun/Prototype/Data/DA_TR_FishingTuning_Prototype"));
		Config->Egis = F.Egis.Get(); Config->Sinkers = F.Sinkers.Get(); Config->InitialSinkerId = TREquipment::NoSinkerId();
		Config->InitialBoatXYM = FVector2D(10.0, 20.0);
		TArray<FText> Errors;
		if (!TestTrue(TEXT("Prototype startup data valid"), Config->ValidateStartup(Errors)) || !TestTrue(TEXT("Save prototype config"), Save(Config))) { return false; }
		UPackage* ModePackage = CreatePackage(*ModePath);
		auto* BP = FKismetEditorUtilities::CreateBlueprint(ATRGameModeBase::StaticClass(), ModePackage,
			*FPackageName::GetLongPackageAssetName(ModePath), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		FKismetEditorUtilities::CompileBlueprint(BP);
		CastChecked<ATRGameModeBase>(BP->GeneratedClass->GetDefaultObject())->SessionConfig = Config;
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);
		TestTrue(TEXT("Compiled default retains config"), CastChecked<ATRGameModeBase>(BP->GeneratedClass->GetDefaultObject())->SessionConfig == Config);
		if (!TestTrue(TEXT("Save prototype game mode"), Save(BP))) { return false; }
	}
	auto* Config = LoadObject<UTRSessionConfigDataAsset>(nullptr, *ConfigPath);
	auto* BP = LoadObject<UBlueprint>(nullptr, *ModePath);
	if (!TestNotNull(TEXT("Saved prototype config"), Config) || !TestNotNull(TEXT("Saved prototype game mode"), BP) ||
		!TestNotNull(TEXT("Compiled game mode class"), BP->GeneratedClass.Get())) { return false; }
	TArray<FText> Errors; TestTrue(TEXT("Reloaded nested data and input references valid"), Config->ValidateStartup(Errors));
	FTRTestSession F;
	auto* Mode = F.World->SpawnActor<ATRGameModeBase>(BP->GeneratedClass);
	TestNotNull(TEXT("Saved game mode spawned"), Mode);
	TestTrue(TEXT("Reloaded default retains config"), CastChecked<ATRGameModeBase>(BP->GeneratedClass->GetDefaultObject())->SessionConfig == Config);
	TestTrue(TEXT("Blueprint references saved config"), Mode && Mode->SessionConfig == Config);
	if (!Mode || !TestTrue(TEXT("Saved setup starts session"), Mode->InitializeSession(Errors))) { return false; }
	auto* PC = F.World->SpawnActor<ATRPlayerController>(); PC->InputConfig = Config->Input;
	TestTrue(TEXT("Saved setup accepts input connection"), PC->BindSession(Mode->GetSession()));
	TestTrue(TEXT("Saved setup queues deploy"), PC->ActionStarted(ETRPlayerAction::Deploy)); F.Step();
	TestTrue(TEXT("Saved setup publishes live HUD"), PC->GetDebugSnapshot().bEgiValid);
	return true;
}
#endif
