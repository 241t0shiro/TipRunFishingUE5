#pragma once
#include "CoreMinimal.h"
#include "Data/TRIdentifiers.h"
#include "TRShakuriSequenceTypes.generated.h"

UENUM(BlueprintType)
enum class ETRShakuriPhase : uint8 { Idle, Up, Recover, Complete, Interrupted };
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRShakuriSequenceSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool bEnabled=false;
 UPROPERTY(BlueprintReadOnly) FTRCastId CastId;
 UPROPERTY(BlueprintReadOnly) int64 Tick=0;
 UPROPERTY(BlueprintReadOnly) ETRShakuriPhase Phase=ETRShakuriPhase::Idle;
 UPROPERTY(BlueprintReadOnly) int64 SequenceIndex=0;
 UPROPERTY(BlueprintReadOnly) int64 CompletedCount=0;
 UPROPERTY(BlueprintReadOnly) int64 QueuedCount=0;
 UPROPERTY(BlueprintReadOnly) double RequestedHandleTurns=0;
 UPROPERTY(BlueprintReadOnly) double TotalRequestedHandleTurns=0;
 UPROPERTY(BlueprintReadOnly) double RequestedRetrieveM=0;
 UPROPERTY(BlueprintReadOnly) double TotalRequestedRetrieveM=0;
 UPROPERTY(BlueprintReadOnly) double ActualRetrieveM=0;
 UPROPERTY(BlueprintReadOnly) double TotalActualRetrieveM=0;
 UPROPERTY(BlueprintReadOnly) double SlackConsumedM=0;
 UPROPERTY(BlueprintReadOnly) double TautRetrieveAppliedM=0;
 UPROPERTY(BlueprintReadOnly) double UnrealizedRetrieveM=0;
 UPROPERTY(BlueprintReadOnly) double TotalSlackConsumedM=0;
 UPROPERTY(BlueprintReadOnly) double TotalTautRetrieveAppliedM=0;
 UPROPERTY(BlueprintReadOnly) double TotalUnrealizedRetrieveM=0;
 UPROPERTY(BlueprintReadOnly) double TickDemandM=0;
 UPROPERTY(BlueprintReadOnly) bool bPendingRetrieve=false;
 UPROPERTY(BlueprintReadOnly) bool bPendingReFall=false;
};
