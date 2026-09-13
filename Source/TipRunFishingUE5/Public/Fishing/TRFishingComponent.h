#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TREquipmentData.h"
#include "Data/TREvents.h"
#include "Data/TRSnapshots.h"
#include "Data/TRSimulationTypes.h"
#include "TRFishingComponent.generated.h"

// Owns operation state; numeric Egi integration is a separate component.
UCLASS()
class TIPRUNFISHINGUE5_API UTRFishingComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTRFishingComponent();
	UFUNCTION(BlueprintPure, Category = "TipRun|Fishing")
	ETRFishingState GetState() const { return State; }
	UFUNCTION(BlueprintPure, Category = "TipRun|Fishing")
	FTREgiSnapshot GetSnapshot() const;
	FTREgiAction GetAction() const;
	FTRFishingStateChanged OnFishingStateChanged;
private:
	friend class ATRFishingSessionActor;
	void Prepare();
	void BeginCast(FTRCastId Id, const FTRSimTime& Time, const FTREquipmentSnapshot& Equipment);
	bool CompleteDeployment(const FTRBoatSnapshot& Boat, const FTROceanSample& Ocean, int64 Tick);
	void FinishCast();
	void Stop();
	void ApplyEgiStep(const FTREgiSnapshot& Updated, ETREgiStepEvent Event, bool bTransientComplete = false);
	ETRCommandResult HandleCommand(const FTRFishingCommand& Command, const FTRSimTime& Time);
	void PrepareStep(const FTRSimTime& Time);
	void TransitionTo(ETRFishingState Next, int64 Tick);
	void BeginJerk(int64 Tick);
	int64 JerkCount = 0, SeriesJerkCount = 0, PendingJerkCount = 0, StayPenaltyJerkCount = 0;
	int64 StateEnteredTick = 0, JerkTicks = 0;
	double RangeObservationSeconds = 0.0, StepSeconds = 0.0;
	bool bSeriesClosed = false, bReeling = false;
	TArray<TPair<ETRFishingState, ETRFishingState>> PendingTransitions;
	void PublishStateChanges();
	UPROPERTY(BlueprintReadOnly, Category = "TipRun|Fishing", meta = (AllowPrivateAccess = "true"))
	ETRFishingState State = ETRFishingState::Inactive;
	FTREgiSnapshot Snapshot;
	FTREquipmentSnapshot CastEquipment;
};
