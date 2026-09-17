#pragma once
#include "CoreMinimal.h"
#include "TRPlayerModeTypes.generated.h"

UENUM(BlueprintType)
enum class ETRPlayerMode : uint8 { Navigation, Fishing };
UENUM(BlueprintType)
enum class ETRFishingSide : uint8 { Unselected, Port, Starboard };
UENUM(BlueprintType)
enum class ETRModeChangeRejection : uint8
{
 None, SessionUnavailable, Paused, InvalidTarget, AlreadyInMode, ActiveCast,
 EgiOffboard, NotReady, EquipmentLocked, InvalidEnvironment, StaleInput
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRPlayerModeSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly, Category="TipRun|Mode") bool bValid = false;
 UPROPERTY(BlueprintReadOnly, Category="TipRun|Mode") ETRPlayerMode Mode = ETRPlayerMode::Navigation;
 UPROPERTY(BlueprintReadOnly, Category="TipRun|Mode") ETRFishingSide FishingSide = ETRFishingSide::Unselected;
 UPROPERTY(BlueprintReadOnly, Category="TipRun|Mode") int64 ModeEpoch = 0;
 UPROPERTY(BlueprintReadOnly, Category="TipRun|Mode") int64 ChangedAtTick = -1;
 UPROPERTY(BlueprintReadOnly, Category="TipRun|Mode") bool bCanChangeMode = false;
 UPROPERTY(BlueprintReadOnly, Category="TipRun|Mode") ETRModeChangeRejection ChangeBlockedReason = ETRModeChangeRejection::SessionUnavailable;
 UPROPERTY(BlueprintReadOnly, Category="TipRun|Mode") ETRModeChangeRejection LastRejectedReason = ETRModeChangeRejection::None;
 // R2 consumes this policy before its boat phase; R1 never changes boat motion.
 UPROPERTY(BlueprintReadOnly, Category="TipRun|Mode") bool bNavigationInputAllowed = false;
 UPROPERTY(BlueprintReadOnly, Category="TipRun|Mode") bool bStopNavigationThrustRequested = true;
 UPROPERTY(BlueprintReadOnly, Category="TipRun|Mode") bool bHoldBoatHeading = true;
};

