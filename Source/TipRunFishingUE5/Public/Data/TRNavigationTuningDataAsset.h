#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TRNavigationTuningDataAsset.generated.h"
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRNavigationParameters
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double EngineForceN=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double ReverseScale=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double EngineResponsePerS=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double SteeringRateRadPerS=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double SteeringResponsePerS=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double MaxSpeedMps=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double NavigationLateralResponsePerS=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double BoostThrustMultiplier=1;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double BoostMaxSpeedMps=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double BoostResponsePerS=2;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double CameraDistanceM=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double CameraHeightM=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double CameraMinPitchDeg=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double CameraMaxPitchDeg=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double CameraInitialPitchDeg=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double LookSensitivityDeg=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double CameraMinDistanceM=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double CameraMaxDistanceM=0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double ZoomMetersPerUnit=0;
 bool Validate() const;
 static FTRNavigationParameters Prototype();
};
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRNavigationSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool bConfigured=false;
 UPROPERTY(BlueprintReadOnly) double Throttle=0;
 UPROPERTY(BlueprintReadOnly) double Steering=0;
 UPROPERTY(BlueprintReadOnly) double EngineForceN=0;
 UPROPERTY(BlueprintReadOnly) double YawRateRadPerS=0;
 UPROPERTY(BlueprintReadOnly) bool bEngineActive=false;
 UPROPERTY(BlueprintReadOnly) bool bBoostRequested=false;
 UPROPERTY(BlueprintReadOnly) double BoostBlend=0;
 UPROPERTY(BlueprintReadOnly) double LateralResponsePerS=0;
};
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRNavigationCameraSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool bActive=false;
 UPROPERTY(BlueprintReadOnly) double YawDeg=0;
 UPROPERTY(BlueprintReadOnly) double PitchDeg=0;
 UPROPERTY(BlueprintReadOnly) double DistanceM=0;
};
UCLASS(BlueprintType)
class TIPRUNFISHINGUE5_API UTRNavigationTuningDataAsset : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Navigation") FTRNavigationParameters Parameters;
#if WITH_EDITOR
 virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
