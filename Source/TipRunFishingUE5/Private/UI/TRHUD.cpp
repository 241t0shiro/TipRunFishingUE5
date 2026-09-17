#include "UI/TRHUD.h"
#include "UI/TRFishingHUDWidget.h"
#include "Game/TRPlayerController.h"
#include "Engine/World.h"
#include "Blueprint/WidgetLayoutLibrary.h"
void ATRHUD::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	if (auto* PC = Cast<ATRPlayerController>(GetOwningPlayerController()); PC && PC->IsLocalController() && PC->GetLocalPlayer())
	{
		RootWidget = CreateWidget<UTRFishingHUDWidget>(PC, UTRFishingHUDWidget::StaticClass());
		if (RootWidget)
		{
			RootWidget->AddToPlayerScreen();
			RootWidget->BindController(PC);
		}
	}
#endif
}
void ATRHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!RootWidget) { return; }
	// Compact in all Prototype maps, independent of observation actor startup.
	{
		int32 Width=0, Height=0;
		GetOwningPlayerController()->GetViewportSize(Width,Height);
		const float DPI=FMath::Max(.1f,UWidgetLayoutLibrary::GetViewportScale(this));
		const auto* Controller=Cast<ATRPlayerController>(GetOwningPlayerController());
		const bool Expanded=Controller && (Controller->IsPrototypePanelOpen() || Controller->IsPrototypeDetailsOpen());
		const float PanelWidth=FMath::Min(Expanded?420.f:320.f,Width*.30f/DPI);
		RootWidget->SetPositionInViewport(FVector2D(Width-PanelWidth*DPI-12,12),true);
		RootWidget->SetDesiredSizeInViewport(FVector2D(PanelWidth,FMath::Min(Expanded?900.f:285.f,(Height-24)/DPI)));
	}
	const auto* PC = Cast<ATRPlayerController>(GetOwningPlayerController());
	const FTRHUDSnapshot S = PC ? PC->GetDebugSnapshot() : FTRHUDSnapshot();
	DisplayElapsedS += GetWorld()->GetDeltaSeconds();
	const auto& Old = RootWidget->DisplaySnapshot;
	if (DisplayElapsedS >= 0.1f || S.CastId != Old.CastId || S.Phase != Old.Phase ||
		S.Egi.FishingState != Old.Egi.FishingState || S.bSessionValid != Old.bSessionValid || S.bEgiValid != Old.bEgiValid ||
		S.bPaused != Old.bPaused || S.bEquipmentLocked != Old.bEquipmentLocked || S.bCanChangeEquipment != Old.bCanChangeEquipment)
	{
		RootWidget->RefreshFromController(); DisplayElapsedS = 0.0f;
	}
}
void ATRHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (RootWidget) { RootWidget->RemoveFromParent(); RootWidget = nullptr; }
	Super::EndPlay(Reason);
}
