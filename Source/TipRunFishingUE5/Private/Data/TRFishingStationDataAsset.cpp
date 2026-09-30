#include "Data/TRFishingStationDataAsset.h"
#include "Misc/DataValidation.h"
const FTRFishingStation* FTRFishingStationParameters::Find(ETRFishingSide Side) const
{return Stations.FindByPredicate([Side](const auto& S){return S.Side==Side;});}
bool FTRFishingStationParameters::Validate() const
{
 if(Stations.Num()!=2 || !Find(ETRFishingSide::Port) || !Find(ETRFishingSide::Starboard)){return false;}
 for(double V:{MinPitchDeg,MaxPitchDeg,InitialPitchDeg,MinYawDeg,MaxYawDeg,FishingCameraYawRateDegPerS,FishingCameraPitchRateDegPerS,SensitivityDeg,FOV}){if(!FMath::IsFinite(V)){return false;}}
 if(MinPitchDeg<-80 || MaxPitchDeg>30 || MinPitchDeg>=MaxPitchDeg || InitialPitchDeg<MinPitchDeg || InitialPitchDeg>MaxPitchDeg || MinYawDeg < -80 || MinYawDeg>0 || MaxYawDeg<=0 || MaxYawDeg>80 || MinYawDeg>=MaxYawDeg || FishingCameraYawRateDegPerS<=0 || FishingCameraYawRateDegPerS>180 || FishingCameraPitchRateDegPerS<=0 || FishingCameraPitchRateDegPerS>180 || SensitivityDeg<=0 || SensitivityDeg>2 || FOV<40 || FOV>110){return false;}
 for(const auto& S:Stations)
 {
  const double Sign=S.Side==ETRFishingSide::Port?-1:1;
  for(const FVector V:{S.PlayerM,S.EyeM,S.CameraM,S.RodMountM}){if(V.ContainsNaN() || V.Size()>20 || V.Y*Sign<=0){return false;}}
  if(!FMath::IsFinite(S.FacingDeg) || FMath::Abs(S.FacingDeg-Sign*90)>30 || S.CameraM.Z<.5 || S.RodMountM.Z<.3){return false;}
 }
 return true;
}
FTRFishingStationParameters FTRFishingStationParameters::Prototype()
{
 FTRFishingStationParameters P;
 for(auto Side:{ETRFishingSide::Port,ETRFishingSide::Starboard})
 {const double S=Side==ETRFishingSide::Port?-1:1;FTRFishingStation Row;Row.Side=Side;Row.PlayerM=FVector(.6,S*.6,.3);Row.EyeM=Row.CameraM=FVector(.6,S*.4,1.6);Row.RodMountM=FVector(.6,S*1.,1.1);Row.FacingDeg=S*90;P.Stations.Add(Row);}
 return P;
}
#if WITH_EDITOR
EDataValidationResult UTRFishingStationDataAsset::IsDataValid(FDataValidationContext& Context) const
{if(!Parameters.Validate()){Context.AddError(FText::FromString(TEXT("Invalid port/starboard anchors or camera settings")));return EDataValidationResult::Invalid;}return EDataValidationResult::Valid;}
#endif
