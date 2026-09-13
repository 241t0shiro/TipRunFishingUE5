#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/TRHUDSnapshot.h"
#include "TRFishingHUDWidget.generated.h"
class UTextBlock;
UCLASS()
class TIPRUNFISHINGUE5_API UTRFishingHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void ApplySnapshot(const FTRHUDSnapshot& Snapshot);
	static FText FormatSnapshot(const FTRHUDSnapshot& Snapshot);
	UPROPERTY(BlueprintReadOnly, Category="TipRun|Debug") FTRHUDSnapshot DisplaySnapshot;
protected:
	virtual void NativeOnInitialized() override;
private:
	UPROPERTY() TObjectPtr<UTextBlock> Readout;
};
