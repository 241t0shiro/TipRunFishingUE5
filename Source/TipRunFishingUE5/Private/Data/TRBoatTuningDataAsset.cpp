#include "Data/TRBoatTuningDataAsset.h"
#include "Data/TRSimulationTypes.h"
#include "Misc/DataValidation.h"

bool FTRBoatParameters::Validate(TArray<FText>& Errors) const
{
	if (!FMath::IsFinite(MaxDriftSpeedMps) || MaxDriftSpeedMps <= 0.0 ||
		!FMath::IsFinite(TRUnits::MetersToCentimeters(HullHeightOffsetM)) ||
		TRUnits::MetersToCentimeters(RodAnchorOffsetM).ContainsNaN())
	{
		Errors.Add(FText::FromString(TEXT("Boat tuning requires response in [0,1], positive finite response rate/speed, and finite hull/rod offsets representable in cm")));
		return false;
	}
	if (ModelRevision == 1)
	{
		if (!FMath::IsFinite(CurrentResponse01) || CurrentResponse01 < 0 || CurrentResponse01 > 1 ||
			!FMath::IsFinite(VelocityResponsePerS) || VelocityResponsePerS <= 0 ||
			WindResponseKgPerS != 0 || CurrentResponseKgPerS != 0 || DragKgPerS != 0 || InertiaKg != 0 ||
			BowWindScale != 0 || SternWindScale != 0 || SideWindScale != 0)
		{
			Errors.Add(FText::FromString(TEXT("Legacy boat requires valid M05 coefficients and no revision 2 coefficients")));
			return false;
		}
	}
	else if (ModelRevision == 2)
	{
		const auto Nonnegative = [](double V) { return FMath::IsFinite(V) && V >= 0; };
		if (!Nonnegative(WindResponseKgPerS) || !Nonnegative(CurrentResponseKgPerS) ||
			!Nonnegative(DragKgPerS) || !FMath::IsFinite(InertiaKg) || InertiaKg <= 0 ||
			!Nonnegative(BowWindScale) || !Nonnegative(SternWindScale) || !Nonnegative(SideWindScale) ||
			CurrentResponse01 != 0 || VelocityResponsePerS != 0)
		{
			Errors.Add(FText::FromString(TEXT("Revision 2 boat requires finite nonnegative responses/scales, positive inertia and no legacy coefficients")));
			return false;
		}
		for (double Scale : {BowWindScale, SternWindScale, SideWindScale})
		{
			const double K = WindResponseKgPerS * Scale + CurrentResponseKgPerS + DragKgPerS;
			if (!FMath::IsFinite(K) || K <= 0 || !FMath::IsFinite(K / InertiaKg) ||
				K / InertiaKg <= 0 || !FMath::IsFinite(InertiaKg / K))
			{
				Errors.Add(FText::FromString(TEXT("Boat damping and response times must be finite and positive on all body axes")));
				return false;
			}
		}
	}
	else { Errors.Add(FText::FromString(TEXT("Unknown boat model revision"))); return false; }
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
