#pragma once
#include "CoreMinimal.h"
#include "Data/TRSnapshots.h"
#include "TRRelativeDriftTypes.generated.h"

// R7A read-only, same-fixed-tick geometry. No target angle/range score.
USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRRelativeDriftSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool bValid=false;
 UPROPERTY(BlueprintReadOnly) int64 Tick=0;
 UPROPERTY(BlueprintReadOnly) FVector2D BoatHorizontalVelocityMps=FVector2D::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector2D EgiHorizontalVelocityMps=FVector2D::ZeroVector;
 // Boat minus Egi. Magnitude alone is NOT the rate of separation growth.
 UPROPERTY(BlueprintReadOnly) FVector2D RelativeHorizontalVelocityMps=FVector2D::ZeroVector;
 UPROPERTY(BlueprintReadOnly) double SeparationRateMps=0;
 UPROPERTY(BlueprintReadOnly) double BoatEgiHorizontalSeparationM=0;
 UPROPERTY(BlueprintReadOnly) double RodEgiHorizontalSeparationM=0;
 UPROPERTY(BlueprintReadOnly) double VerticalSeparationM=0;
 UPROPERTY(BlueprintReadOnly) double StraightDistanceM=0;
 // atan2(abs(vertical separation), horizontal separation), degrees [0,90].
 // Invalid for coincident endpoints; distinct from line length and legacy angle from vertical.
 UPROPERTY(BlueprintReadOnly) double LineAngleToSurfaceDeg=0;
 static FTRRelativeDriftSnapshot Observe(const FTRBoatSnapshot& B,const FTREgiSnapshot& E,const FTRRodSnapshot& R)
 {
  FTRRelativeDriftSnapshot D;
  if(!E.bWorldPositionValid || !E.DriftObservation.bValid || !R.bValid ||
   B.Tick!=E.Tick || R.Tick!=E.Tick || E.DriftObservation.Tick!=E.Tick ||
   B.PositionM.ContainsNaN() || E.WorldPositionM.ContainsNaN() || R.TipWorldPositionM.ContainsNaN() ||
   B.VelocityMps.ContainsNaN() || E.VelocityMps.ContainsNaN()){return D;}
  const FVector Delta=E.WorldPositionM-R.TipWorldPositionM;
  D.StraightDistanceM=Delta.Size();if(!FMath::IsFinite(D.StraightDistanceM) || D.StraightDistanceM<=UE_DOUBLE_SMALL_NUMBER){return {};}
  D.Tick=E.Tick;
  D.BoatHorizontalVelocityMps=FVector2D(B.VelocityMps.X,B.VelocityMps.Y);
  D.EgiHorizontalVelocityMps=FVector2D(E.VelocityMps.X,E.VelocityMps.Y);
  D.RelativeHorizontalVelocityMps=D.BoatHorizontalVelocityMps-D.EgiHorizontalVelocityMps;
  const FVector2D Separation(B.PositionM.X-E.WorldPositionM.X,B.PositionM.Y-E.WorldPositionM.Y);
  D.BoatEgiHorizontalSeparationM=Separation.Size();
  D.SeparationRateMps=FVector2D::DotProduct(Separation.GetSafeNormal(),D.RelativeHorizontalVelocityMps);
  D.RodEgiHorizontalSeparationM=Delta.Size2D();D.VerticalSeparationM=FMath::Abs(Delta.Z);
  D.LineAngleToSurfaceDeg=FMath::RadiansToDegrees(FMath::Atan2(D.VerticalSeparationM,D.RodEgiHorizontalSeparationM));
  D.bValid=true;return D;
 }
};
