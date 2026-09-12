#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/TROceanTypes.h"
#include "TROceanAreaDataAsset.generated.h"

UCLASS(BlueprintType)
class TIPRUNFISHINGUE5_API UTROceanAreaDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	FTROceanAreaSettings Settings;

	bool Validate(TArray<FText>& Errors) const { return Settings.Validate(Errors); }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
