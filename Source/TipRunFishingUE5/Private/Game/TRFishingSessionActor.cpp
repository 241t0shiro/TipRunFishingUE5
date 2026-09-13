#include "Game/TRFishingSessionActor.h"
#include "Fishing/TRFishingComponent.h"
#include "Ocean/TROceanWorldSubsystem.h"
#include "Engine/World.h"

ATRFishingSessionActor::ATRFishingSessionActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Fishing = CreateDefaultSubobject<UTRFishingComponent>(TEXT("Fishing"));
}
bool ATRFishingSessionActor::Initialize(UTRSimulationWorldSubsystem* Simulation, FTRActorSimId InBoatId,
	UDataTable* Egis, UDataTable* Sinkers, UTRFishingTuningDataAsset* Tuning, FName InitialSinkerId, TArray<FText>& Errors)
{
	if (bInitialized || IsActorBeingDestroyed() || !IsValid(Simulation) || !Simulation->IsInitialized() ||
		Simulation->GetWorld() != GetWorld() || !Simulation->GetSimulationTime().IsValid() || Simulation->IsInFixedStep())
	{
		Errors.Add(FText::FromString(TEXT("Session requires an uninitialized actor and a configured clock in the same world, outside fixed update")));
		return false;
	}
	FTREquipmentSnapshot Initial;
	if (!IsValid(Egis) || !IsValid(Sinkers) || !IsValid(Tuning) ||
		!TREquipment::TryBuildSnapshot(Egis, Sinkers, Tuning, TREquipment::InitialEgiId(), InitialSinkerId,
			Simulation->GetSimulationTime().StepSeconds, Initial, Errors))
	{
		Errors.Add(FText::FromString(TEXT("Session equipment references/initial selection are invalid")));
		Phase = ETRSessionPhase::Error;
		return false;
	}
	Coordinator = Simulation; BoatId = InBoatId;
	FTRBoatSnapshot Boat; FTROceanSample Ocean;
	if (!ReadEnvironment(Boat, Ocean))
	{
		Errors.Add(FText::FromString(TEXT("Session requires a registered boat and valid ocean at the rod tip")));
		Coordinator.Reset(); BoatId = {}; Phase = ETRSessionPhase::Error;
		return false;
	}
	EgiTable = Egis; SinkerTable = Sinkers; FishingTuning = Tuning;
	SelectedEquipment = Initial;
	bInitialized = true; Phase = ETRSessionPhase::Ready;
	return true;
}
bool ATRFishingSessionActor::ReadEnvironment(FTRBoatSnapshot& Boat, FTROceanSample& Ocean) const
{
	if (!Coordinator.IsValid() || !Coordinator->IsInitialized() || !Coordinator->GetBoatSnapshot(BoatId, Boat) || Boat.RodTipM.ContainsNaN()) { return false; }
	const UTROceanWorldSubsystem* Sea = GetWorld()->GetSubsystem<UTROceanWorldSubsystem>();
	if (!Sea) { return false; }
	FTROceanQuery Query;
	Query.PositionXYM = FVector2D(Boat.RodTipM.X, Boat.RodTipM.Y);
	Query.SimTick = Coordinator->GetSimulationTime().TickIndex;
	Ocean = Sea->SampleOcean(Query);
	return Ocean.bValid && FMath::IsFinite(Ocean.SurfaceZ_M) && Boat.RodTipM.Z >= double(Ocean.SurfaceZ_M);
}
ETRCommandResult ATRFishingSessionActor::TrySetEquipment(FName EgiId, FName SinkerId, TArray<FText>& Errors)
{
	if (!bInitialized || IsActorBeingDestroyed() || !Coordinator.IsValid() || !Coordinator->IsInitialized()) { return ETRCommandResult::RejectedInvalidState; }
	if (bEquipmentLocked || Coordinator->IsInFixedStep()) { return ETRCommandResult::RejectedBusy; }
	if (!IsValid(EgiTable) || !IsValid(SinkerTable) || !IsValid(FishingTuning)) { return ETRCommandResult::RejectedMissingData; }
	FTREquipmentSnapshot Candidate;
	if (!TREquipment::TryBuildSnapshot(EgiTable, SinkerTable, FishingTuning, EgiId, SinkerId,
		Coordinator->GetSimulationTime().StepSeconds, Candidate, Errors)) { return ETRCommandResult::RejectedMissingData; }
	SelectedEquipment = Candidate;
	return ETRCommandResult::Accepted;
}
bool ATRFishingSessionActor::Register()
{
	if (!Coordinator.IsValid() || !Coordinator->IsInitialized() || IsActorBeingDestroyed()) { return false; }
	RegistrationId = Coordinator->RegisterSession(this,
		FTRSimulationStep::CreateUObject(this, &ATRFishingSessionActor::FixedStep),
		FTRSimulationCommand::CreateUObject(this, &ATRFishingSessionActor::HandleCommand));
	return RegistrationId.IsValid();
}
void ATRFishingSessionActor::Unregister()
{
	if (Coordinator.IsValid()) { Coordinator->Unregister(RegistrationId); }
	RegistrationId = {};
}
ETRCommandResult ATRFishingSessionActor::StartFishing(TArray<FText>& Errors)
{
	if (!bInitialized || IsActorBeingDestroyed() || bEquipmentLocked || !Coordinator.IsValid() || !Coordinator->IsInitialized()) { return ETRCommandResult::RejectedInvalidState; }
	if (Coordinator->IsInFixedStep() || Coordinator->IsSimulationPaused()) { return ETRCommandResult::RejectedBusy; }
	FTRBoatSnapshot Boat; FTROceanSample Ocean;
	if (!ReadEnvironment(Boat, Ocean)) { return ETRCommandResult::RejectedInvalidEnvironment; }
	if (!IsValid(EgiTable) || !IsValid(SinkerTable) || !IsValid(FishingTuning)) { return ETRCommandResult::RejectedMissingData; }
	FTREquipmentSnapshot Candidate;
	if (!TREquipment::TryBuildSnapshot(EgiTable, SinkerTable, FishingTuning, SelectedEquipment.EgiId,
		SelectedEquipment.SinkerId, Coordinator->GetSimulationTime().StepSeconds, Candidate, Errors)) { return ETRCommandResult::RejectedMissingData; }
	if (!Register()) { return ETRCommandResult::RejectedInvalidState; }
	LockedEquipment = Candidate; bEquipmentLocked = true; bHasResult = false; LastResult = {};
	Fishing->Prepare(); Phase = ETRSessionPhase::Ready;
	return ETRCommandResult::Accepted;
}
bool ATRFishingSessionActor::SubmitCommand(ETRFishingCommandType Type, FTRCastId ExpectedCastId, int64 TargetTick)
{
	return bInitialized && bEquipmentLocked && !IsActorBeingDestroyed() && Coordinator.IsValid() &&
		Coordinator->EnqueueCommand(RegistrationId, Type, 0.0f, TargetTick, ExpectedCastId);
}
void ATRFishingSessionActor::HandleCommand(const FTRFishingCommand& Command)
{
	LastCommandResult = ETRCommandResult::RejectedInvalidState;
	if (!bInitialized || !bEquipmentLocked || !Coordinator.IsValid() || IsActorBeingDestroyed() ||
		Command.Sequence <= 0 || Command.TargetTick < LastCommandTick ||
		(Command.TargetTick == LastCommandTick && Command.Sequence <= LastSequence) || Command.ExpectedCastId != CurrentCastId ||
		Command.TargetTick > Coordinator->GetSimulationTime().TickIndex) { return; }
	LastSequence = Command.Sequence;
	LastCommandTick = Command.TargetTick;
	switch (Command.Type)
	{
	case ETRFishingCommandType::Deploy:
		if (Fishing->GetState() == ETRFishingState::Ready && NextCastValue < MAX_int64)
		{
			FTRBoatSnapshot Boat; FTROceanSample Ocean;
			if (!ReadEnvironment(Boat, Ocean)) { LastCommandResult = ETRCommandResult::RejectedInvalidEnvironment; break; }
			CurrentCastId = FTRCastId(NextCastValue++);
			CastStartTick = Coordinator->GetSimulationTime().TickIndex;
			bCastActive = true; bHasResult = false; LastResult = {};
			Fishing->BeginCast(CurrentCastId, CastStartTick, LockedEquipment);
			LastCommandResult = ETRCommandResult::Accepted;
		}
		break;
	case ETRFishingCommandType::NextCast:
		if (bHasResult && !bCastActive) { ResetInternal(); LastCommandResult = ETRCommandResult::Accepted; }
		break;
	case ETRFishingCommandType::EndFishing:
		EndFishing(); LastCommandResult = ETRCommandResult::Accepted;
		break;
	default: break; // Operations requiring M07+ are not implemented or silently accepted.
	}
}
void ATRFishingSessionActor::FixedStep(ETRSimulationPhase StepPhase, const FTRSimTime& Time)
{
	if (StepPhase != ETRSimulationPhase::Fishing || !bCastActive || IsActorBeingDestroyed()) { return; }
	FTRBoatSnapshot Boat; FTROceanSample Ocean;
	if (!ReadEnvironment(Boat, Ocean)) { AbortInternal(); return; }
	if (Fishing->GetState() == ETRFishingState::Deploying)
	{
		if (!Fishing->CompleteDeployment(Boat, Ocean, Time.TickIndex)) { AbortInternal(); return; }
		Phase = ETRSessionPhase::Fishing;
	}
	// M07 will own Egi integration. Never move the deployment snapshot here.
}
void ATRFishingSessionActor::AbortInternal()
{
	if (!bCastActive) { return; }
	bCastActive = false; bHasResult = true;
	LastResult = {}; LastResult.CastId = CurrentCastId;
	LastResult.EgiId = LockedEquipment.EgiId; LastResult.SinkerId = LockedEquipment.SinkerId;
	LastResult.Outcome = ETRCastOutcome::Aborted;
	if (Coordinator.IsValid())
	{
		LastResult.ElapsedSimSeconds = double(Coordinator->GetSimulationTime().TickIndex - CastStartTick) * Coordinator->GetSimulationTime().StepSeconds;
	}
	Fishing->FinishCast(); Phase = ETRSessionPhase::Result;
	Unregister(); // Invalidates all queued and already-extracted commands for this registration.
	if (!IsActorBeingDestroyed()) { Register(); }
}
ETRCommandResult ATRFishingSessionActor::AbortCast(FTRCastId ExpectedCastId)
{
	if (!bCastActive || ExpectedCastId != CurrentCastId || IsActorBeingDestroyed() || !Coordinator.IsValid() || !Coordinator->IsInitialized()) { return ETRCommandResult::RejectedInvalidState; }
	AbortInternal(); return ETRCommandResult::Accepted;
}
void ATRFishingSessionActor::ResetInternal()
{
	bHasResult = false; LastResult = {}; Fishing->Prepare(); Phase = ETRSessionPhase::Ready;
	Unregister(); Register(); // New queue generation, unchanged equipment lock and cast counter.
}
ETRCommandResult ATRFishingSessionActor::ResetCast()
{
	if (!bInitialized || !bEquipmentLocked || !bHasResult || bCastActive || IsActorBeingDestroyed() || !Coordinator.IsValid() || !Coordinator->IsInitialized()) { return ETRCommandResult::RejectedInvalidState; }
	ResetInternal(); return ETRCommandResult::Accepted;
}
void ATRFishingSessionActor::EndFishing()
{
	if (bCastActive) { AbortInternal(); }
	Unregister();
	bEquipmentLocked = false; LockedEquipment = {};
	Fishing->Stop(); Phase = bInitialized ? ETRSessionPhase::Ready : ETRSessionPhase::Initializing;
}
void ATRFishingSessionActor::ReleaseSession()
{
	EndFishing();
	bInitialized = false;
	Coordinator.Reset(); BoatId = {};
	EgiTable = nullptr; SinkerTable = nullptr; FishingTuning = nullptr;
}
void ATRFishingSessionActor::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseSession();
	Super::EndPlay(Reason);
}
void ATRFishingSessionActor::Destroyed()
{
	// Initialization/registration may happen before BeginPlay. RouteEndPlay does not
	// invoke EndPlay in that case, so explicit destruction must also release resources.
	ReleaseSession();
	Super::Destroyed();
}
