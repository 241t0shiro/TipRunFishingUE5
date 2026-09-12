#include "Data/TRSimulationTypes.h"

bool TRTime::TrySecondsToTicks(double DurationS, double StepSeconds, int64& OutTicks)
{
	if (!FMath::IsFinite(DurationS) || DurationS < 0.0 ||
		!FMath::IsFinite(StepSeconds) || StepSeconds <= 0.0)
	{
		return false;
	}

	const double Ratio = DurationS / StepSeconds;
	if (!FMath::IsFinite(Ratio))
	{
		return false;
	}

	const double NearestInteger = FMath::RoundToDouble(Ratio);
	const double RoundedTicks = FMath::Abs(Ratio - NearestInteger) <= 1.e-6
		? NearestInteger : FMath::CeilToDouble(Ratio);

	// MAX_int64 rounds to 2^63 as a double; use that exclusive upper bound.
	constexpr double Int64UpperExclusive = 9223372036854775808.0;
	if (RoundedTicks >= Int64UpperExclusive)
	{
		return false;
	}

	OutTicks = static_cast<int64>(RoundedTicks);
	return true;
}
