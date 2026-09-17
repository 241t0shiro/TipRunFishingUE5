#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/TREvents.h"
#include "Data/TRSimulationTypes.h"
#include "Data/TRSnapshots.h"
#include "TRSimulationWorldSubsystem.generated.h"

class AActor;
class ATRBoatPawn;
class UTRSessionConfigDataAsset;

// Scheduling contract only: no boat, fishing, AI or bite logic is implemented here.
enum class ETRSimulationPhase : uint8
{
	Timers, Boat, Fishing, Squid, BiteResolution, Fight, Publish
};
DECLARE_DELEGATE_TwoParams(FTRSimulationStep, ETRSimulationPhase, const FTRSimTime&);
DECLARE_DELEGATE_OneParam(FTRSimulationCommand, const FTRFishingCommand&);

UCLASS()
class TIPRUNFISHINGUE5_API UTRSimulationWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// Tick during engine pause only to discard input; the fixed clock remains stopped.
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual void Deinitialize() override;

	// One configuration snapshot per world lifetime; casts cannot reset the clock/IDs.
	bool Configure(const UTRSessionConfigDataAsset* Config, TArray<FText>& Errors);
	FTRActorSimId RegisterSession(AActor* Owner, FTRSimulationStep Step, FTRSimulationCommand Command);
	FTRActorSimId RegisterSquid(AActor* Owner, FTRSimulationStep Step);
	FTRActorSimId RegisterBoat(ATRBoatPawn* Boat, const FVector2D& InitialXYM, float HeadingRad, TArray<FText>& Errors);
	bool GetBoatSnapshot(FTRActorSimId BoatId, FTRBoatSnapshot& OutSnapshot) const;
	// Call from participant EndPlay. Repeated removal is safe; dead actors are also pruned.
	void Unregister(FTRActorSimId Id);
	bool IsRegistered(FTRActorSimId Id) const;

	// Live input uses -1. Explicit future ticks support deterministic replay; past ticks
	// are rejected. The coordinator assigns a world-global sequence, never caller input.
	bool EnqueueCommand(FTRActorSimId SessionId, ETRFishingCommandType Type, float AxisValue = 0.0f,
		int64 TargetTick = -1, FTRCastId ExpectedCastId = {}, FVector2D Axis2D = FVector2D::ZeroVector, int64 ExpectedModeEpoch = 0);
	bool IsInFixedStep() const { return bAdvancing; }
	void ClearCommands();
	void SetSimulationPaused(bool bPaused);
	bool IsSimulationPaused() const;
	void AdvanceFrame(double DeltaSeconds);
	FTRSimTime GetSimulationTime() const { return Time; } // next unprocessed tick
	int64 GetCatchUpDropCount() const { return CatchUpDropCount; }
	double GetAccumulatorSeconds() const { return AccumulatorSeconds; }
	int32 GetQueuedCommandCount() const { return Commands.Num(); }
	// Initialize one stream per participant/purpose and retain it in its owning system.
	// Calling again intentionally reconstructs that stream's initial state (replay).
	bool CreateRandomStream(FTRActorSimId Id, uint32 PurposeId, FRandomStream& OutStream) const;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
	struct FRegistration
	{
		FTRActorSimId Id;
		TWeakObjectPtr<AActor> Owner;
		bool bSquid = false;
		bool bBoat = false;
		FTRSimulationStep Step;
		FTRSimulationCommand Command;
	};
	struct FQueuedCommand
	{
		FTRActorSimId SessionId;
		FTRFishingCommand Value;
	};
	FTRActorSimId Register(AActor* Owner, bool bSquid, FTRSimulationStep Step, FTRSimulationCommand Command, bool bBoat = false);
	void StepRegisteredBoat(ETRSimulationPhase Phase, const FTRSimTime& StepTime, TWeakObjectPtr<ATRBoatPawn> Boat);
	UFUNCTION()
	void HandleBoatEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	const FRegistration* FindLive(FTRActorSimId Id) const;
	void AdvanceFixedStep();
	void Dispatch(ETRSimulationPhase Phase, const TArray<FTRActorSimId>& Participants);
	TArray<FRegistration> Registrations;
	TArray<FQueuedCommand> Commands;
	FTRSimTime Time;
	double AccumulatorSeconds = 0.0;
	int32 MaxCatchUpSteps = 0;
	int32 SessionSeed = 0;
	int64 NextId = 1;
	int64 NextSequence = 1;
	int64 CatchUpDropCount = 0;
	uint64 CommandEpoch = 0;
	bool bConfigured = false;
	bool bPaused = false;
	bool bAdvancing = false;
};
