#pragma once
#include "CoreMinimal.h"

namespace TREnvironmentUnits
{
	// International nautical mile, SI seconds. Signed components are supported.
	inline constexpr double MetersPerSecondPerKnot = 1852.0 / 3600.0;
	inline bool TryKnotsToMps(double Knots, double& OutMps)
	{
		const double Value = Knots * MetersPerSecondPerKnot;
		if (!FMath::IsFinite(Knots) || !FMath::IsFinite(Value)) { return false; }
		OutMps = Value; return true;
	}
	inline bool TryMpsToKnots(double Mps, double& OutKnots)
	{
		const double Value = Mps / MetersPerSecondPerKnot;
		if (!FMath::IsFinite(Mps) || !FMath::IsFinite(Value)) { return false; }
		OutKnots = Value; return true;
	}
}
