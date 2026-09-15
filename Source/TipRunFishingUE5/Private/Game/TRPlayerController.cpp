#include "Game/TRPlayerController.h"
#include "Game/TRFishingSessionActor.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"

ATRPlayerController::ATRPlayerController() { PrimaryActorTick.bTickEvenWhenPaused = true; }
void ATRPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (FSlateApplication::IsInitialized())
	{
		ActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ATRPlayerController::SetInputFocus);
	}
}
void ATRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InstallInputBindings(Cast<UEnhancedInputComponent>(InputComponent));
}
bool ATRPlayerController::InstallInputBindings(UEnhancedInputComponent* Component)
{
	RemoveInputBindings();
	TArray<FText> Errors;
	if (!IsValid(Component) || !IsValid(InputConfig) || !InputConfig->Validate(Errors)) { return false; }
	BoundInput = Component;
	for (const auto& B : InputConfig->Bindings)
	{
		if(B.Command==ETRPlayerAction::RodAim)
		{
			BindingHandles.Add(Component->BindAction(B.Action,ETriggerEvent::Triggered,this,&ATRPlayerController::EnhancedRodAim).GetHandle());continue;
		}
		BindingHandles.Add(Component->BindAction(B.Action, ETriggerEvent::Started, this, &ATRPlayerController::EnhancedStarted, B.Command).GetHandle());
		BindingHandles.Add(Component->BindAction(B.Action, ETriggerEvent::Completed, this, &ATRPlayerController::EnhancedReleased, B.Command).GetHandle());
		BindingHandles.Add(Component->BindAction(B.Action, ETriggerEvent::Canceled, this, &ATRPlayerController::EnhancedReleased, B.Command).GetHandle());
	}
	return true;
}
void ATRPlayerController::RemoveInputBindings()
{
	if (BoundInput.IsValid()) { for (uint32 Handle : BindingHandles) { BoundInput->RemoveBindingByHandle(Handle); } }
	BindingHandles.Empty(); BoundInput.Reset();
}
void ATRPlayerController::SetMappingContext(bool bEnabled)
{
	if (const ULocalPlayer* Local = GetLocalPlayer())
	{
		if (auto* Enhanced = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (InstalledContext) { Enhanced->RemoveMappingContext(InstalledContext); InstalledContext = nullptr; }
			if (bEnabled && IsValid(InputConfig) && IsValid(InputConfig->FishingContext))
			{
				FModifyContextOptions Options; Options.bIgnoreAllPressedKeysUntilRelease = true;
				InstalledContext = InputConfig->FishingContext; Enhanced->AddMappingContext(InstalledContext, 0, Options);
			}
		}
	}
}
bool ATRPlayerController::BindSession(ATRFishingSessionActor* Session)
{
	UnbindSession();
	if (!IsValid(Session) || Session->IsActorBeingDestroyed() || Session->GetWorld() != GetWorld() || !Session->IsAcceptingPlayerInput()) { return false; }
	BoundSession = Session; ObservedCast = Session->GetCastId(); ObservedRegistration = Session->GetRegistrationId();
	CommandHandle = Session->OnCommandProcessed.AddUObject(this, &ATRPlayerController::ObserveCommand);
	SetMappingContext(true);
	return true;
}
void ATRPlayerController::UnbindSession()
{
	if (BoundSession.IsValid()) { BoundSession->OnCommandProcessed.Remove(CommandHandle); }
	CommandHandle.Reset();
	ReleaseInput(); BoundSession.Reset(); bPendingStop = false; ObservedCast = {}; ObservedRegistration = {};
	SetMappingContext(false);
}
void ATRPlayerController::ReleaseInput()
{
	for (ETRPlayerAction A : Pressed) { BlockedUntilRelease.Add(A); }
	Pressed.Empty();
	if (bRetrieveHeld) { bPendingStop = true; PendingStopCast = HeldCast; }
	bRetrieveHeld = false;
	if (BoundSession.IsValid()) { BoundSession->ClearPlayerCommands(); }
	FlushPendingStop();
}
void ATRPlayerController::ObserveCommand(const FTRFishingCommand& Command, ETRCommandResult Result)
{
	if (Command.Type != ETRFishingCommandType::QuickRetrieve || Result != ETRCommandResult::Accepted) { return; }
	// Input bookkeeping only. Never discard later same-tick commands: simulation must reject them in sequence.
	for (ETRPlayerAction Action : Pressed) { BlockedUntilRelease.Add(Action); }
	Pressed.Empty(); bRetrieveHeld = false; bPendingStop = false;
}
void ATRPlayerController::FlushPendingStop()
{
	if (!bPendingStop || !BoundSession.IsValid()) { return; }
	if (!BoundSession->IsAcceptingPlayerInput() || BoundSession->GetCastId() != PendingStopCast) { bPendingStop = false; return; }
	if (!BoundSession->IsPlayerPaused())
	{
		BoundSession->SubmitCommand(ETRFishingCommandType::RetrieveStopped, PendingStopCast);
		bPendingStop = false;
	}
}
void ATRPlayerController::SynchronizeSession()
{
	if (!BoundSession.IsValid() || BoundSession->IsActorBeingDestroyed()) { UnbindSession(); return; }
	if (ObservedCast != BoundSession->GetCastId() || ObservedRegistration != BoundSession->GetRegistrationId())
	{
		ReleaseInput(); ObservedCast = BoundSession->GetCastId(); ObservedRegistration = BoundSession->GetRegistrationId();
	}
	const bool bPaused = BoundSession->IsPlayerPaused();
	if (bPaused && !bWasPaused) { ReleaseInput(); }
	bWasPaused = bPaused;
	FlushPendingStop();
}
bool ATRPlayerController::SubmitFishingCommand(ETRFishingCommandType Command, FTRCastId ExpectedCastId, int64 TargetTick)
{
	if (!bInputFocused || IsActorBeingDestroyed() || !BoundSession.IsValid() || !BoundSession->IsAcceptingPlayerInput() ||
		BoundSession->IsPlayerPaused() || ExpectedCastId != BoundSession->GetCastId()) { return false; }
	FlushPendingStop();
	return BoundSession->SubmitCommand(Command, ExpectedCastId, TargetTick);
}
bool ATRPlayerController::ActionStarted(ETRPlayerAction Action, int64 TargetTick)
{
	SynchronizeSession();
	if (!StaticEnum<ETRPlayerAction>()->IsValidEnumValue(int64(Action)) || Pressed.Contains(Action)) { return false; }
	if (Action == ETRPlayerAction::Pause)
	{
		if (!bInputFocused || !BoundSession.IsValid()) { return false; }
		Pressed.Add(Action); SetPauseRequested(!BoundSession->IsPlayerPaused()); return true;
	}
	if (BlockedUntilRelease.Contains(Action)) { return false; }
	if (!BoundSession.IsValid() || !bInputFocused || BoundSession->IsPlayerPaused()) { BlockedUntilRelease.Add(Action); return false; }
	ETRFishingCommandType Command;
	switch (Action)
	{
	case ETRPlayerAction::Deploy: Command = ETRFishingCommandType::Deploy; break;
	case ETRPlayerAction::Jerk: Command = ETRFishingCommandType::Jerk; break;
	case ETRPlayerAction::Fall: Command = ETRFishingCommandType::Fall; break;
	case ETRPlayerAction::TensionFall: Command = ETRFishingCommandType::TensionFall; break;
	case ETRPlayerAction::Hook: Command = ETRFishingCommandType::Hook; break;
	case ETRPlayerAction::Retrieve: Command = ETRFishingCommandType::RetrieveStarted; break;
	case ETRPlayerAction::QuickRetrieve: Command = ETRFishingCommandType::QuickRetrieve; break;
	case ETRPlayerAction::Cancel: Command = ETRFishingCommandType::EndFishing; break;
	case ETRPlayerAction::NextCast: Command = ETRFishingCommandType::NextCast; break;
	default: return false;
	}
	const FTRCastId CastId = BoundSession->GetCastId();
	if (!SubmitFishingCommand(Command, CastId, TargetTick)) { return false; }
	Pressed.Add(Action);
	if (Action == ETRPlayerAction::Retrieve) { bRetrieveHeld = true; HeldCast = CastId; }
	return true;
}
void ATRPlayerController::ActionReleased(ETRPlayerAction Action)
{
	Pressed.Remove(Action);
	// Conservative rearming: releases delivered during pause/focus loss do not rearm gameplay.
	if (Action == ETRPlayerAction::Pause || (bInputFocused && BoundSession.IsValid() && !BoundSession->IsPlayerPaused())) { BlockedUntilRelease.Remove(Action); }
	if (Action == ETRPlayerAction::Retrieve && bRetrieveHeld)
	{
		bRetrieveHeld = false; bPendingStop = true; PendingStopCast = HeldCast; FlushPendingStop();
	}
}
void ATRPlayerController::EnhancedStarted(ETRPlayerAction Action) { ActionStarted(Action); }
void ATRPlayerController::EnhancedRodAim(const FInputActionValue& Value){SubmitMouseDelta(Value.Get<FVector2D>());}
bool ATRPlayerController::SubmitMouseDelta(FVector2D Delta,int64 TargetTick)
{
	SynchronizeSession();
	if(!bInputFocused || IsActorBeingDestroyed() || Delta.ContainsNaN() || !BoundSession.IsValid() || BoundSession->IsPlayerPaused()){return false;}
	return BoundSession->SubmitRodAim(Delta,ObservedCast,ObservedRegistration,TargetTick);
}
void ATRPlayerController::EnhancedReleased(ETRPlayerAction Action) { ActionReleased(Action); }
void ATRPlayerController::SetPauseRequested(bool bPaused)
{
	if (!BoundSession.IsValid()) { return; }
	BoundSession->SetPlayerPaused(bPaused);
	if (bPaused) { ReleaseInput(); } else { FlushPendingStop(); }
	bWasPaused = bPaused;
}
void ATRPlayerController::SetInputFocus(bool bFocused)
{
	bInputFocused = bFocused;
	if (!bFocused) { ReleaseInput(); } else { SetMappingContext(BoundSession.IsValid()); FlushPendingStop(); }
}
void ATRPlayerController::FlushPressedKeys() { ReleaseInput(); Super::FlushPressedKeys(); }
void ATRPlayerController::PlayerTick(float DeltaTime) { SynchronizeSession(); Super::PlayerTick(DeltaTime); }
FTRHUDSnapshot ATRPlayerController::GetDebugSnapshot() const
{
	return BoundSession.IsValid() ? BoundSession->GetHUDSnapshot() : FTRHUDSnapshot();
}
void ATRPlayerController::Cleanup()
{
	UnbindSession(); RemoveInputBindings();
	if (ActivationHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle); ActivationHandle.Reset();
	}
}
void ATRPlayerController::EndPlay(const EEndPlayReason::Type Reason) { Cleanup(); Super::EndPlay(Reason); }
void ATRPlayerController::Destroyed() { Cleanup(); Super::Destroyed(); }

void ATRPlayerController::TRSetEquipment(FName EgiId, FName SinkerId)
{
	if (!bInputFocused || !BoundSession.IsValid()) { return; }
	TArray<FText> Errors;
	const auto Result = BoundSession->TrySetEquipment(EgiId, SinkerId, Errors);
	UE_LOG(LogTemp, Display, TEXT("Prototype equipment request: %s"), *StaticEnum<ETRCommandResult>()->GetNameStringByValue(int64(Result)));
}
