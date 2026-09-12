#pragma once

#include "CoreMinimal.h"
#include "TRSimulationTypes.generated.h"

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRSimTime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 TickIndex = 0;

	// Zero means unconfigured; M04 owns the clock and selects the fixed step.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	double StepSeconds = 0.0;

	bool IsValid() const
	{
		return TickIndex >= 0 && FMath::IsFinite(StepSeconds) && StepSeconds > 0.0;
	}
};

namespace TRUnits
{
	// Coordinate conversions preserve axes/sign. DepthM remains positive downward.
	constexpr double CentimetersPerMeter = 100.0;
	constexpr double MetersToCentimeters(double Meters) { return Meters * CentimetersPerMeter; }
	constexpr double CentimetersToMeters(double Centimeters) { return Centimeters / CentimetersPerMeter; }
	inline FVector MetersToCentimeters(const FVector& PositionM) { return PositionM * CentimetersPerMeter; }
	inline FVector CentimetersToMeters(const FVector& PositionCm) { return PositionCm / CentimetersPerMeter; }
	inline FVector2D MetersToCentimeters(const FVector2D& PositionM) { return PositionM * CentimetersPerMeter; }
	inline FVector2D CentimetersToMeters(const FVector2D& PositionCm) { return PositionCm / CentimetersPerMeter; }
}

namespace TRTime
{
	// GAME_DESIGN section 4: round within 1e-6 Tick of an integer, then ceil.
	// Returns false for invalid/overflowing input and leaves OutTicks unchanged.
	TIPRUNFISHINGUE5_API bool TrySecondsToTicks(double DurationS, double StepSeconds, int64& OutTicks);
}
