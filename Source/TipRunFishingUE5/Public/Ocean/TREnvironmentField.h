#pragma once
#include "Data/TROceanTypes.h"
#include "Data/TRSnapshots.h"

// Immutable, value-only native evaluator. No Actor/UObject references or time advancement.
// A future spatial field can override the two independent evaluations. Ocean validates outputs.
class TIPRUNFISHINGUE5_API FTREnvironmentField
{
public:
	virtual ~FTREnvironmentField() = default;
	virtual FVector2D WindAtLocation(const FTROceanAreaSettings& Settings,
		const FVector2D& PositionXYM, int64 SimTick) const;
	virtual FVector CurrentAtLocationAndDepth(const FTROceanAreaSettings& Settings,
		const FTROceanQuery& Query) const;
};
