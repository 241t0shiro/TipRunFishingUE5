#pragma once
#include "CoreMinimal.h"
#include "TRReelTypes.generated.h"

UENUM(BlueprintType)
enum class ETRReelSource : uint8 { None, Shakuri, NormalRetrieve };

// Last committed spatial tick and cast totals. Reel shortening is measured at
// the spool operation, independently of payout, span growth and float publication.
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRReelSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) int64 Tick=0;
 UPROPERTY(BlueprintReadOnly) ETRReelSource Source=ETRReelSource::None;
 UPROPERTY(BlueprintReadOnly) double RequestedRetrieveM=0;
 UPROPERTY(BlueprintReadOnly) double SlackBeforeM=0;
 UPROPERTY(BlueprintReadOnly) double SlackAfterM=0;
 UPROPERTY(BlueprintReadOnly) double SlackConsumedM=0;
 UPROPERTY(BlueprintReadOnly) double TautRetrieveAppliedM=0;
 UPROPERTY(BlueprintReadOnly) double ActualRetrieveM=0;
 UPROPERTY(BlueprintReadOnly) double UnrealizedRetrieveM=0;
 UPROPERTY(BlueprintReadOnly) double RequiredLineLengthM=0;
 UPROPERTY(BlueprintReadOnly) double RequiredLineBeforeM=0;
 UPROPERTY(BlueprintReadOnly) double LineLengthBeforeM=0;
 UPROPERTY(BlueprintReadOnly) double LineLengthAfterM=0;
 // After - Before; positive means net growth, not negative retrieve.
 UPROPERTY(BlueprintReadOnly) double NetLineLengthDeltaM=0;
 UPROPERTY(BlueprintReadOnly) double PayoutM=0;
 UPROPERTY(BlueprintReadOnly) double GeometryAccommodationM=0;
 UPROPERTY(BlueprintReadOnly) double PublicationDeltaM=0;
 UPROPERTY(BlueprintReadOnly) double TensionBefore01=0;
 UPROPERTY(BlueprintReadOnly) double TensionAfter01=0;
 UPROPERTY(BlueprintReadOnly) double TautBudgetM=0;
 UPROPERTY(BlueprintReadOnly) double TotalRequestedRetrieveM=0;
 UPROPERTY(BlueprintReadOnly) double TotalSlackConsumedM=0;
 UPROPERTY(BlueprintReadOnly) double TotalTautRetrieveAppliedM=0;
 UPROPERTY(BlueprintReadOnly) double TotalActualRetrieveM=0;
 UPROPERTY(BlueprintReadOnly) double TotalUnrealizedRetrieveM=0;
 UPROPERTY(BlueprintReadOnly) double TotalNetLineLengthDeltaM=0;
 UPROPERTY(BlueprintReadOnly) double TotalPayoutM=0;
 UPROPERTY(BlueprintReadOnly) double TotalGeometryAccommodationM=0;
};
