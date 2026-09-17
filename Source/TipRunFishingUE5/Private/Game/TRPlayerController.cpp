#include "Game/TRPlayerController.h"
#include "Game/TRFishingSessionActor.h"
#include "Game/TRPrototypeViewActor.h"
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
	if (InputComponent) { InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ATRPlayerController::TogglePrototypePanel).bExecuteWhenPaused = true; }
	if (InputComponent) { InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ATRPlayerController::TogglePrototypeDetails).bExecuteWhenPaused = true; }
	if (InputComponent) { InputComponent->BindKey(EKeys::Home, IE_Pressed, this, &ATRPlayerController::ResetPrototypeCamera); }
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
			if (bEnabled && BoundSession.IsValid() && BoundSession->IsInputModeAllowed(ETRPlayerMode::Fishing) &&
				IsValid(InputConfig) && IsValid(InputConfig->FishingContext))
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
	ObservedModeEpoch = Session->GetPlayerModeSnapshot().ModeEpoch;
	CommandHandle = Session->OnCommandProcessed.AddUObject(this, &ATRPlayerController::ObserveCommand);
	SetMappingContext(true);
	PrototypeObservedPhase = ETRSessionPhase::Initializing;
	return true;
}
void ATRPlayerController::UnbindSession()
{
	if (BoundSession.IsValid()) { BoundSession->OnCommandProcessed.Remove(CommandHandle); }
	CommandHandle.Reset();
	ReleaseInput(); BoundSession.Reset(); bPendingStop = false; ObservedCast = {}; ObservedRegistration = {};
	SetMappingContext(false);
	SetPrototypePanelOpen(false);
	PrototypeObservedPhase = ETRSessionPhase::Initializing;
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
	const bool bModeChange = Command.Type == ETRFishingCommandType::StartFishingMode || Command.Type == ETRFishingCommandType::ReturnNavigationMode;
	if ((!bModeChange && Command.Type != ETRFishingCommandType::QuickRetrieve) || Result != ETRCommandResult::Accepted) { return; }
	// Input bookkeeping only. Never discard later same-tick commands: simulation must reject them in sequence.
	for (ETRPlayerAction Action : Pressed) { BlockedUntilRelease.Add(Action); }
	Pressed.Empty(); bRetrieveHeld = false; bPendingStop = false;
	if (bModeChange && BoundSession.IsValid())
	{
		ObservedModeEpoch = BoundSession->GetPlayerModeSnapshot().ModeEpoch;
		SetMappingContext(true);
	}
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
	if (ObservedModeEpoch != BoundSession->GetPlayerModeSnapshot().ModeEpoch)
	{
		ReleaseInput();
		ObservedModeEpoch = BoundSession->GetPlayerModeSnapshot().ModeEpoch;
		SetMappingContext(true);
	}
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
	if (bPrototypePanelOpen || !bInputFocused || IsActorBeingDestroyed() || !BoundSession.IsValid() || !BoundSession->IsAcceptingPlayerInput() ||
		!BoundSession->IsInputModeAllowed(ETRPlayerMode::Fishing) || ExpectedCastId != BoundSession->GetCastId()) { return false; }
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
	if (bPrototypePanelOpen) { BlockedUntilRelease.Add(Action); return false; }
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
void ATRPlayerController::EnhancedRodAim(const FInputActionValue& Value){RoutePrototypeMouse(Value.Get<FVector2D>(),IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift));}
bool ATRPlayerController::RoutePrototypeMouse(FVector2D Delta,bool bCameraLook)
{
	if(!bCameraLook){return SubmitMouseDelta(Delta);}
	SynchronizeSession();
	if(bPrototypePanelOpen || !bInputFocused || IsActorBeingDestroyed() || Delta.ContainsNaN() || !BoundSession.IsValid() || BoundSession->IsPlayerPaused()){return false;}
	if(auto* View=Cast<ATRPrototypeViewActor>(GetViewTarget())){View->ApplyCameraLook(Delta);return true;}
	return false; // No fallback into rod input while looking around.
}
void ATRPlayerController::ResetPrototypeCamera()
{
	if(!bPrototypePanelOpen && bInputFocused){if(auto* View=Cast<ATRPrototypeViewActor>(GetViewTarget())){View->ResetCameraLook();}}
}
bool ATRPlayerController::SubmitMouseDelta(FVector2D Delta,int64 TargetTick)
{
	SynchronizeSession();
	if(bPrototypePanelOpen || !bInputFocused || IsActorBeingDestroyed() || Delta.ContainsNaN() || !BoundSession.IsValid() || BoundSession->IsPlayerPaused()){return false;}
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
	auto Copy = BoundSession.IsValid() ? BoundSession->GetHUDSnapshot() : FTRHUDSnapshot();
	if (!Copy.bSessionValid) { return Copy; }
	auto HasKey = [&](ETRPlayerAction Action, FKey Key)
	{
		if (!IsValid(InputConfig) || !IsValid(InputConfig->FishingContext)) { return false; }
		for (const auto& Binding : InputConfig->Bindings)
		{
			if (Binding.Command != Action) { continue; }
			for (const auto& Mapping : InputConfig->FishingContext->GetMappings())
			{
				if (Mapping.Action == Binding.Action && Mapping.Key == Key) { return true; }
			}
		}
		return false;
	};
	auto Check = [&](ETRPlayerAction Action, FKey Key, ETRFishingCommandType Command)
	{
		if (!HasKey(Action, Key)) { Copy.UnmappedPrimaryInputs.Add(Command); }
	};
	Check(ETRPlayerAction::RodAim, EKeys::Mouse2D, ETRFishingCommandType::RodAim);
	Check(ETRPlayerAction::Jerk, EKeys::RightMouseButton, ETRFishingCommandType::Jerk);
	Check(ETRPlayerAction::Retrieve, EKeys::LeftMouseButton, ETRFishingCommandType::RetrieveStarted);
	Check(ETRPlayerAction::Retrieve, EKeys::LeftMouseButton, ETRFishingCommandType::RetrieveStopped);
	Check(ETRPlayerAction::QuickRetrieve, EKeys::Q, ETRFishingCommandType::QuickRetrieve);
	Check(ETRPlayerAction::Fall, EKeys::F, ETRFishingCommandType::Fall);
	if (!Copy.UnmappedPrimaryInputs.IsEmpty())
	{
		Copy.InputConfigurationNote = NSLOCTEXT("TRPrototype", "UnmappedPrimary", "基本操作の一部が未設定です。灰色の項目は現在使用できません。Debugキーは設定済みのものだけ使用できます。");
	}
	return Copy;
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

void ATRPlayerController::EnablePrototypeUI(bool bEnabled)
{
	bPrototypeUIEnabled = bEnabled;
	PrototypeObservedPhase = ETRSessionPhase::Initializing;
	bPrototypeDetailsOpen = false;
	SetPrototypePanelOpen(false);
	RefreshPrototypePanel();
}
void ATRPlayerController::RefreshPrototypePanel()
{
	if (!bPrototypeUIEnabled) { return; }
	if (!BoundSession.IsValid() || !BoundSession->IsAcceptingPlayerInput()) { SetPrototypePanelOpen(false); return; }
	const auto Phase = BoundSession->GetSessionPhase();
	if (Phase != PrototypeObservedPhase)
	{
		PrototypeObservedPhase = Phase;
		// G: entering Ready/Result never opens UI implicitly. Tab owns opening.
		SetPrototypePanelOpen(false);
	}
}
void ATRPlayerController::TogglePrototypeDetails()
{
	if (bPrototypeUIEnabled && bInputFocused) { bPrototypeDetailsOpen = !bPrototypeDetailsOpen; }
}
void ATRPlayerController::TogglePrototypePanel()
{
	if (!bPrototypeUIEnabled || !bInputFocused || !BoundSession.IsValid() || !BoundSession->IsAcceptingPlayerInput()) { return; }
	SetPrototypePanelOpen(!bPrototypePanelOpen);
}
void ATRPlayerController::SetPrototypePanelOpen(bool bOpen)
{
	if (bPrototypePanelOpen == bOpen) { return; }
	ReleaseInput(); bPrototypePanelOpen = bOpen;
	if (!bOpen && FSlateApplication::IsInitialized())
	{
		// Slate may have consumed the press before Enhanced Input saw it.
		const auto& MouseButtons = FSlateApplication::Get().GetPressedMouseButtons();
		if (MouseButtons.Contains(EKeys::LeftMouseButton)) { BlockedUntilRelease.Add(ETRPlayerAction::Retrieve); }
		if (MouseButtons.Contains(EKeys::RightMouseButton)) { BlockedUntilRelease.Add(ETRPlayerAction::Jerk); }
	}
	// UI mode is not application focus. Gameplay adapters are also gated so an
	// unhandled click outside the panel cannot leak through GameAndUI.
	bShowMouseCursor = bOpen;
	if (GetLocalPlayer())
	{
		if (bOpen)
		{
			FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
		}
		else { SetInputMode(FInputModeGameOnly()); }
	}
	SetMappingContext(BoundSession.IsValid());
}
bool ATRPlayerController::SubmitUICommand(ETRFishingCommandType Command, FTRCastId ExpectedCast, FTRActorSimId ExpectedRegistration)
{
	if (!bPrototypePanelOpen || !bInputFocused || !BoundSession.IsValid() ||
		BoundSession->GetCastId() != ExpectedCast || BoundSession->GetRegistrationId() != ExpectedRegistration ||
		(Command != ETRFishingCommandType::Deploy && Command != ETRFishingCommandType::NextCast) || !BoundSession->IsCommandAvailable(Command)) { return false; }
	// Close/flush BEFORE enqueueing, otherwise the mode change would erase this command.
	SetPrototypePanelOpen(false);
	return BoundSession->SubmitCommand(Command, ExpectedCast);
}
ETRCommandResult ATRPlayerController::SubmitUIEquipment(FName EgiId, FName SinkerId, FTRCastId ExpectedCast, FTRActorSimId ExpectedRegistration, TArray<FText>& Errors)
{
	if (!bPrototypePanelOpen || !bInputFocused || !BoundSession.IsValid() || !BoundSession->IsAcceptingPlayerInput() ||
		BoundSession->GetCastId() != ExpectedCast || BoundSession->GetRegistrationId() != ExpectedRegistration) { return ETRCommandResult::RejectedInvalidState; }
	return BoundSession->TrySetEquipment(EgiId, SinkerId, Errors);
}

bool ATRPlayerController::RequestPlayerMode(ETRPlayerMode Target)
{
 if (!bInputFocused || IsActorBeingDestroyed() || !BoundSession.IsValid() || bPrototypePanelOpen) { return false; }
 const auto Mode = BoundSession->GetPlayerModeSnapshot();
 return BoundSession->SubmitModeChange(Target, Mode.ModeEpoch, BoundSession->GetRegistrationId());
}
void ATRPlayerController::TRSetFishingMode(bool bFishing)
{
 RequestPlayerMode(bFishing ? ETRPlayerMode::Fishing : ETRPlayerMode::Navigation);
}
