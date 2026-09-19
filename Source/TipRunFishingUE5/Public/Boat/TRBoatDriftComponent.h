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
	bool ConfigureNavigation(double MaxSpeedMps);
	void SetNavigationForces(double ForceN, double YawRateRadPerS, double LateralResponsePerS=0);
	// Fixed mode boundary only: preserve transform, rebuild environmental drift from rest.
	void ResetDynamicVelocityForFishing();
	void StopMotion() { bInitialized = false; }
private:
 double NavigationLateralResponsePerS=0;
	double NavigationMaxSpeedMps = 0;
	double EngineForceN = 0;
	double NavigationYawRateRadPerS = 0;
	FTRBoatParameters FrozenSettings;
	FTRBoatSnapshot Snapshot;
	int64 LastIntegratedTick = -1;
	bool bInitialized = false;
};
