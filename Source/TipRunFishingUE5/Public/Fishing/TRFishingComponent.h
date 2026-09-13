#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TREquipmentData.h"
#include "Data/TREvents.h"
#include "Data/TRSnapshots.h"
#include "TRFishingComponent.generated.h"

// M06 owns only the minimal operation states and deployment snapshot. No integration.
UCLASS()
class TIPRUNFISHINGUE5_API UTRFishingComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTRFishingComponent();
	UFUNCTION(BlueprintPure, Category = "TipRun|Fishing")
	ETRFishingState GetState() const { return State; }
	UFUNCTION(BlueprintPure, Category = "TipRun|Fishing")
	FTREgiSnapshot GetSnapshot() const { return Snapshot; }
	FTREgiAction GetAction() const;
private:
	friend class ATRFishingSessionActor;
	void Prepare();
	void BeginCast(FTRCastId Id, int64 Tick, const FTREquipmentSnapshot& Equipment);
	bool CompleteDeployment(const FTRBoatSnapshot& Boat, const FTROceanSample& Ocean, int64 Tick);
	void FinishCast();
	void Stop();
	UPROPERTY(BlueprintReadOnly, Category = "TipRun|Fishing", meta = (AllowPrivateAccess = "true"))
	ETRFishingState State = ETRFishingState::Inactive;
	FTREgiSnapshot Snapshot;
	FTREquipmentSnapshot CastEquipment;
};
