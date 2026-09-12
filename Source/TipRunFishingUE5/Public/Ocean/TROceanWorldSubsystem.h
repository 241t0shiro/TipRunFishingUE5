#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/TROceanTypes.h"
#include "Data/TRSnapshots.h"
#include "TROceanWorldSubsystem.generated.h"

class UTROceanAreaDataAsset;
class ATRSeabedProviderActor;

// No tick or clock advancement. Game supplies time and explicitly registers the provider.
UCLASS()
class TIPRUNFISHINGUE5_API UTROceanWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	bool InitializeArea(UTROceanAreaDataAsset* InArea, ATRSeabedProviderActor* InProvider, TArray<FText>& Errors);
	bool SetSimulationTime(double InEnvironmentTimeS);
	FTROceanSample SampleOcean(const FTROceanQuery& Query) const;
	void ShutdownArea();
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "TipRun|Ocean")
	FTROceanSample SampleOceanForDisplay(const FTROceanQuery& Query) const { return SampleOcean(Query); }

	UFUNCTION(BlueprintPure, Category = "TipRun|Ocean")
	ETROceanState GetOceanState() const { return OceanState; }

	UFUNCTION(BlueprintPure, Category = "TipRun|Ocean")
	FName GetAreaId() const { return Settings.AreaId; }

	UFUNCTION(BlueprintPure, Category = "TipRun|Ocean")
	float GetSurfaceZ_M() const { return Settings.SurfaceZ_M; }

	double GetEnvironmentTimeS() const { return EnvironmentTimeS; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UPROPERTY()
	TObjectPtr<UTROceanAreaDataAsset> AreaData;

	UPROPERTY()
	TWeakObjectPtr<ATRSeabedProviderActor> Provider;

	UPROPERTY()
	FTROceanAreaSettings Settings;

	ETROceanState OceanState = ETROceanState::Uninitialized;
	double EnvironmentTimeS = 0.0;
	uint64 ProviderRevision = 0;
};
