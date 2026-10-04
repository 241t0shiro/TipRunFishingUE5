#pragma once
#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "Data/TRNavigationTuningDataAsset.h"
#include "Data/TRFishingStationDataAsset.h"
#include "TRPlayerCameraManager.generated.h"
UCLASS()
class TIPRUNFISHINGUE5_API ATRPlayerCameraManager : public APlayerCameraManager
{
 GENERATED_BODY()
public:
 uint64 DiagnosticCameraFrame=0;
 FVector ObservationBoatWorldM=FVector::ZeroVector;
 double ObservationBoatHeadingRad=0;
 virtual void UpdateCamera(float DeltaTime) override;
 void ConfigureFishing(const FTRFishingStationParameters& P);
 void ResetFishingLook();
 void LookFishing(FVector2D Delta);
 void AdvanceFishingLook(FVector2D Axes,double DeltaSeconds);
 FTRFishingCameraSnapshot GetFishingSnapshot(const FTRFishingStationSnapshot& S) const;
 bool BuildFishingView(const FTRFishingStationSnapshot& S,FMinimalViewInfo& Out) const;
 void ConfigureNavigation(const FTRNavigationParameters& P,double HeadingRad);
 void ClearNavigation(){bConfigured=false;State.bActive=false;bFishingConfigured=false;ResetFishingLook();}
 void Look(FVector2D Delta);
 void Zoom(double Delta);
 void ResetNavigation(double HeadingRad);
 FTRNavigationCameraSnapshot GetNavigationSnapshot() const{return State;}
 bool BuildNavigationView(const FVector& BoatPositionM,FMinimalViewInfo& Out) const;
protected:
 virtual void UpdateViewTarget(FTViewTarget& OutVT,float DeltaTime) override;
private:
 FTRFishingStationParameters FishingSettings;
 bool bFishingConfigured=false;
 double FishingYaw=0,FishingPitch=0;
 FTRNavigationParameters Settings;
 FTRNavigationCameraSnapshot State;
 bool bConfigured=false;
};

