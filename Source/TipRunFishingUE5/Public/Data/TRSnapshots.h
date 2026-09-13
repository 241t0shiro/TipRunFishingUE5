#pragma once

#include "CoreMinimal.h"
#include "Data/TRIdentifiers.h"
#include "Data/TRTypes.h"
#include "TRSnapshots.generated.h"

// These are copies of authoritative values. Consumers receive const references/copies.
// No Actor, Component or UObject ownership is carried across subsystem boundaries.

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTROceanQuery
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FVector2D PositionXYM = FVector2D::ZeroVector;

	// Depth below the local sea surface, positive downward.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float DepthM = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 SimTick = 0;
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTROceanSample
{
	GENERATED_BODY()

	// Check before using numeric fields; zero depth is not a valid fallback.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	ETRSampleError InvalidReason = ETRSampleError::NotInitialized;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FName AreaId = NAME_None;

	// MVP has no season correction.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FName SeasonId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 SampleTick = 0;

	// World Z in meters, positive upward.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float SurfaceZ_M = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float BottomDepthM = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FVector BottomNormal = FVector::UpVector;

	// World axes, positive Z upward. MVP current is horizontal.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FVector CurrentMps = FVector::ZeroVector;

	// MVP output stays zero; no wind physics.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FVector2D WindMps = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRBoatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 Tick = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FVector PositionM = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FVector RodTipM = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FVector VelocityMps = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float HeadingRad = 0.0f;
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTREgiSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FTRCastId CastId = {};

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 Tick = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FVector2D PositionXYM = FVector2D::ZeroVector;

	// Authoritative depth, positive downward; never inferred from the mesh.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float DepthM = 0.0f;

	// World axes, positive Z upward.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FVector VelocityMps = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float LineLengthM = 0.0f;

	// Angle from vertically downward.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float LineAngleRad = 0.0f;

	// Dimensionless proxy, not newtons or Fight tension.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float Tension01 = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	ETRFishingState FishingState = ETRFishingState::Inactive;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 StayPenaltyJerkCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun") float DepthVelocityMps = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "TipRun") bool bBottomContact = false;
	UPROPERTY(BlueprintReadOnly, Category = "TipRun") bool bSurfaceContact = false;
	UPROPERTY(BlueprintReadOnly, Category = "TipRun") int64 JerkCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "TipRun") int64 SeriesJerkCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "TipRun") int64 PendingJerkCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "TipRun") int64 StateEnteredTick = 0;
	// Observation duration only; neither a stability score nor a Stay gate.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun") double RangeObservationSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRSquidSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FTRActorSimId SimId = {};

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FVector2D PositionXYM = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float DepthM = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	ETRSquidState AIState = ETRSquidState::Idle;

	// Initialization placeholder; scenario configuration selects activity.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	ETRSquidActivityLevel ActivityLevel = ETRSquidActivityLevel::Low;

	// Unconfigured, not a product weight default.
	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float WeightKg = 0.0f;
};
