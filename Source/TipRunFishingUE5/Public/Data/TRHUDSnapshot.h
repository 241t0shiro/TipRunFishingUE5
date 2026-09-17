#pragma once
#include "CoreMinimal.h"
#include "Data/TRSnapshots.h"
#include "Data/TREquipmentData.h"
#include "Data/TRPlayerModeTypes.h"
#include "TRHUDSnapshot.generated.h"

// Display copies only. No simulated state or future AI metrics are owned by UI.
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRHUDSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="TipRun") FTRPlayerModeSnapshot PlayerMode;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") bool bSessionValid = false;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") bool bEgiValid = false;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") bool bEnvironmentValid = false;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") bool bEquipmentLocked = false;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") bool bEgiOnboard = false;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") bool bCanChangeEquipment = false;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") ETRSessionPhase Phase = ETRSessionPhase::Initializing;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") FTRCastId CastId;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") FTREgiSnapshot Egi;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") FTRBoatSnapshot Boat;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") FTRRodSnapshot Rod;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") FTRRetrievalSnapshot Retrieval;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") FTROceanSample Ocean;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") FTREquipmentSnapshot Equipment;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") bool bPaused = false;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") FText EquipmentBlockReason;
	// State eligibility only; actual commands still revalidate tick, cast and environment.
	UPROPERTY(BlueprintReadOnly, Category="TipRun") TArray<ETRFishingCommandType> AvailableCommands;
	// Controller fills device availability; Session retains state eligibility above.
	UPROPERTY(BlueprintReadOnly, Category="TipRun") TArray<ETRFishingCommandType> UnmappedPrimaryInputs;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") FText InputConfigurationNote;
};
