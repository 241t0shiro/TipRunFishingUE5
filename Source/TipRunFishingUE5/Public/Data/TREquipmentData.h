#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/TRFishingTuningDataAsset.h"
#include "TREquipmentData.generated.h"

class UStaticMesh;

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTREgiSpecRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	FName EgiId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	FText DisplayName = FText::GetEmpty();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float SizeGo = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float BaseMassG = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	FName SimulationProfileId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	TSoftObjectPtr<UStaticMesh> Mesh;

	bool Validate(TArray<FText>& Errors) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRSinkerSpecRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	FName SinkerId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float MassG = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	FText DisplayName = FText::GetEmpty();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	TSoftObjectPtr<UStaticMesh> VisualMesh;

	bool Validate(TArray<FText>& Errors) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

// Frozen numeric values; no DataAsset, curve or mesh reference survives in the snapshot.
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTREquipmentSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FName EgiId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FName SinkerId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FName SimulationProfileId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float BaseMassG = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float SinkerMassG = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float TotalMassG = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float SinkSpeedMps = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float HorizontalResponsePerS = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FTRFishingParameters Parameters;
};

namespace TREquipment
{
	TIPRUNFISHINGUE5_API FName InitialEgiId();
	TIPRUNFISHINGUE5_API FName NoSinkerId();

	// Configuration-time validation (may load referenced meshes), never call from Tick.
	// Also checks logical ID uniqueness, row-name/ID agreement and profile references.
	TIPRUNFISHINGUE5_API bool ValidateTables(const UDataTable* Egis, const UDataTable* Sinkers,
		const UTRFishingTuningDataAsset* Tuning, TArray<FText>& Errors);

	// All-or-nothing output. Equipment locking belongs to M06, not this data resolver.
	TIPRUNFISHINGUE5_API bool TryBuildSnapshot(const UDataTable* Egis, const UDataTable* Sinkers,
		const UTRFishingTuningDataAsset* Tuning, FName EgiId, FName SinkerId,
		double StepSeconds, FTREquipmentSnapshot& OutSnapshot, TArray<FText>& Errors);
}
