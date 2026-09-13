#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TRBoatTuningDataAsset.generated.h"

UENUM(BlueprintType)
enum class ETRBoatMode : uint8 { Uninitialized, DriftOnly, Disabled };

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRBoatParameters
{
	GENERATED_BODY()
	// Revision 1 is the saved M05 model. Select 2 explicitly; coefficients have different units.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	int32 ModelRevision = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat|WindCurrent")
	double WindResponseKgPerS = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat|WindCurrent")
	double CurrentResponseKgPerS = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat|WindCurrent")
	double DragKgPerS = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat|WindCurrent")
	double InertiaKg = 0.0;
	// Dimensionless effective area multipliers. No product defaults.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat|WindCurrent")
	double BowWindScale = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat|WindCurrent")
	double SternWindScale = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat|WindCurrent")
	double SideWindScale = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	double CurrentResponse01 = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	double VelocityResponsePerS = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	double MaxDriftSpeedMps = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	double HullHeightOffsetM = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	FVector RodAnchorOffsetM = FVector::ZeroVector;
	bool Validate(TArray<FText>& Errors) const;
};

UCLASS(BlueprintType)
class TIPRUNFISHINGUE5_API UTRBoatTuningDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	// Zero rate/speed mean unconfigured, not production defaults.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	FTRBoatParameters Parameters;
	bool Validate(TArray<FText>& Errors) const { return Parameters.Validate(Errors); }
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
