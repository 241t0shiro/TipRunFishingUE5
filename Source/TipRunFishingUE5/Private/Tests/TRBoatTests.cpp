#include "Boat/TRBoatPawn.h"
#include "Boat/TRBoatDriftComponent.h"
#include "Data/TRBoatTuningDataAsset.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Data/TROceanAreaDataAsset.h"
#include "Data/TRSessionConfigDataAsset.h"
#include "Game/TRSimulationWorldSubsystem.h"
#include "Ocean/TROceanWorldSubsystem.h"
#include "Ocean/TRSeabedProviderActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace
{
	struct FTRTestBoat
	{
		UWorld* World = nullptr;
		TStrongObjectPtr<UTROceanAreaDataAsset> Area{NewObject<UTROceanAreaDataAsset>()};
		TStrongObjectPtr<UTRSessionConfigDataAsset> Clock{NewObject<UTRSessionConfigDataAsset>()};
		TStrongObjectPtr<UTRBoatTuningDataAsset> Tuning{NewObject<UTRBoatTuningDataAsset>()};
		TWeakObjectPtr<ATRBoatPawn> Boat;
		TWeakObjectPtr<ATRSeabedProviderActor> Provider;
		FTRActorSimId Id;
		FTRTestBoat()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("TR_Boat_TestWorld")),
				nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			// All tuning values are temporary game approximations for tests.
			Area->Settings.AreaId = TEXT("TestM05Ocean");
			Area->Settings.BoundsMinXYM = FVector2D(-1000.0, -1000.0);
			Area->Settings.BoundsMaxXYM = FVector2D(1000.0, 1000.0);
			Area->Settings.FlatDepthM = 30.0f;
			Area->Settings.SurfaceZ_M = 2.0f;
			Clock->StepSeconds = 1.0 / 60.0; Clock->MaxCatchUpSteps = 8;
			Tuning->Parameters.CurrentResponse01 = 0.5;
			Tuning->Parameters.VelocityResponsePerS = 2.0;
			Tuning->Parameters.MaxDriftSpeedMps = 3.0;
			Tuning->Parameters.HullHeightOffsetM = 0.5;
			Tuning->Parameters.RodAnchorOffsetM = FVector(2.0, 1.0, 1.0);
			if (World)
			{
				Provider = World->SpawnActor<ATRSeabedProviderActor>();
				Boat = World->SpawnActor<ATRBoatPawn>();
				Boat->Tuning = Tuning.Get();
			}
		}
		~FTRTestBoat() { Shutdown(); }
		void Shutdown()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
				World = nullptr;
			}
		}
		FTRTestBoat(const FTRTestBoat&) = delete;
		FTRTestBoat& operator=(const FTRTestBoat&) = delete;
		UTRSimulationWorldSubsystem* Sim() const { return World->GetSubsystem<UTRSimulationWorldSubsystem>(); }
		UTROceanWorldSubsystem* Ocean() const { return World->GetSubsystem<UTROceanWorldSubsystem>(); }
		bool Start(FAutomationTestBase& Test, FVector2D Position = FVector2D::ZeroVector, float Heading = 0.0f)
		{
			if (!Test.TestNotNull(TEXT("Boat created"), Boat.Get()) || !Test.TestNotNull(TEXT("Coordinator created"), Sim())) { return false; }
			TArray<FText> Errors;
			const bool bReady = Ocean()->InitializeArea(Area.Get(), Provider.Get(), Errors) && Sim()->Configure(Clock.Get(), Errors);
			if (bReady) { Id = Sim()->RegisterBoat(Boat.Get(), Position, Heading, Errors); }
			for (const FText& Error : Errors) { Test.AddError(Error.ToString()); }
			return Test.TestTrue(TEXT("Boat initialized and registered"), Id.IsValid());
		}
		void Frames(int32 FPS, int32 Seconds = 1) const { for (int32 I = 0; I < FPS * Seconds; ++I) { Sim()->Tick(1.0f / float(FPS)); } }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRBoatStillTest, "TipRun.M05.B01B03StillWater",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRBoatStillTest::RunTest(const FString& Parameters)
{
	FTRTestBoat F;
	if (!F.Start(*this, FVector2D(10.0, 20.0))) { return false; }
	const FTRBoatSnapshot Initial = F.Boat->GetBoatSnapshot();
	F.Frames(60, 2);
	const FTRBoatSnapshot Last = F.Boat->GetBoatSnapshot();
	TestTrue(TEXT("Zero current and velocity preserve position"), Initial.PositionM.Equals(Last.PositionM, 0.0));
	TestTrue(TEXT("No velocity created"), Last.VelocityMps.IsZero());
	TestFalse(TEXT("Pawn has no simulation tick"), F.Boat->PrimaryActorTick.bCanEverTick);
	TestFalse(TEXT("Component has no simulation tick"), F.Boat->DriftComponent->PrimaryComponentTick.bCanEverTick);
	TestFalse(TEXT("Boat mesh does not simulate physics"), F.Boat->BoatMesh->IsSimulatingPhysics());
	FTROceanQuery Query; Query.SimTick = 120;
	FTROceanSample Sample = F.Ocean()->SampleOcean(Query);
	Sample.WindMps = FVector2D(1000.0, -1000.0); // Display-only payload never contributes.
	FTRSimTime Time; Time.TickIndex = 120; Time.StepSeconds = 1.0 / 60.0;
	F.Boat->StepBoat(Time, Sample, [](const FVector2D&) { return true; });
	TestTrue(TEXT("Wind payload has no drift contribution"), F.Boat->GetBoatSnapshot().PositionM.Equals(Initial.PositionM, 0.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRBoatCurrentTest, "TipRun.M05.B02CurrentAndSpeedLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRBoatCurrentTest::RunTest(const FString& Parameters)
{
	FTRTestBoat F;
	F.Area->Settings.CurrentMps = FVector(2.0, -1.0, 0.0);
	if (!F.Start(*this)) { return false; }
	F.Frames(60, 5);
	const FTRBoatSnapshot Result = F.Boat->GetBoatSnapshot();
	const FVector Expected = FVector(1.0, -0.5, 0.0) * (1.0 - FMath::Exp(-10.0));
	TestTrue(TEXT("Velocity matches analytical relaxation after five seconds"), Result.VelocityMps.Equals(Expected, 1.e-10));
	TestTrue(TEXT("Moves with positive X negative Y current"), Result.PositionM.X > 0.0 && Result.PositionM.Y < 0.0);
	TestEqual(TEXT("Fixed sea plus hull offset"), Result.PositionM.Z, 2.5);
	TestEqual(TEXT("No rotation from current"), Result.HeadingRad, 0.0f);
	F.Sim()->Unregister(F.Id);
	F.Tuning->Parameters.MaxDriftSpeedMps = 0.1;
	TArray<FText> Errors;
	F.Id = F.Sim()->RegisterBoat(F.Boat.Get(), FVector2D::ZeroVector, 0.0f, Errors);
	TestTrue(TEXT("Explicit reinitialization"), F.Id.IsValid());
	for (int32 I = 0; I < 300; ++I)
	{
		F.Sim()->AdvanceFrame(1.0 / 60.0);
		TestTrue(TEXT("Reported and integrated velocity capped"), F.Boat->GetBoatSnapshot().VelocityMps.Size() <= 0.1 + 1.e-12);
	}
	TestTrue(TEXT("Displacement obeys cap"), F.Boat->GetBoatSnapshot().PositionM.Size2D() <= 0.5 + 1.e-12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRBoatAnchorTest, "TipRun.M05.B05RodAnchorAndSnapshotOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRBoatAnchorTest::RunTest(const FString& Parameters)
{
	FTRTestBoat F;
	F.Area->Settings.CurrentMps = FVector(1.0, 0.0, 0.0);
	if (!F.Start(*this, FVector2D(10.0, 20.0), float(UE_DOUBLE_PI / 2.0))) { return false; }
	TestTrue(TEXT("Rotated rod offset uses meters"), F.Boat->GetRodAnchorWorldM().Equals(FVector(9.0, 22.0, 3.5), 1.e-6));
	// Display/component edits must not become authoritative coordinates.
	F.Boat->SetActorLocation(FVector(99999.0));
	F.Boat->RodAnchor->SetRelativeLocation(FVector(99999.0));
	F.Tuning->Parameters.RodAnchorOffsetM = FVector(500.0);
	F.Tuning->Parameters.CurrentResponse01 = 0.0;
	int32 Observations = 0;
	F.Sim()->RegisterSession(F.World->SpawnActor<AActor>(), FTRSimulationStep::CreateLambda([&](ETRSimulationPhase Phase, const FTRSimTime& Time)
	{
		if (Phase == ETRSimulationPhase::Fishing)
		{
			FTRBoatSnapshot Snapshot;
			TestTrue(TEXT("Game exposes valid boat snapshot"), F.Sim()->GetBoatSnapshot(F.Id, Snapshot));
			TestEqual(TEXT("Boat snapshot updated before Fishing"), Snapshot.Tick, Time.TickIndex);
			TestEqual(TEXT("Ocean time already advanced"), F.Ocean()->GetEnvironmentTimeS(), double(Time.TickIndex) * Time.StepSeconds);
			TestTrue(TEXT("Frozen offset is preserved"), (Snapshot.RodTipM - Snapshot.PositionM).Equals(FVector(-1.0, 2.0, 1.0), 1.e-6));
			++Observations;
		}
	}), FTRSimulationCommand::CreateLambda([](const FTRFishingCommand&) {}));
	F.Frames(60);
	TestEqual(TEXT("One update per simulation tick"), Observations, 60);
	const FTRBoatSnapshot Snapshot = F.Boat->GetBoatSnapshot();
	TestTrue(TEXT("Frozen current response still moves boat"), Snapshot.PositionM.X > 10.0 && Snapshot.PositionM.X < 11.0);
	TestTrue(TEXT("Actor cm agrees with authority"), F.Boat->GetActorLocation().Equals(TRUnits::MetersToCentimeters(Snapshot.PositionM), 1.e-6));
	TestTrue(TEXT("Scene anchor cm agrees with snapshot"), F.Boat->RodAnchor->GetComponentLocation().Equals(TRUnits::MetersToCentimeters(Snapshot.RodTipM), 1.e-4));
	TestFalse(TEXT("Boat does not accept fishing commands"), F.Sim()->EnqueueCommand(F.Id, ETRFishingCommandType::Jerk));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRBoatFrameTest, "TipRun.M05.B04FrameRatesAndPause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRBoatFrameTest::RunTest(const FString& Parameters)
{
	FTRBoatSnapshot Reference;
	for (int32 FPS : {30, 60, 120})
	{
		FTRTestBoat F;
		F.Area->Settings.CurrentMps = FVector(0.8, 0.3, 0.0);
		if (!F.Start(*this)) { return false; }
		F.Frames(FPS, 2);
		const FTRBoatSnapshot Before = F.Boat->GetBoatSnapshot();
		F.Sim()->SetSimulationPaused(true);
		F.Frames(FPS, 2);
		TestTrue(TEXT("Paused position unchanged"), F.Boat->GetBoatSnapshot().PositionM.Equals(Before.PositionM, 0.0));
		TestEqual(TEXT("Paused snapshot tick unchanged"), F.Boat->GetBoatSnapshot().Tick, Before.Tick);
		F.Sim()->SetSimulationPaused(false);
		F.World->GetWorldSettings()->SetPauserPlayerState(F.World->SpawnActor<APlayerState>());
		F.Frames(FPS);
		TestEqual(TEXT("Engine pause also freezes boat"), F.Boat->GetBoatSnapshot().Tick, Before.Tick);
		F.World->GetWorldSettings()->SetPauserPlayerState(nullptr);
		F.Frames(FPS, 2);
		const FTRBoatSnapshot Result = F.Boat->GetBoatSnapshot();
		TestEqual(TEXT("Exactly 240 active updates"), Result.Tick, int64(239));
		if (FPS == 30) { Reference = Result; }
		else
		{
			TestTrue(TEXT("30/60/120 position exactly matches"), Result.PositionM.Equals(Reference.PositionM, 0.0));
			TestTrue(TEXT("30/60/120 velocity exactly matches"), Result.VelocityMps.Equals(Reference.VelocityMps, 0.0));
			TestTrue(TEXT("30/60/120 rod position matches"), Result.RodTipM.Equals(Reference.RodTipM, 0.0));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRBoatInvalidTest, "TipRun.M05.B08InvalidConfigurationAndSamples",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRBoatInvalidTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UTRBoatTuningDataAsset> Bad(NewObject<UTRBoatTuningDataAsset>());
	TArray<FText> Errors;
	TestFalse(TEXT("Unconfigured tuning rejected"), Bad->Validate(Errors));
	FTRTestBoat F;
	TestTrue(TEXT("Initial mode uninitialized"), F.Boat->GetBoatMode() == ETRBoatMode::Uninitialized);
	TestFalse(TEXT("Unconfigured coordinator rejects boat"), F.Sim()->RegisterBoat(F.Boat.Get(), {}, 0.0f, Errors).IsValid());
	Bad->Parameters = F.Tuning->Parameters;
	Bad->Parameters.MaxDriftSpeedMps = -1.0;
	FDataValidationContext Context;
	TestTrue(TEXT("Editor validation rejects negative speed"), Bad->IsDataValid(Context) == EDataValidationResult::Invalid);
	Bad->Parameters = F.Tuning->Parameters;
	Bad->Parameters.CurrentResponse01 = 1.1;
	TestFalse(TEXT("Response outside [0,1] rejected"), Bad->Validate(Errors));
	Bad->Parameters = F.Tuning->Parameters;
	Bad->Parameters.RodAnchorOffsetM.X = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("NaN rod offset rejected"), Bad->Validate(Errors));
	if (!F.Start(*this)) { return false; }
	F.Sim()->Unregister(F.Id);
	F.Boat->Tuning = nullptr;
	TestFalse(TEXT("Missing tuning rejected"), F.Sim()->RegisterBoat(F.Boat.Get(), {}, 0.0f, Errors).IsValid());
	F.Boat->Tuning = F.Tuning.Get();
	TestFalse(TEXT("NaN heading rejected"), F.Sim()->RegisterBoat(F.Boat.Get(), {}, std::numeric_limits<float>::quiet_NaN(), Errors).IsValid());
	TestFalse(TEXT("Outside initial position rejected"), F.Sim()->RegisterBoat(F.Boat.Get(), FVector2D(2000.0, 0.0), 0.0f, Errors).IsValid());
	F.Id = F.Sim()->RegisterBoat(F.Boat.Get(), {}, 0.0f, Errors);
	TestTrue(TEXT("Valid reinitialization recovers"), F.Id.IsValid());
	const FTRBoatSnapshot Before = F.Boat->GetBoatSnapshot();
	int32 InvalidEvents = 0;
	F.Boat->OnEnvironmentInvalid.AddLambda([&](ETRSampleError) { ++InvalidEvents; });
	FTROceanSample Sample = F.Ocean()->SampleOcean({});
	Sample.CurrentMps.X = std::numeric_limits<double>::infinity();
	FTRSimTime Time; Time.StepSeconds = 1.0 / 60.0;
	F.Boat->StepBoat(Time, Sample, [](const FVector2D&) { return true; });
	F.Boat->StepBoat(Time, Sample, [](const FVector2D&) { return true; });
	TestTrue(TEXT("Invalid sample disables boat"), F.Boat->GetBoatMode() == ETRBoatMode::Disabled);
	TestTrue(TEXT("Invalid sample preserves last valid position"), F.Boat->GetBoatSnapshot().PositionM.Equals(Before.PositionM, 0.0));
	TestEqual(TEXT("Failure notification occurs once"), InvalidEvents, 1);
	FTRBoatSnapshot Output; Output.PositionM = FVector(123.0);
	TestFalse(TEXT("Disabled boat not supplied as valid snapshot"), F.Sim()->GetBoatSnapshot(F.Id, Output));
	TestTrue(TEXT("Rejected read preserves output"), Output.PositionM.Equals(FVector(123.0), 0.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRBoatLifetimeTest, "TipRun.M05.EnvironmentAndRegistrationLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRBoatLifetimeTest::RunTest(const FString& Parameters)
{
	FTRTestBoat F;
	F.Area->Settings.BoundsMaxXYM.X = 0.00001;
	F.Area->Settings.CurrentMps = FVector(1.0, 0.0, 0.0);
	if (!F.Start(*this)) { return false; }
	const FVector Initial = F.Boat->GetBoatSnapshot().PositionM;
	int32 Failures = 0;
	F.Boat->OnEnvironmentInvalid.AddLambda([&](ETRSampleError) { ++Failures; });
	F.Frames(60);
	TestTrue(TEXT("Invalid destination never committed"), F.Boat->GetBoatSnapshot().PositionM.Equals(Initial, 0.0));
	TestEqual(TEXT("Boundary defense notifies once"), Failures, 1);
	F.Sim()->Unregister(F.Id);
	TArray<FText> Errors;
	F.Id = F.Sim()->RegisterBoat(F.Boat.Get(), FVector2D(-10.0, 0.0), 0.0f, Errors);
	TestTrue(TEXT("Recover only by explicit valid initialization"), F.Id.IsValid());
	F.World->DestroyActor(F.Provider.Get());
	F.Frames(60);
	TestTrue(TEXT("Lost provider disables motion"), F.Boat->GetBoatMode() == ETRBoatMode::Disabled);
	TestEqual(TEXT("Lost provider notifies once per initialization"), Failures, 2);
	F.Boat->DispatchBeginPlay();
	TestTrue(TEXT("Boat destruction succeeds"), F.World->DestroyActor(F.Boat.Get()));
	TestFalse(TEXT("EndPlay unregisters boat"), F.Sim()->IsRegistered(F.Id));
	F.Sim()->Unregister(F.Id); // Idempotent, even after EndPlay.
	F.Frames(60);
	TestEqual(TEXT("No callback after destruction"), Failures, 2);
	return true;
}
#endif
