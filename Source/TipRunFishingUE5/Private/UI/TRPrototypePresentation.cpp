#include "UI/TRFishingHUDWidget.h"
#include "Data/TREnvironmentUnits.h"
#define LOCTEXT_NAMESPACE "TRPrototypeHUD"
namespace
{
 FText Value(const FString& S) { return FText::FromString(S); }
 FString Motion(FVector V)
 {
  const double Speed = V.Size2D();
  if (!FMath::IsFinite(Speed)) { return TEXT("—（無効）"); }
  if (Speed < 1.e-6) { return TEXT("0.000 m/s / 0.000 knot（方向なし）"); }
  double Knots=0; TREnvironmentUnits::TryMpsToKnots(Speed,Knots);
  // World axes, travelling direction; no unestablished north convention.
  return FString::Printf(TEXT("%.3f m/s / %.3f knot\n%+.1f° (+X→+Y)"), Speed, Knots, FMath::RadiansToDegrees(FMath::Atan2(V.Y,V.X)));
 }
 FText StateText(ETRFishingState State)
 {
  switch (State)
  {
  case ETRFishingState::Ready: return LOCTEXT("Ready", "次投準備 (Ready)");
  case ETRFishingState::Deploying: return LOCTEXT("Deploy", "投入中 (Deploying)");
  case ETRFishingState::FreeFall: return LOCTEXT("Fall", "フリーフォール (FreeFall)");
  case ETRFishingState::BottomContact: return LOCTEXT("Bottom", "着底 (BottomContact)");
  case ETRFishingState::Jerking: return LOCTEXT("Jerk", "シャクリ (Jerking)");
  case ETRFishingState::TensionFall: return LOCTEXT("TF", "テンションフォール (TensionFall)");
  case ETRFishingState::Stay: return LOCTEXT("Stay", "ステイ (Stay)");
  case ETRFishingState::Retrieving: return LOCTEXT("Retrieve", "通常回収 (Retrieving)");
  case ETRFishingState::QuickRetrieving: return LOCTEXT("Quick", "クイック回収 (QuickRetrieving)");
  case ETRFishingState::Result: return LOCTEXT("Result", "回収結果・次投へ (Result)");
  default: return LOCTEXT("Inactive", "待機／終了");
  }
 }
}
TArray<FTRPrototypeReadoutRow> UTRFishingHUDWidget::BuildReadout(const FTRHUDSnapshot& S)
{
 TArray<FTRPrototypeReadoutRow> Rows;
 const FString NA = !S.bSessionValid ? TEXT("—（セッションなし）") : S.bEgiOnboard ? TEXT("—（船上）") : TEXT("—（水中情報なし）");
 const FString Frozen = S.Retrieval.bIsQuickRetrieving ? TEXT(" [回収中・凍結値]") : TEXT("");
 auto Add = [&](FText Label, FString Text) { Rows.Add({Label,Value(Text)}); };
 auto Egi = [&](FText Label, FString Text) { Add(Label,S.bSessionValid && S.bEgiValid ? Text + Frozen : NA); };
 auto Sea = [&](FText Label, FString Text) { Add(Label,S.bSessionValid && S.bEnvironmentValid ? Text : TEXT("—（環境無効）")); };
 Add(LOCTEXT("CastId", "キャストID"),S.bSessionValid ? FString::Printf(TEXT("%lld"),S.CastId.Value) : NA);
 Add(LOCTEXT("State", "釣り状態"),S.bSessionValid ? (S.bPaused ? TEXT("一時停止 / ") : TEXT("")) + StateText(S.Egi.FishingState).ToString() : NA);
 Egi(LOCTEXT("Depth", "エギ深度"),FString::Printf(TEXT("%.3f m"),S.Egi.DepthM));
 Sea(LOCTEXT("Water", "水深"),FString::Printf(TEXT("%.3f m"),S.Ocean.BottomDepthM));
 Egi(LOCTEXT("DepthSpeed", "深度変化速度（下向き＋）"),FString::Printf(TEXT("%+.3f m/s (%s)"),S.Egi.DepthVelocityMps,S.Egi.DepthVelocityMps < 0 ? TEXT("上昇") : S.Egi.DepthVelocityMps > 0 ? TEXT("下降") : TEXT("変化なし")));
 Egi(LOCTEXT("BoatOffset", "船からの水平距離"),FString::Printf(TEXT("%.3f m"),S.Egi.HorizontalDistanceFromBoatM));
 Egi(LOCTEXT("RodOffset", "竿先からの水平距離"),FString::Printf(TEXT("%.3f m"),S.Egi.HorizontalDistanceFromRodTipM));
 Egi(LOCTEXT("Length", "ライン長"),FString::Printf(TEXT("%.3f m"),S.Egi.LineLengthM));
 Egi(LOCTEXT("Angle", "ライン角度（鉛直から）"),FString::Printf(TEXT("%.2f°"),FMath::RadiansToDegrees(S.Egi.LineAngleRad)));
 Egi(LOCTEXT("Slack", "ライン弛み"),FString::Printf(TEXT("%.3f m"),S.Egi.SlackM));
 Egi(LOCTEXT("Tension", "張力Proxy（代理値）"),FString::Printf(TEXT("%.3f / 1（実張力Nではない）"),S.Egi.Tension01));
 Sea(LOCTEXT("Drift", "船ドリフト速度／方向"),Motion(S.Boat.VelocityMps));
 Sea(LOCTEXT("Wind", "風向／風速（船地点）"),Motion(FVector(S.Boat.WindMps.X,S.Boat.WindMps.Y,0)));
 Sea(LOCTEXT("Surface", "表層潮（船地点）"),Motion(S.Boat.SurfaceCurrentMps));
 Egi(LOCTEXT("DepthCurrent", "エギ深度の潮流"),Motion(S.Egi.CurrentAtEgiDepthMps));
 Add(LOCTEXT("EgiWeight", "エギ重量"),S.bSessionValid ? FString::Printf(TEXT("%s / %.1f g"),*S.Equipment.EgiId.ToString(),S.Equipment.BaseMassG) : NA);
 Add(LOCTEXT("SinkerWeight", "シンカー重量"),S.bSessionValid ? FString::Printf(TEXT("%s / %.1f g"),S.Equipment.SinkerMassG == 0 ? TEXT("なし") : *S.Equipment.SinkerId.ToString(),S.Equipment.SinkerMassG) : NA);
 Add(LOCTEXT("Total", "総重量"),S.bSessionValid ? FString::Printf(TEXT("%.1f g"),S.Equipment.TotalMassG) : NA);
 Add(LOCTEXT("Lock", "装備ロック状態"),S.bSessionValid ? (S.bEquipmentLocked ? TEXT("ロック中") : TEXT("解除済み")) : NA);
 Add(LOCTEXT("Change", "装備変更可否"),S.bCanChangeEquipment ? TEXT("変更可能（Tabで装備パネル）") : S.EquipmentBlockReason.IsEmpty() ? NA : S.EquipmentBlockReason.ToString());
 Add(LOCTEXT("Normal", "通常巻取り (Normal Retrieve)"),S.bSessionValid ? (S.Retrieval.bIsRetrieving ? FString::Printf(TEXT("巻取り中 %.3f m/s"),S.Retrieval.RetrieveSpeedMps) : TEXT("停止")) : NA);
 Add(LOCTEXT("QuickProgress", "クイック回収／進捗"),S.bSessionValid ? (S.Retrieval.bIsQuickRetrieving ? FString::Printf(TEXT("%.1f%% / 途中停止不可"),100*S.Retrieval.QuickRetrieveProgress01) : TEXT("実行していません")) : NA);
 Egi(LOCTEXT("Count", "シャクリ回数／予約"),FString::Printf(TEXT("%lld / %lld"),S.Egi.JerkCount,S.Egi.PendingJerkCount));
 return Rows;
}
TArray<FTRPrototypeGuideRow> UTRFishingHUDWidget::BuildGuide(const FTRHUDSnapshot& S)
{
 TArray<FTRPrototypeGuideRow> Rows;
 auto Add = [&](FText Text, ETRFishingCommandType Command) { Rows.Add({Text,S.bSessionValid && !S.bPaused && S.AvailableCommands.Contains(Command) && !S.UnmappedPrimaryInputs.Contains(Command)}); };
 Add(LOCTEXT("GuideMouse", "マウス移動：竿操作"),ETRFishingCommandType::RodAim);
 Add(LOCTEXT("GuideRight", "右クリック：シャクリ（1押下1回）"),ETRFishingCommandType::Jerk);
 Add(LOCTEXT("GuideLeft", "左クリック長押し：巻き上げ"),ETRFishingCommandType::RetrieveStarted);
 Add(LOCTEXT("GuideRelease", "左クリックを離す：巻き上げ停止"),ETRFishingCommandType::RetrieveStopped);
 Add(LOCTEXT("GuideFall", "F：再フォール"),ETRFishingCommandType::Fall);
 Add(LOCTEXT("GuideQuick", "Q：クイック回収（途中停止不可）"),ETRFishingCommandType::QuickRetrieve);
 Add(LOCTEXT("GuideDeploy", "Enter：投入（適用済み装備を使用）"),ETRFishingCommandType::Deploy);
 Add(LOCTEXT("GuideNext", "N：次投の準備へ"),ETRFishingCommandType::NextCast);
 Rows.Add({LOCTEXT("GuideEquipment", "Tab：装備パネルを開く／閉じる"),S.bSessionValid});
 Rows.Add({LOCTEXT("GuidePause", "P：一時停止／再開"),S.bSessionValid});
 return Rows;
}
FText UTRFishingHUDWidget::FormatSnapshot(const FTRHUDSnapshot& S)
{
 FString Text;
 for (const auto& Row : BuildReadout(S)) { Text += Row.Label.ToString()+TEXT(": ")+Row.Value.ToString()+TEXT("\n"); }
 return Value(Text);
}
#undef LOCTEXT_NAMESPACE
