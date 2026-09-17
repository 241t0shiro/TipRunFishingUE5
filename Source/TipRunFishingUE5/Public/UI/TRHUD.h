#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TRHUD.generated.h"
class UTRFishingHUDWidget;
UCLASS()
class TIPRUNFISHINGUE5_API ATRHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
	void SetObservationLayout(bool bEnabled) { bObservationLayout = bEnabled; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY() TObjectPtr<UTRFishingHUDWidget> RootWidget;
	float DisplayElapsedS = 0.0f;
	bool bObservationLayout = false;
};
