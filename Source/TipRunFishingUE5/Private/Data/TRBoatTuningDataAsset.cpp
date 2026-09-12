#include "Data/TRBoatTuningDataAsset.h"
#include "Data/TRSimulationTypes.h"
#include "Misc/DataValidation.h"

bool FTRBoatParameters::Validate(TArray<FText>& Errors) const
{
	if (!FMath::IsFinite(CurrentResponse01) || CurrentResponse01 < 0.0 || CurrentResponse01 > 1.0 ||
		!FMath::IsFinite(VelocityResponsePerS) || VelocityResponsePerS <= 0.0 ||
		!FMath::IsFinite(MaxDriftSpeedMps) || MaxDriftSpeedMps <= 0.0 ||
		!FMath::IsFinite(TRUnits::MetersToCentimeters(HullHeightOffsetM)) ||
		TRUnits::MetersToCentimeters(RodAnchorOffsetM).ContainsNaN())
	{
		Errors.Add(FText::FromString(TEXT("Boat tuning requires response in [0,1], positive finite response rate/speed, and finite hull/rod offsets representable in cm")));
		return false;
	}
	return true;
}
#if WITH_EDITOR
EDataValidationResult UTRBoatTuningDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	const bool bValid = Validate(Errors);
	for (const FText& Error : Errors) { Context.AddError(Error); }
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
