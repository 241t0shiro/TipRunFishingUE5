#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/TRHUDSnapshot.h"
#include "TRFishingHUDWidget.generated.h"
class UTextBlock;
class UComboBoxString;
class UButton;
class UVerticalBox;
class ATRPlayerController;
class ATRFishingSessionActor;
struct FTRPrototypeReadoutRow { FText Label; FText Value; };
struct FTRPrototypeGuideRow { FText Text; bool bAvailable = false; };
UCLASS()
class TIPRUNFISHINGUE5_API UTRFishingHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void BindController(ATRPlayerController* Controller);
	void RefreshFromController();
	void ApplySnapshot(const FTRHUDSnapshot& Snapshot);
	static TArray<FTRPrototypeReadoutRow> BuildReadout(const FTRHUDSnapshot& Snapshot);
	static TArray<FTRPrototypeReadoutRow> BuildCompactReadout(const FTRHUDSnapshot& Snapshot);
	static TArray<FTRPrototypeGuideRow> BuildGuide(const FTRHUDSnapshot& Snapshot);
	static FText BuildCompactGuide(const FTRHUDSnapshot& Snapshot);
	static FText FormatSnapshot(const FTRHUDSnapshot& Snapshot);
	static bool IsPrimaryReadout(int32 Index);
	static bool IsPrimaryGuide(int32 Index);
	static FText FormatRevisions(const FTRHUDSnapshot& Snapshot);
	static bool HasRevisionMismatch(const FTRHUDSnapshot& Snapshot);
	bool SelectEquipment(FName EgiId, FName SinkerId);
	bool ApplyEquipmentSelection();
	bool RequestDeploy();
	bool RequestNextCast();
	int32 GetEgiOptionCount() const { return EgiOptions.Num(); }
	int32 GetSinkerOptionCount() const { return SinkerOptions.Num(); }
	FText GetSelectionMessage() const { return SelectionMessage; }
	UPROPERTY(BlueprintReadOnly, Category="TipRun|Debug") FTRHUDSnapshot DisplaySnapshot;
	UPROPERTY(EditAnywhere, Category="TipRun|Prototype Style") FLinearColor LabelColor = FLinearColor(.65f,.78f,.9f);
	UPROPERTY(EditAnywhere, Category="TipRun|Prototype Style") FLinearColor ValueColor = FLinearColor(1.f,.93f,.65f);
	UPROPERTY(EditAnywhere, Category="TipRun|Prototype Style") FLinearColor DisabledColor = FLinearColor(.5f,.55f,.6f);
protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	void BuildLayout();
	void RefreshOptions();
	void UpdatePanel();
	bool HasCurrentSession() const;
	UFUNCTION() void OnApply();
	UFUNCTION() void OnDeploy();
	UFUNCTION() void OnNext();
	UFUNCTION() void OnClose();
	UFUNCTION() void OnSelectionChanged(FString Item, ESelectInfo::Type SelectionType);
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Labels;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Values;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Guides;
	UPROPERTY() TArray<TObjectPtr<UWidget>> ReadoutPairs;
	UPROPERTY() TObjectPtr<UTextBlock> RevisionStatus;
	UPROPERTY() TObjectPtr<UTextBlock> DebugHelp;
	UPROPERTY() TObjectPtr<UTextBlock> CompactGuide;
	UPROPERTY() TObjectPtr<UVerticalBox> EquipmentPanel;
	UPROPERTY() TObjectPtr<UComboBoxString> EgiCombo;
	UPROPERTY() TObjectPtr<UComboBoxString> SinkerCombo;
	UPROPERTY() TObjectPtr<UTextBlock> EquipmentStatus;
	UPROPERTY() TObjectPtr<UTextBlock> InputStatus;
	UPROPERTY() TObjectPtr<UTextBlock> SelectionStatus;
	UPROPERTY() TObjectPtr<UButton> ApplyButton;
	UPROPERTY() TObjectPtr<UButton> DeployButton;
	UPROPERTY() TObjectPtr<UButton> NextButton;
	TWeakObjectPtr<ATRPlayerController> Controller;
	TWeakObjectPtr<ATRFishingSessionActor> DisplaySession;
	TArray<FTREgiSpecRow> EgiOptions;
	TArray<FTRSinkerSpecRow> SinkerOptions;
	FName SelectedEgi, SelectedSinker;
	FTRCastId DisplayCast;
	FTRActorSimId DisplayRegistration;
	FText SelectionMessage;
	bool bUpdatingSelection = false;
	bool bPanelWasOpen = false;
};
