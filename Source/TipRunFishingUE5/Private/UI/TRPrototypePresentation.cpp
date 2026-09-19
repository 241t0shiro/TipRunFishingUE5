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
 if(S.PlayerMode.bValid && S.PlayerMode.Mode==ETRPlayerMode::Navigation)
 {
  Rows[1].Label=LOCTEXT("Mode","モード");
  Rows[1].Value=Value(S.bPaused?TEXT("一時停止 / 操船 (Navigation)"):TEXT("操船 (Navigation)"));
 }
 return Rows;
}
TArray<FTRPrototypeGuideRow> UTRFishingHUDWidget::BuildGuide(const FTRHUDSnapshot& S)
{
 TArray<FTRPrototypeGuideRow> Rows;
 if(S.PlayerMode.bValid && S.PlayerMode.Mode==ETRPlayerMode::Navigation)
 {
  const bool Enabled=S.bSessionValid && S.PlayerMode.bNavigationInputAllowed && S.Navigation.bConfigured;
  for(const TCHAR* Text:{TEXT("W/S：前進／後退"),TEXT("A/D：操舵"),TEXT("Mouse：視点"),TEXT("Wheel：Camera距離"),TEXT("Home：視点リセット"),TEXT("Shift：高速航行")}){Rows.Add({Value(Text),Enabled});}
  Rows.Add({LOCTEXT("FishingStart","Enter：釣り開始"),Enabled && S.PlayerMode.bCanChangeMode && !S.UnmappedPrimaryInputs.Contains(ETRFishingCommandType::StartFishingMode)});
  Rows.Add({LOCTEXT("NavigationPause","P：一時停止／再開"),S.bSessionValid});
  return Rows;
 }
 auto Add = [&](FText Text, ETRFishingCommandType Command) { Rows.Add({Text,S.bSessionValid && !S.bPaused && S.AvailableCommands.Contains(Command) && !S.UnmappedPrimaryInputs.Contains(Command)}); };
 Add(LOCTEXT("GuideMouse", "マウス移動：竿操作"),ETRFishingCommandType::RodAim);
 Add(LOCTEXT("GuideRight", "右クリック：シャクリ"),ETRFishingCommandType::Jerk);
 Add(LOCTEXT("GuideLeft", "左クリック長押し：巻き上げ"),ETRFishingCommandType::RetrieveStarted);
 Add(LOCTEXT("GuideRelease", "左クリックを離す：巻き上げ停止"),ETRFishingCommandType::RetrieveStopped);
 Add(LOCTEXT("GuideFall", "F：再フォール"),ETRFishingCommandType::Fall);
 Add(LOCTEXT("GuideQuick", "Q：クイック回収"),ETRFishingCommandType::QuickRetrieve);
 Add(LOCTEXT("GuideDeploy", "Enter：投入"),ETRFishingCommandType::Deploy);
 Add(LOCTEXT("GuideNext", "N：次投の準備へ"),ETRFishingCommandType::NextCast);
 Rows.Add({LOCTEXT("GuideEquipment", "Tab：装備"),S.bSessionValid});
 Rows.Add({LOCTEXT("GuidePause", "P：一時停止／再開"),S.bSessionValid});
 Rows.Add({LOCTEXT("GuideReturnNavigation","E：Navigationへ戻る（回収後Ready）"),S.PlayerMode.bValid && S.PlayerMode.bCanChangeMode && !S.UnmappedPrimaryInputs.Contains(ETRFishingCommandType::ReturnNavigationMode)});
 return Rows;
}
FText UTRFishingHUDWidget::BuildCompactGuide(const FTRHUDSnapshot& S)
{
 if(S.PlayerMode.bValid && S.PlayerMode.Mode==ETRPlayerMode::Navigation)
 {
  const FText Status=S.bBoostRearmRequired ? LOCTEXT("BoostRearm","高速航行: 再入力待ち") :
   (S.Navigation.bBoostRequested && S.Navigation.Throttle!=0 ? LOCTEXT("BoostOn","高速航行: ON") : LOCTEXT("BoostOff","高速航行: OFF"));
  return FText::Format(LOCTEXT("NavigationBoostStatus","W/S：前進／後退  A/D：操舵  Shift：高速航行\nMouse：視点  Wheel：距離\nHome：視点リセット  Enter：釣り開始\n{0}"),Status);
 }
 return LOCTEXT("FishingCompactReturn","Mouse：竿 / 右：シャクリ / 左保持：巻取り\nF：再落下 / Q：回収 / Enter：投入 / Tab：装備\nE：Navigationへ戻る（回収後Ready）");
}
FText UTRFishingHUDWidget::FormatSnapshot(const FTRHUDSnapshot& S)
{
 FString Text;
 for (const auto& Row : BuildReadout(S)) { Text += Row.Label.ToString()+TEXT(": ")+Row.Value.ToString()+TEXT("\n"); }
 return Value(Text);
}
bool UTRFishingHUDWidget::IsPrimaryReadout(int32 Index)
{
 switch(Index){case 1:case 2:case 5:case 7:case 11:case 14:case 17:case 18:return true;default:return false;}
}
bool UTRFishingHUDWidget::IsPrimaryGuide(int32 Index) { return false; } // One compact block; detailed eligibility stays under F1.
TArray<FTRPrototypeReadoutRow> UTRFishingHUDWidget::BuildCompactReadout(const FTRHUDSnapshot& S)
{
 auto Rows=BuildReadout(S);
 Rows[2].Label=LOCTEXT("CompactDepth","深度 / 水深");
 Rows[5].Label=LOCTEXT("CompactDistance","船からの水平距離");
 Rows[7].Label=LOCTEXT("CompactLine","ライン長 / 角度");
 Rows[11].Label=LOCTEXT("CompactDrift","船ドリフト");
 Rows[14].Label=LOCTEXT("CompactCurrent","エギ地点の潮");
 FString State=Rows[1].Value.ToString(); int32 Parenthesis;
 if(State.FindChar(TEXT('('),Parenthesis)){State=State.Left(Parenthesis).TrimEnd();}
 Rows[1].Value=Value(State);
 if(S.bSessionValid && S.bEnvironmentValid)
 {
  Rows[2].Value=Value(FString::Printf(TEXT("%s / %.1f m"),S.bEgiValid?*FString::Printf(TEXT("%.1f"),S.Egi.DepthM):TEXT("船上"),S.Ocean.BottomDepthM));
  const FVector V=S.Boat.VelocityMps;
  Rows[11].Value=Value(FString::Printf(TEXT("%.2f m/s  %.0f°"),V.Size2D(),FMath::RadiansToDegrees(FMath::Atan2(V.Y,V.X))));
 }
 if(S.bSessionValid && S.bEgiValid)
 {
  Rows[5].Value=Value(FString::Printf(TEXT("%.1f m"),S.Egi.HorizontalDistanceFromBoatM));
  Rows[7].Value=Value(FString::Printf(TEXT("%.1f m / %.0f°"),S.Egi.LineLengthM,FMath::RadiansToDegrees(S.Egi.LineAngleRad)));
  const FVector V=S.Egi.CurrentAtEgiDepthMps;
  Rows[14].Value=Value(FString::Printf(TEXT("%.2f m/s  %.0f°"),V.Size2D(),FMath::RadiansToDegrees(FMath::Atan2(V.Y,V.X))));
 }
 return Rows;
}
bool UTRFishingHUDWidget::HasRevisionMismatch(const FTRHUDSnapshot& S)
{
 const int32 Egi=S.bEgiValid ? S.Egi.EgiModelRevision : S.Equipment.Parameters.EgiModelRevision;
 return S.bSessionValid && ((S.bEnvironmentValid && (S.Ocean.FieldRevision!=2 || S.Boat.ModelRevision!=2)) || Egi!=2);
}
FText UTRFishingHUDWidget::FormatRevisions(const FTRHUDSnapshot& S)
{
 if(!S.bSessionValid || !S.bEnvironmentValid){return LOCTEXT("RevPending","Model Rev：未取得（接続確認中）");}
 const int32 Egi=S.bEgiValid ? S.Egi.EgiModelRevision : S.Equipment.Parameters.EgiModelRevision;
 return Value(FString::Printf(TEXT("%sEnv %d / Boat %d / Egi %d%s"),HasRevisionMismatch(S)?TEXT("警告：Rev2不一致！ "):TEXT("Model Rev: "),
  S.Ocean.FieldRevision,S.Boat.ModelRevision,Egi,S.bEgiValid?TEXT(""):TEXT("（投入設定）")));
}
#undef LOCTEXT_NAMESPACE
