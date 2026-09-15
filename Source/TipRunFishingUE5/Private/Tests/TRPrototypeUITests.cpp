#include "TRSessionTestFixture.h"
#include "UI/TRFishingHUDWidget.h"
#include "Game/TRPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
 struct FPrototypeUI
 {
  FTRTestSession F;
  TWeakObjectPtr<ATRPlayerController> PC;
  TStrongObjectPtr<UTRFishingHUDWidget> UI;
  TStrongObjectPtr<UEnhancedInputComponent> Input;
  TStrongObjectPtr<UEnhancedPlayerInput> PlayerInput;
  bool Start(FAutomationTestBase& Test)
  {
   F.Tuning->Parameters.QuickRetrieveDurationS=1.5;
   if(!F.Start(Test)){return false;}
   PC=F.World->SpawnActor<ATRPlayerController>();
   PC->InputConfig=UTRInputConfigDataAsset::CreateMouseRetrievePrototype(PC.Get());
   Input.Reset(NewObject<UEnhancedInputComponent>(PC.Get()));PC->InputComponent=Input.Get();
   PlayerInput.Reset(NewObject<UEnhancedPlayerInput>(PC.Get()));PC->PlayerInput=PlayerInput.Get();
   if(!PC->InstallInputBindings(Input.Get()) || !PC->BindSession(F.Session.Get())){return false;}
   UI.Reset(CreateWidget<UTRFishingHUDWidget>(F.World,UTRFishingHUDWidget::StaticClass()));
   if(!Test.TestNotNull(TEXT("Runtime UMG widget created"),UI.Get())){return false;}
   UI->TakeWidget();UI->BindController(PC.Get());return true;
  }
  void Inject(FKey Key,bool Down)
  {
   for(const auto& M:PC->InputConfig->FishingContext->GetMappings())
   {if(M.Key==Key){PlayerInput->InjectInputForAction(M.Action,FInputActionValue(Down));PlayerInput->ProcessInputStack({Input.Get()},1.f/60,false);return;}}
  }
  void Refresh(){UI->RefreshFromController();}
 };
 FString RowValue(const FTRHUDSnapshot& S,const TCHAR* Label)
 {
  for(const auto& R:UTRFishingHUDWidget::BuildReadout(S)){if(R.Label.ToString()==Label){return R.Value.ToString();}}return FString();
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRPrototypeHUDTest,"TipRun.M105F.JapaneseSnapshotAndGuide",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRPrototypeHUDTest::RunTest(const FString& Parameters)
{
 FPrototypeUI R;if(!R.Start(*this)){return false;}
 TestTrue(TEXT("Ready automatically opens equipment"),R.PC->IsPrototypePanelOpen());
 const auto Before=R.F.Sim()->GetSimulationTime();const int32 Queue=R.F.Sim()->GetQueuedCommandCount();
 for(int32 I=0;I<20;++I){R.Refresh();}
 TestEqual(TEXT("UI reads do not advance clock"),R.F.Sim()->GetSimulationTime().TickIndex,Before.TickIndex);
 TestEqual(TEXT("UI reads do not change queue"),R.F.Sim()->GetQueuedCommandCount(),Queue);
 TestTrue(TEXT("Separate label and value styles"),R.UI->LabelColor!=R.UI->ValueColor);
 TestTrue(TEXT("Enter is state available"),R.UI->DisplaySnapshot.AvailableCommands.Contains(ETRFishingCommandType::Deploy));
 TestTrue(TEXT("UI deploy enqueued"),R.UI->RequestDeploy());R.F.Step();R.Refresh();
 TestTrue(TEXT("Deployed UI reflects FreeFall"),RowValue(R.UI->DisplaySnapshot,TEXT("釣り状態")).Contains(TEXT("フリーフォール")));
 TestFalse(TEXT("Cast closes panel"),R.PC->IsPrototypePanelOpen());
 auto S=R.UI->DisplaySnapshot;S.bEnvironmentValid=true;S.bEgiValid=true;
 S.Egi.DepthM=12.345f;S.Egi.HorizontalDistanceFromBoatM=6.25;S.Egi.HorizontalDistanceFromRodTipM=7.5;
 S.Boat.VelocityMps=FVector(.3,.4,0);S.Boat.WindMps=FVector2D(0,2);S.Boat.SurfaceCurrentMps=FVector(.4,0,0);
 S.Egi.CurrentAtEgiDepthMps=FVector(0,-.2,0);S.Egi.DepthVelocityMps=-.123f;S.Equipment.TotalMassG=45;
 const auto Text=UTRFishingHUDWidget::FormatSnapshot(S).ToString();
 for(const TCHAR* Label:{TEXT("キャストID"),TEXT("エギ深度"),TEXT("水深"),TEXT("深度変化速度"),TEXT("船からの水平距離"),TEXT("竿先からの水平距離"),TEXT("ライン長"),TEXT("ライン角度"),TEXT("ライン弛み"),TEXT("張力Proxy"),TEXT("船ドリフト"),TEXT("風向／風速"),TEXT("表層潮"),TEXT("エギ深度の潮流"),TEXT("エギ重量"),TEXT("シンカー重量"),TEXT("総重量"),TEXT("装備ロック状態"),TEXT("装備変更可否"),TEXT("Normal Retrieve"),TEXT("クイック回収／進捗")}){TestTrue(Label,Text.Contains(Label));}
 TestTrue(TEXT("Snapshot values formatted, independent vectors"),Text.Contains(TEXT("12.345 m")) && Text.Contains(TEXT("6.250 m")) && Text.Contains(TEXT("7.500 m")) && Text.Contains(TEXT("0.500 m/s")) && Text.Contains(TEXT("2.000 m/s")) && Text.Contains(TEXT("0.400 m/s")) && Text.Contains(TEXT("0.200 m/s")) && Text.Contains(TEXT("45.0 g")) && Text.Contains(TEXT("上昇")));
 R.UI->ApplySnapshot(S);
 TArray<UWidget*> DisplayWidgets;R.UI->WidgetTree->GetAllWidgets(DisplayWidgets);
 bool LabelSeen=false,ValueSeen=false;
 for(UWidget* W:DisplayWidgets){if(auto* T=Cast<UTextBlock>(W))
 {
  if(T->GetText().ToString()==TEXT("エギ深度")){LabelSeen=T->GetColorAndOpacity().GetSpecifiedColor()==R.UI->LabelColor;}
  if(T->GetText().ToString()==TEXT("12.345 m")){ValueSeen=T->GetColorAndOpacity().GetSpecifiedColor()==R.UI->ValueColor;}
 }}
 TestTrue(TEXT("Actual UMG label/value nodes receive separate styles and snapshot"),LabelSeen && ValueSeen);
 FString Guide;for(const auto& G:UTRFishingHUDWidget::BuildGuide(S)){Guide+=G.Text.ToString();}
 for(const TCHAR* Key:{TEXT("マウス移動"),TEXT("左クリック"),TEXT("右クリック"),TEXT("F："),TEXT("Q："),TEXT("Enter：")}){TestTrue(Key,Guide.Contains(Key));}
 S.Retrieval.bIsQuickRetrieving=true;S.Retrieval.QuickRetrieveProgress01=.5;
 TestTrue(TEXT("Quick physical sample clearly frozen"),UTRFishingHUDWidget::FormatSnapshot(S).ToString().Contains(TEXT("回収中・凍結値")));
 TestTrue(TEXT("Progress correct"),RowValue(S,TEXT("クイック回収／進捗")).Contains(TEXT("50.0%")));
 S.bEgiValid=false;S.bEgiOnboard=true;
 TestTrue(TEXT("Onboard not fake zero depth"),RowValue(S,TEXT("エギ深度")).Contains(TEXT("船上")));
 R.PC->InputConfig=UTRInputConfigDataAsset::CreatePrototype(R.PC.Get());
 const auto Legacy=R.PC->GetDebugSnapshot();
 TestTrue(TEXT("Unmigrated input diagnosed without modifying saved assets"),!Legacy.InputConfigurationNote.IsEmpty() && Legacy.UnmappedPrimaryInputs.Contains(ETRFishingCommandType::Jerk));
 TestFalse(TEXT("Missing mouse binding not shown as usable"),UTRFishingHUDWidget::BuildGuide(Legacy)[1].bAvailable);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRPrototypeEquipmentTest,"TipRun.M105F.EquipmentTableAndCastLock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRPrototypeEquipmentTest::RunTest(const FString& Parameters)
{
 FPrototypeUI R;if(!R.Start(*this)){return false;}
 TArray<FTREgiSpecRow> Egis;TArray<FTRSinkerSpecRow> Sinkers;R.F.Session->GetEquipmentOptions(Egis,Sinkers);
 TestEqual(TEXT("Three table egis"),R.UI->GetEgiOptionCount(),3);TestEqual(TEXT("Nine table sinkers"),R.UI->GetSinkerOptionCount(),9);
 for(const auto& E:Egis){for(const auto& S:Sinkers)
 {
  TestTrue(TEXT("Table selection"),R.UI->SelectEquipment(E.EgiId,S.SinkerId));
  TestTrue(TEXT("Ready UI applies all 27 combinations"),R.UI->ApplyEquipmentSelection());
  TestEqual(TEXT("Authoritative total"),R.F.Session->GetEquipmentSnapshot().TotalMassG,E.BaseMassG+S.MassG);
 }}
 TestFalse(TEXT("Unknown ID absent from choices"),R.UI->SelectEquipment(TEXT("NotInTable"),TEXT("Sinker_None")));
 TestTrue(TEXT("Select next cast 40+10"),R.UI->SelectEquipment(TEXT("Egi_4"),TEXT("Sinker_10")));
 TestFalse(TEXT("Unapplied draft cannot deploy"),R.UI->RequestDeploy());
 TestTrue(TEXT("Apply next equipment"),R.UI->ApplyEquipmentSelection());TestTrue(TEXT("Deploy via UI"),R.UI->RequestDeploy());
 R.F.Step();R.Refresh();
 TestEqual(TEXT("Next deploy uses changed total"),R.F.Session->GetEquipmentSnapshot().TotalMassG,50.f);
 TestTrue(TEXT("Deploy locks"),R.F.Session->IsEquipmentLocked());
 R.PC->SetPrototypePanelOpen(true);R.Refresh();R.UI->SelectEquipment(TEXT("Egi_3"),TEXT("Sinker_None"));
 TestFalse(TEXT("Cast UI change rejected"),R.UI->ApplyEquipmentSelection());
 TestTrue(TEXT("Japanese reason"),R.UI->GetSelectionMessage().ToString().Contains(TEXT("キャスト中")));
 TestEqual(TEXT("Rejection preserves mass"),R.F.Session->GetEquipmentSnapshot().TotalMassG,50.f);
 R.PC->SetPrototypePanelOpen(false);
 R.F.Session->SubmitCommand(ETRFishingCommandType::RetrieveStarted,R.F.Session->GetCastId());
 for(int32 I=0;I<12000 && R.F.Session->GetSessionPhase()!=ETRSessionPhase::Result;++I){R.F.Step();}
 R.Refresh();TestTrue(TEXT("Normal Result"),R.F.Session->GetSessionPhase()==ETRSessionPhase::Result);
 TestFalse(TEXT("Result equipment refused"),R.UI->ApplyEquipmentSelection());
 TestTrue(TEXT("Result explains NextCast"),R.UI->GetSelectionMessage().ToString().Contains(TEXT("次投")));
 TestTrue(TEXT("Result UI NextCast"),R.UI->RequestNextCast());R.F.Step();R.Refresh();
 TestTrue(TEXT("Ready unlocked"),R.UI->DisplaySnapshot.bCanChangeEquipment && R.PC->IsPrototypePanelOpen());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRPrototypeQuickUITest,"TipRun.M105F.QuickReadyAndStaleRequests",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRPrototypeQuickUITest::RunTest(const FString& Parameters)
{
 FPrototypeUI R;if(!R.Start(*this)){return false;}
 const auto OldCast=R.F.Session->GetCastId();const auto OldReg=R.F.Session->GetRegistrationId();
 R.UI->RequestDeploy();R.F.Step(30);R.Refresh();
 R.F.Session->SubmitCommand(ETRFishingCommandType::QuickRetrieve,R.F.Session->GetCastId());R.F.Step();R.Refresh();
 TestTrue(TEXT("Quick flags"),R.UI->DisplaySnapshot.Retrieval.bIsQuickRetrieving);
 TestEqual(TEXT("Quick no fishing operations available"),R.UI->DisplaySnapshot.AvailableCommands.Num(),0);
 R.F.Step(90);R.Refresh();
 TestTrue(TEXT("Quick auto opens Ready"),R.PC->IsPrototypePanelOpen() && R.UI->DisplaySnapshot.bCanChangeEquipment);
 TArray<FText> Errors;
 TestTrue(TEXT("Old UI request rejected"),R.PC->SubmitUIEquipment(TEXT("Egi_4"),TEXT("Sinker_10"),OldCast,OldReg,Errors)!=ETRCommandResult::Accepted);
 R.UI->SelectEquipment(TEXT("Egi_3"),TEXT("Sinker_5"));TestTrue(TEXT("Immediate Quick equipment change"),R.UI->ApplyEquipmentSelection());
 TestEqual(TEXT("Quick changed total"),R.F.Session->GetEquipmentSnapshot().TotalMassG,35.f);
 TestTrue(TEXT("Next deploy"),R.UI->RequestDeploy());R.F.Step();R.Refresh();
 TestTrue(TEXT("Changed equipment locked on next deploy"),R.F.Session->IsEquipmentLocked() && R.F.Session->GetEquipmentSnapshot().EgiId==FName(TEXT("Egi_3")));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRPrototypeConflictTest,"TipRun.M105F.EnhancedUIInputConflictAndFocus",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRPrototypeConflictTest::RunTest(const FString& Parameters)
{
 FPrototypeUI R;if(!R.Start(*this)){return false;}
 R.UI->RequestDeploy();R.F.Step(60);R.Refresh();
 R.PC->ActionStarted(ETRPlayerAction::Retrieve);R.F.Step();TestTrue(TEXT("Retrieve held before panel"),R.PC->IsRetrieveHeld());
 R.PC->SetPrototypePanelOpen(true);R.Refresh();R.F.Step();
 TestFalse(TEXT("Opening UI releases held retrieve"),R.PC->IsRetrieveHeld());
 const auto Before=R.F.Session->Fishing->GetSnapshot();const int32 Count=R.F.Sim()->GetQueuedCommandCount();
 for(int32 I=0;I<4;++I){R.Inject(EKeys::LeftMouseButton,true);R.Inject(EKeys::RightMouseButton,true);}
 TestFalse(TEXT("UI mouse delta not delivered"),R.PC->SubmitMouseDelta(FVector2D(5,5)));
 TestEqual(TEXT("UI action clicks never enqueue fishing"),R.F.Sim()->GetQueuedCommandCount(),Count);
 R.F.Step();TestEqual(TEXT("No Shakuri from UI"),R.F.Session->Fishing->GetSnapshot().JerkCount,Before.JerkCount);
 TestFalse(TEXT("No Retrieve from UI"),R.F.Session->GetHUDSnapshot().Retrieval.bIsRetrieving);
 // Actual UMG controls share the same guarded request path, including a stale enabled button.
 TArray<UWidget*> Widgets;R.UI->WidgetTree->GetAllWidgets(Widgets);
 for(UWidget* W:Widgets){if(auto* B=Cast<UButton>(W)){auto* T=Cast<UTextBlock>(B->GetContent());if(T && T->GetText().ToString().Contains(TEXT("選択した装備"))){B->OnClicked.Broadcast();}}}
 TestFalse(TEXT("UMG click cannot retrieve"),R.PC->IsRetrieveHeld());
 R.PC->SetPrototypePanelOpen(false);
 R.Inject(EKeys::LeftMouseButton,true);R.Inject(EKeys::RightMouseButton,true);R.F.Step();
 TestFalse(TEXT("Closing UI never resumes held retrieve"),R.PC->IsRetrieveHeld());
 TestEqual(TEXT("Closing UI never repeats held jerk"),R.F.Session->Fishing->GetSnapshot().JerkCount,Before.JerkCount);
 R.Inject(EKeys::LeftMouseButton,false);R.Inject(EKeys::RightMouseButton,false);
 R.Inject(EKeys::LeftMouseButton,true);R.F.Step();TestTrue(TEXT("Fresh press after release works"),R.PC->IsRetrieveHeld());
 R.PC->SetInputFocus(false);R.PC->SetPauseRequested(true);R.Refresh();const auto Tick=R.F.Sim()->GetSimulationTime().TickIndex;R.F.Step(30);
 TestEqual(TEXT("Paused clock"),R.F.Sim()->GetSimulationTime().TickIndex,Tick);TestFalse(TEXT("Focus/pause clears held"),R.PC->IsRetrieveHeld());
 R.PC->SetPauseRequested(false);R.PC->SetInputFocus(true);R.F.Step();TestFalse(TEXT("Resume does not rearm retrieve"),R.PC->IsRetrieveHeld());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRPrototypeLifetimeTest,"TipRun.M105F.WidgetSessionLifetime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRPrototypeLifetimeTest::RunTest(const FString& Parameters)
{
 FPrototypeUI R;if(!R.Start(*this)){return false;}
 R.F.Session->EndFishing();R.Refresh();
 TestFalse(TEXT("Ended session HUD invalid"),R.UI->DisplaySnapshot.bSessionValid);
 TestEqual(TEXT("Ended options cleared"),R.UI->GetEgiOptionCount(),0);TestFalse(TEXT("Ended UI cannot apply"),R.UI->ApplyEquipmentSelection());
 R.F.Session->Destroy();R.Refresh();TestFalse(TEXT("Destroyed session safe"),R.UI->RequestDeploy());
 FPrototypeUI New;if(!New.Start(*this)){return false;}
 TestEqual(TEXT("New play starts without old cast"),New.UI->DisplaySnapshot.CastId.Value,int64(0));
 TestEqual(TEXT("New play initial gear"),New.UI->DisplaySnapshot.Equipment.TotalMassG,35.f);
 TestTrue(TEXT("New play Ready panel"),New.PC->IsPrototypePanelOpen());
 R.Refresh();TestFalse(TEXT("Old widget cannot target new session"),R.UI->ApplyEquipmentSelection());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRPrototypePanelKeysTest,"TipRun.M105F.NativePanelKeys",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRPrototypePanelKeysTest::RunTest(const FString& Parameters)
{
 FPrototypeUI R;if(!R.Start(*this)){return false;}
 const auto Slate=R.UI->TakeWidget();
 auto Key=[&](FKey K,bool Repeat=false){return Slate->OnPreviewKeyDown(FGeometry(),FKeyEvent(K,FModifierKeysState(),0,Repeat,0,0)).IsEventHandled();};
 TestTrue(TEXT("Native P consumed by UI"),Key(EKeys::P));R.Refresh();
 TestTrue(TEXT("P pauses"),R.UI->DisplaySnapshot.bPaused);
 TestTrue(TEXT("Native P resumes"),Key(EKeys::P));R.Refresh();
 TestFalse(TEXT("Resumed"),R.UI->DisplaySnapshot.bPaused);
 TestTrue(TEXT("Native Tab closes"),Key(EKeys::Tab));TestFalse(TEXT("Closed"),R.PC->IsPrototypePanelOpen());
 R.PC->TogglePrototypePanel();R.Refresh();TestTrue(TEXT("Tab action can reopen"),R.PC->IsPrototypePanelOpen());
 const int32 Count=R.F.Sim()->GetQueuedCommandCount();
 TestTrue(TEXT("Repeated Enter consumed"),Key(EKeys::Enter,true));
 TestEqual(TEXT("Repeat cannot deploy"),R.F.Sim()->GetQueuedCommandCount(),Count);
 TestTrue(TEXT("Native Enter submits deploy"),Key(EKeys::Enter));R.F.Step();R.Refresh();
 TestTrue(TEXT("Native input reaches fixed deploy"),R.UI->DisplaySnapshot.Egi.FishingState==ETRFishingState::FreeFall && R.UI->DisplaySnapshot.bEquipmentLocked);
 return true;
}
#endif
