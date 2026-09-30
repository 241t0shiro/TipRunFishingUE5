#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TRRodTuningDataAsset.generated.h"

// Camera-plane coordinates: +X right, +Y up, both in horizontal half-screen units.
// Prototype values; keep a compact visible working region rather than a sphere-wide Euler aim.
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRRodScreenParameters
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D Min=FVector2D(-.28,.02);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D Max=FVector2D(.28,.28);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D Initial=FVector2D(0,.15);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D Sensitivity=FVector2D(.004,.004);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) double MaxRatePerS=1.5;
	// Upper projected boundary shared by safety remapping and the temporary Shakuri lift.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) double MaxProjectedY=.5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(Units="m")) double SurfaceClearanceM=.02;
	bool Validate() const;
};

// Explicit Prototype/Test configuration. Angles are radians, lengths meters, time seconds.
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRRodParameters
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod|Screen") FTRRodScreenParameters Screen;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double MinPitchRad=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double MaxPitchRad=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double MinYawRad=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double MaxYawRad=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double InitialPitchRad=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double InitialYawRad=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double SensitivityXRad=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double SensitivityYRad=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") bool bInvertX=false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") bool bInvertY=false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double MaxMouseDelta=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double MaxAimRateRadPerS=0;
	// Fixed root-to-tip physical length, frozen at initialization. Never a framing scale.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod", meta=(Units="m")) double LengthM=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") FVector MountOffsetM=FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double ShakuriAmplitudeRad=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double ShakuriUpSeconds=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double ShakuriReturnSeconds=0;
	// Zero/zero preserves old assets. Explicit Prototype pulse, independent of normal retrieve.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double ShakuriReelSpeedMps=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double ShakuriReelSeconds=0;
	bool Validate(TArray<FText>& Errors) const;
};

UCLASS(BlueprintType)
class TIPRUNFISHINGUE5_API UTRRodTuningDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun") FTRRodParameters Parameters;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
