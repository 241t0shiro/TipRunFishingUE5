#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/TRInputConfigDataAsset.h"
#include "Data/TRHUDSnapshot.h"
#include "Data/TRPlayerModeTypes.h"
#include "TRPlayerController.generated.h"
class ATRFishingSessionActor;
class UEnhancedInputComponent;
class UTRFishingHUDWidget;
class ATRPrototypeViewActor;
struct FInputActionValue;
struct FTRFishingCommand;

UCLASS()
class TIPRUNFISHINGUE5_API ATRPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ATRPlayerController();
	bool SubmitNavigationInput(FVector2D Input,int64 TargetTick=-1);
	bool ApplyNavigationLook(FVector2D Delta);
	void SetPrototypeObserver(ATRPrototypeViewActor* Observer);
	UFUNCTION(BlueprintCallable, Category="TipRun|Mode") bool RequestPlayerMode(ETRPlayerMode Target);
	UFUNCTION(Exec) void TRSetFishingMode(bool bFishing);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TipRun") TObjectPtr<UTRInputConfigDataAsset> InputConfig;
	UFUNCTION(BlueprintCallable, Category="TipRun") bool BindSession(ATRFishingSessionActor* Session);
	UFUNCTION(BlueprintCallable, Category="TipRun") void UnbindSession();
	UFUNCTION(BlueprintCallable, Category="TipRun") void SetPauseRequested(bool bPaused);
	FTRHUDSnapshot GetDebugSnapshot() const;
	bool SubmitFishingCommand(ETRFishingCommandType Command, FTRCastId ExpectedCastId, int64 TargetTick = -1);
	// Device-independent event adapter, shared by Enhanced Input and deterministic tests.
	bool ActionStarted(ETRPlayerAction Action, int64 TargetTick = -1);
	bool SubmitMouseDelta(FVector2D Delta,int64 TargetTick=-1);
	bool RoutePrototypeMouse(FVector2D Delta,bool bCameraLook);
	void ResetPrototypeCamera();
	void ActionReleased(ETRPlayerAction Action);
	void SetInputFocus(bool bFocused);
	bool IsRetrieveHeld() const { return bRetrieveHeld; }
	ATRFishingSessionActor* GetBoundSession() const { return BoundSession.Get(); }
	void EnablePrototypeUI(bool bEnabled);
	void RefreshPrototypePanel();
	void TogglePrototypePanel();
	void SetPrototypePanelOpen(bool bOpen);
	bool IsPrototypePanelOpen() const { return bPrototypePanelOpen; }
	void TogglePrototypeDetails();
	bool IsPrototypeDetailsOpen() const { return bPrototypeDetailsOpen; }
	bool SubmitUICommand(ETRFishingCommandType Command, FTRCastId ExpectedCast, FTRActorSimId ExpectedRegistration);
	ETRCommandResult SubmitUIEquipment(FName EgiId, FName SinkerId, FTRCastId ExpectedCast, FTRActorSimId ExpectedRegistration, TArray<FText>& Errors);
	bool InstallInputBindings(UEnhancedInputComponent* Component);
	// Internal MVP console control; the session applies the same onboard/Ready guard as UI.
	UFUNCTION(Exec) void TRSetEquipment(FName EgiId, FName SinkerId);
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void FlushPressedKeys() override;
	virtual void PlayerTick(float DeltaTime) override;
protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Destroyed() override;
private:
	void EnhancedNavigationThrottle(const FInputActionValue& Value);
	void EnhancedNavigationSteering(const FInputActionValue& Value);
	void EnhancedNavigationLook(const FInputActionValue& Value);
	void EnhancedNavigationZoom(const FInputActionValue& Value);
	void SendNavigationAxes();
	FVector2D NavigationAxes=FVector2D::ZeroVector;
	bool bNavigationBlockedUntilNeutral=false;
	bool bPhysicalLeftShift=false, bPhysicalRightShift=false;
	bool bBoostRearmRequired=false;
	void InvalidateBoostInput();
	TWeakObjectPtr<ATRPrototypeViewActor> PrototypeObserver;
	void EnhancedStarted(ETRPlayerAction Action);
	void EnhancedRodAim(const FInputActionValue& Value);
	void EnhancedReleased(ETRPlayerAction Action);
	void ReleaseInput();
	void FlushPendingStop();
	void SynchronizeSession();
	void ObserveCommand(const FTRFishingCommand& Command, ETRCommandResult Result);
	void RemoveInputBindings();
	void SetMappingContext(bool bEnabled);
	void Cleanup();
	TWeakObjectPtr<ATRFishingSessionActor> BoundSession;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	UPROPERTY() TObjectPtr<UInputMappingContext> InstalledContext;
	TArray<uint32> BindingHandles;
	TSet<ETRPlayerAction> Pressed, BlockedUntilRelease;
	FTRCastId ObservedCast, HeldCast, PendingStopCast;
	FTRActorSimId ObservedRegistration;
	int64 ObservedModeEpoch = 0;
	FDelegateHandle ActivationHandle;
	FDelegateHandle CommandHandle;
	bool bInputFocused = true;
	bool bRetrieveHeld = false;
	bool bPendingStop = false;
	bool bWasPaused = false;
	bool bPrototypeUIEnabled = false;
	bool bPrototypePanelOpen = false;
	bool bPrototypeDetailsOpen = false;
	ETRSessionPhase PrototypeObservedPhase = ETRSessionPhase::Initializing;
};
