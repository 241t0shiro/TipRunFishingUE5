#include "Data/TROceanAreaDataAsset.h"
#include "Misc/DataValidation.h"

bool FTROceanAreaSettings::Validate(TArray<FText>& Errors) const
{
	bool bValid = true;
	const auto Require = [&Errors, &bValid](bool Condition, const TCHAR* Message)
	{
		if (!Condition) { bValid = false; Errors.Add(FText::FromString(Message)); }
	};
	Require(!AreaId.IsNone(), TEXT("Ocean AreaId is missing"));
	Require(FMath::IsFinite(BoundsMinXYM.X) && FMath::IsFinite(BoundsMinXYM.Y) &&
		FMath::IsFinite(BoundsMaxXYM.X) && FMath::IsFinite(BoundsMaxXYM.Y) &&
		BoundsMinXYM.X < BoundsMaxXYM.X && BoundsMinXYM.Y < BoundsMaxXYM.Y,
		TEXT("Ocean bounds must be finite with min < max on both axes"));
	Require(FMath::IsFinite(SurfaceZ_M), TEXT("Ocean surface Z must be finite"));
	Require(FMath::IsFinite(FlatDepthM) && FlatDepthM > 0.0f &&
		FMath::IsFinite(SurfaceZ_M - FlatDepthM), TEXT("Ocean bottom depth/world Z must be finite and depth positive"));
	Require(SeabedMode == ETRSeabedMode::Flat, TEXT("M03 supports only Flat seabed"));
	Require(FMath::IsFinite(CurrentMps.X) && FMath::IsFinite(CurrentMps.Y) &&
		FMath::IsFinite(CurrentMps.Z) && CurrentMps.Z == 0.0,
		TEXT("MVP current must be finite and horizontal (Z = 0)"));
	return bValid;
}

bool FTROceanAreaSettings::Contains(const FVector2D& PositionXYM) const
{
	return FMath::IsFinite(PositionXYM.X) && FMath::IsFinite(PositionXYM.Y) &&
		PositionXYM.X >= BoundsMinXYM.X && PositionXYM.X <= BoundsMaxXYM.X &&
		PositionXYM.Y >= BoundsMinXYM.Y && PositionXYM.Y <= BoundsMaxXYM.Y;
}

#if WITH_EDITOR
EDataValidationResult UTROceanAreaDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	const bool bValid = Validate(Errors);
	for (const FText& Error : Errors) { Context.AddError(Error); }
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
