#pragma once

#include "CoreMinimal.h"
#include "Data/TRIdentifiers.h"
#include "Data/TRTypes.h"
#include "TREvents.generated.h"

// Payloads and signatures only. M01 does not process commands, approve bites or resolve casts.

// Tick/Sequence ordering and acceptance are owned by Game/Fishing.
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRFishingCommand
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	ETRFishingCommandType Type = ETRFishingCommandType::Deploy;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 TargetTick = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float AxisValue = 0.0f;
	// Mouse displacement, not rate; no render-dt multiplication.
	UPROPERTY(BlueprintReadOnly, Category="TipRun") FVector2D Axis2D = FVector2D::ZeroVector;

	// M06: identity observed when input was submitted; sessions reject stale casts.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FTRCastId ExpectedCastId;
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTREgiAction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	ETRFishingState FishingState = ETRFishingState::Inactive;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	ETRLineMode LineMode = ETRLineMode::Locked;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float SinkScale = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float LiftMps = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float ReelMps = 0.0f;
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRBiteRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FTRCastId CastId = {};

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FTRActorSimId SimId = {};

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 RequestTick = 0;
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRBiteCue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FTRBiteToken Token = {};

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 StartTick = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	ETRBiteCueType Type = ETRBiteCueType::Prototype;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float Intensity01 = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	double DurationS = 0.0;
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRCatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FTRCastId CastId = {};

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FTRActorSimId SimId = {};

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	ETRCastOutcome Outcome = ETRCastOutcome::Aborted;

	// True only for a completed Quick return, never for an interrupted one.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	bool bQuickRetrieved = false;

	// Only Caught may carry a catch weight. Other outcomes display no catch.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float WeightKg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FName EgiId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FName SinkerId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	double ElapsedSimSeconds = 0.0;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FTRFishingStateChanged, ETRFishingState, ETRFishingState);
DECLARE_MULTICAST_DELEGATE_OneParam(FTRBiteCueEvent, const FTRBiteCue&);
DECLARE_MULTICAST_DELEGATE_FiveParams(FTRHookResolved, FTRCastId, FTRBiteToken, FTRActorSimId, ETRHookOutcome, ETRHookReason);
DECLARE_MULTICAST_DELEGATE_OneParam(FTRCastCompleted, const FTRCatchResult&);
