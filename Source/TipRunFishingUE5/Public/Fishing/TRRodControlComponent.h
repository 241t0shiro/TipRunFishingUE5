#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TRRodTuningDataAsset.h"
#include "Data/TRSnapshots.h"
#include "Data/TREvents.h"
#include "Data/TRFishingStationDataAsset.h"
#include "Data/TRSimulationTypes.h"
#include "TRRodControlComponent.generated.h"

UCLASS()
class TIPRUNFISHINGUE5_API UTRRodControlComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTRRodControlComponent();
	bool Initialize(const FTRRodParameters& Parameters, double StepSeconds, TArray<FText>& Errors);
	bool ApplyAim(FVector2D Delta, const FTRSimTime& Time, const FTRRodAimObservation& View = {}, int64 Sequence = 0);
	bool Step(const FTRSimTime& Time, const FTRBoatSnapshot& Boat, const FTREgiSnapshot& Fishing, double SurfaceZ);
	FTRRodSnapshot GetSnapshot() const { return Snapshot; }
	bool IsInitialized() const { return bInitialized; }
	int64 GetJerkTicks() const { return UpTicks+ReturnTicks; }
	float GetReelPulseMps(const FTRSimTime& Time, const FTREgiSnapshot& Fishing) const;
	void SetStation(FVector MountM,double FacingRad);
	void SetScreenStation(const FTRFishingStation& Station,const FTRFishingStationParameters& Camera);
	bool ApplyView(FVector2D YawPitchDeg);
	void Reset();
	void ClearAction() { ProfileStartTick=-1; Snapshot.bShakuriActive=false; Snapshot.TemporaryShakuriOffsetRad=0; }
	void InvalidateSnapshot() { Snapshot.bValid=false; Snapshot.bShakuriActive=false; }
	void ObserveOperationStart(const FTREgiSnapshot& Fishing);
private:
 // Persistent pose. Camera and snapshot readouts never own this direction.
 FVector RootLocal=FVector::ZeroVector;
 FVector BaseDirectionLocal=FVector::ForwardVector;
 FVector StationOriginM=FVector::ZeroVector;
 FVector CompatibilityCameraLocal=FVector::ZeroVector;
 FRotator CompatibilityCameraRotation=FRotator::ZeroRotator;
 double CompatibilityFOV=0,SurfaceLocalZ=0;
 bool bPoseInitialized=false,bCompatibilitySafetyLimited=false;
 FVector2D DeriveCompatibilityControl() const;
 FVector ConstrainLocalDirection(const FVector& Direction, FString& Reason) const;
 FTRRodSnapshot MakeLocalSolve() const;
 void PublishLocalPose(FTRRodSnapshot& Next,const FVector& FinalDirection) const;
 // One-time station setup only. Temporary actions never call a projection solver.
	bool ResolveInitialScreenPose(FTRRodSnapshot& Next,double SurfaceZ) const;
	bool bUseStation=false;
	double StationYawRad=0;
	FTRRodParameters Frozen;
	FTRRodSnapshot Snapshot;
	int64 UpTicks=0,ReturnTicks=0,ReelTicks=0,BudgetTick=-1;
	FVector2D UsedAimRad=FVector2D::ZeroVector;
	bool bInitialized=false;
	FTRCastId ObservedCast;
	int64 ObservedJerkCount=0;
 int64 ProfileStartTick=-1;
};
