#include "Data/TRRodTuningDataAsset.h"
#include "Misc/DataValidation.h"

bool FTRRodParameters::Validate(TArray<FText>& Errors) const
{
	const double Values[]={MinPitchRad,MaxPitchRad,MinYawRad,MaxYawRad,InitialPitchRad,InitialYawRad,
		SensitivityXRad,SensitivityYRad,MaxMouseDelta,MaxAimRateRadPerS,LengthM,ShakuriAmplitudeRad,ShakuriUpSeconds,ShakuriReturnSeconds,ShakuriReelSpeedMps,ShakuriReelSeconds};
	bool Valid=!MountOffsetM.ContainsNaN();
	Valid &= (ShakuriReelSpeedMps==0 && ShakuriReelSeconds==0) ||
		(ShakuriReelSpeedMps>0 && ShakuriReelSpeedMps<=MAX_flt && ShakuriReelSeconds>0 && ShakuriReelSeconds<=ShakuriReturnSeconds && FMath::IsFinite(ShakuriReelSpeedMps*ShakuriReelSeconds));
	for(double Value:Values){Valid &= FMath::IsFinite(Value);}
	Valid &= MinPitchRad>=-UE_DOUBLE_PI/2 && MaxPitchRad<=UE_DOUBLE_PI/2 && MinPitchRad<MaxPitchRad &&
		MinYawRad>=-UE_DOUBLE_PI && MaxYawRad<=UE_DOUBLE_PI && MinYawRad<MaxYawRad &&
		InitialPitchRad>=MinPitchRad && InitialPitchRad<=MaxPitchRad && InitialYawRad>=MinYawRad && InitialYawRad<=MaxYawRad &&
		SensitivityXRad>0 && SensitivityYRad>0 && MaxMouseDelta>0 && MaxAimRateRadPerS>0 && LengthM>0 &&
		ShakuriAmplitudeRad>0 && ShakuriAmplitudeRad<=MaxPitchRad-MinPitchRad && ShakuriUpSeconds>0 && ShakuriReturnSeconds>0 &&
		FMath::IsFinite(LengthM*100) && FMath::IsFinite(MountOffsetM.SizeSquared()*10000) &&
		FMath::IsFinite(SensitivityXRad*MaxMouseDelta) && FMath::IsFinite(SensitivityYRad*MaxMouseDelta);
	if(!Valid){Errors.Add(FText::FromString(TEXT("Rod tuning: finite ordered angle limits, in-range base, positive length/sensitivity/rate/profile and safe SI transform required")));}
	return Valid;
}
#if WITH_EDITOR
EDataValidationResult UTRRodTuningDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors; const bool Valid=Parameters.Validate(Errors);
	for(const auto& E:Errors){Context.AddError(E);} return Valid?EDataValidationResult::Valid:EDataValidationResult::Invalid;
}
#endif
