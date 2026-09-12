#include "Data/TREvents.h"
#include "Data/TRSimulationTypes.h"
#include "Data/TRSnapshots.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include <limits>
#include <type_traits>

static_assert(!std::is_convertible_v<FTRCastId, FTRActorSimId>);
static_assert(std::is_same_v<decltype(FTREgiSnapshot::DepthM), float>);
static_assert(std::is_same_v<decltype(FTREgiSnapshot::StayPenaltyJerkCount), int64>);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRUnitsTest, "TipRun.M01.Units",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRUnitsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("100 cm is 1 m"), TRUnits::CentimetersToMeters(100.0), 1.0);
	TestEqual(TEXT("1 m is 100 cm"), TRUnits::MetersToCentimeters(1.0), 100.0);
	TestEqual(TEXT("Zero conversion"), TRUnits::MetersToCentimeters(0.0), 0.0);
	const FVector PositionCm(125.0, -250.0, 3000.0);
	const FVector PositionM = TRUnits::CentimetersToMeters(PositionCm);
	TestTrue(TEXT("Conversion preserves axes and signs"), PositionM.Equals(FVector(1.25, -2.5, 30.0), 1.e-10));
	TestTrue(TEXT("3D round trip"), TRUnits::MetersToCentimeters(PositionM).Equals(PositionCm, 1.e-10));
	const FVector2D HorizontalCm(-375.0, 250.0);
	TestTrue(TEXT("2D round trip"),
		TRUnits::MetersToCentimeters(TRUnits::CentimetersToMeters(HorizontalCm)).Equals(HorizontalCm, 1.e-10));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRIdentifiersTest, "TipRun.M01.Identifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIdentifiersTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Default cast is invalid"), FTRCastId().IsValid());
	TestFalse(TEXT("Default actor ID is invalid"), FTRActorSimId().IsValid());
	TestFalse(TEXT("Default token is invalid"), FTRBiteToken().IsValid());
	TestFalse(TEXT("Negative cast is invalid"), FTRCastId(-1).IsValid());
	TestFalse(TEXT("Negative actor ID is invalid"), FTRActorSimId(-1).IsValid());
	TestTrue(TEXT("First cast is valid"), FTRCastId(1).IsValid());
	TestTrue(TEXT("First actor ID is valid"), FTRActorSimId(1).IsValid());
	TestTrue(TEXT("Full int64 ID range"), FTRCastId(MAX_int64).IsValid());

	FTRBiteToken Token;
	Token.CastId = FTRCastId(7);
	TestFalse(TEXT("Zero sequence is invalid"), Token.IsValid());
	Token.Sequence = -1;
	TestFalse(TEXT("Negative sequence is invalid"), Token.IsValid());
	Token.Sequence = 1;
	TestTrue(TEXT("Positive cast and sequence are valid"), Token.IsValid());
	FTRBiteToken NextCast = Token;
	NextCast.CastId = FTRCastId(8);
	FTRBiteToken NextBite = Token;
	NextBite.Sequence = 2;
	TestTrue(TEXT("Cast participates in identity"), Token != NextCast);
	TestTrue(TEXT("Sequence participates in identity"), Token != NextBite);
	TSet<FTRBiteToken> Tokens;
	Tokens.Add(Token);
	Tokens.Add(Token);
	Tokens.Add(NextCast);
	Tokens.Add(NextBite);
	TestEqual(TEXT("Hash/equality preserve distinct casts and bites"), Tokens.Num(), 3);
	Token.CastId = FTRCastId();
	TestFalse(TEXT("Sequence alone cannot make a valid token"), Token.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRTimeConversionTest, "TipRun.M01.TimeConversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRTimeConversionTest::RunTest(const FString& Parameters)
{
	const double StepSeconds = 1.0 / 60.0;
	int64 Ticks = -1;
	const auto Check = [this, &Ticks, StepSeconds](double Seconds, int64 Expected)
	{
		TestTrue(TEXT("Valid duration accepted"), TRTime::TrySecondsToTicks(Seconds, StepSeconds, Ticks));
		TestEqual(TEXT("Expected tick offset"), Ticks, Expected);
	};
	Check(0.0, 0);
	Check(0.10, 6);
	Check(0.55, 33);
	Check(0.8, 48);
	Check(1.5 * StepSeconds, 2);
	Check((33.0 + 0.5e-6) * StepSeconds, 33);
	Check((33.0 - 0.5e-6) * StepSeconds, 33);
	Check((33.0 + 2.e-6) * StepSeconds, 34);
	TestTrue(TEXT("Different fixed step"), TRTime::TrySecondsToTicks(0.55, 1.0 / 30.0, Ticks));
	TestEqual(TEXT("30 Hz offset"), Ticks, int64(17));

	const auto Reject = [this, &Ticks](double Seconds, double Step)
	{
		Ticks = 123;
		TestFalse(TEXT("Invalid or overflowing duration rejected"), TRTime::TrySecondsToTicks(Seconds, Step, Ticks));
		TestEqual(TEXT("Failed conversion does not supply a usable offset"), Ticks, int64(123));
	};
	const double Infinity = std::numeric_limits<double>::infinity();
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	Reject(-1.0, StepSeconds);
	Reject(1.0, 0.0);
	Reject(1.0, -StepSeconds);
	Reject(NaN, StepSeconds);
	Reject(Infinity, StepSeconds);
	Reject(1.0, NaN);
	Reject(1.0, Infinity);
	Reject(9223372036854775808.0, 1.0);
	Reject(std::numeric_limits<double>::max(), std::numeric_limits<double>::min());
	FTRSimTime Time;
	TestFalse(TEXT("Default clock is unconfigured"), Time.IsValid());
	Time.StepSeconds = StepSeconds;
	TestTrue(TEXT("Tick zero is valid for configured clock"), Time.IsValid());
	Time.TickIndex = -1;
	TestFalse(TEXT("Negative simulation tick is invalid"), Time.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRReflectionTest, "TipRun.M01.Reflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRReflectionTest::RunTest(const FString& Parameters)
{
	UScriptStruct* Types[] = {
		FTRCastId::StaticStruct(), FTRActorSimId::StaticStruct(), FTRBiteToken::StaticStruct(),
		FTRSimTime::StaticStruct(), FTROceanQuery::StaticStruct(), FTROceanSample::StaticStruct(),
		FTRBoatSnapshot::StaticStruct(), FTREgiSnapshot::StaticStruct(), FTRSquidSnapshot::StaticStruct(),
		FTRFishingCommand::StaticStruct(), FTREgiAction::StaticStruct(), FTRBiteRequest::StaticStruct(),
		FTRBiteCue::StaticStruct(), FTRCatchResult::StaticStruct()
	};
	for (UScriptStruct* Type : Types)
	{
		TestNotNull(TEXT("UHT registered value type"), Type);
		for (TFieldIterator<FProperty> It(Type); It; ++It)
		{
			const FString Field = Type->GetName() + TEXT(".") + It->GetName();
			TestTrue(Field + TEXT(" is Blueprint read-only"), It->HasAllPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly));
			TestFalse(Field + TEXT(" carries no UObject reference"), It->IsA<FObjectPropertyBase>());
		}
	}
	TestEqual(TEXT("Bite is not a Fishing state"),
		StaticEnum<ETRFishingState>()->GetValueByNameString(TEXT("Bite")), int64(INDEX_NONE));
	TestEqual(TEXT("Hit is not a Fishing state"),
		StaticEnum<ETRFishingState>()->GetValueByNameString(TEXT("Hit")), int64(INDEX_NONE));
	const FTROceanSample Ocean;
	TestFalse(TEXT("Uninitialized ocean is explicitly invalid"), Ocean.bValid);
	TestTrue(TEXT("Uninitialized ocean includes a reason"), Ocean.InvalidReason == ETRSampleError::NotInitialized);
	TestFalse(TEXT("Default snapshot cannot identify an active cast"), FTREgiSnapshot().CastId.IsValid());
	TestTrue(TEXT("Default result is not a catch"), FTRCatchResult().Outcome != ETRCastOutcome::Caught);
	return true;
}

#endif
