#include "TRSessionTestFixture.h"
#include "Data/TRNavigationTuningDataAsset.h"
#include "Game/TRPlayerController.h"
#include "Game/TRPlayerCameraManager.h"
#include "Game/TRGameModeBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Engine/Blueprint.h"
#include "UI/TRFishingHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "InputKeyEventArgs.h"
#include "Engine/GameViewportClient.h"
#include <limits>
#include "Game/TRPrototypeViewActor.h"
#include "Data/TRRodTuningDataAsset.h"
#include "Boat/TRBoatDriftComponent.h"
#include "Components/StaticMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
 struct FNavigationTest
 {
  FTRTestSession F;
  TStrongObjectPtr<UTRNavigationTuningDataAsset> Tuning{NewObject<UTRNavigationTuningDataAsset>()};
  FNavigationTest(bool Environment=false)
  {
   Tuning->Parameters=FTRNavigationParameters::Prototype();F.Session->NavigationTuning=Tuning.Get();
   auto& B=F.BoatTuning->Parameters;B={};B.ModelRevision=2;B.WindResponseKgPerS=.1;B.CurrentResponseKgPerS=1;B.DragKgPerS=1;
   B.InertiaKg=10;B.BowWindScale=1;B.SternWindScale=.5;B.SideWindScale=2;B.MaxDriftSpeedMps=1;B.HullHeightOffsetM=.5;B.RodAnchorOffsetM=FVector(2,1,1);
   F.Area->Settings.FieldRevision=2;F.Area->Settings.CurrentMps=Environment?FVector(.36,0,0):FVector::ZeroVector;
   F.Area->Settings.WindMps=Environment?FVector2D(0,2):FVector2D::ZeroVector;
  }
  bool Start(FAutomationTestBase& Test)
  {if(!F.Initialize(Test)){return false;}TArray<FText> E;return Test.TestTrue(TEXT("Navigation session starts"),F.Session->StartFishing(E)==ETRCommandResult::Accepted);}
  bool Send(FVector2D V,int64 Tick=-1)
  {return F.Session->SubmitNavigationInput(V,F.Session->GetPlayerModeSnapshot().ModeEpoch,F.Session->GetRegistrationId(),Tick);}
 };
 bool SaveNavigationAsset(UObject* Asset)
 {
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  const FString Filename=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  return UPackage::SavePackage(Asset->GetOutermost(),Asset,*Filename,Args);
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNavigationMotionTest,"TipRun.M105R2.MotionAndEnvironment",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNavigationMotionTest::RunTest(const FString&)
{
 for(bool Environment:{false,true})
 {
  FNavigationTest R(Environment);if(!R.Start(*this)){return false;}
  const auto Origin=R.F.Boat->GetBoatSnapshot();R.Send(FVector2D(1,0));R.F.Step();
  TestTrue(TEXT("Force accelerates rather than assigning velocity"),R.F.Boat->GetBoatSnapshot().SpeedMps>0 && R.F.Boat->GetBoatSnapshot().SpeedMps<.1);
  R.F.Step(600);auto Forward=R.F.Boat->GetBoatSnapshot();
  TestTrue(TEXT("Forward moves in heading direction above legacy drift limit"),Forward.PositionM.X>Origin.PositionM.X+5 && Forward.SpeedMps>1);
  if(Environment){TestTrue(TEXT("Wind contribution alongside engine"),Forward.PositionM.Y>Origin.PositionM.Y);}
  R.Send(FVector2D(0,1));R.F.Step(120);auto Turn=R.F.Boat->GetBoatSnapshot();
  TestTrue(TEXT("Steering changes heading independently of velocity"),Turn.HeadingRad>.2 && !Turn.ForwardVector.Equals(Turn.VelocityMps.GetSafeNormal(),.01));
  const auto Epoch=R.F.Session->GetPlayerModeSnapshot().ModeEpoch;
  R.F.Session->SubmitModeChange(ETRPlayerMode::Fishing,Epoch,R.F.Session->GetRegistrationId());
  R.F.Step();auto Fishing=R.F.Boat->GetBoatSnapshot();
  TestTrue(TEXT("Fishing clears thrust/yaw/navigation velocity"),!R.F.Session->GetNavigationSnapshot().bEngineActive &&
   R.F.Session->GetNavigationSnapshot().Steering==0 && Fishing.HeadingRad==Turn.HeadingRad && Fishing.SpeedMps<.01);
  TestFalse(TEXT("Fishing rejects throttle and steering at session boundary"),R.Send(FVector2D(1,1)));
  // Bypass the convenience API: simulation must reject it too.
  R.F.Sim()->EnqueueCommand(R.F.Session->GetRegistrationId(),ETRFishingCommandType::NavigationInput,0,-1,R.F.Session->GetCastId(),FVector2D(1,1),R.F.Session->GetPlayerModeSnapshot().ModeEpoch);
  R.F.Step();TestTrue(TEXT("Fixed handler rejects navigation in Fishing"),R.F.Session->GetLastCommandResult()==ETRCommandResult::RejectedInvalidState);
  R.F.Step(600);TestEqual(TEXT("Fishing heading held"),R.F.Boat->GetBoatSnapshot().HeadingRad,Turn.HeadingRad);
  TestTrue(TEXT("Navigation coasting not disabled by old speed guard"),R.F.Boat->GetBoatMode()==ETRBoatMode::DriftOnly);
  if(Environment){TestTrue(TEXT("Environmental drift continues after engine stopped"),R.F.Boat->GetBoatSnapshot().SpeedMps>.01);}
 }
 FNavigationTest Reverse;if(!Reverse.Start(*this)){return false;}
 Reverse.Send(FVector2D(-1,0));Reverse.F.Step(600);
 TestTrue(TEXT("Reverse travels backward"),Reverse.F.Boat->GetBoatSnapshot().PositionM.X<5);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNavigationLifetimeTest,"TipRun.M105R2.DeterminismPauseAndLifetime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNavigationLifetimeTest::RunTest(const FString&)
{
 FTRBoatSnapshot Reference;
 for(int FPS:{30,60,120})
 {
  FNavigationTest R(true);if(!R.Start(*this)){return false;}
  R.Send(FVector2D(1,0),0);R.Send(FVector2D(.5,1),120);R.Send(FVector2D(-1,-.5),240);R.Send(FVector2D(0,0),360);
  for(int I=0;I<FPS*8;++I){R.F.Sim()->AdvanceFrame(1.0/FPS);}
  auto B=R.F.Boat->GetBoatSnapshot();if(FPS==30){Reference=B;}else{TestTrue(TEXT("Exact frame-rate invariant motion"),B.PositionM==Reference.PositionM && B.VelocityMps==Reference.VelocityMps && B.HeadingRad==Reference.HeadingRad);}
  R.F.Session->SetPlayerPaused(true);R.F.Session->ClearPlayerCommands();R.F.Step(120);
  TestTrue(TEXT("Pause holds boat"),R.F.Boat->GetBoatSnapshot().PositionM==B.PositionM);TestFalse(TEXT("Paused throttle rejected"),R.Send(FVector2D(1,1)));
  R.F.Session->SetPlayerPaused(false);R.F.Step();TestFalse(TEXT("No resumed engine input"),R.F.Session->GetNavigationSnapshot().bEngineActive);
  TestFalse(TEXT("NaN rejected"),R.Send(FVector2D(std::numeric_limits<double>::quiet_NaN(),0)));
  TestFalse(TEXT("Inf rejected"),R.Send(FVector2D(0,std::numeric_limits<double>::infinity())));
  R.Send(FVector2D(1,1));R.F.Step(120);const auto Heading=R.F.Boat->GetBoatSnapshot().HeadingRad;
  const auto Id=R.F.Session->GetRegistrationId();R.F.World->DestroyActor(R.F.Session.Get());R.F.Step(120);
  TestFalse(TEXT("Destroyed session unregistered"),R.F.Sim()->IsRegistered(Id));
  TestEqual(TEXT("No steering survives session destruction"),R.F.Boat->GetBoatSnapshot().HeadingRad,Heading);
  TestFalse(TEXT("Finite motion after lifetime boundary"),R.F.Boat->GetBoatSnapshot().PositionM.ContainsNaN());
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNavigationCameraTest,"TipRun.M105R2.InputCameraAndValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNavigationCameraTest::RunTest(const FString&)
{
 FNavigationTest R;if(!R.Start(*this)){return false;}
 auto* PC=R.F.World->SpawnActor<ATRPlayerController>();
 PC->InputConfig=UTRInputConfigDataAsset::CreateMouseRetrievePrototype(PC);PC->InputConfig->CreateNavigationPrototype();
 auto* Camera=R.F.World->SpawnActor<ATRPlayerCameraManager>();PC->PlayerCameraManager=Camera;
 auto* Input=NewObject<UEnhancedInputComponent>(PC);PC->InputComponent=Input;
 auto* PlayerInput=NewObject<UEnhancedPlayerInput>(PC);PC->PlayerInput=PlayerInput;
 TestTrue(TEXT("Separate context installs"),PC->InstallInputBindings(Input));PC->BindSession(R.F.Session.Get());
 auto Inject=[&](UInputAction* Action,float V){PlayerInput->InjectInputForAction(Action,FInputActionValue(V));PlayerInput->ProcessInputStack({Input},1.f/60,false);};
 Inject(PC->InputConfig->NavigationThrottle,1);R.F.Step();TestEqual(TEXT("Enhanced throttle reaches fixed simulation"),R.F.Session->GetNavigationSnapshot().Throttle,1.);
 PC->SetInputFocus(false);R.F.Step();TestFalse(TEXT("Focus loss stops thrust"),R.F.Session->GetNavigationSnapshot().bEngineActive);
 PC->SetInputFocus(true);Inject(PC->InputConfig->NavigationThrottle,1);R.F.Step();TestFalse(TEXT("Held key does not restart after focus"),R.F.Session->GetNavigationSnapshot().bEngineActive);
 Inject(PC->InputConfig->NavigationThrottle,0);Inject(PC->InputConfig->NavigationThrottle,1);R.F.Step();TestTrue(TEXT("Release and new press rearms"),R.F.Session->GetNavigationSnapshot().bEngineActive);
 Inject(PC->InputConfig->NavigationThrottle,0);R.F.Step();
 const auto Heading=R.F.Boat->GetBoatSnapshot().HeadingRad;
 for(int I=0;I<100;++I){TestTrue(TEXT("Camera look handled"),PC->ApplyNavigationLook(FVector2D(20,20)));}
 auto View=Camera->GetNavigationSnapshot();
 TestTrue(TEXT("Pitch clamp"),View.PitchDeg<=R.Tuning->Parameters.CameraMaxPitchDeg);
 R.F.Step(10);TestEqual(TEXT("Mouse look cannot steer boat"),R.F.Boat->GetBoatSnapshot().HeadingRad,Heading);
 Camera->Zoom(1000);TestEqual(TEXT("Zoom limit"),Camera->GetNavigationSnapshot().DistanceM,R.Tuning->Parameters.CameraMinDistanceM);
 PC->ResetPrototypeCamera();View=Camera->GetNavigationSnapshot();TestEqual(TEXT("Home resets yaw to boat heading"),View.YawDeg,FMath::RadiansToDegrees(double(Heading)));
 FMinimalViewInfo POV;TestTrue(TEXT("World camera constructed"),Camera->BuildNavigationView(R.F.Boat->GetBoatSnapshot().PositionM,POV));
 TestFalse(TEXT("Finite camera"),POV.Location.ContainsNaN());
 auto Bad=R.Tuning->Parameters;Bad.EngineForceN=std::numeric_limits<double>::infinity();TestFalse(TEXT("Invalid config refused"),Bad.Validate());
 PC->UnbindSession();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNavigationAssetsTest,"TipRun.M105R2.SavedPrototype",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNavigationAssetsTest::RunTest(const FString&)
{
 const FString Root=TEXT("/Game/TipRun/Prototype/M105/");
 auto* Config=LoadObject<UTRSessionConfigDataAsset>(nullptr,*(Root+TEXT("Data/DA_TR_M105Session_Prototype")));
 if(!TestNotNull(TEXT("Existing session asset"),Config)){return false;}
 if(FParse::Param(FCommandLine::Get(),TEXT("TRMigrateM105R2")))
 {
  if(!Config->Navigation)
  {
   const FString Path=Root+TEXT("Data/DA_TR_M105Navigation_Prototype");
   Config->Navigation=LoadObject<UTRNavigationTuningDataAsset>(nullptr,*Path,nullptr,LOAD_NoWarn);
   if(!Config->Navigation){Config->Navigation=NewObject<UTRNavigationTuningDataAsset>(CreatePackage(*Path),*FPackageName::GetLongPackageAssetName(Path),RF_Public|RF_Standalone);Config->Navigation->Parameters=FTRNavigationParameters::Prototype();}
  }
  Config->Input->CreateNavigationPrototype();
  TestTrue(TEXT("Save navigation tuning"),SaveNavigationAsset(Config->Navigation));
  TestTrue(TEXT("Save explicit navigation input"),SaveNavigationAsset(Config->Input));
  TestTrue(TEXT("Save session reference"),SaveNavigationAsset(Config));
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("TRReviseM105R2")) || FParse::Param(FCommandLine::Get(),TEXT("TRFinalizeM105R2")))
 {
  if(!TestNotNull(TEXT("Existing navigation to revise"),Config->Navigation.Get())){return false;}
  auto& P=Config->Navigation->Parameters;const auto New=FTRNavigationParameters::Prototype();
  P.EngineForceN=New.EngineForceN;P.ReverseScale=New.ReverseScale;P.EngineResponsePerS=New.EngineResponsePerS;
  P.SteeringRateRadPerS=New.SteeringRateRadPerS;P.SteeringResponsePerS=New.SteeringResponsePerS;P.MaxSpeedMps=New.MaxSpeedMps;
  P.NavigationLateralResponsePerS=New.NavigationLateralResponsePerS;
  Config->Input->CreateNavigationPrototype();
  TestTrue(TEXT("Save revised navigation tuning"),SaveNavigationAsset(Config->Navigation));
  TestTrue(TEXT("Save Fishing Start mapping"),SaveNavigationAsset(Config->Input));
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("TRBoostM105R2")))
 {
  if(!Config->Navigation || !Config->Input){return false;}
  auto& P=Config->Navigation->Parameters;const auto New=FTRNavigationParameters::Prototype();
  P.BoostThrustMultiplier=New.BoostThrustMultiplier;P.BoostMaxSpeedMps=New.BoostMaxSpeedMps;P.BoostResponsePerS=New.BoostResponsePerS;
  Config->Input->CreateNavigationPrototype();
  TestTrue(TEXT("Save boost tuning"),SaveNavigationAsset(Config->Navigation));
  TestTrue(TEXT("Save boost input"),SaveNavigationAsset(Config->Input));
 }
 if(!TestNotNull(TEXT("Saved Navigation asset reference"),Config->Navigation.Get())){return false;}
 TestTrue(TEXT("Saved boost enabled"),Config->Navigation->Parameters.BoostThrustMultiplier>1 && Config->Input->NavigationBoost);
 TArray<FText> Errors;TestTrue(TEXT("Saved startup validates"),Config->ValidateStartup(Errors));
 for(const auto& E:Errors){AddError(E.ToString());}
 TestTrue(TEXT("Distinct navigation context"),Config->Input->NavigationContext && Config->Input->NavigationContext!=Config->Input->FishingContext);
 auto* BP=LoadObject<UBlueprint>(nullptr,*(Root+TEXT("BP_TR_M105GameMode_Prototype")));
 if(!TestNotNull(TEXT("Saved GameMode Blueprint"),BP) || !TestNotNull(TEXT("Saved generated class"),BP->GeneratedClass.Get())){return false;}
 FTRTestSession F;auto* Mode=F.World->SpawnActor<ATRGameModeBase>(BP->GeneratedClass);
 if(!TestTrue(TEXT("Stored startup"),Mode->InitializeSession(Errors))){return false;}
 auto* PC=F.World->SpawnActor<ATRPlayerController>();PC->InputConfig=Config->Input;PC->BindSession(Mode->GetSession());
 TestTrue(TEXT("Saved setup navigation input"),PC->SubmitNavigationInput(FVector2D(1,1)));F.Step(120);
 const auto Snapshot=Mode->GetSession()->GetHUDSnapshot();TestTrue(TEXT("Saved boat moves and turns"),Snapshot.Boat.SpeedMps>0 && Snapshot.Boat.HeadingRad>.1);
 PC->RequestPlayerMode(ETRPlayerMode::Fishing);F.Step();TestFalse(TEXT("Saved setup rejects fishing-mode navigation"),PC->SubmitNavigationInput(FVector2D(1,1)));
 PC->UnbindSession();Mode->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNavigationEntryTest,"TipRun.M105R2.PlayerEntryAndHUD",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNavigationEntryTest::RunTest(const FString&)
{
 FTRTestSession F;
 auto* Config=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));
 if(!TestNotNull(TEXT("Saved config"),Config)){return false;}
 auto* Mode=F.World->SpawnActor<ATRGameModeBase>();Mode->SessionConfig=Config;
 TArray<FText> Errors;if(!TestTrue(TEXT("Saved startup"),Mode->InitializeSession(Errors))){return false;}
 auto* S=Mode->GetSession();auto* PC=F.World->SpawnActor<ATRPlayerController>();PC->InputConfig=Config->Input;
 auto* Input=NewObject<UEnhancedInputComponent>(PC);PC->InputComponent=Input;
 auto* PlayerInput=NewObject<UEnhancedPlayerInput>(PC);PC->PlayerInput=PlayerInput;
 auto* Camera=F.World->SpawnActor<ATRPlayerCameraManager>();PC->PlayerCameraManager=Camera;
 if(!TestTrue(TEXT("Saved actions bind"),PC->InstallInputBindings(Input)) || !PC->BindSession(S)){return false;}
 TStrongObjectPtr<UTRFishingHUDWidget> UI(CreateWidget<UTRFishingHUDWidget>(F.World,UTRFishingHUDWidget::StaticClass()));
 UI->TakeWidget();UI->BindController(PC);
 auto Inject=[&](UInputMappingContext* Context,FKey Key,const FInputActionValue& V)
 {
  int32 Count=0;for(const auto& M:Context->GetMappings()){if(M.Key==Key){++Count;PlayerInput->InjectInputForAction(M.Action,V);}}
  TestEqual(TEXT("One action per tested primary key/context"),Count,1);PlayerInput->ProcessInputStack({Input},1.f/60,false);
 };
 auto HasVisibleText=[&](const TCHAR* Text)
 {
  UI->RefreshFromController();TArray<UWidget*> Widgets;UI->WidgetTree->GetAllWidgets(Widgets);
  for(auto* W:Widgets){if(auto* T=Cast<UTextBlock>(W)){if(T->GetVisibility()!=ESlateVisibility::Collapsed && T->GetText().ToString().Contains(Text)){return true;}}}return false;
 };
 TestTrue(TEXT("Navigation guide and Home visible"),HasVisibleText(TEXT("W/S")) && HasVisibleText(TEXT("Home")) && HasVisibleText(TEXT("Enter：釣り開始")));
 TestFalse(TEXT("No legacy Shift guide in Navigation"),HasVisibleText(TEXT("Shift+Mouse")));
 TestFalse(TEXT("Navigation Deploy rejected"),PC->ActionStarted(ETRPlayerAction::Deploy));
 Inject(Config->Input->NavigationContext,EKeys::W,FInputActionValue(1.f));F.Step(180);
 const auto Before=S->GetHUDSnapshot().Boat;
 PC->ApplyNavigationLook(FVector2D(40,20));Camera->Zoom(4);
 TestTrue(TEXT("Camera changed distance"),Camera->GetNavigationSnapshot().DistanceM!=Config->Navigation->Parameters.CameraDistanceM);
 PC->ResetPrototypeCamera();const auto C=Camera->GetNavigationSnapshot();
 TestTrue(TEXT("Home restores full pose"),C.YawDeg==FMath::RadiansToDegrees(double(Before.HeadingRad)) && C.PitchDeg==Config->Navigation->Parameters.CameraInitialPitchDeg && C.DistanceM==Config->Navigation->Parameters.CameraDistanceM);
 TestEqual(TEXT("Look and reset leave heading unchanged"),S->GetHUDSnapshot().Boat.HeadingRad,Before.HeadingRad);
 bool TransitionObserved=false;
 const auto Handle=S->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& Command,ETRCommandResult Result)
 {
  if(Command.Type!=ETRFishingCommandType::StartFishingMode || Result!=ETRCommandResult::Accepted){return;}
  TransitionObserved=true;const auto B=S->GetHUDSnapshot().Boat;
  // HUD remains the last published boat until the Publish phase; exact physics boundary is tested separately.
  TestTrue(TEXT("HUD retains last published boat until Publish"),B.PositionM==Before.PositionM && B.VelocityMps==Before.VelocityMps && B.HeadingRad==Before.HeadingRad);
 });
 Inject(Config->Input->NavigationContext,EKeys::Enter,FInputActionValue(true));
 TestTrue(TEXT("Enter only queues mode change"),S->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Navigation);
 F.Step();S->OnCommandProcessed.Remove(Handle);
 TestTrue(TEXT("Published Fishing boat has no navigation speed"),S->GetHUDSnapshot().Boat.SpeedMps<.1);
 TestTrue(TEXT("Navigation Enter enters Fishing without Deploy"),TransitionObserved && S->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Fishing && !S->GetHUDSnapshot().bEgiValid);
 TestTrue(TEXT("Engine and steering stopped"),!S->GetNavigationSnapshot().bEngineActive && S->GetNavigationSnapshot().Steering==0);
 for(int32 I=0;I<3;++I){Inject(Config->Input->NavigationContext,EKeys::Enter,FInputActionValue(true));F.Step();}
 TestFalse(TEXT("Holding start does not Deploy"),S->GetHUDSnapshot().bEgiValid);
 Inject(Config->Input->NavigationContext,EKeys::Enter,FInputActionValue(false));
 TestTrue(TEXT("Fishing guide replaces Navigation"),HasVisibleText(TEXT("Enter：投入")) && !HasVisibleText(TEXT("W/S")) && !HasVisibleText(TEXT("Enter：釣り開始")));
 for(auto Key:{EKeys::W,EKeys::S,EKeys::A,EKeys::D}){Inject(Config->Input->NavigationContext,Key,FInputActionValue(1.f));F.Step();}
 TestFalse(TEXT("Fishing W/S/A/D cannot restart engine"),S->GetNavigationSnapshot().bEngineActive);
 TestEqual(TEXT("Fishing cannot steer"),S->GetHUDSnapshot().Boat.HeadingRad,Before.HeadingRad);
 const auto Position=S->GetHUDSnapshot().Boat.PositionM;F.Step(120);
 TestTrue(TEXT("Natural motion continues"),S->GetHUDSnapshot().Boat.PositionM!=Position && S->GetHUDSnapshot().Boat.SpeedMps>0);
 Inject(Config->Input->FishingContext,EKeys::Enter,FInputActionValue(true));F.Step();
 TestTrue(TEXT("New Fishing Enter deploys"),S->GetHUDSnapshot().bEgiValid && S->GetHUDSnapshot().Egi.FishingState==ETRFishingState::FreeFall);
 PC->UnbindSession();Mode->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNavigationTuningTest,"TipRun.M105R2.PrototypeResponseComparison",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNavigationTuningTest::RunTest(const FString&)
{
 double OldSpeed=0,OldYaw=0,OldReverse=0;
 for(bool Revised:{false,true})
 {
  FNavigationTest R;
  if(!Revised){auto& P=R.Tuning->Parameters;P.EngineForceN=8;P.ReverseScale=.5;P.EngineResponsePerS=2;P.SteeringRateRadPerS=.45;P.SteeringResponsePerS=3;P.MaxSpeedMps=8;}
  if(!R.Start(*this)){return false;}
  R.Send(FVector2D(1,0));R.F.Step(300);double Speed=R.F.Boat->GetBoatSnapshot().SpeedMps;
  R.Send(FVector2D(0,1));R.F.Step(60);double Yaw=R.F.Boat->GetBoatSnapshot().HeadingRad;
  FNavigationTest Reverse;Reverse.Tuning->Parameters=R.Tuning->Parameters;if(!Reverse.Start(*this)){return false;}
  Reverse.Send(FVector2D(-1,0));Reverse.F.Step(300);double Back=Reverse.F.Boat->GetBoatSnapshot().SpeedMps;
  if(!Revised){OldSpeed=Speed;OldYaw=Yaw;OldReverse=Back;}
  else{TestTrue(TEXT("DataAsset tuning clearly increases forward/reverse/turn response"),Speed>OldSpeed*2 && Back>OldReverse*2 && Yaw>OldYaw*2);}
  TestTrue(TEXT("Below configured navigation safety speed"),Speed<R.Tuning->Parameters.MaxSpeedMps && Back<R.Tuning->Parameters.MaxSpeedMps);
  AddInfo(FString::Printf(TEXT("%s: forward at 5s %.6f m/s, reverse %.6f m/s, heading after 1s steering %.6f rad"),Revised?TEXT("Revised"):TEXT("Original"),Speed,Back,Yaw));
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNavigationAssistTest,"TipRun.M105R2.SteeringAssistIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNavigationAssistTest::RunTest(const FString&)
{
 double UnassistedError=0;
 for(bool Assisted:{false,true})
 {
  FNavigationTest R(true);R.Tuning->Parameters.NavigationLateralResponsePerS=Assisted?1.5:0;
  if(!R.Start(*this)){return false;}R.Send(FVector2D(1,0));R.F.Step(300);
  R.Send(FVector2D(1,1));const auto Before=R.F.Boat->GetBoatSnapshot();R.F.Step();const auto First=R.F.Boat->GetBoatSnapshot();
  TestTrue(TEXT("Assist changes velocity continuously, not snap"),(First.VelocityMps-Before.VelocityMps).Size()<.25 && !First.VelocityMps.GetSafeNormal().Equals(First.ForwardVector,1.e-6));
  R.F.Step(119);const auto B=R.F.Boat->GetBoatSnapshot();
  const double Angle=FMath::Acos(FMath::Clamp(FVector::DotProduct(B.VelocityMps.GetSafeNormal(),B.ForwardVector),-1.,1.));
  if(!Assisted){UnassistedError=Angle;}else{TestTrue(TEXT("Assist reduces turn slip"),Angle<UnassistedError*.7);}
  TestTrue(TEXT("Wind and surface current still exert forces"),B.WindDeltaVelocityMps.Size()>0 && B.CurrentDeltaVelocityMps.Size()>0);
  R.Send(FVector2D(0,0));R.F.Step();TestEqual(TEXT("Throttle release removes assist immediately"),R.F.Session->GetNavigationSnapshot().LateralResponsePerS,0.);
  R.F.Session->SubmitModeChange(ETRPlayerMode::Fishing,R.F.Session->GetPlayerModeSnapshot().ModeEpoch,R.F.Session->GetRegistrationId());R.F.Step();
  TestEqual(TEXT("Fishing never enables assist"),R.F.Session->GetNavigationSnapshot().LateralResponsePerS,0.);
  AddInfo(FString::Printf(TEXT("%s turn slip %.3f degrees"),Assisted?TEXT("Assisted"):TEXT("Unassisted"),FMath::RadiansToDegrees(Angle)));
 }
 // Identical initial/environment state: optional Navigation coefficient must have
 // absolutely no effect in Fishing, including after many integration steps.
 FNavigationTest A(true),B(true);A.Tuning->Parameters.NavigationLateralResponsePerS=0;B.Tuning->Parameters.NavigationLateralResponsePerS=1.5;
 if(!A.Start(*this)||!B.Start(*this)){return false;}
 for(auto* R:{&A,&B}){R->F.Session->SubmitModeChange(ETRPlayerMode::Fishing,R->F.Session->GetPlayerModeSnapshot().ModeEpoch,R->F.Session->GetRegistrationId());R->F.Step(600);}
 TestTrue(TEXT("Fishing result bitwise independent of assist tuning"),A.F.Boat->GetBoatSnapshot().PositionM==B.F.Boat->GetBoatSnapshot().PositionM && A.F.Boat->GetBoatSnapshot().VelocityMps==B.F.Boat->GetBoatSnapshot().VelocityMps);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNavigationReturnTest,"TipRun.M105R2.ReturnNavigationInput",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNavigationReturnTest::RunTest(const FString&)
{
 FNavigationTest R;R.F.Tuning->Parameters.QuickRetrieveDurationS=1.5;if(!R.Start(*this)){return false;}
 auto* PC=R.F.World->SpawnActor<ATRPlayerController>();PC->InputConfig=UTRInputConfigDataAsset::CreateMouseRetrievePrototype(PC);PC->InputConfig->CreateNavigationPrototype();
 auto* Input=NewObject<UEnhancedInputComponent>(PC);PC->InputComponent=Input;auto* PlayerInput=NewObject<UEnhancedPlayerInput>(PC);PC->PlayerInput=PlayerInput;
 PC->InstallInputBindings(Input);PC->BindSession(R.F.Session.Get());
 auto Return=[&]()
 {
  int32 Count=0;for(const auto& M:PC->InputConfig->FishingContext->GetMappings()){if(M.Key==EKeys::E){++Count;PlayerInput->InjectInputForAction(M.Action,FInputActionValue(true));}}
  TestEqual(TEXT("E maps once"),Count,1);PlayerInput->ProcessInputStack({Input},1.f/60,false);R.F.Step();PlayerInput->ProcessInputStack({Input},1.f/60,false);
 };
 PC->RequestPlayerMode(ETRPlayerMode::Fishing);R.F.Step();Return();
 TestTrue(TEXT("Ready E returns to Navigation"),R.F.Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Navigation);
 TestTrue(TEXT("Navigation input policy restored"),PC->SubmitNavigationInput(FVector2D(1,1)));R.F.Step();
 PC->RequestPlayerMode(ETRPlayerMode::Fishing);R.F.Step();R.F.Deploy();
 auto Reject=[&](){Return();TestTrue(TEXT("In-water E rejected by fixed simulation"),R.F.Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Fishing && R.F.Session->GetPlayerModeSnapshot().LastRejectedReason==ETRModeChangeRejection::ActiveCast);};
 Reject();R.F.Step(120);R.F.Session->SubmitCommand(ETRFishingCommandType::TensionFall,R.F.Session->GetCastId());R.F.Step(120);
 TestTrue(TEXT("Stay setup"),R.F.Session->Fishing->GetState()==ETRFishingState::Stay);Reject();
 R.F.Session->SubmitCommand(ETRFishingCommandType::RetrieveStarted,R.F.Session->GetCastId());R.F.Step();
 TestTrue(TEXT("Retrieve setup"),R.F.Session->Fishing->GetState()==ETRFishingState::Retrieving);Reject();
 R.F.Session->SubmitCommand(ETRFishingCommandType::QuickRetrieve,R.F.Session->GetCastId());R.F.Step();
 TestTrue(TEXT("Quick setup"),R.F.Session->Fishing->GetState()==ETRFishingState::QuickRetrieving);Reject();R.F.Step(120);
 Return();TestTrue(TEXT("Quick Ready E returns without N"),R.F.Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Navigation);
 PC->UnbindSession();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNavigationDebugKeyTest,"TipRun.M105R2.DetailsKeyIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNavigationDebugKeyTest::RunTest(const FString&)
{
 FNavigationTest R;if(!R.Start(*this)){return false;}
 auto* PC=R.F.World->SpawnActor<ATRPlayerController>();PC->InputConfig=UTRInputConfigDataAsset::CreateMouseRetrievePrototype(PC);PC->InputConfig->CreateNavigationPrototype();
 PC->InitInputSystem();PC->BindSession(R.F.Session.Get());PC->EnablePrototypeUI(true);
 auto* PlayerInput=PC->PlayerInput.Get();
 for(const auto& B:PlayerInput->DebugExecBindings){TestTrue(TEXT("Neither Insert nor removed F1 has a debug exec"),B.Key!=EKeys::Insert && B.Key!=EKeys::F1);}
 for(const auto& B:PC->InputComponent->KeyBindings){TestTrue(TEXT("No Prototype F1 binding"),B.Chord.Key!=EKeys::F1);}
 auto Key=[&](FKey K,EInputEvent E){PlayerInput->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,0));PlayerInput->ProcessInputStack({PC->InputComponent},1.f/60,false);};
 TStrongObjectPtr<UGameViewportClient> Viewport(NewObject<UGameViewportClient>(GEngine));
 auto& Context=GEngine->GetWorldContextFromWorldChecked(R.F.World);auto* Old=Context.GameViewport.Get();Context.GameViewport=Viewport.Get();
 const auto Flags=Viewport->EngineShowFlags.ToString();const auto ViewMode=Viewport->ViewModeIndex;auto* ViewTarget=PC->GetViewTarget();
 for(int I=0;I<20;++I)
 {
  Key(EKeys::Insert,IE_Pressed);TestEqual(TEXT("Insert toggles once"),PC->IsPrototypeDetailsOpen(),I%2==0);
  Key(EKeys::Insert,IE_Repeat);TestEqual(TEXT("Repeat does not toggle"),PC->IsPrototypeDetailsOpen(),I%2==0);Key(EKeys::Insert,IE_Released);
 }
 TestTrue(TEXT("No rendering or camera side effect"),Viewport->EngineShowFlags.ToString()==Flags && Viewport->ViewModeIndex==ViewMode && PC->GetViewTarget()==ViewTarget);
 Key(EKeys::F1,IE_Pressed);Key(EKeys::F1,IE_Released);TestFalse(TEXT("F1 no longer opens HUD"),PC->IsPrototypeDetailsOpen());
 Context.GameViewport=Old;PC->UnbindSession();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNavigationPresentationTest,"TipRun.M105R2.ModePresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNavigationPresentationTest::RunTest(const FString&)
{
 FNavigationTest R;if(!R.Start(*this)){return false;}
 auto* View=R.F.World->SpawnActor<ATRPrototypeViewActor>();
 const int32 Count=View->GetComponents().Num();const auto* Rod=View->RodVisual.Get();
 for(int I=0;I<10;++I)
 {
  R.F.Session->SubmitModeChange(ETRPlayerMode::Fishing,R.F.Session->GetPlayerModeSnapshot().ModeEpoch,R.F.Session->GetRegistrationId());R.F.Step();
  auto S=R.F.Session->GetHUDSnapshot();View->ApplyObservation(S,FTRRodParameters{},0,30);
  TestTrue(TEXT("Fishing restores rod and tip"),View->RodVisual->IsVisible() && View->TipVisual->IsVisible());
  // Even an out-of-date fishing render snapshot must not leak through a newer mode.
  S.bEgiValid=true;S.Egi.bWorldPositionValid=true;S.Egi.WorldPositionM=FVector(1,1,-5);
  View->ApplyObservation(S,FTRRodParameters{},0,30);TestTrue(TEXT("Fishing renders line and egi"),View->LineVisual->IsVisible() && View->EgiVisual->IsVisible());
  R.F.Session->SubmitModeChange(ETRPlayerMode::Navigation,R.F.Session->GetPlayerModeSnapshot().ModeEpoch,R.F.Session->GetRegistrationId());R.F.Step();
  S.PlayerMode=R.F.Session->GetPlayerModeSnapshot();View->ApplyObservation(S,FTRRodParameters{},0,30);
  TestTrue(TEXT("Navigation hides all fishing visuals"),!View->RodVisual->IsVisible() && !View->TipVisual->IsVisible() && !View->LineVisual->IsVisible() && !View->EgiVisual->IsVisible());
  TestTrue(TEXT("Boat retained, components reused"),View->BoatVisual->IsVisible() && View->RodVisual.Get()==Rod && View->GetComponents().Num()==Count);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNavigationBoostTest,"TipRun.M105R2.BoostResponseAndModes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNavigationBoostTest::RunTest(const FString&)
{
 FNavigationTest Normal,Boost;if(!Normal.Start(*this)||!Boost.Start(*this)){return false;}
 auto* PC=Boost.F.World->SpawnActor<ATRPlayerController>();PC->InputConfig=UTRInputConfigDataAsset::CreateMouseRetrievePrototype(PC);PC->InputConfig->CreateNavigationPrototype();PC->BindSession(Boost.F.Session.Get());
 auto* Input=NewObject<UEnhancedInputComponent>(PC);PC->InputComponent=Input;
 auto* PlayerInput=NewObject<UEnhancedPlayerInput>(PC);PC->PlayerInput=PlayerInput;TestTrue(TEXT("Boost binding installs"),PC->InstallInputBindings(Input));
 auto Inject=[&](bool Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::LeftShift,Down?IE_Pressed:IE_Released,0));PlayerInput->InjectInputForAction(PC->InputConfig->NavigationBoost,FInputActionValue(Down));PlayerInput->ProcessInputStack({Input},1.f/60,false);};
 Inject(true);Boost.F.Step(60);TestTrue(TEXT("Shift alone does not move"),Boost.F.Boat->GetBoatSnapshot().SpeedMps==0 && Boost.F.Session->GetNavigationSnapshot().EngineForceN==0);
 Normal.Send(FVector2D(1,0));Boost.Send(FVector2D(1,0));Boost.F.Step();
 TestTrue(TEXT("Boost force ramps"),Boost.F.Session->GetNavigationSnapshot().EngineForceN<Boost.Tuning->Parameters.EngineForceN);
 Normal.F.Step(900);Boost.F.Step(899);
 TestTrue(TEXT("Boost materially faster"),Boost.F.Boat->GetBoatSnapshot().SpeedMps>Normal.F.Boat->GetBoatSnapshot().SpeedMps*1.5);
 const double Force=Boost.F.Session->GetNavigationSnapshot().EngineForceN;Inject(false);Boost.F.Step();
 TestTrue(TEXT("Release smooth force decay"),Boost.F.Session->GetNavigationSnapshot().EngineForceN<Force && Boost.F.Session->GetNavigationSnapshot().EngineForceN>Boost.Tuning->Parameters.EngineForceN);
 Boost.F.Step(600);TestTrue(TEXT("Normal force restored"),FMath::IsNearlyEqual(Boost.F.Session->GetNavigationSnapshot().EngineForceN,Boost.Tuning->Parameters.EngineForceN,.001));
 Inject(true);Boost.Send(FVector2D(1,1));Boost.F.Step(60);TestTrue(TEXT("Boost steering"),Boost.F.Boat->GetBoatSnapshot().HeadingRad>.1);
 PC->RequestPlayerMode(ETRPlayerMode::Fishing);Boost.F.Step();
 TestTrue(TEXT("Fishing clears boost and force"),!Boost.F.Session->GetNavigationSnapshot().bBoostRequested && Boost.F.Session->GetNavigationSnapshot().BoostBlend==0 && !Boost.F.Session->GetNavigationSnapshot().bEngineActive);
 Boost.F.Session->SubmitCommand(ETRFishingCommandType::NavigationBoostStarted,Boost.F.Session->GetCastId());Boost.F.Step();
 TestFalse(TEXT("Simulation rejects fishing boost"),Boost.F.Session->GetNavigationSnapshot().bBoostRequested);
 TestFalse(TEXT("Fishing guide excludes boost"),UTRFishingHUDWidget::BuildCompactGuide(PC->GetDebugSnapshot()).ToString().Contains(TEXT("高速航行")));
 PC->RequestPlayerMode(ETRPlayerMode::Navigation);Boost.F.Step();
 TestFalse(TEXT("Held boost blocked after return"),PC->ActionStarted(ETRPlayerAction::NavigationBoost));
 Inject(false);Inject(true);Boost.F.Step();TestTrue(TEXT("Repress enables boost"),Boost.F.Session->GetNavigationSnapshot().bBoostRequested);
 TestTrue(TEXT("Navigation guide includes boost"),UTRFishingHUDWidget::BuildCompactGuide(PC->GetDebugSnapshot()).ToString().Contains(TEXT("Shift：高速航行")));
 PC->SetInputFocus(false);Boost.F.Step();TestFalse(TEXT("Focus clears boost"),Boost.F.Session->GetNavigationSnapshot().bBoostRequested);
 PC->SetInputFocus(true);TestFalse(TEXT("Focus return does not replay boost"),PC->ActionStarted(ETRPlayerAction::NavigationBoost));
 PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::LeftShift,IE_Released,0));PC->ActionReleased(ETRPlayerAction::NavigationBoost);PC->ActionStarted(ETRPlayerAction::NavigationBoost);Boost.F.Step();PC->SetPauseRequested(true);Boost.F.Step(60);PC->SetPauseRequested(false);Boost.F.Step();
 TestFalse(TEXT("Pause release cannot restart boost"),Boost.F.Session->GetNavigationSnapshot().bBoostRequested);
 PC->UnbindSession();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRNavigationBoostDeterminismTest,"TipRun.M105R2.BoostDeterminism",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRNavigationBoostDeterminismTest::RunTest(const FString&)
{
 FTRBoatSnapshot Reference;
 for(int FPS:{30,60,120})
 {
  FNavigationTest R(true);if(!R.Start(*this)){return false;}
  R.Send(FVector2D(1,0),0);R.Send(FVector2D(1,.4),120);R.Send(FVector2D(-1,0),240);
  R.F.Session->SubmitCommand(ETRFishingCommandType::NavigationBoostStarted,R.F.Session->GetCastId(),30);
  R.F.Session->SubmitCommand(ETRFishingCommandType::NavigationBoostStopped,R.F.Session->GetCastId(),300);
  R.F.Session->SubmitModeChange(ETRPlayerMode::Fishing,R.F.Session->GetPlayerModeSnapshot().ModeEpoch,R.F.Session->GetRegistrationId(),360);
  for(int I=0;I<FPS*8;++I){R.F.Sim()->AdvanceFrame(1.0/FPS);}
  auto B=R.F.Boat->GetBoatSnapshot();if(FPS==30){Reference=B;}else{TestTrue(TEXT("Boost and Fishing entry exact FPS invariance"),B.PositionM==Reference.PositionM && B.VelocityMps==Reference.VelocityMps && B.HeadingRad==Reference.HeadingRad);}
  TestTrue(TEXT("Finite boost result"),!B.PositionM.ContainsNaN() && !B.VelocityMps.ContainsNaN());
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRFishingEntryDriftTest,"TipRun.M105R2.FishingEntryEnvironmentOnly",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRFishingEntryDriftTest::RunTest(const FString&)
{
 for(bool Env:{false,true}) for(bool Boost:{false,true})
 {
  FNavigationTest R(Env);if(!R.Start(*this)){return false;}
  R.Send(FVector2D(1,0));if(Boost){R.F.Session->SubmitCommand(ETRFishingCommandType::NavigationBoostStarted,R.F.Session->GetCastId());}
  R.F.Step(900);const auto Before=R.F.Boat->GetBoatSnapshot();TestTrue(TEXT("Starts at substantial navigation speed"),Before.SpeedMps>5);
  auto Time=R.F.Sim()->GetSimulationTime();auto Ocean=R.F.Session->GetHUDSnapshot().Ocean;Ocean.SampleTick=Time.TickIndex;
  TStrongObjectPtr<UTRBoatDriftComponent> Reference(NewObject<UTRBoatDriftComponent>());TArray<FText> Errors;
  TestTrue(TEXT("Environment-only independent boat from rest"),Reference->InitializeMotion(R.F.BoatTuning->Parameters,FVector2D(Before.PositionM),Before.HeadingRad,Ocean,Errors));
  bool Observed=false;
  const auto Handle=R.F.Session->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result)
  {
   if(C.Type!=ETRFishingCommandType::StartFishingMode || Result!=ETRCommandResult::Accepted){return;}
   Observed=true;const auto B=R.F.Boat->GetBoatSnapshot();const auto N=R.F.Session->GetNavigationSnapshot();
   TestTrue(TEXT("Exact boundary preserves position/heading, clears dynamic velocity"),B.PositionM==Before.PositionM && B.HeadingRad==Before.HeadingRad && B.VelocityMps==FVector::ZeroVector && B.SpeedMps==0);
   TestTrue(TEXT("All propulsion response cleared"),N.EngineForceN==0 && N.Throttle==0 && N.Steering==0 && N.YawRateRadPerS==0 && N.LateralResponsePerS==0 && !N.bBoostRequested && N.BoostBlend==0);
  });
  R.F.Session->SubmitModeChange(ETRPlayerMode::Fishing,R.F.Session->GetPlayerModeSnapshot().ModeEpoch,R.F.Session->GetRegistrationId());
  for(int I=0;I<600;++I)
  {
   Time=R.F.Sim()->GetSimulationTime();Ocean.SampleTick=Time.TickIndex;
   TestTrue(TEXT("Reference drift valid"),Reference->StepDrift(Time,Ocean,[](const FVector2D&){return true;}));R.F.Step();
   const auto B=R.F.Boat->GetBoatSnapshot();const auto Expected=Reference->BuildSnapshot();
   TestTrue(TEXT("Only environment motion remains at every step"),B.VelocityMps==Expected.VelocityMps && B.PositionM==Expected.PositionM && !B.VelocityMps.ContainsNaN());
  }
  TestTrue(TEXT("Mode accepted"),Observed);R.F.Session->OnCommandProcessed.Remove(Handle);
  TestTrue(TEXT("Calm stays stopped; wind/current rebuild drift"),Env?R.F.Boat->GetBoatSnapshot().SpeedMps>.01:R.F.Boat->GetBoatSnapshot().SpeedMps==0);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRBoostPhysicalRearmTest,"TipRun.M105R2.PhysicalShiftRearmAndHUD",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRBoostPhysicalRearmTest::RunTest(const FString&)
{
 FNavigationTest R;if(!R.Start(*this)){return false;}
 auto* PC=R.F.World->SpawnActor<ATRPlayerController>();PC->InputConfig=UTRInputConfigDataAsset::CreateMouseRetrievePrototype(PC);PC->InputConfig->CreateNavigationPrototype();PC->InitInputSystem();PC->BindSession(R.F.Session.Get());
 auto* PlayerInput=CastChecked<UEnhancedPlayerInput>(PC->PlayerInput);
 auto Raw=[&](FKey Key,EInputEvent Event){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,Event,0));};
 auto Enhanced=[&](bool Down){PlayerInput->InjectInputForAction(PC->InputConfig->NavigationBoost,FInputActionValue(Down));PlayerInput->ProcessInputStack({PC->InputComponent},1.f/60,false);R.F.Step();};
 TStrongObjectPtr<UTRFishingHUDWidget> UI(CreateWidget<UTRFishingHUDWidget>(R.F.World,UTRFishingHUDWidget::StaticClass()));
 UI->TakeWidget();UI->BindController(PC);
 auto HUD=[&]()
 {
  UI->RefreshFromController();TArray<UWidget*> Widgets;UI->WidgetTree->GetAllWidgets(Widgets);FString Text;
  for(auto* W:Widgets){if(auto* T=Cast<UTextBlock>(W)){if(T->GetVisibility()!=ESlateVisibility::Collapsed){Text+=T->GetText().ToString();}}}
  return Text;
 };
 TestTrue(TEXT("OFF HUD"),HUD().Contains(TEXT("高速航行: OFF")));
 for(int Boundary=0;Boundary<3;++Boundary)
 {
  R.Send(FVector2D(1,0));Raw(EKeys::LeftShift,IE_Pressed);Enhanced(true);
  TestTrue(TEXT("Physical down then enhanced starts boost"),R.F.Session->GetNavigationSnapshot().bBoostRequested && HUD().Contains(TEXT("高速航行: ON")));
  if(Boundary==0)
  {
   PC->RequestPlayerMode(ETRPlayerMode::Fishing);R.F.Step();
   TestFalse(TEXT("Fishing has no boost status"),HUD().Contains(TEXT("高速航行:")));
   Enhanced(false); // Context removal Canceled/Completed is NOT a physical release.
   PC->RequestPlayerMode(ETRPlayerMode::Navigation);R.F.Step();
  }
  else if(Boundary==1){PC->SetInputFocus(false);PC->FlushPressedKeys();Enhanced(false);PC->SetInputFocus(true);}
  else{PC->SetPauseRequested(true);PC->FlushPressedKeys();Enhanced(false);PC->SetPauseRequested(false);}
  R.Send(FVector2D(1,0));Enhanced(true);Raw(EKeys::LeftShift,IE_Repeat);R.F.Step();
  TestTrue(TEXT("Held/synthetic completion cannot rearm"),!R.F.Session->GetNavigationSnapshot().bBoostRequested && HUD().Contains(TEXT("高速航行: 再入力待ち")));
  Raw(EKeys::RightShift,IE_Pressed);Raw(EKeys::LeftShift,IE_Released);Enhanced(false);Enhanced(true);
  TestFalse(TEXT("Other Shift still held prevents rearm"),R.F.Session->GetNavigationSnapshot().bBoostRequested);
  Raw(EKeys::RightShift,IE_Released);Enhanced(false);
  TestTrue(TEXT("Real release rearms but does not start"),!PC->GetDebugSnapshot().bBoostRearmRequired && HUD().Contains(TEXT("高速航行: OFF")));
  Raw(EKeys::LeftShift,IE_Pressed);Enhanced(true);TestTrue(TEXT("New physical down restarts"),R.F.Session->GetNavigationSnapshot().bBoostRequested);
  Raw(EKeys::LeftShift,IE_Released);Enhanced(false);
 }
 PC->UnbindSession();return true;
}
#endif
