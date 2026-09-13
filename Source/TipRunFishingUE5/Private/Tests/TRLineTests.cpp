#include "TRSessionTestFixture.h"
#include "Fishing/TREgiSimulationComponent.h"
#include "Fishing/TREgiActor.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include <limits>

namespace
{
	struct FTRTestLine
	{
		FTRTestSession F;
		TStrongObjectPtr<UTREgiSimulationComponent> Egi{NewObject<UTREgiSimulationComponent>()};
		FTREquipmentSnapshot Equipment;
		FTRBoatSnapshot Boat;
		FTRSimTime Time;
		FTREgiAction Action;
		FTRTestLine()
		{
			F.Area->Settings.SurfaceZ_M = 0.0f;
			F.Area->Settings.FlatDepthM = 100.0f;
			F.Area->Settings.CurrentMps = FVector::ZeroVector;
			F.Tuning->Parameters.StaySinkScale = 1.0f; // Temporary test balance only.
			Time.StepSeconds = 1.0 / 60.0;
			Action.FishingState = ETRFishingState::Stay;
			Action.LineMode = ETRLineMode::Locked;
		}
		FTROceanSample Sea(const FTROceanQuery& Q) const { return F.World->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(Q); }
		FTREgiSnapshot Snapshot() const { return Egi->BuildSnapshot(Action.FishingState); }
		bool Start(FAutomationTestBase& Test, FName EgiId = TEXT("Egi_3_5"), FName SinkerId = TEXT("Sinker_None"),
			float LineM = 10.0f, float DepthM = 6.0f, FVector2D XYM = FVector2D(-8.0, 0.0))
		{
			if (!F.Environment(Test)) { return false; }
			TArray<FText> Errors;
			if (!Test.TestTrue(TEXT("M02 equipment resolves"), TREquipment::TryBuildSnapshot(F.Egis.Get(), F.Sinkers.Get(), F.Tuning.Get(),
				EgiId, SinkerId, Time.StepSeconds, Equipment, Errors))) { return false; }
			FTREgiSnapshot Initial; Initial.CastId = FTRCastId(1); Initial.DepthM = DepthM; Initial.PositionXYM = XYM; Initial.LineLengthM = LineM;
			FTROceanQuery Q; Q.PositionXYM = XYM; Q.DepthM = DepthM;
			Action.SinkScale = Equipment.Parameters.StaySinkScale;
			return Test.TestTrue(TEXT("Egi initializes"), Egi->InitializeCast(Initial, Equipment, Sea(Q), Errors));
		}
		ETREgiStepEvent Step(const FVector& BoatVelocity = FVector::ZeroVector)
		{
			++Time.TickIndex; Boat.Tick = Time.TickIndex;
			Boat.VelocityMps = BoatVelocity; Boat.PositionM += BoatVelocity * Time.StepSeconds; Boat.RodTipM += BoatVelocity * Time.StepSeconds;
			const FTREgiSnapshot S = Snapshot();
			FTROceanQuery Q; Q.PositionXYM = S.PositionXYM; Q.DepthM = S.DepthM; Q.SimTick = Time.TickIndex;
			return Egi->StepEgi(S.CastId, Time, Sea(Q), Boat, Action, [this](const FTROceanQuery& D) { return Sea(D); });
		}
		void CheckLine(FAutomationTestBase& Test) const
		{
			const FTREgiSnapshot S = Snapshot();
			const FVector Offset = FVector(S.PositionXYM.X, S.PositionXYM.Y, -double(S.DepthM)) - Boat.RodTipM;
			Test.TestTrue(TEXT("Finite bounded position/velocity"), !Offset.ContainsNaN() && !S.VelocityMps.ContainsNaN() &&
				S.VelocityMps.Size() <= Equipment.Parameters.MaxEgiSpeedMps + 1.e-6);
			Test.TestTrue(TEXT("Line distance within 0.01 mm tolerance"), Offset.Size() <= double(S.LineLengthM) + 1.e-5);
			Test.TestTrue(TEXT("Line length and tension bounded"), S.LineLengthM >= Equipment.Parameters.MinLineM &&
				S.LineLengthM <= Equipment.Parameters.MaxLineLengthM && S.Tension01 >= 0.0f && S.Tension01 <= 1.0f);
			Test.TestTrue(TEXT("Depth in water"), S.DepthM >= 0.0f && S.DepthM <= F.Area->Settings.FlatDepthM);
			Test.TestTrue(TEXT("Angle agrees with rod geometry"), FMath::IsNearlyEqual(double(S.LineAngleRad),
				FMath::Atan2(Offset.Size2D(), FMath::Max(-Offset.Z, UE_DOUBLE_SMALL_NUMBER)), 1.e-6));
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRLineCurrentTest, "TipRun.M08.F04CurrentAndWeight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRLineCurrentTest::RunTest(const FString& Parameters)
{
	FTRTestLine Still;
	if (!Still.Start(*this, TEXT("Egi_3_5"), TEXT("Sinker_None"), 100.0f)) { return false; }
	for (int32 I = 0; I < 60; ++I) { Still.Step(); }
	TestTrue(TEXT("Zero current/stationary boat produces no horizontal displacement"), Still.Snapshot().PositionXYM.Equals(FVector2D(-8.0, 0.0), 0.0));
	TestEqual(TEXT("Slack line never pushes"), Still.Snapshot().Tension01, 0.0f);
	FTREgiSnapshot Light;
	for (bool bHeavy : {false, true})
	{
		FTRTestLine R; R.F.Area->Settings.CurrentMps = FVector(1.0, -0.5, 0.0);
		if (!R.Start(*this, bHeavy ? TEXT("Egi_3") : TEXT("Egi_3_5"), bHeavy ? TEXT("Sinker_50") : TEXT("Sinker_None"), 100.0f)) { return false; }
		for (int32 I = 0; I < 60; ++I) { TestTrue(TEXT("Valid current step"), R.Step() != ETREgiStepEvent::EnvironmentInvalid); }
		const FTREgiSnapshot S = R.Snapshot();
		TestTrue(TEXT("Responds along positive X/negative Y current"), S.PositionXYM.X > -8.0 && S.PositionXYM.Y < 0.0);
		TestTrue(TEXT("Exponential horizontal velocity"), FMath::IsNearlyEqual(S.VelocityMps.X, 1.0 - FMath::Exp(-double(R.Equipment.HorizontalResponsePerS)), 1.e-9));
		if (!bHeavy) { Light = S; }
		else { TestTrue(TEXT("80g falls further and responds slower than 35g on Prototype curves"), S.DepthM > Light.DepthM && S.PositionXYM.X < Light.PositionXYM.X); }
		AddInfo(FString::Printf(TEXT("Test mass=%.0fg, depth=%.6fm, XY=(%.6f,%.6f)m"), R.Equipment.TotalMassG, S.DepthM, S.PositionXYM.X, S.PositionXYM.Y));
		R.CheckLine(*this);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRLineBalanceTest, "TipRun.M08.F05WeightDriftBalance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRLineBalanceTest::RunTest(const FString& Parameters)
{
	float Change[3] = {};
	for (int32 I = 0; I < 3; ++I)
	{
		FTRTestLine R;
		const FName Id = I == 1 ? TEXT("Egi_3_5") : TEXT("Egi_3");
		if (!R.Start(*this, Id, I == 2 ? TEXT("Sinker_50") : TEXT("Sinker_None"))) { return false; }
		// Same 8/6/10 geometry, current and rod speed. Balance derived from the 35g test sink speed:
		// tangent depth rate = sink * 8^2/10^2 - rodSpeed * 8*6/10^2.
		TestTrue(TEXT("Valid locked-line step"), R.Step(FVector(0.7 * 8.0 / 6.0, 0.0, 0.0)) == ETREgiStepEvent::None);
		const FTREgiSnapshot S = R.Snapshot(); Change[I] = S.DepthM - 6.0f;
		TestTrue(TEXT("Rod pulls egi toward moving boat"), S.PositionXYM.X > -8.0 && S.Tension01 > 0.0f);
		TestEqual(TEXT("Locked length unchanged"), S.LineLengthM, 10.0f);
		R.CheckLine(*this);
		AddInfo(FString::Printf(TEXT("Test mass=%.0fg, depth delta=%+.7fm"), R.Equipment.TotalMassG, Change[I]));
	}
	TestTrue(TEXT("Light rises"), Change[0] < -0.0005f);
	TestTrue(TEXT("Heavy sinks"), Change[2] > 0.005f);
	TestTrue(TEXT("Balanced depth change is smaller than rising/falling"), FMath::Abs(Change[1]) < 0.0001f &&
		FMath::Abs(Change[1]) < FMath::Abs(Change[0]) && FMath::Abs(Change[1]) < FMath::Abs(Change[2]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRLineBoundsTest, "TipRun.M08.F12LineBoundsAndLargeStep", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRLineBoundsTest::RunTest(const FString& Parameters)
{
	for (ETRLineMode Mode : {ETRLineMode::Locked, ETRLineMode::Payout, ETRLineMode::ControlledPayout})
	{
		FTRTestLine R; R.F.Area->Settings.CurrentMps = FVector(20.0, -10.0, 0.0);
		if (!R.Start(*this, TEXT("Egi_3_5"), TEXT("Sinker_None"), 1.0f, 0.0f, FVector2D::ZeroVector)) { return false; }
		R.Action.LineMode = Mode; R.Time.StepSeconds = 2.0; // Stress numerical solver, not a product clock setting.
		for (int32 I = 0; I < 50; ++I)
		{
			TestTrue(TEXT("Large step remains valid"), R.Step(FVector(0.5, 0.0, 0.0)) != ETREgiStepEvent::EnvironmentInvalid);
			R.CheckLine(*this);
		}
		const float Expected = Mode == ETRLineMode::Locked ? 1.0f : (Mode == ETRLineMode::Payout ? 100.0f : 11.0f);
		TestTrue(TEXT("Configured payout/locked/max length"), FMath::IsNearlyEqual(R.Snapshot().LineLengthM, Expected, 0.001f));
	}
	FTRTestLine Zero;
	if (!Zero.Start(*this, TEXT("Egi_3_5"), TEXT("Sinker_None"), 1.0f, 0.0f, FVector2D::ZeroVector)) { return false; }
	TestTrue(TEXT("Coincident rod/egi has no division by zero"), Zero.Step() == ETREgiStepEvent::None); Zero.CheckLine(*this);
	const FTREgiSnapshot Before = Zero.Snapshot();
	Zero.Boat.RodTipM.Z = 2.0;
	TestTrue(TEXT("Incompatible rod height and locked line rejected"), Zero.Step() == ETREgiStepEvent::EnvironmentInvalid);
	TestEqual(TEXT("Infeasible solve does not commit tick"), Zero.Snapshot().Tick, Before.Tick);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRLineDestinationTest, "TipRun.M08.DestinationAndInvalidValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRLineDestinationTest::RunTest(const FString& Parameters)
{
	FTRTestLine R; R.F.Area->Settings.BoundsMinXYM = FVector2D(-8.01, -100.0); R.F.Area->Settings.CurrentMps = FVector(-10.0, 0.0, 0.0);
	if (!R.Start(*this, TEXT("Egi_3_5"), TEXT("Sinker_None"), 100.0f)) { return false; }
	R.Time.StepSeconds = 1.0;
	TestTrue(TEXT("Destination outside M03 area rejected"), R.Step() == ETREgiStepEvent::EnvironmentInvalid);
	TestEqual(TEXT("Destination rejection preserves source XY"), R.Snapshot().PositionXYM.X, -8.0);
	R.Boat.VelocityMps.X = std::numeric_limits<double>::infinity();
	FTRSimTime T = R.Time; ++T.TickIndex; R.Boat.Tick = T.TickIndex;
	FTROceanQuery Q; Q.PositionXYM = R.Snapshot().PositionXYM; Q.SimTick = T.TickIndex;
	TestTrue(TEXT("Invalid boat velocity rejected"), R.Egi->StepEgi(FTRCastId(1), T, R.Sea(Q), R.Boat, R.Action,
		[&](const FTROceanQuery& D) { return R.Sea(D); }) == ETREgiStepEvent::EnvironmentInvalid);
		FTRTestSession Boundary;
	Boundary.Area->Settings.BoundsMinXYM = FVector2D(9.0, 19.0);
	Boundary.Area->Settings.BoundsMaxXYM = FVector2D(12.01, 22.0);
	Boundary.BoatTuning->Parameters.CurrentResponse01 = 0.0;
	if (!Boundary.Start(*this)) { return false; }
	Boundary.Deploy(); Boundary.Step(60);
	TestTrue(TEXT("Invalid egi destination safely aborts Session"), Boundary.Session->HasResult() &&
		Boundary.Session->GetLastResult().Outcome == ETRCastOutcome::Aborted);
	TestNull(TEXT("Invalid destination releases visual"), Boundary.Session->GetEgiActor());
	FTRBoatSnapshot StillValid;
	TestTrue(TEXT("Boat remains valid; egi destination caused abort"), Boundary.Sim()->GetBoatSnapshot(Boundary.BoatId, StillValid));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRLineIntegrationTest, "TipRun.M08.B07FixedBoatIntegration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRLineIntegrationTest::RunTest(const FString& Parameters)
{
	FTREgiSnapshot Reference;
	for (int32 FPS : {30, 60, 120})
	{
		FTRTestLine R; R.F.Area->Settings.CurrentMps = FVector(1.0, 0.0, 0.0);
		if (!R.Start(*this)) { return false; }
		// Controlled locked-line action, driven by real M04/M05. No Stay command/state machine is installed.
		int32 Steps = 0;
		const FTRActorSimId Id = R.F.Sim()->RegisterSession(R.F.Session.Get(), FTRSimulationStep::CreateLambda(
			[&](ETRSimulationPhase Phase, const FTRSimTime& T)
			{
				if (Phase != ETRSimulationPhase::Fishing || T.TickIndex == 0) { return; }
				FTRBoatSnapshot B;
				if (!TestTrue(TEXT("Updated M05 snapshot available"), R.F.Sim()->GetBoatSnapshot(R.F.BoatId, B))) { return; }
				TestEqual(TEXT("Boat updated before egi"), B.Tick, T.TickIndex);
				// Translate the configured test rod origin; preserve M05 motion and velocity.
				B.PositionM -= FVector(12.0, 21.0, 1.5); B.RodTipM -= FVector(12.0, 21.0, 1.5);
				R.Boat = B;
				const auto S = R.Snapshot(); FTROceanQuery Q; Q.PositionXYM = S.PositionXYM; Q.DepthM = S.DepthM; Q.SimTick = T.TickIndex;
				TestTrue(TEXT("Boat/egi step succeeds"), R.Egi->StepEgi(S.CastId, T, R.Sea(Q), B, R.Action,
					[&](const FTROceanQuery& D) { return R.Sea(D); }) != ETREgiStepEvent::EnvironmentInvalid);
				++Steps; R.CheckLine(*this);
			}), FTRSimulationCommand::CreateLambda([](const FTRFishingCommand&) {}));
		if (!TestTrue(TEXT("Controlled integration registered"), Id.IsValid())) { return false; }
		for (int32 I = 0; I < FPS; ++I) { R.F.Sim()->Tick(1.0f / FPS); }
		const auto Before = R.Snapshot(); const int32 StepsBefore = Steps;
		R.F.Sim()->SetSimulationPaused(true);
		for (int32 I = 0; I < FPS; ++I) { R.F.Sim()->Tick(1.0f / FPS); }
		TestEqual(TEXT("Pause stops solver"), Steps, StepsBefore);
		TestTrue(TEXT("Pause preserves all geometry"), R.Snapshot().PositionXYM.Equals(Before.PositionXYM, 0.0) && R.Snapshot().DepthM == Before.DepthM && R.Snapshot().LineLengthM == Before.LineLengthM);
		R.F.Sim()->SetSimulationPaused(false);
		for (int32 I = 0; I < FPS; ++I) { R.F.Sim()->Tick(1.0f / FPS); }
		const auto S = R.Snapshot();
		TestTrue(TEXT("Moving boat/current changes line angle and depth"), S.PositionXYM.X > -8.0 && S.LineAngleRad > 0.0f && S.DepthM != 6.0f);
		if (FPS == 30) { Reference = S; }
		else { TestTrue(TEXT("30/60/120 identical fixed results"), S.Tick == Reference.Tick && S.DepthM == Reference.DepthM &&
			S.PositionXYM.Equals(Reference.PositionXYM, 0.0) && S.VelocityMps.Equals(Reference.VelocityMps, 0.0) &&
			S.LineLengthM == Reference.LineLengthM && S.LineAngleRad == Reference.LineAngleRad && S.Tension01 == Reference.Tension01); }
		R.F.Sim()->Unregister(Id);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRLineSessionTest, "TipRun.M08.SessionLifetimeAndDestination", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRLineSessionTest::RunTest(const FString& Parameters)
{
	FTRTestSession F;
	if (!F.Start(*this)) { return false; }
	F.Deploy(); F.Step(60);
	const auto Old = F.Session->Fishing->GetSnapshot();
	TestTrue(TEXT("Real Session integrates current and payout"), Old.VelocityMps.X > 0.0 && Old.LineLengthM > 1.5f);
	TWeakObjectPtr<ATREgiActor> OldVisual = F.Session->GetEgiActor();
	if (!TestTrue(TEXT("Retrieve before next cast"), F.ReturnToReady())) { return false; } F.Deploy();
	TestTrue(TEXT("Old visual released"), !OldVisual.IsValid() || OldVisual->IsActorBeingDestroyed());
	FTRSimTime T = F.Sim()->GetSimulationTime(); FTRBoatSnapshot B = F.Boat->GetBoatSnapshot(); B.Tick = T.TickIndex;
	FTROceanQuery Q; Q.SimTick = T.TickIndex;
	const auto Sea = F.World->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(Q);
	const auto Action = F.Session->Fishing->GetAction();
	const auto Current = F.Session->Fishing->GetSnapshot();
	TestTrue(TEXT("Stale cast ignored"), F.Session->EgiSimulation->StepEgi(Old.CastId, T, Sea, B, Action,
		[&](const FTROceanQuery&) { AddError(TEXT("Stale cast must not query destination")); return Sea; }) == ETREgiStepEvent::None);
	TStrongObjectPtr<UTREgiSimulationComponent> Retained(F.Session->EgiSimulation.Get());
	F.World->DestroyActor(F.Session.Get()); F.Step(10);
	TestTrue(TEXT("Destroyed session cannot update"), Retained->StepEgi(Current.CastId, T, Sea, B, Action,
		[&](const FTROceanQuery&) { AddError(TEXT("Destroyed session must not query destination")); return Sea; }) == ETREgiStepEvent::None);
	TestFalse(TEXT("Old numeric cast cleared"), Retained->BuildSnapshot(ETRFishingState::Inactive).CastId.IsValid());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRLineContactTest, "TipRun.M08.BottomContactAndRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRLineContactTest::RunTest(const FString& Parameters)
{
	FTRTestLine R; R.F.Area->Settings.FlatDepthM = 6.0f;
	if (!R.Start(*this)) { return false; }
	TestTrue(TEXT("Initial contact reported once"), R.Step() == ETREgiStepEvent::ReachedBottom);
	TestTrue(TEXT("Resting contact not repeated"), R.Step() == ETREgiStepEvent::None);
	TestTrue(TEXT("Moving rod lifts from bottom"), R.Step(FVector(10.0, 0.0, 0.0)) == ETREgiStepEvent::LeftBottom);
	TestTrue(TEXT("Egi above bottom after pull"), R.Snapshot().DepthM < 6.0f); R.CheckLine(*this);
	TestTrue(TEXT("Continued pulling does not repeat release"), R.Step(FVector(10.0, 0.0, 0.0)) == ETREgiStepEvent::None);

	FTRTestSession F; F.Area->Settings.FlatDepthM = 0.1f;
	F.Tuning->Parameters.PayoutMps = 0.1f; // Keep line taut when first touching shallow test bottom.
	F.BoatTuning->Parameters.CurrentResponse01 = 0.0;
	if (!F.Start(*this)) { return false; }
	int32 Released = 0;
	F.Session->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState From, ETRFishingState To)
	{
		if (From == ETRFishingState::BottomContact && To == ETRFishingState::TensionFall) { ++Released; }
	});
	F.Deploy();
	for (int32 I = 0; I < 600 && Released == 0; ++I) { F.Step(); }
	TestTrue(TEXT("Session handles physical release through Fishing"), Released > 0);
	TestFalse(TEXT("Release does not end cast"), F.Session->HasResult());
	TestTrue(TEXT("No M10 automatic Stay transition"), F.Session->Fishing->GetState() != ETRFishingState::Stay);
	return true;
}
#endif
