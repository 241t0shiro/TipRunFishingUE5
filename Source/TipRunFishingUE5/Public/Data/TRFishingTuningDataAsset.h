#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "TRFishingTuningDataAsset.generated.h"

// All unspecified zeros mean unconfigured, not product balance.
// Only the three approved timing values have usable defaults.
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRFishingParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float MaxLineLengthM = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float MinLineM = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float PayoutMps = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float TensionPayoutMps = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float ReelMps = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float FreeFallSinkScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float TensionFallSinkScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float StaySinkScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	double JerkDurationS = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float JerkLiftMps = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float JerkReelMps = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	double AutoStayDelayS = 0.8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float TensionReferenceM = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float MaxEgiSpeedMps = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	double HookOpenDelayS = 0.10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	double HookCloseDelayS = 0.55;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float InitialFightTension01 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float ReelTensionRisePerS = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float TensionRecoveryPerS = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float OverTensionThreshold01 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	double OverTensionDurationS = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float ReelProgressPerSecond = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float RetrievalToleranceM = 0.0f;

	bool Validate(TArray<FText>& Errors) const;
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTREgiSimulationProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	FName ProfileId = NAME_None;

	// M02 supports finite positive linear curves covering the MVP mass domain [30, 90] g.
	UPROPERTY(EditAnywhere, Category = "TipRun")
	FRuntimeFloatCurve SinkSpeedByTotalMass;

	UPROPERTY(EditAnywhere, Category = "TipRun")
	FRuntimeFloatCurve HorizontalResponseByTotalMass;

	bool Validate(TArray<FText>& Errors) const;
};

UCLASS(BlueprintType)
class TIPRUNFISHINGUE5_API UTRFishingTuningDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	FTRFishingParameters Parameters;

	UPROPERTY(EditAnywhere, Category = "TipRun")
	TArray<FTREgiSimulationProfile> Profiles;

	bool Validate(TArray<FText>& Errors) const;
	const FTREgiSimulationProfile* FindProfile(FName ProfileId) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
