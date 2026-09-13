#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/TREquipmentData.h"
#include "Data/TREvents.h"
#include "Data/TRSnapshots.h"
#include "Game/TRSimulationWorldSubsystem.h"
#include "TRFishingSessionActor.generated.h"

class UTRFishingComponent;

UCLASS()
class TIPRUNFISHINGUE5_API ATRFishingSessionActor : public AActor
{
	GENERATED_BODY()
public:
	ATRFishingSessionActor();
	// Configuration-time entry points may resolve assets; never invoke inside a fixed step.
	bool Initialize(UTRSimulationWorldSubsystem* Simulation, FTRActorSimId InBoatId,
		UDataTable* Egis, UDataTable* Sinkers, UTRFishingTuningDataAsset* Tuning,
		FName InitialSinkerId, TArray<FText>& Errors);
	ETRCommandResult TrySetEquipment(FName EgiId, FName SinkerId, TArray<FText>& Errors);
	ETRCommandResult StartFishing(TArray<FText>& Errors);
	bool SubmitCommand(ETRFishingCommandType Type, FTRCastId ExpectedCastId, int64 TargetTick = -1);
	ETRCommandResult AbortCast(FTRCastId ExpectedCastId);
	ETRCommandResult ResetCast();
	void EndFishing();

	UFUNCTION(BlueprintPure, Category = "TipRun|Session")
	ETRSessionPhase GetSessionPhase() const { return Phase; }
	UFUNCTION(BlueprintPure, Category = "TipRun|Session")
	bool IsEquipmentLocked() const { return bEquipmentLocked; }
	UFUNCTION(BlueprintPure, Category = "TipRun|Session")
	FTRCastId GetCastId() const { return CurrentCastId; }
	UFUNCTION(BlueprintPure, Category = "TipRun|Session")
	FTREquipmentSnapshot GetEquipmentSnapshot() const { return bEquipmentLocked ? LockedEquipment : SelectedEquipment; }
	UFUNCTION(BlueprintPure, Category = "TipRun|Session")
	FTRCatchResult GetLastResult() const { return LastResult; }
	bool HasResult() const { return bHasResult; }
	FTRActorSimId GetRegistrationId() const { return RegistrationId; }
	ETRCommandResult GetLastCommandResult() const { return LastCommandResult; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TipRun|Session")
	TObjectPtr<UTRFishingComponent> Fishing;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Destroyed() override;
private:
	void FixedStep(ETRSimulationPhase StepPhase, const FTRSimTime& Time);
	void HandleCommand(const FTRFishingCommand& Command);
	bool ReadEnvironment(FTRBoatSnapshot& Boat, FTROceanSample& Ocean) const;
	bool Register();
	void Unregister();
	void AbortInternal();
	void ResetInternal();
	void ReleaseSession();
	UPROPERTY() TObjectPtr<UDataTable> EgiTable;
	UPROPERTY() TObjectPtr<UDataTable> SinkerTable;
	UPROPERTY() TObjectPtr<UTRFishingTuningDataAsset> FishingTuning;
	TWeakObjectPtr<UTRSimulationWorldSubsystem> Coordinator;
	FTRActorSimId BoatId, RegistrationId;
	FTRCastId CurrentCastId;
	FTREquipmentSnapshot SelectedEquipment, LockedEquipment;
	FTRCatchResult LastResult;
	ETRSessionPhase Phase = ETRSessionPhase::Initializing;
	ETRCommandResult LastCommandResult = ETRCommandResult::RejectedInvalidState;
	int64 NextCastValue = 1;
	int64 LastSequence = 0;
	int64 LastCommandTick = -1;
	int64 CastStartTick = 0;
	bool bInitialized = false;
	bool bEquipmentLocked = false;
	bool bCastActive = false;
	bool bHasResult = false;
};
