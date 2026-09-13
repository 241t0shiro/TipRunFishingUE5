#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TREquipmentData.h"
#include "Data/TREvents.h"
#include "Data/TRSnapshots.h"
#include "Data/TRSimulationTypes.h"
#include "TREgiSimulationComponent.generated.h"

UCLASS()
class TIPRUNFISHINGUE5_API UTREgiSimulationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTREgiSimulationComponent();
	bool InitializeCast(const FTREgiSnapshot& Initial, const FTREquipmentSnapshot& Equipment,
		const FTROceanSample& Ocean, TArray<FText>& Errors);
	// Only fixed updates may call this. None also denotes an ignored stale/duplicate call.
	ETREgiStepEvent StepEgi(FTRCastId ExpectedCastId, const FTRSimTime& Time,
		const FTROceanSample& Ocean, const FTRBoatSnapshot& Boat, const FTREgiAction& Action,
		TFunctionRef<FTROceanSample(const FTROceanQuery&)> SampleDestination);
	// Fishing owns operation state; numeric authority lives here.
	FTREgiSnapshot BuildSnapshot(ETRFishingState FishingState) const;
	void Reset();
private:
	FTREgiSnapshot Snapshot;
	FTREquipmentSnapshot FrozenEquipment;
	// Retained for world velocity when the queried surface height changes.
	float LastSurfaceZ_M = 0.0f;
	int64 HighestCastValue = 0;
	bool bActive = false;
	bool bOnBottom = false;
};
