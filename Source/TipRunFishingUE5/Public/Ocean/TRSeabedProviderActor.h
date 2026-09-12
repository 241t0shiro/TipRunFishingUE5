#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/TROceanTypes.h"
#include "TRSeabedProviderActor.generated.h"

class UTROceanAreaDataAsset;
class USceneComponent;

UCLASS()
class TIPRUNFISHINGUE5_API ATRSeabedProviderActor : public AActor
{
	GENERATED_BODY()

public:
	ATRSeabedProviderActor();
	bool InitializeFromArea(const UTROceanAreaDataAsset& Area, TArray<FText>& Errors);
	FTRDepthSample SampleBottomDepth(const FVector2D& PositionXYM) const;

	bool IsConfigured() const { return bConfigured && !IsActorBeingDestroyed(); }
	uint64 GetConfigurationRevision() const { return ConfigurationRevision; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "TipRun")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	FTROceanAreaSettings Settings;

	bool bConfigured = false;
	uint64 ConfigurationRevision = 0;
};
