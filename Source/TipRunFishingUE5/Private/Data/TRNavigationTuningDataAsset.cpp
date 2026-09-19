#include "Data/TRNavigationTuningDataAsset.h"
#include "Misc/DataValidation.h"
bool FTRNavigationParameters::Validate() const
{
 for(double V : {EngineForceN,ReverseScale,EngineResponsePerS,SteeringRateRadPerS,SteeringResponsePerS,MaxSpeedMps,
 CameraDistanceM,CameraHeightM,LookSensitivityDeg,CameraMinDistanceM,CameraMaxDistanceM,ZoomMetersPerUnit})
 {if(!FMath::IsFinite(V)||V<=0||V>10000){return false;}}
 return FMath::IsFinite(BoostThrustMultiplier) && BoostThrustMultiplier>=1 && BoostThrustMultiplier<=4 &&
 FMath::IsFinite(BoostResponsePerS) && BoostResponsePerS>0 && BoostResponsePerS<=20 &&
 FMath::IsFinite(BoostMaxSpeedMps) && BoostMaxSpeedMps>=0 && BoostMaxSpeedMps<=100 &&
 (BoostThrustMultiplier==1 || BoostMaxSpeedMps>=MaxSpeedMps) &&
 FMath::IsFinite(NavigationLateralResponsePerS) && NavigationLateralResponsePerS>=0 && NavigationLateralResponsePerS<=20 &&
 ReverseScale<=1 && SteeringRateRadPerS<=UE_DOUBLE_PI && MaxSpeedMps<=100 &&
 FMath::IsFinite(CameraMinPitchDeg)&&FMath::IsFinite(CameraMaxPitchDeg)&&FMath::IsFinite(CameraInitialPitchDeg) &&
 CameraMinPitchDeg>=-80 && CameraMaxPitchDeg<=-5 && CameraMinPitchDeg<CameraMaxPitchDeg &&
 CameraInitialPitchDeg>=CameraMinPitchDeg && CameraInitialPitchDeg<=CameraMaxPitchDeg &&
 CameraMinDistanceM<=CameraDistanceM && CameraDistanceM<=CameraMaxDistanceM;
}
FTRNavigationParameters FTRNavigationParameters::Prototype()
{
 FTRNavigationParameters P;
 P.EngineForceN=20;P.ReverseScale=.6;P.EngineResponsePerS=4;P.SteeringRateRadPerS=1;P.SteeringResponsePerS=6;P.MaxSpeedMps=14;
 P.NavigationLateralResponsePerS=1.5;
 P.BoostThrustMultiplier=2;P.BoostMaxSpeedMps=24;P.BoostResponsePerS=2;
 P.CameraDistanceM=14;P.CameraHeightM=1;P.CameraMinPitchDeg=-65;P.CameraMaxPitchDeg=-10;P.CameraInitialPitchDeg=-25;
 P.LookSensitivityDeg=.25;P.CameraMinDistanceM=6;P.CameraMaxDistanceM=35;P.ZoomMetersPerUnit=1;
 return P; // Explicit opt-in game approximation, not production or measured boat data.
}
#if WITH_EDITOR
EDataValidationResult UTRNavigationTuningDataAsset::IsDataValid(FDataValidationContext& Context) const
{
 if(!Parameters.Validate()){Context.AddError(FText::FromString(TEXT("Invalid navigation/camera parameters")));return EDataValidationResult::Invalid;}
 return Super::IsDataValid(Context)==EDataValidationResult::Invalid?EDataValidationResult::Invalid:EDataValidationResult::Valid;
}
#endif
