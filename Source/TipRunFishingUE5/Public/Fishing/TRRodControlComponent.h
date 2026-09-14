#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TRRodTuningDataAsset.h"
#include "Data/TRSnapshots.h"
#include "Data/TRSimulationTypes.h"
#include "TRRodControlComponent.generated.h"

UCLASS()
class TIPRUNFISHINGUE5_API UTRRodControlComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTRRodControlComponent();
	bool Initialize(const FTRRodParameters& Parameters, double StepSeconds, TArray<FText>& Errors);
	bool ApplyAim(FVector2D Delta, const FTRSimTime& Time);
	bool Step(const FTRSimTime& Time, const FTRBoatSnapshot& Boat, const FTREgiSnapshot& Fishing, double SurfaceZ);
	FTRRodSnapshot GetSnapshot() const { return Snapshot; }
	bool IsInitialized() const { return bInitialized; }
	int64 GetJerkTicks() const { return UpTicks+ReturnTicks; }
	void Reset();
	void InvalidateSnapshot() { Snapshot.bValid=false; Snapshot.bShakuriActive=false; }
	void ObserveOperationStart(const FTREgiSnapshot& Fishing);
private:
	FTRRodParameters Frozen;
	FTRRodSnapshot Snapshot;
	int64 UpTicks=0,ReturnTicks=0,BudgetTick=-1;
	FVector2D UsedAimRad=FVector2D::ZeroVector;
	bool bInitialized=false;
	FTRCastId ObservedCast;
	int64 ObservedJerkCount=0;
};
