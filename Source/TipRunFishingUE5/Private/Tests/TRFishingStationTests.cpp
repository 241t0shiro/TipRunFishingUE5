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
  TestTrue(TEXT("Real FPS viewport uses the same horizontal FOV as the rod inverse"),POV.AspectRatioAxisConstraint.IsSet() && POV.AspectRatioAxisConstraint.GetValue()==AspectRatio_MaintainXFOV);
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
  R.PC->SetFishingLookInput(FVector2D(1,-1));for(int I=0;I<100;++I){R.PC->AdvanceFishingCamera(.1);}R.PC->SetFishingLookInput(FVector2D::ZeroVector);R.F.Step(60);S=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("FPS limits and heading isolation"),S.FishingCamera.YawDeg==R.Stations->Parameters.MaxYawDeg && S.FishingCamera.PitchDeg==R.Stations->Parameters.MinPitchDeg && S.Boat.HeadingRad==HeadingBefore && S.Rod.ScreenControl==Rod.ScreenControl);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR4Assets,"TipRun.M105R4.SavedInputAndParameters",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR4Assets::RunTest(const FString&)
{
 auto* Config=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));if(!Config){return false;}
 if(FParse::Param(FCommandLine::Get(),TEXT("TRMigrateM105R4")))
 {
  Config->Input->CreateFishingCameraPrototype();
  for(UObject* A:{static_cast<UObject*>(Config->Input.Get()),static_cast<UObject*>(Config->FishingStations.Get())})
  {FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;TestTrue(TEXT("Save explicit R4 asset"),UPackage::SavePackage(A->GetOutermost(),A,*FPackageName::LongPackageNameToFilename(A->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));}
 }
 TArray<FText> Errors;TestTrue(TEXT("Saved R4 references validate"),Config->ValidateStartup(Errors) && Config->Input->FishingCameraYaw && Config->Input->FishingCameraPitch);
 for(auto Key:{EKeys::W,EKeys::S,EKeys::A,EKeys::D})
 {
  int Count=0;for(const auto& M:Config->Input->FishingContext->GetMappings())
  {if(M.Key==Key){++Count;TestTrue(TEXT("Fishing axes are independent actions"),M.Action==((Key==EKeys::W || Key==EKeys::S)?Config->Input->FishingCameraPitch:Config->Input->FishingCameraYaw));TestEqual(TEXT("Negative direction modifier"),M.Modifiers.Num(),(Key==EKeys::S || Key==EKeys::A)?1:0);}}
  TestEqual(TEXT("One mapping per camera key"),Count,1);
 }
 const auto& Rod=Config->Rod->Parameters;TestTrue(TEXT("Outward limited rod, not vertical or behind"),Rod.MinYawRad>-UE_DOUBLE_PI/2 && Rod.MaxYawRad<UE_DOUBLE_PI/2 && Rod.MinPitchRad>-UE_DOUBLE_PI/2 && Rod.MaxPitchRad<UE_DOUBLE_PI/2);
 auto Bad=Config->FishingStations->Parameters;Bad.FishingCameraYawRateDegPerS=-1;TestFalse(TEXT("Invalid rate rejected"),Bad.Validate());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR4Axes,"TipRun.M105R4.CameraRodIndependenceAndSides",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR4Axes::RunTest(const FString&)
{
 for(double Heading:{0.,UE_DOUBLE_PI/2,UE_DOUBLE_PI,UE_DOUBLE_PI*1.5})for(bool Port:{true,false})
 {
  FStationTest R;if(!R.Start(*this,Heading)){return false;}R.EnterSide(Port);
  const auto Initial=R.PC->GetDebugSnapshot();
  auto* Input=NewObject<UEnhancedInputComponent>(R.PC);auto* Enhanced=NewObject<UEnhancedPlayerInput>(R.PC);R.PC->InputComponent=Input;R.PC->PlayerInput=Enhanced;R.PC->InstallInputBindings(Input);
  auto Axis=[&](UInputAction* A,float Value){Enhanced->InjectInputForAction(A,FInputActionValue(Value));Enhanced->ProcessInputStack({Input},1.f/60,false);R.PC->AdvanceFishingCamera(.1);};
  Axis(R.PC->InputConfig->FishingCameraPitch,1);auto S=R.PC->GetDebugSnapshot();TestTrue(TEXT("W pitch up"),S.FishingCamera.PitchDeg>Initial.FishingCamera.PitchDeg);
  Axis(R.PC->InputConfig->FishingCameraPitch,-1);TestEqual(TEXT("S pitch down"),R.PC->GetDebugSnapshot().FishingCamera.PitchDeg,Initial.FishingCamera.PitchDeg);
  Axis(R.PC->InputConfig->FishingCameraYaw,-1);TestTrue(TEXT("A local yaw left"),R.PC->GetDebugSnapshot().FishingCamera.YawDeg<0);
  Axis(R.PC->InputConfig->FishingCameraYaw,1);Axis(R.PC->InputConfig->FishingCameraYaw,1);Axis(R.PC->InputConfig->FishingCameraYaw,0);
  S=R.PC->GetDebugSnapshot();TestTrue(TEXT("D local yaw right"),S.FishingCamera.YawDeg>0);
  R.PC->AdvanceFishingCamera(.1);TestEqual(TEXT("Release holds angle"),R.PC->GetDebugSnapshot().FishingCamera.YawDeg,S.FishingCamera.YawDeg);
  TestTrue(TEXT("Camera input does not mutate boat or rod"),S.Boat.PositionM==Initial.Boat.PositionM && S.Boat.HeadingRad==Initial.Boat.HeadingRad && S.Rod.ScreenControl==Initial.Rod.ScreenControl);
  R.PC->RoutePrototypeMouse(FVector2D::ZeroVector,false); // discard the boundary sample
  TestTrue(TEXT("Mouse dispatched to queue"),R.PC->RoutePrototypeMouse(FVector2D(8,8),false));
  TestEqual(TEXT("Rod changes only at fixed step"),R.PC->GetDebugSnapshot().Rod.ScreenControl.X,S.Rod.ScreenControl.X);R.F.Step();auto Aim=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("Mouse up/right changes rod only"),Aim.Rod.ScreenControl.X>S.Rod.ScreenControl.X && Aim.Rod.ScreenControl.Y>S.Rod.ScreenControl.Y && Aim.FishingCamera.YawDeg==S.FishingCamera.YawDeg && Aim.FishingCamera.PitchDeg==S.FishingCamera.PitchDeg);
  R.PC->RoutePrototypeMouse(FVector2D(-8,-8),false);R.F.Step();TestTrue(TEXT("Mouse down/left"),R.PC->GetDebugSnapshot().Rod.ScreenControl.Y<Aim.Rod.ScreenControl.Y && R.PC->GetDebugSnapshot().Rod.ScreenControl.X<Aim.Rod.ScreenControl.X);
  R.PC->SetFishingLookInput(FVector2D(1,1));for(int I=0;I<50;++I){R.PC->AdvanceFishingCamera(.1);R.PC->RoutePrototypeMouse(FVector2D(100,100),false);R.F.Step();}
  auto Clamped=R.PC->GetDebugSnapshot();const auto& Rod=R.F.Session->RodTuning->Parameters;
  TestTrue(TEXT("Camera and rod clamp"),Clamped.FishingCamera.YawDeg==R.Stations->Parameters.MaxYawDeg && Clamped.FishingCamera.PitchDeg==R.Stations->Parameters.MaxPitchDeg && Clamped.Rod.ScreenControl.X<=Rod.Screen.Max.X && Clamped.Rod.ScreenControl.Y<=Rod.Screen.Max.Y);
  TestTrue(TEXT("Local to world direction follows heading/side"),Clamped.Rod.TipWorldRotation.Rotator().Vector().Equals(Clamped.Rod.TipDirection,1.e-6));
  R.PC->SetFishingLookInput(FVector2D::ZeroVector);R.Action(ETRPlayerAction::Deploy);R.F.Step(300);
  R.PC->RoutePrototypeMouse(FVector2D::ZeroVector,false);const auto Before=R.PC->GetDebugSnapshot();R.PC->RoutePrototypeMouse(FVector2D(-5,-5),false);R.F.Step();TestTrue(TEXT("Rod available during cast"),R.PC->GetDebugSnapshot().Rod.ScreenControl.X<Before.Rod.ScreenControl.X && R.PC->GetDebugSnapshot().bMouseRodInputActive);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR4Guards,"TipRun.M105R4.SelectionUIFocusPauseAndHeld",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR4Guards::RunTest(const FString&)
{
 FStationTest R;if(!R.Start(*this)){return false;}
 auto Down=[&](FKey K){R.PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,IE_Pressed,1));};
 auto Up=[&](FKey K){R.PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,IE_Released,0));};
 Down(EKeys::W);R.EnterSide(true);TestFalse(TEXT("Held navigation W cannot become camera input"),R.PC->SetFishingLookInput(FVector2D(0,1)));Up(EKeys::W);TestTrue(TEXT("Physical release rearms"),R.PC->SetFishingLookInput(FVector2D(0,1)));
 R.PC->AdvanceFishingCamera(.1);R.Key(EKeys::C);auto Before=R.PC->GetDebugSnapshot();R.Key(EKeys::D);
 TestFalse(TEXT("Selection masks camera"),R.PC->SetFishingLookInput(FVector2D(1,0)));R.PC->AdvanceFishingCamera(.1);TestEqual(TEXT("Selection does not move camera"),R.PC->GetDebugSnapshot().FishingCamera.YawDeg,Before.FishingCamera.YawDeg);
 R.Key(EKeys::BackSpace);TestTrue(TEXT("Cancel restores camera input"),R.PC->SetFishingLookInput(FVector2D(1,0)));R.Key(EKeys::C);R.Key(EKeys::D);R.Key(EKeys::Enter);TestTrue(TEXT("Confirm restores camera input"),R.PC->SetFishingLookInput(FVector2D(1,0)));
 for(int Guard=0;Guard<3;++Guard)
 {
  Down(EKeys::W);R.PC->SetFishingLookInput(FVector2D(0,1));R.PC->SubmitMouseDelta(FVector2D(5,5));
  const auto Base=R.PC->GetDebugSnapshot().Rod;
  if(Guard==0){R.PC->SetInputFocus(false);}else if(Guard==1){R.PC->SetPauseRequested(true);}else{R.PC->SetPrototypePanelOpen(true);}
  TestFalse(TEXT("Guard blocks relative mouse"),R.PC->RoutePrototypeMouse(FVector2D(5,5),false));TestFalse(TEXT("Guard blocks look"),R.PC->SetFishingLookInput(FVector2D(0,1)));TestFalse(TEXT("Guard blocks jerk"),R.PC->ActionStarted(ETRPlayerAction::Jerk));TestFalse(TEXT("Guard blocks retrieve"),R.PC->ActionStarted(ETRPlayerAction::Retrieve));R.F.Step(2);
  if(Guard==0){R.PC->SetInputFocus(true);}else if(Guard==1){R.PC->SetPauseRequested(false);}else{R.PC->SetPrototypePanelOpen(false);}
  R.F.Step();TestEqual(TEXT("Queued stale mouse cleared"),R.PC->GetDebugSnapshot().Rod.BaseYawRad,Base.BaseYawRad);TestFalse(TEXT("Held input stays disarmed"),R.PC->SetFishingLookInput(FVector2D(0,1)));Up(EKeys::W);TestTrue(TEXT("New input after release"),R.PC->SetFishingLookInput(FVector2D(0,1)));
  TestFalse(TEXT("First stale mouse sample dropped"),R.PC->RoutePrototypeMouse(FVector2D(1000,1000),false));R.PC->ActionReleased(ETRPlayerAction::Jerk);R.PC->ActionReleased(ETRPlayerAction::Retrieve);
 }
 R.PC->SetFishingLookInput(FVector2D::ZeroVector);R.Action(ETRPlayerAction::Deploy);R.F.Step(600);R.Action(ETRPlayerAction::Jerk);TestEqual(TEXT("Existing jerk remains one action"),R.PC->GetDebugSnapshot().Egi.JerkCount,int64(1));
 R.F.Step(180);R.PC->ActionStarted(ETRPlayerAction::Retrieve);R.F.Step();TestTrue(TEXT("Existing retrieve reaches simulation"),R.PC->GetDebugSnapshot().Retrieval.bIsRetrieving);R.PC->ActionReleased(ETRPlayerAction::Retrieve);R.F.Step();TestFalse(TEXT("Retrieve release remains"),R.PC->GetDebugSnapshot().Retrieval.bIsRetrieving);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR4Determinism,"TipRun.M105R4.RodFixedFPSAndLifetime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR4Determinism::RunTest(const FString&)
{
 FTRRodSnapshot Reference;
 for(int FPS:{30,60,120})
 {
  FStationTest R;if(!R.Start(*this)){return false;}R.EnterSide(true);const auto Tick=R.F.Sim()->GetSimulationTime().TickIndex;
  for(int I=0;I<40;++I){TestTrue(TEXT("Scheduled rod input"),R.PC->SubmitMouseDelta(FVector2D(I%2?-.5:1,1),Tick+I*2));}
  for(int I=0;I<FPS*2;++I){R.F.Sim()->AdvanceFrame(1./FPS);}const auto S=R.PC->GetDebugSnapshot();
  if(FPS==30){Reference=S.Rod;}else{TestTrue(TEXT("Fixed rod identical across render FPS"),S.Rod.ScreenControl==Reference.ScreenControl && S.Rod.TipWorldPositionM==Reference.TipWorldPositionM);}
  R.F.Session->Destroy();TestFalse(TEXT("Destroyed target rejects rod"),R.PC->SubmitMouseDelta(FVector2D(1,1)));TestFalse(TEXT("Destroyed target rejects look"),R.PC->SetFishingLookInput(FVector2D(1,1)));
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRScreenQuickRod,"TipRun.M105R4.Screen.QuickRetrieveGripAndControl",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRScreenQuickRod::RunTest(const FString&)
{
 for(bool Port:{true,false})
 {
  FStationTest R;if(!R.Start(*this,UE_DOUBLE_PI/2)){return false;}R.EnterSide(Port);
  R.PC->SubmitMouseDelta(FVector2D(5,5));R.F.Step();R.Action(ETRPlayerAction::Deploy);R.F.Step(240);
  const auto Before=R.PC->GetDebugSnapshot();const auto* Row=R.Stations->Parameters.Find(Port?ETRFishingSide::Port:ETRFishingSide::Starboard);
  int32 Results=0;R.F.Session->OnCastCompleted.AddLambda([&](const FTRCatchResult&){++Results;});
  R.Action(ETRPlayerAction::QuickRetrieve);double MaxRootError=0;bool SawQuick=false;
  for(int I=0;I<180;++I)
  {
   const auto S=R.PC->GetDebugSnapshot();SawQuick|=S.Retrieval.bIsQuickRetrieving;
   const FQuat Heading(FVector::UpVector,S.Boat.HeadingRad);
   const FVector LocalRoot=Heading.UnrotateVector(S.Rod.RootWorldPositionM-S.Boat.PositionM);
   MaxRootError=FMath::Max(MaxRootError,(LocalRoot-Row->RodMountM).Size());
   TestTrue(TEXT("Quick keeps valid same-tick fixed grip and length"),S.Rod.bValid && FMath::IsNearlyEqual((S.Rod.TipWorldPositionM-S.Rod.RootWorldPositionM).Size(),Before.Rod.LengthM,1.e-9));
   TestTrue(TEXT("Quick never resets screen base or camera look"),S.Rod.ScreenControl==Before.Rod.ScreenControl && S.FishingCamera.Rotation.Equals(Before.FishingCamera.Rotation,1.e-9));
   TestTrue(TEXT("Quick camera follows only boat drift"),Heading.UnrotateVector(S.FishingCamera.PositionM-S.Boat.PositionM).Equals(Row->CameraM,1.e-9));
   if(S.Retrieval.bIsQuickRetrieving){TestEqual(TEXT("Quick does not replace water egi with rod motion"),S.Egi.WorldPositionM,Before.Egi.WorldPositionM);}
   R.F.Step();
  }
  const auto After=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("Quick progresses once to Ready/Onboard/Unlock while drift continues"),SawQuick && Results==1 && R.F.Session->IsEgiOnboard() && R.F.Session->CanChangeEquipment() && After.Egi.FishingState==ETRFishingState::Ready && !After.Retrieval.bIsQuickRetrieving && After.Boat.PositionM!=Before.Boat.PositionM);
  TestTrue(TEXT("No stale root during Quick or completion"),MaxRootError<1.e-9);
  R.F.Session->OnCastCompleted.Clear();
 }
 return true;
}
#endif
