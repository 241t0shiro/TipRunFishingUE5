#pragma once
#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "Data/TRNavigationTuningDataAsset.h"
#include "TRPlayerCameraManager.generated.h"
UCLASS()
class TIPRUNFISHINGUE5_API ATRPlayerCameraManager : public APlayerCameraManager
{
 GENERATED_BODY()
public:
 void ConfigureNavigation(const FTRNavigationParameters& P,double HeadingRad);
 void ClearNavigation(){bConfigured=false;State.bActive=false;}
 void Look(FVector2D Delta);
 void Zoom(double Delta);
 void ResetNavigation(double HeadingRad);
 FTRNavigationCameraSnapshot GetNavigationSnapshot() const{return State;}
 bool BuildNavigationView(const FVector& BoatPositionM,FMinimalViewInfo& Out) const;
protected:
 virtual void UpdateViewTarget(FTViewTarget& OutVT,float DeltaTime) override;
private:
 FTRNavigationParameters Settings;
 FTRNavigationCameraSnapshot State;
 bool bConfigured=false;
};

