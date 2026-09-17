#include "Game/TRFishingSessionActor.h"
#include "Fishing/TRFishingComponent.h"
#include "Fishing/TREgiSimulationComponent.h"
#include "Fishing/TREgiActor.h"
#include "Fishing/TRRodControlComponent.h"
#include "Engine/StaticMesh.h"
#include "Ocean/TROceanWorldSubsystem.h"
#include "Engine/World.h"

ATRFishingSessionActor::ATRFishingSessionActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PlayerMode = CreateDefaultSubobject<UTRPlayerModeComponent>(TEXT("PlayerMode"));
	Fishing = CreateDefaultSubobject<UTRFishingComponent>(TEXT("Fishing"));
	EgiSimulation = CreateDefaultSubobject<UTREgiSimulationComponent>(TEXT("EgiSimulation"));
	RodControl = CreateDefaultSubobject<UTRRodControlComponent>(TEXT("RodControl"));
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
	if(RodTuning && (Initial.Parameters.EgiModelRevision!=2 || !RodControl->Initialize(RodTuning->Parameters,Simulation->GetSimulationTime().StepSeconds,Errors)))
	{ Coordinator.Reset(); Phase=ETRSessionPhase::Error;return false; }
	if(RodTuning && Boat.PositionM.Z+RodTuning->Parameters.MountOffsetM.Z+RodTuning->Parameters.LengthM*FMath::Sin(RodTuning->Parameters.MinPitchRad)<Ocean.SurfaceZ_M)
	{Errors.Add(FText::FromString(TEXT("Rod minimum pitch places tip below surface")));RodControl->Reset();Coordinator.Reset();Phase=ETRSessionPhase::Error;return false;}
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
	PlayerMode->Start();
	Fishing->Prepare(); Phase = ETRSessionPhase::Ready;
	CaptureHUD(Coordinator->GetSimulationTime()); return ETRCommandResult::Accepted;
}bool ATRFishingSessionActor::SubmitCommand(ETRFishingCommandType Type, FTRCastId ExpectedCastId, int64 TargetTick)
{
	return bInitialized && bFishingStarted && !IsActorBeingDestroyed() && Coordinator.IsValid() &&
		Coordinator->EnqueueCommand(RegistrationId, Type, 0.0f, TargetTick, ExpectedCastId, FVector2D::ZeroVector, PlayerMode->GetSnapshot().ModeEpoch);
}
bool ATRFishingSessionActor::SubmitRodAim(FVector2D Delta,FTRCastId ExpectedCastId,FTRActorSimId ExpectedRegistration,int64 TargetTick)
{
	return IsInputModeAllowed(ETRPlayerMode::Fishing) && RodControl->IsInitialized() && ExpectedCastId==CurrentCastId && ExpectedRegistration==RegistrationId &&
		Coordinator->EnqueueCommand(RegistrationId,ETRFishingCommandType::RodAim,0,TargetTick,ExpectedCastId,Delta,PlayerMode->GetSnapshot().ModeEpoch);
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
	if (Command.ExpectedModeEpoch != PlayerMode->GetSnapshot().ModeEpoch)
	{
		PlayerMode->Reject(ETRModeChangeRejection::StaleInput);
		return;
	}
	ConsumeInputReset();
	if (Command.Type == ETRFishingCommandType::StartFishingMode || Command.Type == ETRFishingCommandType::ReturnNavigationMode)
	{
		const ETRPlayerMode Target = Command.Type == ETRFishingCommandType::StartFishingMode ? ETRPlayerMode::Fishing : ETRPlayerMode::Navigation;
		const ETRModeChangeRejection Rejection = GetModeChangeRejection(Target);
		if (Rejection != ETRModeChangeRejection::None)
		{
			PlayerMode->Reject(Rejection);
			LastCommandResult = ETRCommandResult::RejectedBusy;
			return;
		}
		PlayerMode->Transition(Target, Coordinator->GetSimulationTime().TickIndex);
		Fishing->PendingJerkCount = 0;
		bClearRodReservations = false;
		bClearNormalRetrieve = false;
		// Epoch rejects old due/future inputs without clearing another session's queue.
		// No boat writes: R2 must consume the stop/heading policy before integrating.
		LastCommandResult = ETRCommandResult::Accepted;
		return;
	}
	if (!IsInputModeAllowed(ETRPlayerMode::Fishing) && Command.Type != ETRFishingCommandType::EndFishing) { return; }
	if (Fishing->GetState() == ETRFishingState::QuickRetrieving && Command.Type != ETRFishingCommandType::EndFishing)
	{
		LastCommandResult = ETRCommandResult::RejectedBusy; return;
	}
	switch (Command.Type)
	{
	case ETRFishingCommandType::RodAim:
		if((bCastActive || Phase==ETRSessionPhase::Ready) && RodControl->ApplyAim(Command.Axis2D,Coordinator->GetSimulationTime()))
		{LastCommandResult=ETRCommandResult::Accepted;} break;
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
			if(RodControl->IsInitialized()){Fishing->JerkTicks=RodControl->GetJerkTicks();}
			LastCommandResult = ETRCommandResult::Accepted;
		}
		break;
	case ETRFishingCommandType::NextCast:
		if (bHasResult && !bCastActive) { ResetInternal(); LastCommandResult = ETRCommandResult::Accepted; }
		break;
	case ETRFishingCommandType::EndFishing:
		EndFishing(); LastCommandResult = ETRCommandResult::Accepted;
		break;
	default: if (bCastActive) { LastCommandResult = Fishing->HandleCommand(Command, Coordinator->GetSimulationTime());
		if(RodControl->IsInitialized()){RodControl->ObserveOperationStart(Fishing->GetSnapshot());} } break;
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
	if (StepPhase != ETRSimulationPhase::Fishing || IsActorBeingDestroyed() ||
		PlayerMode->GetSnapshot().Mode != ETRPlayerMode::Fishing || (!bCastActive && !RodControl->IsInitialized())) { return; }
	FTRBoatSnapshot Boat; FTROceanSample Ocean;
	if (!ReadEnvironment(Boat, Ocean)) { AbortInternal(); return; }
	ConsumeInputReset();
	if(bCastActive){Fishing->PrepareStep(Time);}
	if (bCastActive && Fishing->GetState() == ETRFishingState::QuickRetrieving)
	{
		if (!IsValid(ActiveEgi) || ActiveEgi->IsActorBeingDestroyed()) { AbortInternal(); return; }
		// Tempo return: preserve the last physical sample, do not integrate or lerp the Egi.
		// Boat continues in its own phase; the snapshot explicitly marks water evaluation inactive.
		if (Fishing->IsQuickRetrieveComplete(Time)) { FinishInternal(ETRCastOutcome::Retrieved, true, true); }
		return;
	}
	if(RodControl->IsInitialized())
	{
		auto RodFishing=Fishing->GetSnapshot();RodFishing.CastId=CurrentCastId;
		if(!RodControl->Step(Time,Boat,RodFishing,Ocean.SurfaceZ_M)){AbortInternal();return;}
		Boat.RodTipM=RodControl->GetSnapshot().TipWorldPositionM;
		FTROceanQuery RodQuery;RodQuery.PositionXYM=FVector2D(Boat.RodTipM.X,Boat.RodTipM.Y);RodQuery.SimTick=Time.TickIndex;
		Ocean=GetWorld()->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(RodQuery);
		if(!Ocean.bValid || Boat.RodTipM.Z<Ocean.SurfaceZ_M){AbortInternal();return;}
	}
	if(!bCastActive){return;}
	if (Fishing->GetState() == ETRFishingState::Deploying)
	{
		if (!Fishing->CompleteDeployment(Boat, Ocean, Time.TickIndex)) { AbortInternal(); return; }
		TArray<FText> Errors;
		if (!EgiSimulation->InitializeCast(Fishing->GetSnapshot(), LockedEquipment, Ocean, Errors, &Boat)) { AbortInternal(); return; }
		FActorSpawnParameters SpawnParameters; SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ActiveEgi = GetWorld()->SpawnActor<ATREgiActor>(ATREgiActor::StaticClass(), FTransform::Identity, SpawnParameters);
		if (!IsValid(ActiveEgi) || !ActiveEgi->InitializeVisual(CurrentCastId, LockedEgiMesh) ||
			!ActiveEgi->ApplySimulationSnapshot(Fishing->GetSnapshot(), Ocean.SurfaceZ_M)) { AbortInternal(); return; }
		Phase = ETRSessionPhase::Fishing;
		return; // Preserve deployment tick at depth zero. Integration starts next fixed tick.
	}
	if (!IsValid(ActiveEgi) || ActiveEgi->IsActorBeingDestroyed()) { AbortInternal(); return; }
	const FTREgiSnapshot Before = EgiSimulation->BuildSnapshot(Fishing->GetState());
	FTROceanQuery Query; Query.PositionXYM = Before.PositionXYM; Query.DepthM = Before.DepthM; Query.SimTick = Time.TickIndex;
	Ocean = GetWorld()->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(Query);
	FTROceanSample DestinationOcean = Ocean;
	auto Action=Fishing->GetAction();
	if(RodControl->IsInitialized())
	{
		Action.LiftMps=0;
		if(Action.FishingState==ETRFishingState::Jerking)
		{
			Action.ReelMps=RodControl->GetReelPulseMps(Time,Fishing->GetSnapshot());
			// Reel mode also preserves the minimum rod-to-surface span during the up phase.
			Action.LineMode=ETRLineMode::ReelIn;
		}
	}
	const ETREgiStepEvent Event = EgiSimulation->StepEgi(CurrentCastId, Time, Ocean, Boat, Action,
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
void ATRFishingSessionActor::FinishInternal(ETRCastOutcome Outcome, bool bReturnedOnboard, bool bQuickReturned)
{
	if (!bCastActive) { return; }
	bCastActive = false; bHasResult = true;
	ReleaseEgi();
	LastResult = {}; LastResult.CastId = CurrentCastId;
	LastResult.bQuickRetrieved = bQuickReturned;
	LastResult.EgiId = LockedEquipment.EgiId; LastResult.SinkerId = LockedEquipment.SinkerId;
	LastResult.Outcome = Outcome; LastResultEquipment = LockedEquipment; bEgiOnboard = bReturnedOnboard; bPendingResultNotification = true;
	if (Coordinator.IsValid())
	{
		LastResult.ElapsedSimSeconds = double(Coordinator->GetSimulationTime().TickIndex - CastStartTick) * Coordinator->GetSimulationTime().StepSeconds;
	}
	Fishing->FinishCast(Coordinator.IsValid() ? Coordinator->GetSimulationTime().TickIndex : LastCommandTick, bQuickReturned);
	Phase = bQuickReturned ? ETRSessionPhase::Ready : ETRSessionPhase::Result;
	if (bQuickReturned) { bEquipmentLocked = false; }
	RodControl->InvalidateSnapshot(); bClearRodReservations=false; bClearNormalRetrieve=false;
	if (Coordinator.IsValid()) { CaptureHUD(Coordinator->GetSimulationTime()); }
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
	PlayerMode->Stop();
	ReleaseEgi(); LockedEgiMesh = nullptr;
	Fishing->Stop(); Phase = bInitialized ? ETRSessionPhase::Ready : ETRSessionPhase::Initializing;
	RodControl->InvalidateSnapshot(); bClearRodReservations=false; bClearNormalRetrieve=false;
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
	RodControl->Reset();
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
void ATRFishingSessionActor::ClearPlayerCommands()
{
	if (Coordinator.IsValid()) { Coordinator->ClearCommands(); }
	if (RodControl->IsInitialized()) { bClearRodReservations = true; }
	bClearNormalRetrieve = true; // Safety stop survives a paused/cleared queue, applied only at a fixed boundary.
}
void ATRFishingSessionActor::ConsumeInputReset()
{
	if (bClearRodReservations) { Fishing->PendingJerkCount = 0; bClearRodReservations = false; }
	if (bClearNormalRetrieve)
	{
		Fishing->StopNormalRetrieve(Coordinator->GetSimulationTime().TickIndex);
		bClearNormalRetrieve = false;
	}
}
void ATRFishingSessionActor::CaptureHUD(const FTRSimTime& Time)
{
	PublishedHUD = {};
	if (!bInitialized || !Coordinator.IsValid()) { return; }
	PublishedHUD.Egi = Fishing->GetSnapshot();
	PublishedHUD.Rod = RodControl->GetSnapshot();
	PublishedHUD.Retrieval = Fishing->GetRetrievalSnapshot(Time.TickIndex);
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
	Copy.PlayerMode = GetPlayerModeSnapshot();
	Copy.bSessionValid = true; Copy.bEgiValid = bCastActive; Copy.bEquipmentLocked = bEquipmentLocked;
	Copy.bEgiOnboard = bEgiOnboard; Copy.bCanChangeEquipment = CanChangeEquipment();
	Copy.Phase = Phase; Copy.CastId = CurrentCastId; Copy.Equipment = GetEquipmentSnapshot();
	Copy.bPaused = IsPlayerPaused(); Copy.EquipmentBlockReason = GetEquipmentBlockReason();
	Copy.AvailableCommands.Reset();
	for (const auto Command : { ETRFishingCommandType::Deploy, ETRFishingCommandType::NextCast, ETRFishingCommandType::RodAim,
		ETRFishingCommandType::Jerk, ETRFishingCommandType::Fall, ETRFishingCommandType::RetrieveStarted,
		ETRFishingCommandType::RetrieveStopped, ETRFishingCommandType::QuickRetrieve })
	{
		if (IsCommandAvailable(Command)) { Copy.AvailableCommands.Add(Command); }
	}
	Copy.Egi.FishingState = Fishing->GetState();
	Copy.Retrieval = Fishing->GetRetrievalSnapshot(PublishedHUD.Retrieval.Tick);
	Copy.Retrieval.CastId = CurrentCastId;
	if (!bCastActive && bHasResult && LastResult.bQuickRetrieved) { Copy.Retrieval.QuickRetrieveProgress01 = 1.0; }
	return Copy;
}

bool ATRFishingSessionActor::CanChangeEquipment() const
{
	return GetEquipmentBlockReason().IsEmpty();
}

FText ATRFishingSessionActor::GetEquipmentBlockReason() const
{
	if (!bInitialized || IsActorBeingDestroyed() || !Coordinator.IsValid()) { return NSLOCTEXT("TRPrototype", "NoSession", "セッションがありません"); }
	if (Coordinator->IsSimulationPaused()) { return NSLOCTEXT("TRPrototype", "PausedEquipment", "一時停止中は変更できません（Pで再開）"); }
	if (Phase == ETRSessionPhase::Result) { return NSLOCTEXT("TRPrototype", "ResultEquipment", "次投へ進むと装備を変更できます（N）"); }
	if (bCastActive) { return NSLOCTEXT("TRPrototype", "CastEquipment", "キャスト中は変更できません。クイック回収後に変更できます"); }
	if (!bEgiOnboard) { return NSLOCTEXT("TRPrototype", "OffboardEquipment", "エギを回収してください"); }
	if (bEquipmentLocked || Phase != ETRSessionPhase::Ready) { return NSLOCTEXT("TRPrototype", "LockedEquipment", "次投準備状態ではありません"); }
	return FText::GetEmpty();
}

bool ATRFishingSessionActor::IsCommandAvailable(ETRFishingCommandType Command) const
{
	if (!IsInputModeAllowed(ETRPlayerMode::Fishing) || Fishing->GetState() == ETRFishingState::QuickRetrieving) { return false; }
	if (Command == ETRFishingCommandType::Deploy) { return Fishing->GetState() == ETRFishingState::Ready && CanChangeEquipment() && IsValid(SelectedEgiMesh) && NextCastValue < MAX_int64; }
	if (Command == ETRFishingCommandType::NextCast) { return bHasResult && !bCastActive; }
	if (Command == ETRFishingCommandType::RodAim) { return (bCastActive || Phase == ETRSessionPhase::Ready) && RodControl->IsInitialized(); }
	return bCastActive && Fishing->GetCommandAvailability(Command) == ETRCommandResult::Accepted;
}

void ATRFishingSessionActor::GetEquipmentOptions(TArray<FTREgiSpecRow>& Egis, TArray<FTRSinkerSpecRow>& Sinkers) const
{
	Egis.Reset(); Sinkers.Reset();
	if (!bInitialized || IsActorBeingDestroyed() || !IsValid(EgiTable) || !IsValid(SinkerTable)) { return; }
	TArray<FTREgiSpecRow*> EgiRows; EgiTable->GetAllRows(TEXT("Prototype equipment choices"), EgiRows);
	TArray<FTRSinkerSpecRow*> SinkerRows; SinkerTable->GetAllRows(TEXT("Prototype equipment choices"), SinkerRows);
	for (const auto* Row : EgiRows) { if (Row) { Egis.Add(*Row); } }
	for (const auto* Row : SinkerRows) { if (Row) { Sinkers.Add(*Row); } }
	Egis.Sort([](const auto& A, const auto& B) { return A.BaseMassG == B.BaseMassG ? A.EgiId.LexicalLess(B.EgiId) : A.BaseMassG < B.BaseMassG; });
	Sinkers.Sort([](const auto& A, const auto& B) { return A.MassG == B.MassG ? A.SinkerId.LexicalLess(B.SinkerId) : A.MassG < B.MassG; });
}

ETRModeChangeRejection ATRFishingSessionActor::GetModeChangeRejection(ETRPlayerMode Target) const
{
 if (!IsAcceptingPlayerInput() || !PlayerMode->GetSnapshot().bValid) { return ETRModeChangeRejection::SessionUnavailable; }
 if (IsPlayerPaused()) { return ETRModeChangeRejection::Paused; }
 if (!StaticEnum<ETRPlayerMode>()->IsValidEnumValue(int64(Target))) { return ETRModeChangeRejection::InvalidTarget; }
 if (Target == PlayerMode->GetSnapshot().Mode) { return ETRModeChangeRejection::AlreadyInMode; }
 if (bCastActive) { return ETRModeChangeRejection::ActiveCast; }
 if (!bEgiOnboard) { return ETRModeChangeRejection::EgiOffboard; }
 if (Phase != ETRSessionPhase::Ready || Fishing->GetState() != ETRFishingState::Ready) { return ETRModeChangeRejection::NotReady; }
 if (bEquipmentLocked) { return ETRModeChangeRejection::EquipmentLocked; }
 FTRBoatSnapshot Boat; FTROceanSample Ocean;
 if (!ReadEnvironment(Boat, Ocean)) { return ETRModeChangeRejection::InvalidEnvironment; }
 if (PlayerMode->GetSnapshot().ModeEpoch == MAX_int64) { return ETRModeChangeRejection::SessionUnavailable; }
 return ETRModeChangeRejection::None;
}
FTRPlayerModeSnapshot ATRFishingSessionActor::GetPlayerModeSnapshot() const
{
 FTRPlayerModeSnapshot Copy = PlayerMode->GetSnapshot();
 Copy.bValid = Copy.bValid && IsAcceptingPlayerInput();
 Copy.ChangeBlockedReason = GetModeChangeRejection(Copy.Mode == ETRPlayerMode::Navigation ? ETRPlayerMode::Fishing : ETRPlayerMode::Navigation);
 Copy.bCanChangeMode = Copy.ChangeBlockedReason == ETRModeChangeRejection::None;
 Copy.bNavigationInputAllowed = IsInputModeAllowed(ETRPlayerMode::Navigation);
 Copy.bStopNavigationThrustRequested = !Copy.bNavigationInputAllowed;
 Copy.bHoldBoatHeading = !Copy.bNavigationInputAllowed;
 return Copy;
}
bool ATRFishingSessionActor::IsInputModeAllowed(ETRPlayerMode RequiredMode) const
{
 const auto State = PlayerMode->GetSnapshot();
 return State.bValid && IsAcceptingPlayerInput() && !IsPlayerPaused() && State.Mode == RequiredMode;
}
bool ATRFishingSessionActor::SubmitModeChange(ETRPlayerMode Target, int64 ExpectedEpoch, FTRActorSimId ExpectedRegistration, int64 TargetTick)
{
 if (!IsAcceptingPlayerInput() || IsPlayerPaused() || ExpectedRegistration != RegistrationId ||
     ExpectedEpoch != PlayerMode->GetSnapshot().ModeEpoch || !StaticEnum<ETRPlayerMode>()->IsValidEnumValue(int64(Target))) { return false; }
 return SubmitCommand(Target == ETRPlayerMode::Fishing ? ETRFishingCommandType::StartFishingMode : ETRFishingCommandType::ReturnNavigationMode,
                      CurrentCastId, TargetTick);
}
