#include "UI/TRFishingHUDWidget.h"
#include "Game/TRPlayerController.h"
#include "Game/TRFishingSessionActor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "TRPrototypePanel"
void UTRFishingHUDWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 BuildLayout();
}
TSharedRef<SWidget> UTRFishingHUDWidget::RebuildWidget()
{
 // NativeOnInitialized can be deferred without a local player context (e.g. test worlds).
 BuildLayout();
 return Super::RebuildWidget();
}
void UTRFishingHUDWidget::BuildLayout()
{
 SetIsFocusable(true);
 if (!WidgetTree || WidgetTree->RootWidget) { return; }
 auto* Border = WidgetTree->ConstructWidget<UBorder>(); Border->SetPadding(FMargin(6)); Border->SetBrushColor(FLinearColor(.015f,.025f,.04f,.78f));
 auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>(); Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible); Border->SetContent(Scroll);
 auto* ReadRows = WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(ReadRows);
 auto* Side = ReadRows;
 auto Text = [&](FText Content, int32 FontSize, FLinearColor Color)
 {
  auto* T = WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(Content);
  T->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FontSize)); T->SetColorAndOpacity(FSlateColor(Color)); T->SetAutoWrapText(true); return T;
 };
 ReadRows->AddChildToVerticalBox(Text(LOCTEXT("Title", "ティップラン / Prototype"),13,ValueColor));
 RevisionStatus=Text(FText(),11,ValueColor); ReadRows->AddChildToVerticalBox(RevisionStatus);
 for (const auto& Row : BuildReadout(DisplaySnapshot))
 {
  auto* Pair = WidgetTree->ConstructWidget<UHorizontalBox>(); ReadRows->AddChildToVerticalBox(Pair)->SetPadding(FMargin(0,1)); ReadoutPairs.Add(Pair);
  auto* Label = Text(Row.Label,11,LabelColor); auto* LabelSize=WidgetTree->ConstructWidget<USizeBox>(); LabelSize->SetWidthOverride(125); LabelSize->SetContent(Label); Pair->AddChildToHorizontalBox(LabelSize);
  auto* V = Text(Row.Value,11,ValueColor); Pair->AddChildToHorizontalBox(V)->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); Labels.Add(Label); Values.Add(V);
 }
 Side->AddChildToVerticalBox(Text(LOCTEXT("GuideTitleInsert", "操作 / Insert：詳細を開閉"),13,ValueColor))->SetPadding(FMargin(0,8,0,0));
 CompactGuide=Text(BuildCompactGuide(DisplaySnapshot),10,LabelColor); Side->AddChildToVerticalBox(CompactGuide);
 const auto InitialGuide=BuildGuide(DisplaySnapshot);
 for (int32 I=0;I<FMath::Max(InitialGuide.Num(),BuildGuide(FTRHUDSnapshot()).Num());++I)
 {auto* T=Text(InitialGuide.IsValidIndex(I)?InitialGuide[I].Text:FText(),12,LabelColor);Side->AddChildToVerticalBox(T);Guides.Add(T);}
 InputStatus=Text(FText(),12,ValueColor);Side->AddChildToVerticalBox(InputStatus);
 DebugHelp=Text(LOCTEXT("Debug", "Debug: Space=シャクリ / R=回収 / T=TF\nBackspace=中断（再開はPIE再起動）\n方向は流れる向き：+X=0°、+Y=90°\n白=船・橙=竿・黄=ライン・赤=エギ\nHook / BITE は未実装"),11,DisabledColor); Side->AddChildToVerticalBox(DebugHelp);
 EquipmentPanel = WidgetTree->ConstructWidget<UVerticalBox>(); Side->AddChildToVerticalBox(EquipmentPanel);
 EquipmentPanel->AddChildToVerticalBox(Text(LOCTEXT("EquipmentTitle", "次投の装備（Prototype・購入なし）"),18,ValueColor));
 EquipmentStatus=Text(FText(),13,LabelColor); EquipmentPanel->AddChildToVerticalBox(EquipmentStatus);
 EgiCombo=WidgetTree->ConstructWidget<UComboBoxString>(); EquipmentPanel->AddChildToVerticalBox(EgiCombo);
 SinkerCombo=WidgetTree->ConstructWidget<UComboBoxString>(); EquipmentPanel->AddChildToVerticalBox(SinkerCombo);
 EgiCombo->OnSelectionChanged.AddDynamic(this,&UTRFishingHUDWidget::OnSelectionChanged);
 SinkerCombo->OnSelectionChanged.AddDynamic(this,&UTRFishingHUDWidget::OnSelectionChanged);
 SelectionStatus=Text(FText(),13,ValueColor); EquipmentPanel->AddChildToVerticalBox(SelectionStatus);
 auto Button = [&](FText Caption)
 {
  auto* B=WidgetTree->ConstructWidget<UButton>(); B->SetContent(Text(Caption,15,FLinearColor::White)); EquipmentPanel->AddChildToVerticalBox(B)->SetPadding(FMargin(0,2)); return B;
 };
 ApplyButton=Button(LOCTEXT("Apply", "選択した装備を適用")); ApplyButton->OnClicked.AddDynamic(this,&UTRFishingHUDWidget::OnApply);
 DeployButton=Button(LOCTEXT("Deploy", "投入（Enter）")); DeployButton->OnClicked.AddDynamic(this,&UTRFishingHUDWidget::OnDeploy);
 NextButton=Button(LOCTEXT("Next", "次投の準備へ（N）")); NextButton->OnClicked.AddDynamic(this,&UTRFishingHUDWidget::OnNext);
 Button(LOCTEXT("Close", "パネルを閉じる（Tab）"))->OnClicked.AddDynamic(this,&UTRFishingHUDWidget::OnClose);
 WidgetTree->RootWidget=Border;
 ApplySnapshot(DisplaySnapshot);
}
void UTRFishingHUDWidget::BindController(ATRPlayerController* PC)
{
 if (Controller.IsValid() && Controller.Get()!=PC) { Controller->EnablePrototypeUI(false); }
 Controller=PC; DisplaySession.Reset(); SelectionMessage=FText::GetEmpty();
 if (Controller.IsValid()) { Controller->EnablePrototypeUI(true); }
 RefreshFromController();
}
bool UTRFishingHUDWidget::HasCurrentSession() const
{
 return Controller.IsValid() && DisplaySession.IsValid() && !DisplaySession->IsActorBeingDestroyed() &&
  Controller->GetBoundSession()==DisplaySession.Get() && DisplaySession->IsAcceptingPlayerInput();
}
void UTRFishingHUDWidget::RefreshFromController()
{
 ATRFishingSessionActor* Session=Controller.IsValid()?Controller->GetBoundSession():nullptr;
 if (DisplaySession.Get()!=Session)
 {
  DisplaySession=Session; EgiOptions.Reset(); SinkerOptions.Reset(); SelectionMessage=FText::GetEmpty();
  RefreshOptions();
 }
 if (Controller.IsValid()) { Controller->RefreshPrototypePanel(); }
 if (!HasCurrentSession())
 {
  EgiOptions.Reset(); SinkerOptions.Reset(); SelectedEgi=SelectedSinker=NAME_None;
  bUpdatingSelection=true; if(EgiCombo){EgiCombo->ClearOptions();SinkerCombo->ClearOptions();} bUpdatingSelection=false;
  SelectionMessage=FText::GetEmpty(); ApplySnapshot({}); return;
 }
 DisplayCast=DisplaySession->GetCastId(); DisplayRegistration=DisplaySession->GetRegistrationId();
 ApplySnapshot(Controller->GetDebugSnapshot());
}
void UTRFishingHUDWidget::RefreshOptions()
{
 if (!HasCurrentSession()) { return; }
 DisplaySession->GetEquipmentOptions(EgiOptions,SinkerOptions);
 bUpdatingSelection=true;
 if (EgiCombo)
 {
  EgiCombo->ClearOptions(); SinkerCombo->ClearOptions();
  for(const auto& E:EgiOptions) { EgiCombo->AddOption(FString::Printf(TEXT("エギ %.1f号 / %.0f g [%s]"),E.SizeGo,E.BaseMassG,*E.EgiId.ToString())); }
  for(const auto& S:SinkerOptions) { SinkerCombo->AddOption(S.MassG==0?TEXT("シンカー なし (0 g)"):FString::Printf(TEXT("シンカー %.0f g [%s]"),S.MassG,*S.SinkerId.ToString())); }
 }
 const auto Equipment=DisplaySession->GetEquipmentSnapshot();
 SelectEquipment(Equipment.EgiId,Equipment.SinkerId);
 bUpdatingSelection=false;
}
bool UTRFishingHUDWidget::SelectEquipment(FName EgiId,FName SinkerId)
{
 const int32 E=EgiOptions.IndexOfByPredicate([&](const auto& R){return R.EgiId==EgiId;});
 const int32 S=SinkerOptions.IndexOfByPredicate([&](const auto& R){return R.SinkerId==SinkerId;});
 if(E==INDEX_NONE || S==INDEX_NONE){return false;}
 SelectedEgi=EgiId; SelectedSinker=SinkerId;
 const bool OldUpdating=bUpdatingSelection; bUpdatingSelection=true;
 if(EgiCombo){EgiCombo->SetSelectedIndex(E);SinkerCombo->SetSelectedIndex(S);}
 bUpdatingSelection=OldUpdating; UpdatePanel(); return true;
}
void UTRFishingHUDWidget::OnSelectionChanged(FString Item,ESelectInfo::Type SelectionType)
{
 if(bUpdatingSelection){return;}
 if(EgiOptions.IsValidIndex(EgiCombo->GetSelectedIndex())){SelectedEgi=EgiOptions[EgiCombo->GetSelectedIndex()].EgiId;}
 if(SinkerOptions.IsValidIndex(SinkerCombo->GetSelectedIndex())){SelectedSinker=SinkerOptions[SinkerCombo->GetSelectedIndex()].SinkerId;}
 SelectionMessage=FText::GetEmpty(); UpdatePanel();
}
bool UTRFishingHUDWidget::ApplyEquipmentSelection()
{
 if(!HasCurrentSession()){SelectionMessage=LOCTEXT("Stale", "セッションが終了したため適用できません");UpdatePanel();return false;}
 TArray<FText> Errors;
 const auto Result=Controller->SubmitUIEquipment(SelectedEgi,SelectedSinker,DisplayCast,DisplayRegistration,Errors);
 if(Result==ETRCommandResult::Accepted){SelectionMessage=LOCTEXT("Applied", "適用済み：次の投入に使用します");}
 else
 {
  SelectionMessage=DisplaySession->GetEquipmentBlockReason();
  if(SelectionMessage.IsEmpty()){SelectionMessage=LOCTEXT("Invalid", "適用できません。設定・装備ID・画面の状態を確認してください");}
 }
 ApplySnapshot(Controller->GetDebugSnapshot()); return Result==ETRCommandResult::Accepted;
}
bool UTRFishingHUDWidget::RequestDeploy()
{
 if(!HasCurrentSession()){return false;}
 const auto Current=DisplaySession->GetEquipmentSnapshot();
 if(Current.EgiId!=SelectedEgi || Current.SinkerId!=SelectedSinker)
 {SelectionMessage=LOCTEXT("ApplyFirst", "選択を適用してから投入してください");UpdatePanel();return false;}
 return Controller->SubmitUICommand(ETRFishingCommandType::Deploy,DisplayCast,DisplayRegistration);
}
bool UTRFishingHUDWidget::RequestNextCast()
{
 return HasCurrentSession() && Controller->SubmitUICommand(ETRFishingCommandType::NextCast,DisplayCast,DisplayRegistration);
}
void UTRFishingHUDWidget::OnApply(){ApplyEquipmentSelection();}
void UTRFishingHUDWidget::OnDeploy(){RequestDeploy();}
void UTRFishingHUDWidget::OnNext(){RequestNextCast();}
void UTRFishingHUDWidget::OnClose(){if(Controller.IsValid()){Controller->SetPrototypePanelOpen(false);} UpdatePanel();}
void UTRFishingHUDWidget::ApplySnapshot(const FTRHUDSnapshot& S)
{
 DisplaySnapshot=S;
 if(InputStatus){InputStatus->SetText(S.InputConfigurationNote);}
 const bool Details=Controller.IsValid() && Controller->IsPrototypeDetailsOpen();
 const auto Rows=Details?BuildReadout(S):BuildCompactReadout(S);
 if(CompactGuide){CompactGuide->SetText(BuildCompactGuide(S));CompactGuide->SetVisibility(Details?ESlateVisibility::Collapsed:ESlateVisibility::Visible);}
 for(int32 I=0;I<Rows.Num() && I<Values.Num();++I)
 {
  Labels[I]->SetText(Rows[I].Label); Labels[I]->SetColorAndOpacity(LabelColor);
  FString Value=Rows[I].Value.ToString();
  if(!Details){Value.ReplaceInline(TEXT("\n"),TEXT(" / "));}
  Values[I]->SetText(FText::FromString(Value)); Values[I]->SetColorAndOpacity(ValueColor);
  ReadoutPairs[I]->SetVisibility(Details || IsPrimaryReadout(I)?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 }
 if(RevisionStatus){RevisionStatus->SetText(FormatRevisions(S));RevisionStatus->SetColorAndOpacity(HasRevisionMismatch(S)?FLinearColor(1,.25f,.15f):LabelColor);RevisionStatus->SetVisibility(Details||HasRevisionMismatch(S)?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}
 if(DebugHelp){DebugHelp->SetVisibility(Details && (!S.PlayerMode.bValid || S.PlayerMode.Mode==ETRPlayerMode::Fishing)?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}
 const auto Guide=BuildGuide(S);
 const bool bOpen=Controller.IsValid() && Controller->IsPrototypePanelOpen();
 for(int32 I=0;I<Guides.Num();++I)
 {
  if(!Guide.IsValidIndex(I)){Guides[I]->SetVisibility(ESlateVisibility::Collapsed);continue;}
  // Session decides state eligibility; UI capture only disables mouse gameplay locally.
  const bool Enabled=Guide[I].bAvailable && !(bOpen && I<6);
  Guides[I]->SetText(FText::FromString((Enabled?TEXT("○ "):TEXT("— "))+Guide[I].Text.ToString()));
  Guides[I]->SetColorAndOpacity(Enabled?ValueColor:DisabledColor);
  Guides[I]->SetVisibility(Details || IsPrimaryGuide(I)?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 }
 UpdatePanel();
}
void UTRFishingHUDWidget::UpdatePanel()
{
 const bool Open=Controller.IsValid() && Controller->IsPrototypePanelOpen() && HasCurrentSession();
 SetVisibility(Open?ESlateVisibility::Visible:ESlateVisibility::HitTestInvisible);
 if(EquipmentPanel){EquipmentPanel->SetVisibility(Open?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}
 if(Open && !bPanelWasOpen && GetOwningPlayer() && GetOwningPlayer()->GetLocalPlayer()){SetKeyboardFocus();}
 bPanelWasOpen=Open;
 if(!EquipmentStatus){return;}
 const bool Can=DisplaySnapshot.bCanChangeEquipment && HasCurrentSession();
 EquipmentStatus->SetText(Can?LOCTEXT("CanChange", "エギ／シンカーを選択して適用してください"):DisplaySnapshot.EquipmentBlockReason);
 EgiCombo->SetIsEnabled(Can);SinkerCombo->SetIsEnabled(Can);ApplyButton->SetIsEnabled(Can);
 DeployButton->SetIsEnabled(DisplaySnapshot.AvailableCommands.Contains(ETRFishingCommandType::Deploy));
 NextButton->SetIsEnabled(DisplaySnapshot.AvailableCommands.Contains(ETRFishingCommandType::NextCast));
 const auto* E=EgiOptions.FindByPredicate([&](const auto& R){return R.EgiId==SelectedEgi;});
 const auto* S=SinkerOptions.FindByPredicate([&](const auto& R){return R.SinkerId==SelectedSinker;});
 FString Summary=FString::Printf(TEXT("%s：%s + %s / %.1f g"),DisplaySnapshot.bEquipmentLocked?TEXT("今回の投（ロック中）"):TEXT("次投に使用（適用済み）"),*DisplaySnapshot.Equipment.EgiId.ToString(),*DisplaySnapshot.Equipment.SinkerId.ToString(),DisplaySnapshot.Equipment.TotalMassG);
 if(E && S){Summary+=FString::Printf(TEXT("\n選択候補：%.1f号 %.0f g + %.0f g（未適用候補）"),E->SizeGo,E->BaseMassG,S->MassG);}
 Summary+=TEXT("\n")+SelectionMessage.ToString();SelectionStatus->SetText(FText::FromString(Summary));
}
FReply UTRFishingHUDWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
 if(Controller.IsValid() && Controller->IsPrototypePanelOpen())
 {
  const FKey Key=Event.GetKey();
  if(Key==EKeys::Tab){if(!Event.IsRepeat()){OnClose();}return FReply::Handled();}
  if(Key==EKeys::Insert){if(!Event.IsRepeat()){Controller->TogglePrototypeDetails();ApplySnapshot(DisplaySnapshot);}return FReply::Handled();}
  if(Key==EKeys::P){if(!Event.IsRepeat()){Controller->SetPauseRequested(!DisplaySnapshot.bPaused);}return FReply::Handled();}
  // Let an open combo consume Enter to finish editing, never deploy that same key.
  if(Key==EKeys::Enter && ((EgiCombo && EgiCombo->IsOpen()) || (SinkerCombo && SinkerCombo->IsOpen()))){return Super::NativeOnPreviewKeyDown(Geometry,Event);}
  if(Key==EKeys::Enter){if(!Event.IsRepeat()){RequestDeploy();}return FReply::Handled();}
  if(Key==EKeys::N){if(!Event.IsRepeat()){RequestNextCast();}return FReply::Handled();}
 }
 return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
void UTRFishingHUDWidget::NativeDestruct()
{
 if(Controller.IsValid()){Controller->EnablePrototypeUI(false);}
 Controller.Reset();DisplaySession.Reset();EgiOptions.Reset();SinkerOptions.Reset();DisplaySnapshot={};SelectionMessage=FText::GetEmpty();
 Super::NativeDestruct();
}
#undef LOCTEXT_NAMESPACE
