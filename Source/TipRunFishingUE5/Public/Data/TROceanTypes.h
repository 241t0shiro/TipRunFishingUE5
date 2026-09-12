#pragma once

#include "CoreMinimal.h"
#include "Data/TRTypes.h"
#include "TROceanTypes.generated.h"

// M03 implements Flat only. PlaneSlope is added with M16's slope scenario.
UENUM(BlueprintType)
enum class ETRSeabedMode : uint8 { Flat };

UENUM(BlueprintType)
enum class ETROceanState : uint8 { Uninitialized, Ready, Unloaded, Error };

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRDepthSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	ETRSampleError InvalidReason = ETRSampleError::NotInitialized;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	float BottomDepthM = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FVector BottomNormal = FVector::UpVector;
};

// Configuration values are copied at initialization; no live asset reads while sampling.
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTROceanAreaSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	FName AreaId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	FVector2D BoundsMinXYM = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	FVector2D BoundsMaxXYM = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float SurfaceZ_M = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	ETRSeabedMode SeabedMode = ETRSeabedMode::Flat;

	// Zero means unconfigured. Test depth 30 m is not a production default.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	float FlatDepthM = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TipRun")
	FVector CurrentMps = FVector::ZeroVector;

	bool Validate(TArray<FText>& Errors) const;
	bool Contains(const FVector2D& PositionXYM) const;
};
