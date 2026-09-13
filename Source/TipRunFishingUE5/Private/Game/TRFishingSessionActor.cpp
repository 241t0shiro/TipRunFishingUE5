#include "Game/TRFishingSessionActor.h"
#include "Fishing/TRFishingComponent.h"
#include "Fishing/TREgiSimulationComponent.h"
#include "Fishing/TREgiActor.h"
#include "Engine/StaticMesh.h"
#include "Ocean/TROceanWorldSubsystem.h"
#include "Engine/World.h"

ATRFishingSessionActor::ATRFishingSessionActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Fishing = CreateDefaultSubobject<UTRFishingComponent>(TEXT("Fishing"));
	EgiSimulation = CreateDefaultSubobject<UTREgiSimulationComponent>(TEXT("EgiSimulation"));
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
	if (!CanChangeEquipment() || Coordinator->IsInFixedStep()) { return ETRCommandResult::RejectedBusy; }
	if (!IsValid(EgiTable) || !IsValid(SinkerTable) || !IsValid(FishingTuning)) { return ETRCommandResult::RejectedMissingData; }
	FTREquipmentSnapshot Candidate;
	if (!TREquipment::TryBuildSnapshot(EgiTable, SinkerTable, FishingTuning, EgiId, SinkerId,
		Coordinator->GetSimulationTime().StepSeconds, Candidate, Errors)) { return ETRCommandResult::RejectedMissingData; }
	const FTREgiSpecRow* Row = EgiTable->FindRow<FTREgiSpecRow>(EgiId, TEXT("Prepare next cast visual"), false);
	UStaticMesh* Mesh = Row ? Row->Mesh.LoadSynchronous() : nullptr;
	if (!IsValid(Mesh)) { return ETRCommandResult::RejectedMissingData; }
	SelectedEquipment = Candidate; SelectedEgiMesh = Mesh;
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
	if (!bInitialized || bFishingStarted || !bEgiOnboard || IsActorBeingDestroyed() || !Coordinator.IsValid()) { return ETRCommandResult::RejectedInvalidState; }
	if (Coordinator->IsInFixedStep() || Coordinator->IsSimulationPaused()) { return ETRCommandResult::RejectedBusy; }
	FTRBoatSnapshot Boat; FTROceanSample Ocean;
	if (!ReadEnvironment(Boat, Ocean)) { return ETRCommandResult::RejectedInvalidEnvironment; }
	const auto Prepared = TrySetEquipment(SelectedEquipment.EgiId, SelectedEquipment.SinkerId, Errors);
	if (Prepared != ETRCommandResult::Accepted) { return Prepared; }
	if (!Register()) { return ETRCommandResult::RejectedInvalidState; }
	bFishingStarted = true; bEquipmentLocked = false;
	Fishing->Prepare(); Phase = ETRSessionPhase::Ready;
	CaptureHUD(Coordinator->GetSimulationTime()); return ETRCommandResult::Accepted;
}bool ATRFishingSessionActor::SubmitCommand(ETRFishingCommandType Type, FTRCastId ExpectedCastId, int64 TargetTick)
{
	return bInitialized && bFishingStarted && !IsActorBeingDestroyed() && Coordinator.IsValid() &&
		Coordinator->EnqueueCommand(RegistrationId, Type, 0.0f, TargetTick, ExpectedCastId);
}
void ATRFishingSessionActor::ProcessCommand(const FTRFishingCommand& Command)
{
	LastCommandResult = ETRCommandResult::RejectedInvalidState;
	if (!bInitialized || !bFishingStarted || !Coordinator.IsValid() || IsActorBeingDestroyed() ||
		Command.Sequence <= 0 || Command.TargetTick < LastCommandTick ||
		(Command.TargetTick == LastCommandTick && Command.Sequence <= LastSequence) || Command.ExpectedCastId != CurrentCastId ||
		Command.TargetTick > Coordinator->GetSimulationTime().TickIndex) { return; }
	LastSequence = Command.Sequence;
	LastCommandTick = Command.TargetTick;
	switch (Command.Type)
	{
	case ETRFishingCommandType::Deploy:
		if (Fishing->GetState() == ETRFishingState::Ready && CanChangeEquipment() && IsValid(SelectedEgiMesh) && NextCastValue < MAX_int64)
		{
			FTRBoatSnapshot Boat; FTROceanSample Ocean;
			if (!ReadEnvironment(Boat, Ocean)) { LastCommandResult = ETRCommandResult::RejectedInvalidEnvironment; break; }
			LockedEquipment = SelectedEquipment; LockedEgiMesh = SelectedEgiMesh; bEquipmentLocked = true; bEgiOnboard = false;
			CurrentCastId = FTRCastId(NextCastValue++);
			CastStartTick = Coordinator->GetSimulationTime().TickIndex;
			bCastActive = true; bHasResult = false;
			Fishing->BeginCast(CurrentCastId, Coordinator->GetSimulationTime(), LockedEquipment);
			LastCommandResult = ETRCommandResult::Accepted;
		}
		break;
	case ETRFishingCommandType::NextCast:
		if (bHasResult && !bCastActive) { ResetInternal(); LastCommandResult = ETRCommandResult::Accepted; }
		break;
	case ETRFishingCommandType::EndFishing:
		EndFishing(); LastCommandResult = ETRCommandResult::Accepted;
		break;
	default: if (bCastActive) { LastCommandResult = Fishing->HandleCommand(Command, Coordinator->GetSimulationTime()); } break;
	}
}
void ATRFishingSessionActor::FixedStep(ETRSimulationPhase StepPhase, const FTRSimTime& Time)
{
	if (StepPhase == ETRSimulationPhase::Publish && !IsActorBeingDestroyed())
	{
		CaptureHUD(Time);
		Fishing->PublishStateChanges();
		if (bPendingResultNotification)
		{
			bPendingResultNotification = false; const FTRCatchResult Copy = LastResult; OnCastCompleted.Broadcast(Copy);
		}
		return;
	}
	if (StepPhase != ETRSimulationPhase::Fishing || !bCastActive || IsActorBeingDestroyed()) { return; }
	FTRBoatSnapshot Boat; FTROceanSample Ocean;
	if (!ReadEnvironment(Boat, Ocean)) { AbortInternal(); return; }
	if (Fishing->GetState() == ETRFishingState::Deploying)
	{
		if (!Fishing->CompleteDeployment(Boat, Ocean, Time.TickIndex)) { AbortInternal(); return; }
		TArray<FText> Errors;
		if (!EgiSimulation->InitializeCast(Fishing->GetSnapshot(), LockedEquipment, Ocean, Errors)) { AbortInternal(); return; }
		FActorSpawnParameters SpawnParameters; SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ActiveEgi = GetWorld()->SpawnActor<ATREgiActor>(ATREgiActor::StaticClass(), FTransform::Identity, SpawnParameters);
		if (!IsValid(ActiveEgi) || !ActiveEgi->InitializeVisual(CurrentCastId, LockedEgiMesh) ||
			!ActiveEgi->ApplySimulationSnapshot(Fishing->GetSnapshot(), Ocean.SurfaceZ_M)) { AbortInternal(); return; }
		Phase = ETRSessionPhase::Fishing;
		return; // Preserve deployment tick at depth zero. Integration starts next fixed tick.
	}
	if (!IsValid(ActiveEgi) || ActiveEgi->IsActorBeingDestroyed()) { AbortInternal(); return; }
	Fishing->PrepareStep(Time);
	const FTREgiSnapshot Before = EgiSimulation->BuildSnapshot(Fishing->GetState());
	FTROceanQuery Query; Query.PositionXYM = Before.PositionXYM; Query.DepthM = Before.DepthM; Query.SimTick = Time.TickIndex;
	Ocean = GetWorld()->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(Query);
	FTROceanSample DestinationOcean = Ocean;
	const ETREgiStepEvent Event = EgiSimulation->StepEgi(CurrentCastId, Time, Ocean, Boat, Fishing->GetAction(),
		[this, &DestinationOcean](const FTROceanQuery& DestinationQuery)
		{
			DestinationOcean = GetWorld()->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(DestinationQuery);
			return DestinationOcean;
		});
	if (Event == ETREgiStepEvent::EnvironmentInvalid) { AbortInternal(); return; }
	Fishing->ApplyEgiStep(EgiSimulation->BuildSnapshot(Fishing->GetState()), Event, EgiSimulation->IsTransientComplete());
	if (Event == ETREgiStepEvent::Retrieved) { FinishInternal(ETRCastOutcome::Retrieved, true); return; }
	if (!ActiveEgi->ApplySimulationSnapshot(Fishing->GetSnapshot(), DestinationOcean.SurfaceZ_M)) { AbortInternal(); }
}
void ATRFishingSessionActor::AbortInternal() { FinishInternal(ETRCastOutcome::Aborted, false); }
void ATRFishingSessionActor::FinishInternal(ETRCastOutcome Outcome, bool bReturnedOnboard)
{
	if (!bCastActive) { return; }
	bCastActive = false; bHasResult = true;
	ReleaseEgi();
	LastResult = {}; LastResult.CastId = CurrentCastId;
	LastResult.EgiId = LockedEquipment.EgiId; LastResult.SinkerId = LockedEquipment.SinkerId;
	LastResult.Outcome = Outcome; LastResultEquipment = LockedEquipment; bEgiOnboard = bReturnedOnboard; bPendingResultNotification = true;
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
	Fishing->Prepare(); Phase = ETRSessionPhase::Ready; bEquipmentLocked = !bEgiOnboard;
	CaptureHUD(Coordinator->GetSimulationTime());
	Unregister(); Register(); // New queue generation, unchanged equipment lock and cast counter.
}
ETRCommandResult ATRFishingSessionActor::ResetCast()
{
	if (!bInitialized || !bFishingStarted || !bHasResult || bCastActive || IsActorBeingDestroyed() || !Coordinator.IsValid() || !Coordinator->IsInitialized()) { return ETRCommandResult::RejectedInvalidState; }
	ResetInternal(); return ETRCommandResult::Accepted;
}
void ATRFishingSessionActor::EndFishing()
{
	if (bCastActive) { AbortInternal(); }
	Unregister();
	bFishingStarted = false; bEquipmentLocked = !bEgiOnboard;
	ReleaseEgi(); LockedEgiMesh = nullptr;
	Fishing->Stop(); Phase = bInitialized ? ETRSessionPhase::Ready : ETRSessionPhase::Initializing;
	// A stopped session has no later Publish phase. Deliver its pending terminal event once.
	if (bPendingResultNotification && !IsActorBeingDestroyed())
	{
		bPendingResultNotification = false; const FTRCatchResult Copy = LastResult; OnCastCompleted.Broadcast(Copy);
	}
}
void ATRFishingSessionActor::ReleaseSession()
{
	EndFishing();
	bInitialized = false;
	Coordinator.Reset(); BoatId = {};
	EgiTable = nullptr; SinkerTable = nullptr; FishingTuning = nullptr;
	Fishing->OnFishingStateChanged.Clear();
	OnCommandProcessed.Clear(); OnCastCompleted.Clear(); SelectedEgiMesh = nullptr;
	PublishedHUD = {};
}
void ATRFishingSessionActor::ReleaseEgi()
{
	EgiSimulation->Reset();
	ATREgiActor* Previous = ActiveEgi.Get(); ActiveEgi = nullptr;
	if (IsValid(Previous) && !Previous->IsActorBeingDestroyed()) { Previous->Destroy(); }
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

void ATRFishingSessionActor::HandleCommand(const FTRFishingCommand& Command)
{
	ProcessCommand(Command);
	if (!IsActorBeingDestroyed()) { OnCommandProcessed.Broadcast(Command, LastCommandResult); }
}
bool ATRFishingSessionActor::IsAcceptingPlayerInput() const
{
	return bInitialized && bFishingStarted && !IsActorBeingDestroyed() && Coordinator.IsValid() && Coordinator->IsRegistered(RegistrationId);
}
bool ATRFishingSessionActor::IsPlayerPaused() const { return !Coordinator.IsValid() || Coordinator->IsSimulationPaused(); }
void ATRFishingSessionActor::SetPlayerPaused(bool bPaused) { if (Coordinator.IsValid()) { Coordinator->SetSimulationPaused(bPaused); } }
void ATRFishingSessionActor::ClearPlayerCommands() { if (Coordinator.IsValid()) { Coordinator->ClearCommands(); } } // MVP: one local session.
void ATRFishingSessionActor::CaptureHUD(const FTRSimTime& Time)
{
	PublishedHUD = {};
	if (!bInitialized || !Coordinator.IsValid()) { return; }
	PublishedHUD.Egi = Fishing->GetSnapshot();
	if (!Coordinator->GetBoatSnapshot(BoatId, PublishedHUD.Boat)) { return; }
	FTROceanQuery Q; Q.SimTick = Time.TickIndex;
	Q.PositionXYM = bCastActive ? PublishedHUD.Egi.PositionXYM : FVector2D(PublishedHUD.Boat.RodTipM.X, PublishedHUD.Boat.RodTipM.Y);
	Q.DepthM = bCastActive ? PublishedHUD.Egi.DepthM : 0.0f;
	if (const auto* Sea = GetWorld()->GetSubsystem<UTROceanWorldSubsystem>()) { PublishedHUD.Ocean = Sea->SampleOcean(Q); }
	PublishedHUD.bEnvironmentValid = PublishedHUD.Ocean.bValid;
}
FTRHUDSnapshot ATRFishingSessionActor::GetHUDSnapshot() const
{
	if (!bInitialized || IsActorBeingDestroyed()) { return {}; }
	FTRHUDSnapshot Copy = PublishedHUD;
	Copy.bSessionValid = true; Copy.bEgiValid = bCastActive; Copy.bEquipmentLocked = bEquipmentLocked;
	Copy.bEgiOnboard = bEgiOnboard; Copy.bCanChangeEquipment = CanChangeEquipment();
	Copy.Phase = Phase; Copy.CastId = CurrentCastId; Copy.Equipment = GetEquipmentSnapshot();
	Copy.Egi.FishingState = Fishing->GetState();
	return Copy;
}

bool ATRFishingSessionActor::CanChangeEquipment() const
{
	return bInitialized && !IsActorBeingDestroyed() && bEgiOnboard && !bCastActive && !bEquipmentLocked &&
		Phase == ETRSessionPhase::Ready && Coordinator.IsValid() && !Coordinator->IsSimulationPaused();
}
