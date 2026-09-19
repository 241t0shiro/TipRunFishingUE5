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
 Out.Rotation=Rotation;Out.FOV=DefaultFOV;Out.bConstrainAspectRatio=false;
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
 Super::UpdateViewTarget(OutVT,DeltaTime); // R2 keeps the existing Fishing observer; no R3 camera.
}

