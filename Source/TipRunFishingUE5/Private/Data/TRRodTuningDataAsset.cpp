#include "Data/TRRodTuningDataAsset.h"
#include "Misc/DataValidation.h"

bool FTRRodScreenParameters::Validate() const
{
	return !Min.ContainsNaN() && !Max.ContainsNaN() && !Initial.ContainsNaN() && !Sensitivity.ContainsNaN() &&
		Min.X>=-.9 && Max.X<=.9 && Min.Y>=-.5 && Max.Y<=.5 && Min.X<Max.X && Min.Y<Max.Y &&
		Initial.X>=Min.X && Initial.X<=Max.X && Initial.Y>=Min.Y && Initial.Y<=Max.Y &&
		Sensitivity.X>0 && Sensitivity.X<=1 && Sensitivity.Y>0 && Sensitivity.Y<=1 &&
		FMath::IsFinite(MaxRatePerS) && MaxRatePerS>0 && MaxRatePerS<=10 &&
		FMath::IsFinite(MaxProjectedY) && MaxProjectedY>Max.Y && MaxProjectedY<=.9 &&
		FMath::IsFinite(SurfaceClearanceM) && SurfaceClearanceM>=0 && SurfaceClearanceM<=.5;
}

bool FTRRodEnvelopeParameters::Validate() const
{
 for(double V:{MinYawDeg,MaxYawDeg,MinPitchDeg,MaxPitchDeg}){if(!FMath::IsFinite(V)){return false;}}
 return MinYawDeg>-85 && MaxYawDeg<85 && MinYawDeg<=0 && MaxYawDeg>=0 && MinYawDeg<MaxYawDeg &&
  MinPitchDeg>-60 && MaxPitchDeg<75 && MinPitchDeg<MaxPitchDeg;
}

bool FTRRodParameters::Validate(TArray<FText>& Errors) const
{
	const double Values[]={MinPitchRad,MaxPitchRad,MinYawRad,MaxYawRad,InitialPitchRad,InitialYawRad,
		SensitivityXRad,SensitivityYRad,MaxMouseDelta,MaxAimRateRadPerS,LengthM,ShakuriAmplitudeRad,ShakuriUpSeconds,ShakuriReturnSeconds,ShakuriReelSpeedMps,ShakuriReelSeconds};
	bool Valid=!MountOffsetM.ContainsNaN() && Screen.Validate() && Envelope.Validate();
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
