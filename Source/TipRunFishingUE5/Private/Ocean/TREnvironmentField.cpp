#include "Ocean/TREnvironmentField.h"

FVector2D FTREnvironmentField::WindAtLocation(const FTROceanAreaSettings& Settings,
	const FVector2D& PositionXYM, int64 SimTick) const
{
	return Settings.WindMps;
}
FVector FTREnvironmentField::CurrentAtLocationAndDepth(const FTROceanAreaSettings& Settings,
	const FTROceanQuery& Query) const
{
	if (Settings.CurrentMode == ETRCurrentFieldMode::Constant) { return Settings.CurrentMps; }
	const auto& Keys = Settings.CurrentDepthProfile;
	// Settings have already been validated and frozen by Ocean.
	if (Keys.IsEmpty()) { return FVector::ZeroVector; }
	if (Query.DepthM <= Keys[0].DepthM) { return Keys[0].CurrentMps; }
	for (int32 I = 1; I < Keys.Num(); ++I)
	{
		if (Query.DepthM <= Keys[I].DepthM)
		{
			const double Alpha = (double(Query.DepthM) - Keys[I-1].DepthM) /
				(double(Keys[I].DepthM) - Keys[I-1].DepthM);
			return Keys[I-1].CurrentMps * (1.0 - Alpha) + Keys[I].CurrentMps * Alpha;
		}
	}
	return Keys.Last().CurrentMps;
}
