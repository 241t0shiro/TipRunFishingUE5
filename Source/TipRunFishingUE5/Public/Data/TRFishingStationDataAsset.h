#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/TRPlayerModeTypes.h"
#include "TRFishingStationDataAsset.generated.h"
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRFishingStation
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETRFishingSide Side=ETRFishingSide::Unselected;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector PlayerM=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector EyeM=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector CameraM=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector RodMountM=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double FacingDeg=0;
};
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRFishingStationParameters
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTRFishingStation> Stations;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double MinPitchDeg=-65;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double MaxPitchDeg=10;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double InitialPitchDeg=-20;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double MinYawDeg=-55;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double MaxYawDeg=55;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double FishingCameraYawRateDegPerS=45;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double FishingCameraPitchRateDegPerS=35;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double SensitivityDeg=.2;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) double FOV=80;
 bool Validate() const;
 const FTRFishingStation* Find(ETRFishingSide Side) const;
 static FTRFishingStationParameters Prototype();
};
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRFishingStationSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool bValid=false;
 UPROPERTY(BlueprintReadOnly) ETRFishingSide Side=ETRFishingSide::Unselected;
 UPROPERTY(BlueprintReadOnly) FVector PlayerWorldM=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector EyeWorldM=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector CameraWorldM=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector RodRootWorldM=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) double FacingWorldDeg=0;
};
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRFishingCameraSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool bActive=false;
 UPROPERTY(BlueprintReadOnly) FVector PositionM=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FRotator Rotation=FRotator::ZeroRotator;
 UPROPERTY(BlueprintReadOnly) double YawDeg=0;
 UPROPERTY(BlueprintReadOnly) double PitchDeg=0;
};
UCLASS(BlueprintType)
class TIPRUNFISHINGUE5_API UTRFishingStationDataAsset : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FTRFishingStationParameters Parameters;
#if WITH_EDITOR
 virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
