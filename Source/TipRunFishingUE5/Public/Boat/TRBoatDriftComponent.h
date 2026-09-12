#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TRBoatTuningDataAsset.h"
#include "Data/TRSnapshots.h"
#include "Data/TRSimulationTypes.h"
#include "TRBoatDriftComponent.generated.h"

UCLASS()
class TIPRUNFISHINGUE5_API UTRBoatDriftComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTRBoatDriftComponent();
	bool InitializeMotion(const FTRBoatParameters& Settings, const FVector2D& InitialXYM,
		float HeadingRad, const FTROceanSample& Ocean, TArray<FText>& Errors);
	// Destination validation is supplied by Game as an Ocean value query. No references
	// to Ocean/Game or the callback are retained. Commit only after both samples are valid.
	bool StepDrift(const FTRSimTime& Time, const FTROceanSample& Ocean,
		TFunctionRef<bool(const FVector2D&)> IsDestinationValid);
	FTRBoatSnapshot BuildSnapshot() const { return Snapshot; }
	void StopMotion() { bInitialized = false; }
private:
	FTRBoatParameters FrozenSettings;
	FTRBoatSnapshot Snapshot;
	int64 LastIntegratedTick = -1;
	bool bInitialized = false;
};
