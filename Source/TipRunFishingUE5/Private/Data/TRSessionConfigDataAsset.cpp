#include "Data/TRSessionConfigDataAsset.h"
#include "Data/TRNavigationTuningDataAsset.h"
#include "Misc/DataValidation.h"
#include <limits>
#include "Data/TROceanAreaDataAsset.h"
#include "Data/TRBoatTuningDataAsset.h"
#include "Data/TRInputConfigDataAsset.h"
#include "Data/TREquipmentData.h"
#include "Data/TRRodTuningDataAsset.h"

bool UTRSessionConfigDataAsset::ValidateStartup(TArray<FText>& Errors) const
{
	if (!Validate(Errors) || !IsValid(Ocean) || !IsValid(Boat) || !IsValid(Input) ||
		!FMath::IsFinite(InitialBoatXYM.X) || !FMath::IsFinite(InitialBoatXYM.Y) || !FMath::IsFinite(InitialHeadingRad))
	{
		Errors.Add(FText::FromString(TEXT("Startup requires valid clock, ocean, boat, input and finite initial transform")));
		return false;
	}
	FTREquipmentSnapshot Equipment;
	if(Navigation && (!IsValid(Navigation) || !Navigation->Parameters.Validate() || Boat->Parameters.ModelRevision!=2 || !Input->NavigationContext))
	{Errors.Add(FText::FromString(TEXT("Navigation requires explicit revision2 boat, tuning and Navigation input context")));return false;}
	if (Input->Bindings.ContainsByPredicate([](const FTRInputBinding& B) { return B.Command == ETRPlayerAction::QuickRetrieve; }) &&
		(!IsValid(Fishing) || Fishing->Parameters.QuickRetrieveDurationS <= 0.0))
	{
		Errors.Add(FText::FromString(TEXT("Mapped Quick Retrieve requires an explicit positive duration"))); return false;
	}
	if(Rod && (!IsValid(Rod) || !Rod->Parameters.Validate(Errors) || !IsValid(Fishing) || Fishing->Parameters.EgiModelRevision!=2 ||
		!Input->Bindings.ContainsByPredicate([](const FTRInputBinding& B){return B.Command==ETRPlayerAction::RodAim;})))
	{Errors.Add(FText::FromString(TEXT("Rod startup requires valid Rod tuning, C revision 2 and mapped RodAim input")));return false;}
	return Ocean->Validate(Errors) && Boat->Validate(Errors) && Input->Validate(Errors) &&
		TREquipment::TryBuildSnapshot(Egis, Sinkers, Fishing, TREquipment::InitialEgiId(), InitialSinkerId, StepSeconds, Equipment, Errors);
}

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
	// Clock-only assets remain valid for M04. Any startup reference opts into full validation.
	const bool bValid = (Ocean || Boat || Input || Fishing || Egis || Sinkers || Rod || Navigation) ? ValidateStartup(Errors) : Validate(Errors);
	for (const FText& Error : Errors) { Context.AddError(Error); }
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
