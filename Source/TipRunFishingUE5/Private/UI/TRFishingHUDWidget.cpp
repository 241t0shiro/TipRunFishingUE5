#include "UI/TRFishingHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UTRFishingHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) { return; }
	auto* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PrototypePanel"));
	Border->SetPadding(FMargin(12.0f)); Border->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.04f, 0.9f));
	Readout = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DebugReadout"));
	Readout->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 14));
	Readout->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Border->SetContent(Readout); WidgetTree->RootWidget = Border;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplySnapshot(DisplaySnapshot);
}
FText UTRFishingHUDWidget::FormatSnapshot(const FTRHUDSnapshot& S)
{
	if (!S.bSessionValid) { return FText::FromString(TEXT("TipRun Prototype Debug HUD\nSession unavailable")); }
	FString Text = FString::Printf(TEXT("TipRun Prototype Debug HUD\nCastId: %lld | State: %s\nEgi weight: %.1f g | Sinker: %.1f g | Total: %.1f g\n"),
		S.CastId.Value, *StaticEnum<ETRFishingState>()->GetNameStringByValue(int64(S.Egi.FishingState)),
		S.Equipment.BaseMassG, S.Equipment.SinkerMassG, S.Equipment.TotalMassG);
	if (S.bEgiValid)
	{
		Text += FString::Printf(TEXT("Egi depth: %.3f m | Tick: %lld\nVertical velocity (world Z, up +): %+.3f m/s\nLine: %.3f m | Angle: %.2f deg\nTension proxy: %.3f\n"),
			S.Egi.DepthM, S.Egi.Tick, S.Egi.VelocityMps.Z, S.Egi.LineLengthM, FMath::RadiansToDegrees(S.Egi.LineAngleRad), S.Egi.Tension01);
	}
	else { Text += TEXT("Egi depth / velocity / line / tension: N/A (no active cast)\n"); }
	if (S.bEnvironmentValid)
	{
		Text += FString::Printf(TEXT("Water depth: %.3f m\nBoat drift: (%.3f, %.3f, %.3f) m/s\nCurrent: (%.3f, %.3f, %.3f) m/s\n"),
			S.Ocean.BottomDepthM, S.Boat.VelocityMps.X, S.Boat.VelocityMps.Y, S.Boat.VelocityMps.Z,
			S.Ocean.CurrentMps.X, S.Ocean.CurrentMps.Y, S.Ocean.CurrentMps.Z);
	}
	else { Text += TEXT("Water depth / boat / current: N/A (invalid environment)\n"); }
	Text += TEXT("Prototype controls: see assigned InputConfig\nShakuri / Fall / Hook / Reel: input routing only (logic pending)");
	return FText::FromString(Text);
}
void UTRFishingHUDWidget::ApplySnapshot(const FTRHUDSnapshot& Snapshot)
{
	DisplaySnapshot = Snapshot;
	if (Readout) { Readout->SetText(FormatSnapshot(DisplaySnapshot)); }
}
