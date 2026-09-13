#include "Data/TROceanAreaDataAsset.h"
#include "Data/TREnvironmentUnits.h"
#include "Data/TRSessionConfigDataAsset.h"
#include "Ocean/TREnvironmentField.h"
#include "Ocean/TROceanWorldSubsystem.h"
#include "Ocean/TRSeabedProviderActor.h"
#include "Game/TRSimulationWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace
{
	struct FTREnvironmentTestWorld
	{
		UWorld* World;
		TStrongObjectPtr<UTROceanAreaDataAsset> Area{NewObject<UTROceanAreaDataAsset>()};
		TWeakObjectPtr<ATRSeabedProviderActor> Provider;
		FTREnvironmentTestWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			if (World) { Provider = World->SpawnActor<ATRSeabedProviderActor>(); }
			auto& S = Area->Settings;
			S.AreaId = TEXT("M105TestOcean"); S.BoundsMinXYM = FVector2D(-100,-100); S.BoundsMaxXYM = FVector2D(100,100);
			S.FlatDepthM = 30; S.FieldRevision = 2; S.WindMps = FVector2D(2,0);
			S.CurrentMode = ETRCurrentFieldMode::DepthProfile;
			for (int32 I=0; I<3; ++I)
			{
				FTRCurrentDepthKey K; K.DepthM = float(15*I);
				K.CurrentMps = I==0 ? FVector(0.2,0,0) : (I==1 ? FVector(0,0.4,0) : FVector(-0.2,0,0));
				S.CurrentDepthProfile.Add(K);
			}
		}
		~FTREnvironmentTestWorld()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
		UTROceanWorldSubsystem* Ocean() const { return World ? World->GetSubsystem<UTROceanWorldSubsystem>() : nullptr; }
		bool Start(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Ocean"), Ocean()) || !Test.TestNotNull(TEXT("Provider"), Provider.Get())) { return false; }
			TArray<FText> Errors;
			const bool Ok = Ocean()->InitializeArea(Area.Get(), Provider.Get(), Errors);
			for (const auto& Error : Errors) { Test.AddError(Error.ToString()); }
			return Test.TestTrue(TEXT("Initialize environment"), Ok);
		}
	};
	class FTRSpatialTestField : public FTREnvironmentField
	{
	public:
		virtual FVector2D WindAtLocation(const FTROceanAreaSettings&, const FVector2D& XY, int64 Tick) const override
		{
			return FVector2D(-XY.Y * 0.1, 2.0);
		}
		virtual FVector CurrentAtLocationAndDepth(const FTROceanAreaSettings&, const FTROceanQuery& Q) const override
		{
			return FVector(Q.PositionXYM.X * 0.01, Q.DepthM * 0.01 + Q.SimTick * 0.001, 0);
		}
	};
	class FTRInvalidTestField : public FTREnvironmentField
	{
	public:
		virtual FVector2D WindAtLocation(const FTROceanAreaSettings&, const FVector2D&, int64) const override
		{
			return FVector2D(std::numeric_limits<double>::quiet_NaN(), 0);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTREnvironmentUnitsTest, "TipRun.M105A.UnitsAndDirections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTREnvironmentUnitsTest::RunTest(const FString& Parameters)
{
	const double Knots[] = {0.4,0.7,1.0};
	const double Expected[] = {0.20577777777777778,0.3601111111111111,0.5144444444444445};
	FTREnvironmentTestWorld F;
	F.Area->Settings.CurrentMode = ETRCurrentFieldMode::Constant; F.Area->Settings.CurrentDepthProfile.Empty();
	for (int32 I=0; I<3; ++I)
	{
		double Mps=0, RoundTrip=0;
		TestTrue(TEXT("Convert knots"), TREnvironmentUnits::TryKnotsToMps(Knots[I], Mps));
		TestTrue(TEXT("Independent SI reference"), FMath::Abs(Mps-Expected[I]) < 1e-6);
		TestTrue(TEXT("Convert back"), TREnvironmentUnits::TryMpsToKnots(Mps, RoundTrip));
		TestTrue(TEXT("Round trip"), FMath::Abs(RoundTrip-Knots[I]) < 1e-12);
		for (const FVector Direction : {FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0)})
		{
			F.Area->Settings.CurrentMps = Direction*Mps;
			if (!F.Start(*this)) { return false; }
			const auto S = F.Ocean()->SampleSurfaceCurrent({}, 12);
			TestTrue(TEXT("Independent wind in same/opposite/orthogonal conditions"), S.bValid && S.WindMps.Equals(FVector2D(2,0)));
			TestTrue(TEXT("Current direction and SI speed"), S.SurfaceCurrentMps.Equals(Direction*Expected[I],1e-6));
			TestTrue(TEXT("Surface alias"), S.CurrentMps.Equals(S.SurfaceCurrentMps));
		}
	}
	F.Area->Settings.CurrentMps = FVector::ZeroVector;
	if (!F.Start(*this)) { return false; }
	TestTrue(TEXT("No current does not remove wind"), F.Ocean()->SampleOcean({}).WindMps.Equals(FVector2D(2,0)));
	F.Area->Settings.WindMps = FVector2D::ZeroVector; F.Area->Settings.CurrentMps = FVector(0.2,0,0);
	if (!F.Start(*this)) { return false; }
	TestTrue(TEXT("No wind does not remove current"), F.Ocean()->SampleOcean({}).CurrentMps.Equals(FVector(0.2,0,0)));
	double Out=123;
	TestFalse(TEXT("NaN rejected"), TREnvironmentUnits::TryKnotsToMps(std::numeric_limits<double>::quiet_NaN(), Out));
	TestFalse(TEXT("Infinity rejected"), TREnvironmentUnits::TryKnotsToMps(std::numeric_limits<double>::infinity(), Out));
	TestFalse(TEXT("Overflow rejected"), TREnvironmentUnits::TryMpsToKnots(std::numeric_limits<double>::max(), Out));
	TestEqual(TEXT("Failure preserves output"), Out, 123.0);
	TestTrue(TEXT("Signed components"), TREnvironmentUnits::TryKnotsToMps(-1, Out) && FMath::Abs(Out+Expected[2])<1e-12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTREnvironmentProfileTest, "TipRun.M105A.ProfileAndFrozenSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTREnvironmentProfileTest::RunTest(const FString& Parameters)
{
	FTREnvironmentTestWorld F; if (!F.Start(*this)) { return false; }
	const float Depths[] = {0,7.5f,15,22.5f,30,100};
	const FVector Expected[] = {{0.2,0,0},{0.1,0.2,0},{0,0.4,0},{-0.1,0.2,0},{-0.2,0,0},{-0.2,0,0}};
	F.Area->Settings.CurrentDepthProfile[0].CurrentMps = FVector(99,0,0);
	F.Area->Settings.WindMps = FVector2D(99,99);
	for (int32 I=0; I<6; ++I)
	{
		FTROceanQuery Q; Q.DepthM=Depths[I]; Q.SimTick=42;
		const auto S = F.Ocean()->SampleCurrentAtLocationAndDepth(Q);
		TestTrue(TEXT("Frozen profile and interpolation/clamping"), S.bValid && S.CurrentMps.Equals(Expected[I],1e-6));
		TestTrue(TEXT("Same field at surface"), S.SurfaceCurrentMps.Equals(FVector(0.2,0,0),1e-6));
		TestTrue(TEXT("Frozen wind"), S.WindMps.Equals(FVector2D(2,0)));
		TestEqual(TEXT("Requested tick"), S.SampleTick, int64(42));
	}
	// Constant tail past last layer but still inside seabed.
	F.Area->Settings.CurrentDepthProfile.SetNum(2);
	if (!F.Start(*this)) { return false; }
	FTROceanQuery Q; Q.DepthM=25;
	TestTrue(TEXT("Profile endpoint extension"), F.Ocean()->SampleOcean(Q).CurrentMps.Equals(FVector(0,0.4,0)));
	F.Ocean()->ShutdownArea();
	TestFalse(TEXT("Unloaded area"), F.Ocean()->SampleOcean(Q).bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTREnvironmentValidationTest, "TipRun.M105A.ValidationAndLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTREnvironmentValidationTest::RunTest(const FString& Parameters)
{
	FTREnvironmentTestWorld F;
	const auto Good = F.Area->Settings;
	for (int32 Case=0; Case<10; ++Case)
	{
		F.Area->Settings=Good; auto& S=F.Area->Settings;
		switch (Case)
		{
		case 0: S.CurrentDepthProfile.Empty(); break;
		case 1: S.CurrentDepthProfile[0].DepthM=1; break;
		case 2: S.CurrentDepthProfile[1].DepthM=0; break;
		case 3: S.CurrentDepthProfile[1].DepthM=-1; break;
		case 4: S.CurrentDepthProfile[1].DepthM=std::numeric_limits<float>::quiet_NaN(); break;
		case 5: S.CurrentDepthProfile[1].CurrentMps.Z=1; break;
		case 6: S.WindMps.X=std::numeric_limits<double>::infinity(); break;
		case 7: S.FieldRevision=3; break;
		case 8: S.CurrentMode=ETRCurrentFieldMode(99); break;
		case 9: S.CurrentDepthProfile[2].CurrentMps.X=std::numeric_limits<double>::max(); break;
		}
		TArray<FText> Errors;
		TestFalse(TEXT("Reject invalid settings"), S.Validate(Errors));
		FDataValidationContext Context;
		TestTrue(TEXT("Editor validator rejects too"), F.Area->IsDataValid(Context)==EDataValidationResult::Invalid);
		TestFalse(TEXT("Invalid initialization fails"), F.Ocean()->InitializeArea(F.Area.Get(),F.Provider.Get(),Errors));
		TestFalse(TEXT("No fallback valid sea"), F.Ocean()->SampleOcean({}).bValid);
	}
	F.Area->Settings=Good; if (!F.Start(*this)) { return false; }
	FTROceanQuery Q; Q.PositionXYM.X=101;
	TestTrue(TEXT("Outside area"), F.Ocean()->SampleOcean(Q).InvalidReason==ETRSampleError::OutsideArea);
	Q={}; Q.DepthM=-1;
	TestTrue(TEXT("Negative depth"), F.Ocean()->SampleOcean(Q).InvalidReason==ETRSampleError::InvalidDepth);
	Q={}; Q.PositionXYM.X=std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("NaN query"),F.Ocean()->SampleOcean(Q).bValid);
	TArray<FText> Errors;
	TestTrue(TEXT("Install invalid-output evaluator"), F.Ocean()->InitializeAreaWithField(F.Area.Get(), F.Provider.Get(), MakeShared<FTRInvalidTestField>(), Errors));
	const auto Bad = F.Ocean()->SampleWindAtLocation({},0);
	TestFalse(TEXT("Invalid wind invalidates sample"), Bad.bValid);
	TestFalse(TEXT("NaN never exposed as numeric output"), Bad.WindMps.ContainsNaN());
	if (!F.Start(*this)) { return false; }
	F.Provider->Destroy();
	TestTrue(TEXT("Destroyed provider"), F.Ocean()->SampleOcean({}).InvalidReason==ETRSampleError::MissingProvider);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTREnvironmentCompatibilityTest, "TipRun.M105A.LegacyAndSerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTREnvironmentCompatibilityTest::RunTest(const FString& Parameters)
{
	FTREnvironmentTestWorld F;
	TArray<uint8> Bytes;
	FObjectWriter Writer(F.Area.Get(),Bytes);
	TStrongObjectPtr<UTROceanAreaDataAsset> Copy(NewObject<UTROceanAreaDataAsset>());
	FObjectReader Reader(Copy.Get(),Bytes);
	TestEqual(TEXT("Revision survives serialization"), Copy->Settings.FieldRevision,2);
	TestEqual(TEXT("Profile survives serialization"), Copy->Settings.CurrentDepthProfile.Num(),3);
	F.Area->Settings=Copy->Settings; if (!F.Start(*this)) { return false; }
	FTROceanQuery Q; Q.DepthM=7.5f;
	TestTrue(TEXT("Serialized profile evaluation"),F.Ocean()->SampleOcean(Q).CurrentMps.Equals(FVector(0.1,0.2,0),1e-6));
	TArray<FText> Errors;
	F.Area->Settings.FieldRevision=1;
	TestFalse(TEXT("Revision 1 cannot silently acquire wind/profile"),F.Area->Validate(Errors));
	TStrongObjectPtr<UTRSessionConfigDataAsset> Legacy(LoadObject<UTRSessionConfigDataAsset>(nullptr,
		TEXT("/Game/TipRun/Prototype/Data/DA_TR_M09Session_Prototype.DA_TR_M09Session_Prototype")));
	if (!TestNotNull(TEXT("Existing saved Prototype"),Legacy.Get()) || !TestNotNull(TEXT("Legacy ocean"),Legacy->Ocean.Get())) { return false; }
	F.Area->Settings=Legacy->Ocean->Settings;
	TestEqual(TEXT("Old package explicitly remains revision 1"),F.Area->Settings.FieldRevision,1);
	if (!F.Start(*this)) { return false; }
	const auto Sample=F.Ocean()->SampleOcean(Q);
	TestTrue(TEXT("Legacy constant at depth"),Sample.bValid && Sample.CurrentMps.Equals(F.Area->Settings.CurrentMps));
	TestTrue(TEXT("Legacy zero wind"),Sample.WindMps.IsZero());
	F.Area->Settings.FieldRevision=2;
	if (!F.Start(*this)) { return false; }
	TestEqual(TEXT("Explicit upgrade preserves values"),F.Ocean()->SampleOcean(Q).FieldRevision,2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTREnvironmentFrameTest, "TipRun.M105A.SpatialDelegationAndFrameRates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTREnvironmentFrameTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Baseline;
	for (const int32 Fps : {30,60,120})
	{
		FTREnvironmentTestWorld F;
		TArray<FText> Errors;
		if (!TestTrue(TEXT("Spatial field initialize"),F.Ocean()->InitializeAreaWithField(F.Area.Get(),F.Provider.Get(),MakeShared<FTRSpatialTestField>(),Errors))) { return false; }
		auto* Sim=F.World->GetSubsystem<UTRSimulationWorldSubsystem>();
		TStrongObjectPtr<UTRSessionConfigDataAsset> Config(NewObject<UTRSessionConfigDataAsset>());
		Config->StepSeconds=1.0/60.0; Config->MaxCatchUpSteps=8;
		if (!TestNotNull(TEXT("Fixed coordinator"),Sim) || !TestTrue(TEXT("Configure"),Sim->Configure(Config.Get(),Errors))) { return false; }
		TArray<FVector> Trace;
		auto* Owner=F.World->SpawnActor<AActor>();
		const auto Id=Sim->RegisterSession(Owner,FTRSimulationStep::CreateLambda([&](ETRSimulationPhase Phase,const FTRSimTime& Time)
		{
			if (Phase!=ETRSimulationPhase::Fishing) { return; }
			FTROceanQuery Q; Q.PositionXYM=FVector2D(Time.TickIndex,5); Q.DepthM=7.5f; Q.SimTick=Time.TickIndex;
			const auto S=F.Ocean()->SampleOcean(Q);
			TestTrue(TEXT("Spatial/depth/tick delegation"),S.bValid && S.CurrentMps.Equals(FVector(Time.TickIndex*.01,.075+Time.TickIndex*.001,0),1e-6));
			TestTrue(TEXT("Wind uses XY independently"),S.WindMps.Equals(FVector2D(-.5,2),1e-6));
			TestTrue(TEXT("Surface uses depth zero"),S.SurfaceCurrentMps.Equals(FVector(Time.TickIndex*.01,Time.TickIndex*.001,0),1e-6));
			Trace.Add(S.CurrentMps);
		}),FTRSimulationCommand::CreateLambda([](const FTRFishingCommand&) {}));
		TestTrue(TEXT("Session registered"),Id.IsValid());
		for (int32 Frame=0; Frame<Fps; ++Frame) { Sim->AdvanceFrame(1.0/Fps); }
		TestEqual(TEXT("60 fixed samples"),Trace.Num(),60);
		if (Fps==30) { Baseline=Trace; }
		else { TestTrue(TEXT("Identical fixed results across rendering fps"),Trace==Baseline); }
		Sim->SetSimulationPaused(true);
		Sim->AdvanceFrame(1); TestEqual(TEXT("Pause adds no samples"),Trace.Num(),60);
		const double TimeBefore=F.Ocean()->GetEnvironmentTimeS();
		for (int32 I=0; I<10; ++I) { F.Ocean()->SampleWindAtLocation({},10); }
		TestEqual(TEXT("Read does not advance environment"),F.Ocean()->GetEnvironmentTimeS(),TimeBefore);
	}
	return true;
}
#endif
