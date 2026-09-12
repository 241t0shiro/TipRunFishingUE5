#include "Data/TROceanAreaDataAsset.h"
#include "Data/TRSimulationTypes.h"
#include "Ocean/TRSeabedProviderActor.h"
#include "Ocean/TROceanWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace
{
	// Test-only 30 m seabed. No production map, environment or clock is created.
	struct FTRTestOcean
	{
		UWorld* World = nullptr;
		TStrongObjectPtr<UTROceanAreaDataAsset> Area{NewObject<UTROceanAreaDataAsset>()};
		TWeakObjectPtr<ATRSeabedProviderActor> Provider;

		FTRTestOcean()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("TR_Ocean_TestWorld")),
				nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			Area->Settings.AreaId = FName(TEXT("TestOcean"));
			Area->Settings.BoundsMinXYM = FVector2D(-100.0, -50.0);
			Area->Settings.BoundsMaxXYM = FVector2D(100.0, 50.0);
			Area->Settings.FlatDepthM = 30.0f;
			Area->Settings.SurfaceZ_M = 2.5f;
			Area->Settings.CurrentMps = FVector(0.4, -0.2, 0.0);
			if (World) { Provider = World->SpawnActor<ATRSeabedProviderActor>(); }
		}

		~FTRTestOcean()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		FTRTestOcean(const FTRTestOcean&) = delete;
		FTRTestOcean& operator=(const FTRTestOcean&) = delete;

		UTROceanWorldSubsystem* Ocean() const { return World ? World->GetSubsystem<UTROceanWorldSubsystem>() : nullptr; }
		bool Start(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Game world Ocean subsystem"), Ocean()) ||
				!Test.TestNotNull(TEXT("Spawned provider"), Provider.Get())) { return false; }
			TArray<FText> Errors;
			const bool bSuccess = Ocean()->InitializeArea(Area.Get(), Provider.Get(), Errors);
			for (const FText& Error : Errors) { Test.AddError(Error.ToString()); }
			return Test.TestTrue(TEXT("Area initialization"), bSuccess);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTROceanFlatTest, "TipRun.M03.O01FlatBottom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTROceanFlatTest::RunTest(const FString& Parameters)
{
	FTRTestOcean Fixture;
	if (!Fixture.Start(*this)) { return false; }
	TestTrue(TEXT("Ready"), Fixture.Ocean()->GetOceanState() == ETROceanState::Ready);
	TestFalse(TEXT("Provider does not tick"), Fixture.Provider->PrimaryActorTick.bCanEverTick);
	for (const FVector2D XY : { FVector2D::ZeroVector, FVector2D(-40.0, 25.0), FVector2D(70.0, -35.0) })
	{
		FTROceanQuery Query; Query.PositionXYM = XY; Query.SimTick = 123;
		const FTROceanSample Sample = Fixture.Ocean()->SampleOcean(Query);
		TestTrue(TEXT("Valid flat sample"), Sample.bValid);
		TestTrue(TEXT("No error on valid sample"), Sample.InvalidReason == ETRSampleError::None);
		TestEqual(TEXT("O01 depth 30 m"), Sample.BottomDepthM, 30.0f);
		TestTrue(TEXT("Normal points up"), Sample.BottomNormal.Equals(FVector::UpVector));
		TestEqual(TEXT("Sample preserves query tick"), Sample.SampleTick, int64(123));
		TestTrue(TEXT("Area identity"), Sample.AreaId == FName(TEXT("TestOcean")));
		TestEqual(TEXT("Fixed sea surface"), Sample.SurfaceZ_M, 2.5f);
	}
	Fixture.Provider->SetActorLocation(FVector(10000.0, 20000.0, 30000.0));
	TestEqual(TEXT("Actor transform is not the seabed authority"), Fixture.Ocean()->SampleOcean({}).BottomDepthM, 30.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTROceanDepthTest, "TipRun.M03.O03DepthQueries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTROceanDepthTest::RunTest(const FString& Parameters)
{
	FTRTestOcean Fixture;
	if (!Fixture.Start(*this)) { return false; }
	for (float Depth : { 0.0f, 30.0f, 100.0f, std::numeric_limits<float>::max() })
	{
		FTROceanQuery Query; Query.DepthM = Depth;
		const FTROceanSample Sample = Fixture.Ocean()->SampleOcean(Query);
		TestTrue(TEXT("Finite nonnegative depth remains queryable past the seabed"), Sample.bValid);
		TestEqual(TEXT("Seabed information remains available"), Sample.BottomDepthM, 30.0f);
		TestTrue(TEXT("MVP current is constant at all evaluation depths"), Sample.CurrentMps.Equals(Fixture.Area->Settings.CurrentMps));
	}
	for (float Depth : { -1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
	{
		FTROceanQuery Query; Query.DepthM = Depth;
		const FTROceanSample Sample = Fixture.Ocean()->SampleOcean(Query);
		TestFalse(TEXT("Invalid depth is not a valid sea"), Sample.bValid);
		TestTrue(TEXT("Explicit depth error"), Sample.InvalidReason == ETRSampleError::InvalidDepth);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTROceanBoundaryTest, "TipRun.M03.O04Boundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTROceanBoundaryTest::RunTest(const FString& Parameters)
{
	FTRTestOcean Fixture;
	if (!Fixture.Start(*this)) { return false; }
	for (const FVector2D XY : { FVector2D(-100.0, -50.0), FVector2D(-100.0, 50.0), FVector2D(100.0, -50.0), FVector2D(100.0, 50.0) })
	{
		FTROceanQuery Query; Query.PositionXYM = XY;
		TestTrue(TEXT("Inclusive corner boundary"), Fixture.Ocean()->SampleOcean(Query).bValid);
	}
	for (const FVector2D XY : { FVector2D(-100.0001, 0), FVector2D(100.0001, 0), FVector2D(0, -50.0001), FVector2D(0, 50.0001) })
	{
		FTROceanQuery Query; Query.PositionXYM = XY;
		const FTROceanSample Sample = Fixture.Ocean()->SampleOcean(Query);
		TestFalse(TEXT("Outside area never reports valid zero-depth sea"), Sample.bValid);
		TestTrue(TEXT("OutsideArea reason"), Sample.InvalidReason == ETRSampleError::OutsideArea);
		TestFalse(TEXT("Provider also rejects outside query"), Fixture.Provider->SampleBottomDepth(XY).bValid);
	}
	FTROceanQuery Query;
	Query.PositionXYM.X = std::numeric_limits<double>::quiet_NaN();
	TestTrue(TEXT("NaN position has explicit error"), Fixture.Ocean()->SampleOcean(Query).InvalidReason == ETRSampleError::InvalidQuery);
	Query.PositionXYM = FVector2D(0, std::numeric_limits<double>::infinity());
	TestFalse(TEXT("Infinite position rejected"), Fixture.Ocean()->SampleOcean(Query).bValid);
	Query.PositionXYM = FVector2D::ZeroVector; Query.SimTick = -1;
	TestTrue(TEXT("Negative tick rejected"), Fixture.Ocean()->SampleOcean(Query).InvalidReason == ETRSampleError::InvalidQuery);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTROceanLifetimeTest, "TipRun.M03.O05ProviderLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTROceanLifetimeTest::RunTest(const FString& Parameters)
{
	FTRTestOcean Fixture;
	if (!TestNotNull(TEXT("Ocean"), Fixture.Ocean())) { return false; }
	TestTrue(TEXT("Initial state"), Fixture.Ocean()->GetOceanState() == ETROceanState::Uninitialized);
	TestFalse(TEXT("Uninitialized sample is invalid"), Fixture.Ocean()->SampleOcean({}).bValid);
	TestFalse(TEXT("Uninitialized provider is invalid"), Fixture.Provider->SampleBottomDepth({}).bValid);
	TArray<FText> Errors;
	TestFalse(TEXT("Missing provider rejected"), Fixture.Ocean()->InitializeArea(Fixture.Area.Get(), nullptr, Errors));
	TestTrue(TEXT("Initialization failure enters Error"), Fixture.Ocean()->GetOceanState() == ETROceanState::Error);
	if (!Fixture.Start(*this)) { return false; }
	TestTrue(TEXT("Provider destroyed"), Fixture.World->DestroyActor(Fixture.Provider.Get()));
	const FTROceanSample Sample = Fixture.Ocean()->SampleOcean({});
	TestFalse(TEXT("Destroyed provider cannot supply depth"), Sample.bValid);
	TestTrue(TEXT("Provider loss is explicit"), Sample.InvalidReason == ETRSampleError::MissingProvider);
	Fixture.Ocean()->ShutdownArea();
	Fixture.Ocean()->ShutdownArea();
	TestTrue(TEXT("Repeated shutdown is safe"), Fixture.Ocean()->GetOceanState() == ETROceanState::Unloaded);
	TestTrue(TEXT("Area reference cleared"), Fixture.Ocean()->GetAreaId().IsNone());
	TestFalse(TEXT("Shutdown sample invalid"), Fixture.Ocean()->SampleOcean({}).bValid);
	Fixture.Provider = Fixture.World->SpawnActor<ATRSeabedProviderActor>();
	Fixture.Area->Settings.AreaId = FName(TEXT("TestOceanNext"));
	Fixture.Area->Settings.FlatDepthM = 40.0f;
	if (!Fixture.Start(*this)) { return false; }
	TestEqual(TEXT("New environment replaces old depth"), Fixture.Ocean()->SampleOcean({}).BottomDepthM, 40.0f);
	TestTrue(TEXT("New area identity"), Fixture.Ocean()->SampleOcean({}).AreaId == FName(TEXT("TestOceanNext")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTROceanUnitsTest, "TipRun.M03.O06UnitsAndCurrent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTROceanUnitsTest::RunTest(const FString& Parameters)
{
	FTRTestOcean Fixture;
	if (!Fixture.Start(*this)) { return false; }
	FTROceanQuery Query;
	Query.PositionXYM = TRUnits::CentimetersToMeters(FVector2D(10000.0, 5000.0));
	const FTROceanSample Sample = Fixture.Ocean()->SampleOcean(Query);
	TestTrue(TEXT("cm converted exactly once reaches boundary"), Sample.bValid);
	TestEqual(TEXT("100 cm is 1 m"), TRUnits::CentimetersToMeters(100.0), 1.0);
	TestEqual(TEXT("Positive depth places seabed below surface"), Sample.SurfaceZ_M - Sample.BottomDepthM, -27.5f);
	Query.PositionXYM = FVector2D(10000.0, 5000.0);
	TestFalse(TEXT("No implicit second conversion in Ocean"), Fixture.Ocean()->SampleOcean(Query).bValid);
	for (double Vertical : { 0.001, -0.001 })
	{
		TArray<FText> Errors;
		Fixture.Area->Settings.CurrentMps.Z = Vertical;
		TestFalse(TEXT("Nonzero vertical current rejected"), Fixture.Ocean()->InitializeArea(Fixture.Area.Get(), Fixture.Provider.Get(), Errors));
		TestFalse(TEXT("Failed initialization cannot use previous environment"), Fixture.Ocean()->SampleOcean({}).bValid);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTROceanValidationTest, "TipRun.M03.ValidationAndWorldScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTROceanValidationTest::RunTest(const FString& Parameters)
{
	FTRTestOcean Fixture;
	TArray<FText> Errors;
	const FTROceanAreaSettings Good = Fixture.Area->Settings;
	TestTrue(TEXT("Valid test configuration"), Good.Validate(Errors));
	TestFalse(TEXT("Unconfigured data rejected"), FTROceanAreaSettings().Validate(Errors));
	for (float Depth : { 0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
	{
		Fixture.Area->Settings = Good;
		Fixture.Area->Settings.FlatDepthM = Depth;
		TestFalse(TEXT("Invalid seabed rejected"), Fixture.Area->Validate(Errors));
	}
	Fixture.Area->Settings = Good; Fixture.Area->Settings.AreaId = NAME_None;
	TestFalse(TEXT("Missing area ID rejected"), Fixture.Area->Validate(Errors));
	Fixture.Area->Settings = Good; Fixture.Area->Settings.BoundsMinXYM = Good.BoundsMaxXYM;
	TestFalse(TEXT("Empty area rejected"), Fixture.Area->Validate(Errors));
	Fixture.Area->Settings = Good; Fixture.Area->Settings.BoundsMaxXYM.X = -200.0;
	TestFalse(TEXT("Inverted bounds rejected"), Fixture.Area->Validate(Errors));
	Fixture.Area->Settings = Good; Fixture.Area->Settings.BoundsMinXYM.Y = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("NaN bounds rejected"), Fixture.Area->Validate(Errors));
	Fixture.Area->Settings = Good; Fixture.Area->Settings.CurrentMps.X = std::numeric_limits<double>::infinity();
	TestFalse(TEXT("Infinite current rejected"), Fixture.Area->Validate(Errors));
	Fixture.Area->Settings = Good; Fixture.Area->Settings.SurfaceZ_M = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN surface rejected"), Fixture.Area->Validate(Errors));
	Fixture.Area->Settings = Good; Fixture.Area->Settings.SeabedMode = static_cast<ETRSeabedMode>(255);
	TestFalse(TEXT("Unsupported mode rejected"), Fixture.Area->Validate(Errors));
	FDataValidationContext BadContext;
	TestTrue(TEXT("Editor validation rejects invalid asset"), Fixture.Area->IsDataValid(BadContext) == EDataValidationResult::Invalid);
	Fixture.Area->Settings = Good;
	FDataValidationContext GoodContext;
	TestTrue(TEXT("Editor validation accepts valid asset"), Fixture.Area->IsDataValid(GoodContext) == EDataValidationResult::Valid);
	TStrongObjectPtr<UWorld> ScopeWorld(NewObject<UWorld>());
	const UTROceanWorldSubsystem* DefaultOcean = GetDefault<UTROceanWorldSubsystem>();
	for (EWorldType::Type Type : { EWorldType::Editor, EWorldType::EditorPreview, EWorldType::GamePreview, EWorldType::Inactive })
	{
		ScopeWorld->WorldType = Type;
		TestFalse(TEXT("No Ocean in editor/preview/inactive worlds"), DefaultOcean->ShouldCreateSubsystem(ScopeWorld.Get()));
	}
	for (EWorldType::Type Type : { EWorldType::Game, EWorldType::PIE })
	{
		ScopeWorld->WorldType = Type;
		TestTrue(TEXT("Ocean supports Game/PIE"), DefaultOcean->ShouldCreateSubsystem(ScopeWorld.Get()));
	}
	if (!Fixture.Start(*this)) { return false; }
	// A provider from a separate real world cannot be bound to this subsystem.
	FTRTestOcean Other;
	TestFalse(TEXT("Cross-world provider rejected"), Fixture.Ocean()->InitializeArea(Fixture.Area.Get(), Other.Provider.Get(), Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTROceanSnapshotTest, "TipRun.M03.SnapshotAndTimeIndependence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTROceanSnapshotTest::RunTest(const FString& Parameters)
{
	FTRTestOcean Fixture;
	if (!Fixture.Start(*this)) { return false; }
	FTROceanQuery Query; Query.SimTick = 50;
	const FTROceanSample Before = Fixture.Ocean()->SampleOcean(Query);
	Fixture.Area->Settings.FlatDepthM = 70.0f;
	Fixture.Area->Settings.SurfaceZ_M = 9.0f;
	Fixture.Area->Settings.CurrentMps = FVector(3.0, 2.0, 0.0);
	Fixture.Area->Settings.BoundsMaxXYM = FVector2D(-1, -1);
	TestTrue(TEXT("External simulation time accepted"), Fixture.Ocean()->SetSimulationTime(100.0));
	TestFalse(TEXT("Negative time rejected"), Fixture.Ocean()->SetSimulationTime(-1.0));
	TestFalse(TEXT("Nonfinite time rejected"), Fixture.Ocean()->SetSimulationTime(std::numeric_limits<double>::infinity()));
	TestEqual(TEXT("Failed time update preserves time"), Fixture.Ocean()->GetEnvironmentTimeS(), 100.0);
	const FTROceanSample After = Fixture.Ocean()->SampleOcean(Query);
	TestTrue(TEXT("Asset edits cannot change active bounds"), After.bValid);
	TestEqual(TEXT("Depth snapshot retained"), After.BottomDepthM, Before.BottomDepthM);
	TestEqual(TEXT("Surface snapshot retained"), After.SurfaceZ_M, Before.SurfaceZ_M);
	TestTrue(TEXT("Current snapshot retained"), After.CurrentMps.Equals(Before.CurrentMps));
	TestTrue(TEXT("No MVP wind"), After.WindMps.IsZero());
	TestTrue(TEXT("No MVP season"), After.SeasonId.IsNone());
	Query.SimTick = 1000; Query.DepthM = 100.0f;
	const FTROceanSample Later = Fixture.Ocean()->SampleOceanForDisplay(Query);
	TestTrue(TEXT("Time/depth do not change current"), Later.CurrentMps.Equals(Before.CurrentMps));
	TestEqual(TEXT("Display wrapper preserves tick"), Later.SampleTick, int64(1000));
	Fixture.Area->Settings.BoundsMaxXYM = FVector2D(100.0, 50.0);
	TArray<FText> Errors;
	TestTrue(TEXT("Explicit provider reconfiguration"), Fixture.Provider->InitializeFromArea(*Fixture.Area, Errors));
	TestFalse(TEXT("Old registration cannot mix a new provider with old area settings"), Fixture.Ocean()->SampleOcean(Query).bValid);
	return true;
}

#endif
