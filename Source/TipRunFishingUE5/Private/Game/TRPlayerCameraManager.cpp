#include "Game/TRPlayerCameraManager.h"
#include "Game/TRPlayerController.h"
#include "Game/TRFishingSessionActor.h"
#include "Data/TRSimulationTypes.h"
void ATRPlayerCameraManager::ConfigureNavigation(const FTRNavigationParameters& P,double HeadingRad)
{bConfigured=P.Validate();Settings=P;if(bConfigured){ResetNavigation(HeadingRad);}}
void ATRPlayerCameraManager::ResetNavigation(double HeadingRad)
{
 if(!bConfigured||!FMath::IsFinite(HeadingRad)){return;}
 State.YawDeg=FMath::RadiansToDegrees(HeadingRad);State.PitchDeg=Settings.CameraInitialPitchDeg;State.DistanceM=Settings.CameraDistanceM;
}
void ATRPlayerCameraManager::Look(FVector2D Delta)
{
 if(!bConfigured||Delta.ContainsNaN()){return;}
 State.YawDeg=FMath::UnwindDegrees(State.YawDeg+FMath::Clamp(Delta.X,-50.,50.)*Settings.LookSensitivityDeg);
 State.PitchDeg=FMath::Clamp(State.PitchDeg+FMath::Clamp(Delta.Y,-50.,50.)*Settings.LookSensitivityDeg,Settings.CameraMinPitchDeg,Settings.CameraMaxPitchDeg);
}
void ATRPlayerCameraManager::Zoom(double Delta)
{if(bConfigured&&FMath::IsFinite(Delta)){State.DistanceM=FMath::Clamp(State.DistanceM-FMath::Clamp(Delta,-10.,10.)*Settings.ZoomMetersPerUnit,Settings.CameraMinDistanceM,Settings.CameraMaxDistanceM);}}
bool ATRPlayerCameraManager::BuildNavigationView(const FVector& BoatPositionM,FMinimalViewInfo& Out) const
{
 if(!bConfigured||BoatPositionM.ContainsNaN()){return false;}
 const FRotator Rotation(State.PitchDeg,State.YawDeg,0);
 const FVector Target=BoatPositionM+FVector(0,0,Settings.CameraHeightM);
 Out.Location=TRUnits::MetersToCentimeters(Target-Rotation.Vector()*State.DistanceM);
 Out.Rotation=Rotation;Out.FOV=DefaultFOV;Out.bConstrainAspectRatio=false;Out.AspectRatioAxisConstraint.Reset();
 return !Out.Location.ContainsNaN();
}
void ATRPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT,float DeltaTime)
{
 State.bActive=false;
 const auto* PC=Cast<ATRPlayerController>(PCOwner);
 const auto* Session=PC?PC->GetBoundSession():nullptr;
 if(Session && Session->GetPlayerModeSnapshot().bValid && Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Navigation)
 {
  const auto Snapshot=Session->GetHUDSnapshot();
  if(Snapshot.bEnvironmentValid && BuildNavigationView(Snapshot.Boat.PositionM,OutVT.POV)){State.bActive=true;return;}
 }
 if(Session && BuildFishingView(Session->GetStationSnapshot(),OutVT.POV)){return;}
 Super::UpdateViewTarget(OutVT,DeltaTime); // Only legacy unconfigured fixtures retain the observer.
}


void ATRPlayerCameraManager::ConfigureFishing(const FTRFishingStationParameters& P)
{FishingSettings=P;bFishingConfigured=P.Validate();ResetFishingLook();}
void ATRPlayerCameraManager::ResetFishingLook(){FishingYaw=0;FishingPitch=FishingSettings.InitialPitchDeg;}
void ATRPlayerCameraManager::LookFishing(FVector2D Delta)
{
 if(!bFishingConfigured || Delta.ContainsNaN()){return;}
 FishingYaw=FMath::Clamp(FishingYaw+FMath::Clamp(Delta.X,-50.,50.)*FishingSettings.SensitivityDeg,FishingSettings.MinYawDeg,FishingSettings.MaxYawDeg);
 FishingPitch=FMath::Clamp(FishingPitch+FMath::Clamp(Delta.Y,-50.,50.)*FishingSettings.SensitivityDeg,FishingSettings.MinPitchDeg,FishingSettings.MaxPitchDeg);
}
FTRFishingCameraSnapshot ATRPlayerCameraManager::GetFishingSnapshot(const FTRFishingStationSnapshot& S) const
{
 FTRFishingCameraSnapshot Out;if(!bFishingConfigured || !S.bValid){return Out;}
 Out.bActive=true;Out.PositionM=S.CameraWorldM;Out.YawDeg=FishingYaw;Out.PitchDeg=FishingPitch;Out.Rotation=FRotator(FishingPitch,S.FacingWorldDeg+FishingYaw,0);return Out;
}
bool ATRPlayerCameraManager::BuildFishingView(const FTRFishingStationSnapshot& S,FMinimalViewInfo& Out) const
{
 const auto View=GetFishingSnapshot(S);if(!View.bActive){return false;}
 Out.Location=View.PositionM*100;Out.Rotation=View.Rotation;Out.FOV=FishingSettings.FOV;Out.bConstrainAspectRatio=false;Out.ProjectionMode=ECameraProjectionMode::Perspective;
 // Rod inverse projection uses horizontal FOV. Override LocalPlayer's optional Y-FOV
 // policy so the real viewport and fixed-step inverse agree at both test resolutions.
 Out.AspectRatioAxisConstraint=AspectRatio_MaintainXFOV;return true;
}

void ATRPlayerCameraManager::AdvanceFishingLook(FVector2D Axes,double DeltaSeconds)
{
 if(!bFishingConfigured || Axes.ContainsNaN() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds<=0){return;}
 const double Dt=FMath::Min(DeltaSeconds,.1);
 FishingYaw=FMath::Clamp(FishingYaw+FMath::Clamp(Axes.X,-1.,1.)*FishingSettings.FishingCameraYawRateDegPerS*Dt,FishingSettings.MinYawDeg,FishingSettings.MaxYawDeg);
 FishingPitch=FMath::Clamp(FishingPitch+FMath::Clamp(Axes.Y,-1.,1.)*FishingSettings.FishingCameraPitchRateDegPerS*Dt,FishingSettings.MinPitchDeg,FishingSettings.MaxPitchDeg);
}

void ATRPlayerCameraManager::UpdateCamera(float DeltaTime)
{
 Super::UpdateCamera(DeltaTime);DiagnosticCameraFrame=GFrameCounter;
 const auto* PC=Cast<ATRPlayerController>(PCOwner);
 if(const auto* Session=PC?PC->GetBoundSession():nullptr)
 {const auto Boat=Session->GetHUDSnapshot().Boat;ObservationBoatWorldM=Boat.PositionM;ObservationBoatHeadingRad=Boat.HeadingRad;}
}
