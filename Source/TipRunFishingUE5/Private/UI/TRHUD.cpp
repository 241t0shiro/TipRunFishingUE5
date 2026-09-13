#include "UI/TRHUD.h"
#include "UI/TRFishingHUDWidget.h"
#include "Game/TRPlayerController.h"
#include "Engine/World.h"
void ATRHUD::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	if (auto* PC = Cast<ATRPlayerController>(GetOwningPlayerController()); PC && PC->IsLocalController() && PC->GetLocalPlayer())
	{
		RootWidget = CreateWidget<UTRFishingHUDWidget>(PC, UTRFishingHUDWidget::StaticClass());
		if (RootWidget)
		{
			RootWidget->AddToPlayerScreen(); RootWidget->SetPositionInViewport(FVector2D(16.0, 16.0));
			RootWidget->SetDesiredSizeInViewport(FVector2D(650.0, 330.0));
		}
	}
#endif
}
void ATRHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!RootWidget) { return; }
	const auto* PC = Cast<ATRPlayerController>(GetOwningPlayerController());
	const FTRHUDSnapshot S = PC ? PC->GetDebugSnapshot() : FTRHUDSnapshot();
	DisplayElapsedS += GetWorld()->GetDeltaSeconds();
	const auto& Old = RootWidget->DisplaySnapshot;
	if (DisplayElapsedS >= 0.1f || S.CastId != Old.CastId || S.Phase != Old.Phase ||
		S.Egi.FishingState != Old.Egi.FishingState || S.bSessionValid != Old.bSessionValid || S.bEgiValid != Old.bEgiValid)
	{
		RootWidget->ApplySnapshot(S); DisplayElapsedS = 0.0f;
	}
}
void ATRHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (RootWidget) { RootWidget->RemoveFromParent(); RootWidget = nullptr; }
	Super::EndPlay(Reason);
}
