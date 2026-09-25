#include "TRSessionTestFixture.h"
#include "Data/TRFishingStationDataAsset.h"
#include "Data/TRRodTuningDataAsset.h"
#include "Game/TRPlayerController.h"
#include "Game/TRPlayerCameraManager.h"
#include "Game/TRPrototypeViewActor.h"
#include "Fishing/TRRodControlComponent.h"
#include "UI/TRFishingHUDWidget.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
 struct FStationTest
 {
  FTRTestSession F;
  TStrongObjectPtr<UTRFishingStationDataAsset> Stations{NewObject<UTRFishingStationDataAsset>()};
  ATRPlayerController* PC=nullptr;
  ATRPlayerCameraManager* Camera=nullptr;
  bool Start(FAutomationTestBase& Test,double Heading=0)
  {
   auto* Config=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));
   if(!Config){return false;}
   F.Tuning->Parameters=Config->Fishing->Parameters;F.BoatTuning->Parameters=Config->Boat->Parameters;
   F.Area->Settings=Config->Ocean->Settings;
   F.Session->RodTuning=Config->Rod;F.Session->NavigationTuning=Config->Navigation;
   Stations->Parameters=FTRFishingStationParameters::Prototype();F.Session->FishingStations=Stations.Get();
   if(!F.Initialize(Test)){return false;}
   // Test headings use the component's initialization contract, before starting the Session.
   auto Ocean=F.Session->GetHUDSnapshot().Ocean;
   FTROceanQuery Q;Q.SimTick=F.Sim()->GetSimulationTime().TickIndex;Q.PositionXYM=FVector2D(F.Boat->GetBoatSnapshot().PositionM);
   Ocean=F.World->GetSubsystem<UTROceanWorldSubsystem>()->SampleOcean(Q);TArray<FText> Errors;
   if(!F.Boat->InitializeBoat(Q.PositionXYM,float(Heading),Ocean,Errors)){return false;}
   Test.TestTrue(TEXT("Start session"),F.Session->StartFishing(Errors)==ETRCommandResult::Accepted);
   PC=F.World->SpawnActor<ATRPlayerController>();PC->InputConfig=Config->Input;PC->InitInputSystem();
   Camera=Cast<ATRPlayerCameraManager>(PC->PlayerCameraManager);
   if(!Camera){Camera=F.World->SpawnActor<ATRPlayerCameraManager>();PC->PlayerCameraManager=Camera;Camera->InitializeFor(PC);}
   return PC->BindSession(F.Session.Get());
  }
  void Action(ETRPlayerAction A){PC->ActionStarted(A);F.Step();PC->ActionReleased(A);}
  void Key(FKey K){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,IE_Pressed,0));F.Step();PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,IE_Released,0));}
  void EnterSide(bool Port){Action(ETRPlayerAction::FishingStart);Key(Port?EKeys::A:EKeys::D);Action(ETRPlayerAction::FishingStart);}
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRStationFlow,"TipRun.M105R3.SelectionInputAndLifetime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRStationFlow::RunTest(const FString&)
{
 FStationTest R;if(!R.Start(*this)){return false;}
 R.Action(ETRPlayerAction::FishingStart);
 TestTrue(TEXT("Start opens selection without changing mode"),R.F.Session->GetPlayerModeSnapshot().bSideSelectionActive && R.F.Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Navigation);
 TestFalse(TEXT("Selection cannot deploy"),R.PC->ActionStarted(ETRPlayerAction::Deploy));
 R.Action(ETRPlayerAction::FishingStart);TestTrue(TEXT("Unselected confirm stays in selection"),R.F.Session->GetPlayerModeSnapshot().bSideSelectionActive);
 R.Key(EKeys::A);TestTrue(TEXT("A selects port"),R.F.Session->GetPlayerModeSnapshot().PendingSide==ETRFishingSide::Port);
 TestFalse(TEXT("Steering rejected in selection"),R.PC->SubmitNavigationInput(FVector2D(1,1)));
 R.Key(EKeys::D);TestTrue(TEXT("D selects starboard without steering"),R.F.Session->GetPlayerModeSnapshot().PendingSide==ETRFishingSide::Starboard && R.F.Session->GetNavigationSnapshot().Steering==0);
 TestTrue(TEXT("Japanese selection guide"),UTRFishingHUDWidget::BuildCompactGuide(R.PC->GetDebugSnapshot()).ToString().Contains(TEXT("釣り座を選択")));
 R.Key(EKeys::BackSpace);TestTrue(TEXT("Cancel stays Navigation"),!R.F.Session->GetPlayerModeSnapshot().bSideSelectionActive && R.F.Session->IsInputModeAllowed(ETRPlayerMode::Navigation));
 auto* View=R.F.World->SpawnActor<ATRPrototypeViewActor>();const int32 Count=View->GetComponents().Num();
 for(int I=0;I<6;++I)
 {
  R.EnterSide(I%2==0);const auto Snapshot=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("Confirmed Fishing camera"),Snapshot.Station.bValid && Snapshot.FishingCamera.bActive && Snapshot.PlayerMode.Mode==ETRPlayerMode::Fishing);
  View->ApplyObservation(Snapshot,R.F.Session->RodTuning->Parameters,0,30);TestTrue(TEXT("Rod shown"),View->RodVisual->IsVisible());
  R.Action(ETRPlayerAction::ReturnNavigation);View->ApplyObservation(R.PC->GetDebugSnapshot(),R.F.Session->RodTuning->Parameters,0,30);
  TestTrue(TEXT("Return ends FPS and fishing visuals"),!R.PC->GetDebugSnapshot().FishingCamera.bActive && !View->RodVisual->IsVisible() && View->GetComponents().Num()==Count);
  R.Action(ETRPlayerAction::FishingStart);TestTrue(TEXT("Every entry requires selection"),R.F.Session->GetPlayerModeSnapshot().bSideSelectionActive && R.F.Session->GetPlayerModeSnapshot().PendingSide==ETRFishingSide::Unselected);R.Key(EKeys::BackSpace);
 }
 R.F.Session->Destroy();TestFalse(TEXT("Destroyed session clears station snapshot"),R.PC->GetDebugSnapshot().Station.bValid);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRStationTransforms,"TipRun.M105R3.AnchorsCameraRodAndVisuals",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRStationTransforms::RunTest(const FString&)
{
 for(double Heading:{0.,UE_DOUBLE_PI/2,UE_DOUBLE_PI}) for(bool Port:{true,false})
 {
  FStationTest R;if(!R.Start(*this,Heading)){return false;}R.EnterSide(Port);auto S=R.PC->GetDebugSnapshot();
  const auto* Row=R.Stations->Parameters.Find(Port?ETRFishingSide::Port:ETRFishingSide::Starboard);const FQuat H(FVector::UpVector,S.Boat.HeadingRad);
  TestTrue(TEXT("Camera and mount follow correct side and heading"),S.Station.CameraWorldM.Equals(S.Boat.PositionM+H.RotateVector(Row->CameraM),1.e-8) && S.Rod.RootWorldPositionM.Equals(S.Boat.PositionM+H.RotateVector(Row->RodMountM),1.e-8));
  FMinimalViewInfo POV;TestTrue(TEXT("FPS builds"),R.Camera->BuildFishingView(S.Station,POV));
  TestTrue(TEXT("Camera at anchor, faces out"),POV.Location.Equals(S.Station.CameraWorldM*100,1.e-8) && FVector::DotProduct(POV.Rotation.Vector(),H.RotateVector(FVector(0,Port?-1:1,0)))>.8);
  auto* View=R.F.World->SpawnActor<ATRPrototypeViewActor>();View->ApplyObservation(S,R.F.Session->RodTuning->Parameters,0,30);
  TArray<UStaticMeshComponent*> Meshes;View->GetComponents(Meshes);bool Reel=false,Rail=false;
  for(auto* Mesh:Meshes)
  {
   if(Mesh->GetName()==TEXT("ReelObservation") || Mesh->GetName()==(Port?TEXT("PortGunwale"):TEXT("StarboardGunwale")))
   {
    const FVector Local=POV.Rotation.UnrotateVector(Mesh->GetComponentLocation()-POV.Location);
    const double HAngle=FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Local.Y),Local.X));
    const double VAngle=FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Local.Z),Local.X));
    // Horizontal 80 degrees at 16:9 gives about 50.5 degrees vertical.
    TestTrue(TEXT("Reel/gunwale visible in initial 16:9 frustum"),Mesh->IsVisible() && Local.X>0 && HAngle<40 && VAngle<26);
    Reel|=Mesh->GetName()==TEXT("ReelObservation");Rail|=Mesh->GetName()!=(TEXT("ReelObservation"));
   }
  }
  TestTrue(TEXT("Both reel and selected rail exist"),Reel && Rail);
  const auto Rod=S.Rod;const auto HeadingBefore=S.Boat.HeadingRad;
  for(int I=0;I<100;++I){R.PC->RoutePrototypeMouse(FVector2D(50,-50),false);}R.F.Step(60);S=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("FPS limits and heading isolation"),S.FishingCamera.YawDeg==R.Stations->Parameters.MaxYawDeg && S.FishingCamera.PitchDeg==R.Stations->Parameters.MinPitchDeg && S.Boat.HeadingRad==HeadingBefore && S.Rod.BaseYawRad==Rod.BaseYawRad);
  TestTrue(TEXT("Drifting station follows boat"),S.Station.CameraWorldM.Equals(S.Boat.PositionM+H.RotateVector(Row->CameraM),1.e-8));
  R.Action(ETRPlayerAction::Deploy);R.F.Step(60);TestTrue(TEXT("Deploy uses side rod tip, finite egi"),R.PC->GetDebugSnapshot().bEgiValid && !R.PC->GetDebugSnapshot().Egi.WorldPositionM.ContainsNaN());
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRStationDeterminism,"TipRun.M105R3.FixedQueuePauseAndFPS",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRStationDeterminism::RunTest(const FString&)
{
 FTRHUDSnapshot Ref;
 for(int FPS:{30,60,120})
 {
  FStationTest R;if(!R.Start(*this)){return false;}
  R.F.Session->SubmitCommand(ETRFishingCommandType::StartFishingMode,{},0);
  for(int I=0;I<FPS;++I){R.F.Sim()->AdvanceFrame(1./FPS);}
  R.F.Session->SubmitCommand(ETRFishingCommandType::SelectPort,{},60);R.F.Session->SubmitCommand(ETRFishingCommandType::SelectStarboard,{},60);R.F.Session->SubmitCommand(ETRFishingCommandType::StartFishingMode,{},61);
  for(int I=0;I<FPS*2;++I){R.F.Sim()->AdvanceFrame(1./FPS);}
  const auto S=R.PC->GetDebugSnapshot();TestTrue(TEXT("Sequence picks last side"),S.Station.Side==ETRFishingSide::Starboard);
  if(FPS==30){Ref=S;}else{TestTrue(TEXT("FPS matches transforms"),S.Rod.TipWorldPositionM==Ref.Rod.TipWorldPositionM && S.Station.CameraWorldM==Ref.Station.CameraWorldM);}
  R.PC->SetPauseRequested(true);TestFalse(TEXT("Paused camera rejected"),R.PC->RoutePrototypeMouse(FVector2D(1,1),false));R.F.Step(60);TestEqual(TEXT("Pause freezes rod"),R.PC->GetDebugSnapshot().Rod.TipWorldPositionM,S.Rod.TipWorldPositionM);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRStationAssets,"TipRun.M105R3.SavedStations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRStationAssets::RunTest(const FString&)
{
 auto* Config=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));if(!Config){return false;}
 if(FParse::Param(FCommandLine::Get(),TEXT("TRMigrateM105R3")))
 {
  const FString Path=TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_R3FishingStations_Prototype");
  if(!Config->FishingStations){Config->FishingStations=NewObject<UTRFishingStationDataAsset>(CreatePackage(*Path),TEXT("DA_TR_R3FishingStations_Prototype"),RF_Public|RF_Standalone);Config->FishingStations->Parameters=FTRFishingStationParameters::Prototype();}
  for(UObject* A:{static_cast<UObject*>(Config),static_cast<UObject*>(Config->FishingStations.Get())})
  {FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;TestTrue(TEXT("Save explicit R3 asset"),UPackage::SavePackage(A->GetOutermost(),A,*FPackageName::LongPackageNameToFilename(A->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));}
 }
 TArray<FText> E;TestTrue(TEXT("Saved R3 references valid"),Config->FishingStations && Config->ValidateStartup(E));
 auto Bad=FTRFishingStationParameters::Prototype();Bad.Stations[0].Side=ETRFishingSide::Starboard;TestFalse(TEXT("Duplicate side rejected"),Bad.Validate());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRStationReselection,"TipRun.M105R3.FishingReadyReselection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRStationReselection::RunTest(const FString&)
{
 FStationTest R;if(!R.Start(*this)){return false;}
 R.PC->ActionStarted(ETRPlayerAction::FishingStart);R.F.Step();
 R.PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::Enter,IE_Released,0));
 R.Key(EKeys::BackSpace);
 TestTrue(TEXT("Physical Enter release rearms after selection"),R.PC->ActionStarted(ETRPlayerAction::FishingStart));R.F.Step();R.PC->ActionReleased(ETRPlayerAction::FishingStart);R.Key(EKeys::A);R.Key(EKeys::Enter);

 auto* View=R.F.World->SpawnActor<ATRPrototypeViewActor>();const int32 Count=View->GetComponents().Num();
 for(int I=0;I<6;++I)
 {
  const auto Old=R.PC->GetDebugSnapshot();R.Key(EKeys::C);
  TestTrue(TEXT("C opens within Fishing"),R.F.Session->GetPlayerModeSnapshot().bSideSelectionActive && R.F.Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Fishing);
  R.Key(I%2==0?EKeys::D:EKeys::A);
  // Cancel must not commit the pending side.
  R.Key(EKeys::BackSpace);TestTrue(TEXT("Cancel retains Fishing and side"),R.PC->GetDebugSnapshot().Station.Side==Old.Station.Side && R.PC->GetDebugSnapshot().PlayerMode.Mode==ETRPlayerMode::Fishing);
  R.Key(EKeys::C);R.Key(I%2==0?EKeys::D:EKeys::A);
  const auto Before=R.F.Boat->GetBoatSnapshot();
  R.Key(EKeys::Enter);const auto S=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("Confirmed side and mode"),S.Station.Side==(I%2==0?ETRFishingSide::Starboard:ETRFishingSide::Port) && S.PlayerMode.Mode==ETRPlayerMode::Fishing && !S.PlayerMode.bSideSelectionActive);
  TestTrue(TEXT("No boat reset; continuous natural drift"),S.Boat.HeadingRad==Before.HeadingRad && S.Boat.VelocityMps.Size()>Before.VelocityMps.Size()*.95 && (S.Boat.PositionM-Before.PositionM).Size()<.1);
  TestTrue(TEXT("Eye camera and rod switch sides"),S.Station.EyeWorldM!=Old.Station.EyeWorldM && S.FishingCamera.PositionM==S.Station.CameraWorldM && S.Rod.RootWorldPositionM.Equals(S.Station.RodRootWorldM,1.e-8));
  View->ApplyObservation(S,R.F.Session->RodTuning->Parameters,0,30);TestEqual(TEXT("Presentation reused"),View->GetComponents().Num(),Count);
  TestTrue(TEXT("Ready guide exposes C"),UTRFishingHUDWidget::BuildCompactGuide(S).ToString().Contains(TEXT("C：釣り座変更")));
 }
 R.Action(ETRPlayerAction::Deploy);R.F.Step(600);
 auto Reject=[&](const TCHAR* Name){R.Key(EKeys::C);TestFalse(Name,R.F.Session->GetPlayerModeSnapshot().bSideSelectionActive);};
 Reject(TEXT("FreeFall rejects reselection"));
 R.Action(ETRPlayerAction::Jerk);
 for(int I=0;I<600 && R.PC->GetDebugSnapshot().Egi.FishingState!=ETRFishingState::Stay;++I){R.F.Step();}
 TestTrue(TEXT("Reached Stay"),R.PC->GetDebugSnapshot().Egi.FishingState==ETRFishingState::Stay);Reject(TEXT("Stay rejects reselection"));
 R.PC->ActionStarted(ETRPlayerAction::Retrieve);R.F.Step();
 TestTrue(TEXT("Reached Retrieve"),R.PC->GetDebugSnapshot().Egi.FishingState==ETRFishingState::Retrieving);Reject(TEXT("Retrieve rejects reselection"));R.PC->ActionReleased(ETRPlayerAction::Retrieve);R.F.Step();
 R.Action(ETRPlayerAction::QuickRetrieve);TestTrue(TEXT("Reached Quick"),R.PC->GetDebugSnapshot().Retrieval.bIsQuickRetrieving);Reject(TEXT("Quick rejects reselection"));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRStationCancelContext,"TipRun.M105R3.CancelOriginAndEnhancedInput",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRStationCancelContext::RunTest(const FString&)
{
 FStationTest R;if(!R.Start(*this)){return false;}
 TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>(GEngine));R.F.World->SetGameInstance(GI.Get());
 TStrongObjectPtr<ULocalPlayer> Local(NewObject<ULocalPlayer>(GEngine));
 Local->PlayerController=R.PC;R.PC->Player=Local.Get();Local->PlayerAdded(nullptr,0);
 auto* Subsystem=Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
 auto* Input=NewObject<UEnhancedInputComponent>(R.PC);R.PC->InputComponent=Input;
 auto* EnhancedPlayer=NewObject<UEnhancedPlayerInput>(R.PC);R.PC->PlayerInput=EnhancedPlayer;
 R.PC->InstallInputBindings(Input);R.PC->BindSession(R.F.Session.Get());
 auto* View=R.F.World->SpawnActor<ATRPrototypeViewActor>();R.PC->SetPrototypeObserver(View);R.Camera->InitializeFor(R.PC);
 const int32 Count=View->GetComponents().Num();
 auto InjectCancel=[&](bool Down)
 {
  for(const auto& B:R.PC->InputConfig->Bindings){if(B.Command==ETRPlayerAction::Cancel){EnhancedPlayer->InjectInputForAction(B.Action,FInputActionValue(Down));}}
  EnhancedPlayer->ProcessInputStack({Input},1.f/60,false);
 };
 R.Action(ETRPlayerAction::FishingStart);
 TestTrue(TEXT("Navigation origin explicit"),R.F.Session->GetPlayerModeSnapshot().SideSelectionOrigin==ETRFishingSideSelectionOrigin::NavigationStart);
 R.Key(EKeys::BackSpace);R.Camera->UpdateCamera(1.f/60);
 TestTrue(TEXT("Navigation cancel restores camera/context"),R.Camera->GetNavigationSnapshot().bActive && Subsystem->HasMappingContext(R.PC->InputConfig->NavigationContext) && !Subsystem->HasMappingContext(R.PC->InputConfig->FishingContext));
 TestTrue(TEXT("Navigation usable after cancel"),R.PC->SubmitNavigationInput(FVector2D(.1,0)));R.F.Step();R.PC->SubmitNavigationInput(FVector2D::ZeroVector);R.F.Step();
 for(bool Port:{true,false})
 {
  R.EnterSide(Port);
  for(int I=0;I<10;++I)
  {
   R.Key(EKeys::C);R.Key(Port?EKeys::D:EKeys::A);
   const auto Selection=R.F.Session->GetPlayerModeSnapshot();
   TestTrue(TEXT("Fishing origin and previous side explicit"),Selection.SideSelectionOrigin==ETRFishingSideSelectionOrigin::FishingReadyChange && Selection.PreviousFishingSide==(Port?ETRFishingSide::Port:ETRFishingSide::Starboard));
   FTRBoatSnapshot AtCommand;bool Preserved=false;
   const auto Handle=R.F.Session->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result)
   {
    if(C.Type==ETRFishingCommandType::CancelSideSelection && Result==ETRCommandResult::Accepted)
    {const auto B=R.F.Boat->GetBoatSnapshot();Preserved=B.PositionM==AtCommand.PositionM && B.HeadingRad==AtCommand.HeadingRad && B.VelocityMps==AtCommand.VelocityMps;}
   });
   AtCommand=R.F.Boat->GetBoatSnapshot();R.Key(EKeys::BackSpace);R.F.Session->OnCommandProcessed.Remove(Handle);
   // A delayed Started from the saved Backspace abort mapping must not end the Session.
   InjectCancel(true);R.F.Step();InjectCancel(false);R.F.Step();
   const auto S=R.PC->GetDebugSnapshot();R.Camera->UpdateCamera(1.f/60);
   TestTrue(TEXT("Cancel preserves dynamics at command boundary"),Preserved);
   TestTrue(TEXT("Fishing Ready, original side, live session"),S.bSessionValid && S.PlayerMode.Mode==ETRPlayerMode::Fishing && !S.PlayerMode.bSideSelectionActive && S.Station.Side==(Port?ETRFishingSide::Port:ETRFishingSide::Starboard) && S.Egi.FishingState==ETRFishingState::Ready);
   TestTrue(TEXT("Actual FPS and exclusive Fishing context"),!R.Camera->GetNavigationSnapshot().bActive && R.Camera->GetCameraLocation().Equals(S.FishingCamera.PositionM*100,.001) && Subsystem->HasMappingContext(R.PC->InputConfig->FishingContext) && !Subsystem->HasMappingContext(R.PC->InputConfig->NavigationContext));
   TestFalse(TEXT("Navigation axes remain blocked"),R.PC->SubmitNavigationInput(FVector2D(1,1)));
   View->ApplyObservation(S,R.F.Session->RodTuning->Parameters,0,30);TestTrue(TEXT("Original rod presentation without duplicates"),View->RodVisual->IsVisible() && View->GetComponents().Num()==Count && S.Rod.RootWorldPositionM.Equals(S.Station.RodRootWorldM,1.e-8));
  }
  R.Action(ETRPlayerAction::Deploy);R.F.Step(60);TestTrue(TEXT("Deploy usable after repeated cancel"),R.PC->GetDebugSnapshot().bEgiValid);
  R.Action(ETRPlayerAction::QuickRetrieve);R.F.Step(180);R.Action(ETRPlayerAction::ReturnNavigation);
 }
 R.PC->UnbindSession();Local->PlayerRemoved();R.PC->Player=nullptr;Local->PlayerController=nullptr;R.F.World->SetGameInstance(nullptr);
 return true;
}
#endif
