#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TRRodTuningDataAsset.generated.h"

// Explicit Prototype/Test configuration. Angles are radians, lengths meters, time seconds.
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRRodParameters
{
	GENERATED_BODY()
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double LengthM=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") FVector MountOffsetM=FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double ShakuriAmplitudeRad=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double ShakuriUpSeconds=0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Rod") double ShakuriReturnSeconds=0;
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
