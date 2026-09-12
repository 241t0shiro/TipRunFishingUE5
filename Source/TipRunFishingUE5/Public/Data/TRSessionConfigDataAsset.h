#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TRSessionConfigDataAsset.generated.h"

// M04 subset of session configuration. Gameplay references arrive in their own tasks.
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

	bool Validate(TArray<FText>& Errors) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
