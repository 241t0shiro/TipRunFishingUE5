#include "Data/TRSessionConfigDataAsset.h"
#include "Misc/DataValidation.h"
#include <limits>

bool UTRSessionConfigDataAsset::Validate(TArray<FText>& Errors) const
{
	if (!FMath::IsFinite(StepSeconds) || StepSeconds <= 0.0 ||
		!FMath::IsFinite(1.0 / StepSeconds) || MaxCatchUpSteps <= 0 ||
		StepSeconds > std::numeric_limits<double>::max() / (double(MaxCatchUpSteps) + 1.0))
	{
		Errors.Add(FText::FromString(TEXT("Clock requires finite positive StepSeconds and positive MaxCatchUpSteps with a finite frame budget")));
		return false;
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UTRSessionConfigDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	const bool bValid = Validate(Errors);
	for (const FText& Error : Errors) { Context.AddError(Error); }
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
