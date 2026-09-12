#pragma once

#include "CoreMinimal.h"
#include "TRTypes.generated.h"

// Value vocabulary from GAME_DESIGN and the subsystem design documents.
// State transitions and decisions belong to their owning systems, not Data.

UENUM(BlueprintType)
enum class ETRSessionPhase : uint8
{
	Initializing,
	Ready,
	Fishing,
	Result,
	Error
};

UENUM(BlueprintType)
enum class ETRFishingState : uint8
{
	Inactive,
	Ready,
	Deploying,
	FreeFall,
	BottomContact,
	Jerking,
	TensionFall,
	Stay,
	Retrieving,
	Fighting,
	Landing,
	Result
};

UENUM(BlueprintType)
enum class ETRSquidState : uint8
{
	Idle,
	Interested,
	Approach,
	Attack,
	Bite,
	Caution,
	Cooldown
};

UENUM(BlueprintType)
enum class ETRSquidActivityLevel : uint8
{
	Low,
	Medium,
	High
};

UENUM(BlueprintType)
enum class ETRSquidDisposition : uint8
{
	Available,
	Hooked,
	Caught,
	Removed
};

UENUM(BlueprintType)
enum class ETRHookOutcome : uint8
{
	Hit,
	Miss,
	Cancelled,
	Ignored
};

UENUM(BlueprintType)
enum class ETRHookReason : uint8
{
	None,
	Early,
	InWindow,
	Late,
	NoBite,
	Expired,
	TargetLost,
	StayReleased,
	CastEnded,
	AlreadyResolved
};

UENUM(BlueprintType)
enum class ETRCastOutcome : uint8
{
	Caught,
	Missed,
	Retrieved,
	Aborted,
	Escaped
};

UENUM(BlueprintType)
enum class ETRFightState : uint8
{
	Inactive,
	Active,
	Won,
	Escaped,
	Aborted
};

UENUM(BlueprintType)
enum class ETRFishingCommandType : uint8
{
	Deploy,
	Fall,
	Jerk,
	TensionFall,
	Stay,
	Hook,
	RetrieveStarted,
	RetrieveStopped,
	EndFishing,
	NextCast
};

UENUM(BlueprintType)
enum class ETRCommandResult : uint8
{
	Accepted,
	RejectedInvalidState,
	RejectedBusy,
	RejectedInvalidEnvironment,
	RejectedMissingData
};

UENUM(BlueprintType)
enum class ETRLineMode : uint8
{
	Payout,
	ControlledPayout,
	Locked,
	ReelIn
};

UENUM(BlueprintType)
enum class ETREgiStepEvent : uint8
{
	None,
	ReachedBottom,
	LeftBottom,
	Retrieved,
	EnvironmentInvalid
};

UENUM(BlueprintType)
enum class ETRSampleError : uint8
{
	None,
	NotInitialized,
	OutsideArea,
	InvalidDepth,
	MissingProvider,
	InvalidQuery
};

UENUM(BlueprintType)
enum class ETRBiteCueType : uint8
{
	Prototype,
	TipLoad,
	TipUnload,
	Subtle
};
