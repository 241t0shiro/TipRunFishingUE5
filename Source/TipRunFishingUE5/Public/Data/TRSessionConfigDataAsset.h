#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TRSessionConfigDataAsset.generated.h"

class UTROceanAreaDataAsset;
class UTRBoatTuningDataAsset;
class UTRFishingTuningDataAsset;
class UTRInputConfigDataAsset;
class UDataTable;
// Clock validation remains independent; M09 startup validates its references separately.
UCLASS(BlueprintType)
class TIPRUNFISHINGUE5_API UTRSessionConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	// Unconfigured by default. 1/60 s and 8 steps are the initial technical proposal,
	// explicitly selected by tests, not production balance defaults.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Clock")
	double StepSeconds = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Clock")
	int32 MaxCatchUpSteps = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun|Clock")
	int32 SessionSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Startup") TObjectPtr<UTROceanAreaDataAsset> Ocean;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Startup") TObjectPtr<UTRBoatTuningDataAsset> Boat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Startup") TObjectPtr<UTRFishingTuningDataAsset> Fishing;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Startup") TObjectPtr<UTRInputConfigDataAsset> Input;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Startup") TObjectPtr<UDataTable> Egis;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Startup") TObjectPtr<UDataTable> Sinkers;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Startup") FName InitialSinkerId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Startup") FVector2D InitialBoatXYM = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Startup") float InitialHeadingRad = 0.0f;
	bool Validate(TArray<FText>& Errors) const;
	bool ValidateStartup(TArray<FText>& Errors) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
