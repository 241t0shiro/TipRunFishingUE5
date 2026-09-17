#include "TRSessionTestFixture.h"
#include "Fishing/TREgiSimulationComponent.h"
#include "Fishing/TREgiActor.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Components/StaticMeshComponent.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTREgiWeightFallTest, "TipRun.M07.F02WeightAndZeroSinker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTREgiWeightFallTest::RunTest(const FString& Parameters)
{
	float PreviousDepth = 0.0f;
	const TCHAR* Ids[] = { TEXT("Egi_3"), TEXT("Egi_3_5"), TEXT("Egi_4") };
	for (int32 I = 0; I < 3; ++I)
	{
		FTRTestSession F;
		F.Area->Settings.CurrentMps = FVector::ZeroVector; // F02/M07 isolates vertical motion.
		if (!F.Initialize(*this)) { return false; }
		TArray<FText> Errors;
		TestTrue(TEXT("Select actual M02 equipment"), F.Session->TrySetEquipment(FName(Ids[I]), TREquipment::NoSinkerId(), Errors) == ETRCommandResult::Accepted);
		if (!TestTrue(TEXT("Start with selected equipment"), F.Session->StartFishing(Errors) == ETRCommandResult::Accepted)) { return false; }
		if (!F.EnterFishingMode(*this)) { return false; }
		const FTREquipmentSnapshot Equipment = F.Session->GetEquipmentSnapshot();
		TestEqual(TEXT("30/35/40 g"), Equipment.TotalMassG, 30.0f + 5.0f * I);
		TestEqual(TEXT("Zero grams is valid"), Equipment.SinkerMassG, 0.0f);
		F.Deploy();
		const FVector2D InitialXY = F.Session->Fishing->GetSnapshot().PositionXYM;
		F.Tuning->Parameters.FreeFallSinkScale = 100.0f; // Must not alter frozen cast data.
		F.Step(60);
		const FTREgiSnapshot Snapshot = F.Session->Fishing->GetSnapshot();
		// M02 Prototype uses a linear 0.6 -> 1.8 m/s curve over 30 -> 90 g.
		const float ExpectedDepth = 0.6f + 0.1f * I;
		TestTrue(TEXT("F02 depth = test speed * one second"), FMath::IsNearlyEqual(Snapshot.DepthM, ExpectedDepth, 1.e-5f));
		TestTrue(TEXT("Larger mass falls further with this test profile"), Snapshot.DepthM > PreviousDepth);
		TestTrue(TEXT("World Z velocity is negative downward"), FMath::IsNearlyEqual(Snapshot.VelocityMps.Z, -double(ExpectedDepth), 1.e-5));
		TestTrue(TEXT("Static ocean and boat preserve horizontal position"), Snapshot.PositionXYM.Equals(InitialXY, 0.0));
		TestTrue(TEXT("No horizontal velocity"), Snapshot.VelocityMps.X == 0.0 && Snapshot.VelocityMps.Y == 0.0);
		PreviousDepth = Snapshot.DepthM;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTREgiBottomTest, "TipRun.M07.F03DepthsAndSingleBottomTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTREgiBottomTest::RunTest(const FString& Parameters)
{
	for (float Depth : {0.1f, 3.0f, 30.0f})
	{
		FTRTestSession F;
		F.Area->Settings.CurrentMps = FVector::ZeroVector; // F02/M07 isolates vertical motion.
		F.Area->Settings.FlatDepthM = Depth;
		if (!F.Start(*this)) { return false; }
		int32 BottomTransitions = 0;
		F.Session->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState From, ETRFishingState To)
		{
			if (To == ETRFishingState::BottomContact)
			{
				++BottomTransitions;
				TestTrue(TEXT("Bottom is reached from FreeFall"), From == ETRFishingState::FreeFall);
				TestEqual(TEXT("State notification sees committed depth"), F.Session->Fishing->GetSnapshot().DepthM, Depth);
			}
		});
		F.Deploy();
		for (int32 I = 0; I < 4000 && F.Session->Fishing->GetState() != ETRFishingState::BottomContact; ++I)
		{
			F.Step();
			TestTrue(TEXT("Never penetrates seabed"), F.Session->Fishing->GetSnapshot().DepthM <= Depth);
		}
		TestTrue(TEXT("Bottom reached at each water depth"), F.Session->Fishing->GetState() == ETRFishingState::BottomContact);
		F.Step(30);
		TestEqual(TEXT("Depth stays exactly at bottom"), F.Session->Fishing->GetSnapshot().DepthM, Depth);
		TestTrue(TEXT("Resting bottom velocity is zero"), F.Session->Fishing->GetSnapshot().VelocityMps.IsZero());
		TestEqual(TEXT("Bottom transition delivered once per cast"), BottomTransitions, 1);
		TestTrue(TEXT("Bottom does not end or unlock cast"), F.Session->IsEquipmentLocked() && !F.Session->HasResult());
		ATREgiActor* Visual = F.Session->GetEgiActor();
		if (!TestNotNull(TEXT("Owned visual actor"), Visual)) { return false; }
		TestEqual(TEXT("Visual Z converts meters to centimeters"), Visual->GetActorLocation().Z, (2.0 - double(Depth)) * 100.0);
		TestFalse(TEXT("No Chaos authority"), Visual->EgiMesh->IsSimulatingPhysics());
		TestFalse(TEXT("Visual does not advance simulation"), Visual->PrimaryActorTick.bCanEverTick);
		Visual->SetActorLocation(FVector(99999.0)); F.Step();
		TestEqual(TEXT("Display transform cannot change authoritative depth"), F.Session->Fishing->GetSnapshot().DepthM, Depth);
		TestEqual(TEXT("Display restored from snapshot"), Visual->GetActorLocation().Z, (2.0 - double(Depth)) * 100.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTREgiLargeStepTest, "TipRun.M07.LargeFixedStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTREgiLargeStepTest::RunTest(const FString& Parameters)
{
	FTRTestSession F;
	F.Area->Settings.CurrentMps = FVector::ZeroVector; // F02/M07 isolates vertical motion.
	F.Clock->StepSeconds = 0.25; // 15 times the normal technical-proposal step; hook window remains representable.
	F.Area->Settings.FlatDepthM = 0.01f;
	if (!F.Start(*this)) { return false; }
	TestTrue(TEXT("Deploy queued"), F.Session->SubmitCommand(ETRFishingCommandType::Deploy, {}));
	F.Sim()->AdvanceFrame(0.25);
	TestEqual(TEXT("Deployment stays at surface"), F.Session->Fishing->GetSnapshot().DepthM, 0.0f);
	F.Sim()->AdvanceFrame(0.25);
	TestEqual(TEXT("Overshoot clamps to 1 cm bottom"), F.Session->Fishing->GetSnapshot().DepthM, 0.01f);
	TestTrue(TEXT("One large step reaches bottom"), F.Session->Fishing->GetState() == ETRFishingState::BottomContact);
	TestTrue(TEXT("Corrected velocity uses actual displacement"), FMath::IsNearlyEqual(F.Session->Fishing->GetSnapshot().VelocityMps.Z, -0.04, 1.e-7));
	F.Sim()->AdvanceFrame(0.25);
	TestTrue(TEXT("Next bottom step has zero velocity"), F.Session->Fishing->GetSnapshot().VelocityMps.IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTREgiFramePauseTest, "TipRun.M07.FrameRatesAndPause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTREgiFramePauseTest::RunTest(const FString& Parameters)
{
	FTREgiSnapshot Reference;
	int64 ReferenceBottomTick = -1;
	for (int32 FPS : {30, 60, 120})
	{
		FTRTestSession F;
		F.Area->Settings.CurrentMps = FVector::ZeroVector; // F02/M07 isolates vertical motion.
		F.Area->Settings.FlatDepthM = 2.0f;
		if (!F.Start(*this)) { return false; }
		int64 BottomTick = -1;
		F.Session->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState, ETRFishingState To)
		{
			if (To == ETRFishingState::BottomContact) { BottomTick = F.Session->Fishing->GetSnapshot().Tick; }
		});
		F.Deploy();
		for (int32 I = 0; I < FPS; ++I) { F.Sim()->Tick(1.0f / FPS); }
		const FTREgiSnapshot Before = F.Session->Fishing->GetSnapshot();
		const FVector VisualBefore = F.Session->GetEgiActor()->GetActorLocation();
		F.Sim()->SetSimulationPaused(true);
		for (int32 I = 0; I < FPS * 3; ++I) { F.Sim()->Tick(1.0f / FPS); }
		TestEqual(TEXT("Pause preserves depth"), F.Session->Fishing->GetSnapshot().DepthM, Before.DepthM);
		TestEqual(TEXT("Pause preserves tick"), F.Session->Fishing->GetSnapshot().Tick, Before.Tick);
		TestTrue(TEXT("Pause preserves visual"), F.Session->GetEgiActor()->GetActorLocation().Equals(VisualBefore, 0.0));
		F.Sim()->SetSimulationPaused(false);
		for (int32 I = 0; I < FPS * 3; ++I) { F.Sim()->Tick(1.0f / FPS); }
		const FTREgiSnapshot Result = F.Session->Fishing->GetSnapshot();
		TestTrue(TEXT("Eventually reaches bottom"), Result.FishingState == ETRFishingState::BottomContact);
		if (FPS == 30) { Reference = Result; ReferenceBottomTick = BottomTick; }
		else
		{
			TestEqual(TEXT("Same bottom event tick at 30/60/120 fps"), BottomTick, ReferenceBottomTick);
			TestEqual(TEXT("Same final simulation tick"), Result.Tick, Reference.Tick);
			TestEqual(TEXT("Identical depth"), Result.DepthM, Reference.DepthM);
			TestTrue(TEXT("Identical velocity and XY"), Result.VelocityMps.Equals(Reference.VelocityMps, 0.0) && Result.PositionXYM.Equals(Reference.PositionXYM, 0.0));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTREgiLifetimeTest, "TipRun.M07.CastAndSessionLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTREgiLifetimeTest::RunTest(const FString& Parameters)
{
	FTRTestSession F;
	F.Area->Settings.CurrentMps = FVector::ZeroVector; // F02/M07 isolates vertical motion.
	if (!F.Start(*this)) { return false; }
	F.Deploy(); F.Step(5);
	const FTREgiSnapshot Old = F.Session->Fishing->GetSnapshot();
	TWeakObjectPtr<ATREgiActor> OldVisual = F.Session->GetEgiActor();
	if (!TestTrue(TEXT("Retrieve returns onboard"), F.ReturnToReady())) { return false; }
	TestTrue(TEXT("Retrieve destroys old visual"), !OldVisual.IsValid() || OldVisual->IsActorBeingDestroyed());
	F.Deploy();
	const FTREgiSnapshot Current = F.Session->Fishing->GetSnapshot();
	FTRSimTime Time = F.Sim()->GetSimulationTime();
	FTROceanQuery Query; Query.PositionXYM = Current.PositionXYM; Query.SimTick = Time.TickIndex;
	FTROceanSample Ocean = F.World->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(Query);
	FTRBoatSnapshot Boat = F.Boat->GetBoatSnapshot(); Boat.Tick = Time.TickIndex;
	const FTREgiAction Action = F.Session->Fishing->GetAction();
	TestTrue(TEXT("Old cast cannot step new simulation"), F.Session->EgiSimulation->StepEgi(Old.CastId, Time, Ocean, Boat, Action, [&Ocean](const FTROceanQuery&) { return Ocean; }) == ETREgiStepEvent::None);
	TestEqual(TEXT("Rejected old cast does not mutate tick"), F.Session->EgiSimulation->BuildSnapshot(ETRFishingState::FreeFall).Tick, Current.Tick);
	TestFalse(TEXT("Old cast cannot reposition new visual"), F.Session->GetEgiActor()->ApplySimulationSnapshot(Old, Ocean.SurfaceZ_M));
	TStrongObjectPtr<UTREgiSimulationComponent> Retained(F.Session->EgiSimulation.Get());
	TWeakObjectPtr<ATREgiActor> Visual = F.Session->GetEgiActor();
	F.World->DestroyActor(F.Session.Get());
	TestTrue(TEXT("Session destruction destroys visual"), !Visual.IsValid() || Visual->IsActorBeingDestroyed());
	F.Step(20);
	TestTrue(TEXT("Destroyed session cannot advance retained component"), Retained->StepEgi(Current.CastId, Time, Ocean, Boat, Action, [&Ocean](const FTROceanQuery&) { return Ocean; }) == ETREgiStepEvent::None);
	TestFalse(TEXT("Destroyed session clears active cast"), Retained->BuildSnapshot(ETRFishingState::Inactive).CastId.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTREgiInvalidTest, "TipRun.M07.F12InvalidDataAndEnvironment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTREgiInvalidTest::RunTest(const FString& Parameters)
{
	FTRTestSession F;
	F.Area->Settings.CurrentMps = FVector::ZeroVector; // F02/M07 isolates vertical motion.
	if (!F.Start(*this)) { return false; }
	F.Deploy();
	const FTREgiSnapshot Initial = F.Session->Fishing->GetSnapshot();
	FTRSimTime Time = F.Sim()->GetSimulationTime();
	FTRBoatSnapshot Boat = F.Boat->GetBoatSnapshot(); Boat.Tick = Time.TickIndex;
	FTROceanQuery Query; Query.PositionXYM = Initial.PositionXYM; Query.SimTick = Time.TickIndex;
	FTROceanSample Ocean = F.World->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(Query);
	const FTREgiAction Action = F.Session->Fishing->GetAction();
	FTROceanSample BadOcean = Ocean; BadOcean.BottomDepthM = std::numeric_limits<float>::quiet_NaN();
	TestTrue(TEXT("NaN water depth rejected"), F.Session->EgiSimulation->StepEgi(Initial.CastId, Time, BadOcean, Boat, Action, [&Ocean](const FTROceanQuery&) { return Ocean; }) == ETREgiStepEvent::EnvironmentInvalid);
	TestEqual(TEXT("Invalid sample never commits depth"), F.Session->EgiSimulation->BuildSnapshot(ETRFishingState::FreeFall).DepthM, Initial.DepthM);
	FTRSimTime BadTime = Time; BadTime.StepSeconds = 0.0;
	TestTrue(TEXT("Zero dt rejected without division"), F.Session->EgiSimulation->StepEgi(Initial.CastId, BadTime, Ocean, Boat, Action, [&Ocean](const FTROceanQuery&) { return Ocean; }) == ETREgiStepEvent::EnvironmentInvalid);
	TStrongObjectPtr<UTREgiSimulationComponent> Isolated(NewObject<UTREgiSimulationComponent>());
	FTREquipmentSnapshot Invalid = F.Session->GetEquipmentSnapshot(); Invalid.SinkSpeedMps = std::numeric_limits<float>::infinity();
	TArray<FText> Errors;
	Ocean.SampleTick = Initial.Tick;
	TestFalse(TEXT("Infinite sink coefficient rejected at initialization"), Isolated->InitializeCast(Initial, Invalid, Ocean, Errors));
	FTREgiSnapshot ZeroLine = Initial; ZeroLine.LineLengthM = 0.0f;
	TestFalse(TEXT("M08 rejects line below configured minimum"), Isolated->InitializeCast(ZeroLine, F.Session->GetEquipmentSnapshot(), Ocean, Errors));
	TestTrue(TEXT("Valid line initializes"), Isolated->InitializeCast(Initial, F.Session->GetEquipmentSnapshot(), Ocean, Errors));
	Ocean.SampleTick = Time.TickIndex;
	TestTrue(TEXT("Finite vertical update with valid line"), Isolated->StepEgi(Initial.CastId, Time, Ocean, Boat, Action, [&Ocean](const FTROceanQuery&) { return Ocean; }) == ETREgiStepEvent::None);
	const FTREgiSnapshot Once = Isolated->BuildSnapshot(ETRFishingState::FreeFall);
	Isolated->StepEgi(Initial.CastId, Time, Ocean, Boat, Action, [&Ocean](const FTROceanQuery&) { return Ocean; });
	TestEqual(TEXT("Duplicate tick cannot integrate twice"), Isolated->BuildSnapshot(ETRFishingState::FreeFall).DepthM, Once.DepthM);
	F.World->DestroyActor(F.Provider.Get()); F.Step();
	TestTrue(TEXT("Invalid environment aborts instead of false bottom"), F.Session->GetSessionPhase() == ETRSessionPhase::Result && F.Session->GetLastResult().Outcome == ETRCastOutcome::Aborted);
	TestNull(TEXT("Invalid environment releases visual"), F.Session->GetEgiActor());
	return true;
}
#endif
