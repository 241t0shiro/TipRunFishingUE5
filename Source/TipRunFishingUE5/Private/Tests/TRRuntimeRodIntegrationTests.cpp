// R4A-1: actual Game World tick, saved map, LocalPlayer projection and mesh endpoints.
// R4A-2 retains the A1 camera failure as a before/after acceptance test.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "SceneView.h"
#include "HAL/FileManager.h"
#include "Tickable.h"
#include "Game/TRPlayerController.h"
#include "Game/TRPlayerCameraManager.h"
#include "Game/TRFishingSessionActor.h"
#include "Game/TRPrototypeViewActor.h"
#include "Game/TRGameModeBase.h"
#include "Fishing/TRFishingComponent.h"
#include "Fishing/TREgiSimulationComponent.h"
#include "Fishing/TRRodControlComponent.h"
#include <limits>
#include "UI/TRFishingHUDWidget.h"
#include "Data/TRSessionConfigDataAsset.h"
#include "Data/TRRodTuningDataAsset.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
 struct FRuntimeRodWorld
 {
  TStrongObjectPtr<UGameInstance> Instance;
  TStrongObjectPtr<UGameViewportClient> Client;
  TSharedPtr<SViewport> Widget;
  TSharedPtr<FSceneViewport> Viewport;
  UWorld* World=nullptr;
  UWorld* PreviousWorld=nullptr;
  UGameViewportClient* PreviousViewport=nullptr;
  ATRPlayerController* PC=nullptr;
  ATRPrototypeViewActor* Visual=nullptr;
  double Dt=1./60,OldDelta=0,ConfiguredLengthM=0;
  FString Trace,Name;
  bool bTraceFrames=true;
  bool Start(FAutomationTestBase& Test,const FString& CaseName,int FPS=60,bool Port=true,int Width=1920,int Height=1080,double HeadingDeg=0)
  {
   Name=CaseName;Dt=1./FPS;OldDelta=FApp::GetDeltaTime();PreviousWorld=GWorld;PreviousViewport=GEngine->GameViewport;
   Instance.Reset(NewObject<UGameInstance>(GEngine));Instance->InitializeStandalone();
   Client.Reset(NewObject<UGameViewportClient>(GEngine));
   auto& Context=*Instance->GetWorldContext();Client->Init(Context,Instance.Get(),false);Context.GameViewport=Client.Get();GEngine->GameViewport=Client.Get();
   // LoadMap's Game-world path requires an instance ID to load an independent
   // disk copy when the saved map is already open in the Editor. Never reuse it.
   static int32 RuntimeInstance=1000;Context.PIEInstance=++RuntimeInstance;
   Widget=SNew(SViewport);Viewport=FSceneViewport::Create(TStrongPtrVariant<FViewportClient>(Client.Get()),Widget);Widget->SetViewportInterface(Viewport.ToSharedRef());
   Viewport->SetInitialSize(FIntPoint(Width,Height));
   FString Error;
   auto* LP=Instance->CreateLocalPlayer(0,Error,false);
   if(!Test.TestNotNull(TEXT("Runtime LocalPlayer"),LP)){return false;}
   const FURL URL(nullptr,TEXT("/Game/TipRun/Prototype/M105/L_TR_M105_Prototype"),TRAVEL_Absolute);
   if(!GEngine->LoadMap(Context,URL,nullptr,Error)){Test.AddError(TEXT("Runtime LoadMap: ")+Error);return false;}
   World=Context.World();PC=Cast<ATRPlayerController>(LP->PlayerController);
   for(TActorIterator<ATRPrototypeViewActor> It(World);It;++It){Visual=*It;break;}
   if(!Test.TestNotNull(TEXT("Saved map PlayerController"),PC) || !Test.TestNotNull(TEXT("Saved map observation actor"),Visual)){return false;}
   if(!Test.TestTrue(TEXT("Real runtime GameMode, session and camera"),World->GetAuthGameMode<ATRGameModeBase>() && PC->GetBoundSession() && Cast<ATRPlayerCameraManager>(PC->PlayerCameraManager))){return false;}
   ConfiguredLengthM=World->GetAuthGameMode<ATRGameModeBase>()->SessionConfig->Rod->Parameters.LengthM;
   Frames(6);
   if(HeadingDeg>0)
   {
    // Reach representative headings through the real navigation queue, not a boat setter.
    Key(EKeys::D,true);const double Stop=FMath::DegreesToRadians(HeadingDeg)-1./6;
    double Progress=0,Previous=PC->GetDebugSnapshot().Boat.HeadingRad;
    for(int I=0;I<FPS*8 && Progress<Stop;++I){Frame();const double Now=PC->GetDebugSnapshot().Boat.HeadingRad;Progress+=FMath::UnwindRadians(Now-Previous);Previous=Now;}
    Key(EKeys::D,false);Frames(FPS*2);
    Test.AddInfo(FString::Printf(TEXT("Heading requested %.0f actual %.6f degrees"),HeadingDeg,FMath::RadiansToDegrees(PC->GetDebugSnapshot().Boat.HeadingRad)));
    Test.TestTrue(TEXT("Runtime navigation reached representative heading within two degrees"),FMath::Abs(FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(PC->GetDebugSnapshot().Boat.HeadingRad),HeadingDeg))<2);
   }
   Tap(EKeys::Enter);Tap(Port?EKeys::A:EKeys::D);Tap(EKeys::Enter);Frames(6);
   const auto S=PC->GetDebugSnapshot();const auto O=Observe();
   Trace+=FString::Printf(TEXT("BOOT saved map=%s mode=%s worldType=%d begun=%d local=%d\n"),*World->GetPathName(),*World->GetAuthGameMode()->GetClass()->GetPathName(),int32(World->WorldType),World->HasBegunPlay(),PC->IsLocalController());
   return Test.TestTrue(TEXT("Saved world reaches Fishing Ready with real projection"),S.PlayerMode.Mode==ETRPlayerMode::Fishing && S.Egi.FishingState==ETRFishingState::Ready && O.bValid && O.bProjected && O.ViewRect.Width()==Width && O.ViewRect.Height()==Height && O.VisualFrame==O.CameraFrame);
  }
  void Frame()
  {
   ++GFrameCounter;FApp::SetDeltaTime(Dt);GWorld=World;
   World->Tick(LEVELTICK_All,float(Dt));
   // UGameEngine/UEditorEngine tick world-less services after their worlds.
   // Enhanced Input rebuilds pending mapping contexts here; a World tick alone
   // would leave every saved mapping pending forever in this synchronous test.
   FTickableGameObject::TickObjects(nullptr,LEVELTICK_All,false,float(Dt));
   if(bTraceFrames && PC && Visual){Trace+=FString::Printf(TEXT("\nFRAME %llu\n%s\n"),GFrameCounter,*PC->GetDebugSnapshot().RuntimeDiagnostics);}
  }
  void Frames(int N){for(int I=0;I<N;++I){Frame();}}
  void Key(FKey K,bool Down)
  {Client->InputKey(FInputKeyEventArgs(Viewport.Get(),FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,0));}
  void Tap(FKey K){Key(K,true);Frames(2);Key(K,false);Frames(2);}
  void Mouse(FVector2D D)
  {
   // Device-level axis events, never InjectInputForAction or solver calls.
   for(int Axis=0;Axis<2;++Axis)
   {Client->InputAxis(FInputKeyEventArgs(Viewport.Get(),FInputDeviceId::CreateFromInternalId(0),Axis?EKeys::MouseY:EKeys::MouseX,float(D[Axis]),float(Dt),1,0));}
   Frame();
  }
  FTRRuntimeObservation Observe() const{return Visual->GetRuntimeObservation();}
  ~FRuntimeRodWorld()
  {
   if(!Name.IsEmpty())
   {const FString Dir=FPaths::ProjectSavedDir()/TEXT("Automation/R4A1Observations");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Trace,*(Dir/(Name+TEXT(".txt"))));}
   UWorld* ContextWorld=Instance.IsValid()?Instance->GetWorld():nullptr;
   if(World){World->BeginTearingDown();World->EndPlay(EEndPlayReason::Quit);}
   if(Instance.IsValid()){Instance->Shutdown();}
   if(ContextWorld){GEngine->DestroyWorldContext(ContextWorld);ContextWorld->DestroyWorld(false);}
   if(Viewport.IsValid()){Viewport->SetViewportClient(TStrongPtrVariant<FViewportClient>());}Viewport.Reset();Widget.Reset();
   GEngine->GameViewport=PreviousViewport;GWorld=PreviousWorld;FApp::SetDeltaTime(OldDelta);
  }
 };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeCameraFailure,"TipRun.R4A1.Acceptance.CameraOnlyIndependence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeCameraFailure::RunTest(const FString&)
{
 for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;if(!R.Start(*this,Port?TEXT("Camera_Port"):TEXT("Camera_Starboard"),60,Port)){return false;}
  const auto Base=R.PC->GetDebugSnapshot().Rod;
  const auto Before=R.Observe();R.Key(EKeys::D,true);R.Frames(20);R.Key(EKeys::D,false);R.Frames(4);const auto After=R.Observe();
  TestTrue(TEXT("PASS fixed boat-local grip"),After.RootLocal.Equals(Before.RootLocal,1.e-8));
  TestTrue(TEXT("PASS camera actually changed"),!After.CameraRotation.Equals(Before.CameraRotation,1.e-4));
  AddInfo(FString::Printf(TEXT("LOCAL movement root=%.9g tip=%.9g direction=%.9g"),(After.RootLocal-Before.RootLocal).Size(),(After.TipLocal-Before.TipLocal).Size(),(After.DirectionLocal-Before.DirectionLocal).Size()));
  TestTrue(TEXT("R4A2: Camera-only must preserve local rod direction"),After.StationDirectionLocal.Equals(Before.StationDirectionLocal,1.e-8));
  TestTrue(TEXT("R4A2: Camera-only must preserve local rod tip"),After.StationTipLocal.Equals(Before.StationTipLocal,1.e-8));
  for(FKey K:{EKeys::W,EKeys::A,EKeys::S,EKeys::D})
  {
   R.Key(K,true);
   for(int I=0;I<90;++I)
   {
    R.Frame();const auto Now=R.PC->GetDebugSnapshot().Rod;
    TestTrue(TEXT("Camera-only keeps exact persistent local pose"),Now.RodRootLocal==Base.RodRootLocal && Now.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && Now.BaseRodTipLocal==Base.BaseRodTipLocal && Now.LengthM==Base.LengthM);
    TestTrue(TEXT("Actual mesh remains on the unchanged local rod"),R.Observe().StationTipLocal.Equals(Before.StationTipLocal,1.e-8) && R.Observe().TipErrorM<1.e-8);
   }
   R.Key(K,false);R.Frames(3);
  }

 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeMouse,"TipRun.R4A1.Runtime.MouseProjectionAndLength",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeMouse::RunTest(const FString&)
{
 for(int FPS:{30,60,120})for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;if(!R.Start(*this,FString::Printf(TEXT("Mouse_%d_%d"),FPS,Port),FPS,Port)){return false;}
  double Min=DBL_MAX,Max=0,Cross[2]={0,0},BoundaryCross[2]={0,0},MaxTipError=0;int Moved[2]={0,0};
  const auto Camera=R.Observe().CameraRotation;
  for(int Axis=0;Axis<2;++Axis)for(double Sign:{1.,-1.})
  {
   // Drain previous-axis device smoothing before isolating the next axis.
   for(int I=0;I<30;++I){R.Mouse(FVector2D::ZeroVector);}
   for(int I=0;I<180;++I)
   {
    const auto Before=R.Observe();FVector2D D=FVector2D::ZeroVector;D[Axis]=Sign*20;R.Mouse(D);const auto O=R.Observe();
    MaxTipError=FMath::Max(MaxTipError,O.TipErrorM);
    TestTrue(TEXT("Runtime mesh endpoints match snapshot"),O.RootErrorM<1.e-7 && O.TipErrorM<1.e-7);
    TestTrue(TEXT("World/visual fixed length and active projection"),O.bProjected && FMath::IsNearlyEqual(O.LengthM,R.ConfiguredLengthM,1.e-8));
    const double CrossMove=FMath::Abs((O.PixelTip-Before.PixelTip)[1-Axis]);
    if(R.PC->GetDebugSnapshot().Rod.bEnvelopeLimited){BoundaryCross[Axis]=FMath::Max(BoundaryCross[Axis],CrossMove);}
    else{Cross[Axis]=FMath::Max(Cross[Axis],CrossMove);}
    const double Along=(O.PixelTip-Before.PixelTip)[Axis]*(Axis?-Sign:Sign);if(Along>.001){++Moved[Axis];}
    Min=FMath::Min(Min,(O.PixelTip-O.PixelRoot).Size());Max=FMath::Max(Max,(O.PixelTip-O.PixelRoot).Size());
   }
  }
  AddInfo(FString::Printf(TEXT("Mesh maximum tip error=%.12g m; explicit envelope boundary cross X/Y=%.9g/%.9g px"),MaxTipError,BoundaryCross[0],BoundaryCross[1]));
  TestTrue(TEXT("Mouse never changes camera rotation"),R.Observe().CameraRotation.Equals(Camera,1.e-8));
  TestTrue(TEXT("Raw Mouse X actually reaches visible rod"),Moved[0]>0);TestTrue(TEXT("Raw Mouse Y actually reaches visible rod"),Moved[1]>0);
  TestTrue(TEXT("Pure Mouse X: visual Y invariant within 0.05px"),Cross[0]<.05);TestTrue(TEXT("Pure Mouse Y: visual X invariant within 0.05px"),Cross[1]<.05);
  AddInfo(FString::Printf(TEXT("OBSERVATION fps=%d port=%d crossX=%.9g crossY=%.9g projectedLength min=%.6g max=%.6g ratio=%.6g raw=%s enhanced=%s queued=%s consumed=%s"),FPS,Port,Cross[0],Cross[1],Min,Max,Max/Min,*R.PC->DiagnosticRawMouse.ToString(),*R.PC->DiagnosticEnhancedMouse.ToString(),*R.PC->DiagnosticQueuedMouse.ToString(),*R.PC->GetBoundSession()->DiagnosticConsumedMouse.ToString()));
  for(FKey K:{EKeys::W,EKeys::A,EKeys::S,EKeys::D})
  {R.Key(K,true);for(int I=0;I<FPS/3;++I){R.Frame();const auto O=R.Observe();if(O.bProjected){Min=FMath::Min(Min,(O.PixelTip-O.PixelRoot).Size());Max=FMath::Max(Max,(O.PixelTip-O.PixelRoot).Size());}}R.Key(K,false);R.Frames(3);}
  AddInfo(FString::Printf(TEXT("MEASUREMENT ONLY including camera look: min=%.6g max=%.6g ratio=%.6g (no perceptual acceptance threshold)"),Min,Max,Max/Min));
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeLock,"TipRun.R4A1.Runtime.DeployJerkRetrieve",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeLock::RunTest(const FString&)
{
 int Aborts=0,Results=0,Active=0;
 for(int Pose:{0,-1,1})for(int DeployFrames:{1,4})for(int Delay:{0,2,5,9,15,24,32})
 {
  FRuntimeRodWorld R;if(!R.Start(*this,FString::Printf(TEXT("Lock_pose%d_deploy%d_delay%d"),Pose,DeployFrames,Delay))){return false;}
  if(Pose){for(int I=0;I<24;++I){R.Mouse(FVector2D(0,Pose*100));}R.Frames(4);}
  R.Key(EKeys::Enter,true);R.Frames(DeployFrames);R.Key(EKeys::Enter,false);
  R.Key(EKeys::RightMouseButton,true);R.Frame();R.Key(EKeys::RightMouseButton,false);R.Frames(Delay);
  R.Key(EKeys::LeftMouseButton,true);
  for(int I=0;I<360 && !R.PC->GetBoundSession()->HasResult();++I){R.Frame();}
  R.Key(EKeys::LeftMouseButton,false);R.Frames(3);
  auto* S=R.PC->GetBoundSession();
  TestTrue(TEXT("Runtime input really accepted a Jerk"),R.Trace.Contains(TEXT("Action: Jerk Accepted")));
  if(S->HasResult())
  {
   if(S->GetLastResult().Outcome==ETRCastOutcome::Aborted){++Aborts;AddError(FString::Printf(TEXT("REPRODUCED Lock pose=%d deploy=%d delay=%d\nBEFORE: %s\nAFTER: %s"),Pose,DeployFrames,Delay,*S->DiagnosticAbortContext,*S->GetRuntimeDiagnostics()));}
   else{++Results;TestTrue(TEXT("Normal Result is onboard, not false Abort"),S->IsEgiOnboard());R.Tap(EKeys::N);TestTrue(TEXT("Normal NextCast unlocks"),S->CanChangeEquipment());}
  }
  else{++Active;AddInfo(FString::Printf(TEXT("LIVE RELEASE delay=%d state=%s"),Delay,*S->GetRuntimeDiagnostics()));
    TestTrue(TEXT("Live release remains usable, not action lock"),S->IsCommandAvailable(ETRFishingCommandType::Jerk) && S->IsCommandAvailable(ETRFishingCommandType::QuickRetrieve) && !S->Fishing->HasPendingRetrieve());
    // Separate follow-up after the EXACT original probe has finished: live cases
    // may be cleared by ordinary Quick, without masquerading as normal Retrieved.
    R.Tap(EKeys::Q);R.Frames(180);
    TestTrue(TEXT("Live case remains clearable without restart"),S->CanChangeEquipment() && S->IsEgiOnboard() && S->GetLastResult().bQuickRetrieved);
   }
 }
 TestEqual(TEXT("Same 42 legal input sequences produce no technical Abort"),Aborts,0);
 AddInfo(FString::Printf(TEXT("Lock sweep aborted=%d normalResults=%d active=%d; active cases retain a usable release/Quick path; perceptual acceptance remains manual"),Aborts,Results,Active));
 return true;
}
// Diagnostic data must be observational; it cannot repair or advance the simulation.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeDiagnostics,"TipRun.R4A1.Runtime.DiagnosticsReadOnly",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeDiagnostics::RunTest(const FString&)
{
 FRuntimeRodWorld R;if(!R.Start(*this,TEXT("DiagnosticsReadOnly"))){return false;}
 auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
 const auto Before=R.PC->GetDebugSnapshot();const auto Time=Sim->GetSimulationTime();const int32 Queue=Sim->GetQueuedCommandCount();
 for(int I=0;I<20;++I){R.PC->GetDebugSnapshot();R.Observe();}
 const auto After=R.PC->GetDebugSnapshot();
 TestEqual(TEXT("Diagnostics never advance tick"),Sim->GetSimulationTime().TickIndex,Time.TickIndex);
 TestEqual(TEXT("Diagnostics never enqueue input"),Sim->GetQueuedCommandCount(),Queue);
 TestTrue(TEXT("Diagnostics never modify rod"),Before.Rod.TipWorldPositionM==After.Rod.TipWorldPositionM && Before.Rod.ScreenControl==After.Rod.ScreenControl);
 for(const TCHAR* Field:{TEXT("InputContext="),TEXT("Mouse raw="),TEXT("Smoothing="),TEXT("pendingJerk="),TEXT("RetrieveHeld="),TEXT("registration="),TEXT("Failure="),TEXT("StationRootLocal="),TEXT("cameraFrame="),TEXT("errorTip="),TEXT("Available="),TEXT("R4A5 Envelope enabled="),TEXT("Envelope"),TEXT("projectedLength="),TEXT("PendingAction="),TEXT("RecoveryAvailable=")})
 {TestTrue(Field,After.RuntimeDiagnostics.Contains(Field));}
 const auto Rows=UTRFishingHUDWidget::BuildReadout(After);
 TestTrue(TEXT("Detailed HUD receives runtime values"),Rows.Last().Value.ToString()==After.RuntimeDiagnostics);
 TestFalse(TEXT("Runtime detail row stays out of compact HUD"),UTRFishingHUDWidget::IsPrimaryReadout(Rows.Num()-1));
 TestFalse(TEXT("Details initially closed"),R.PC->IsPrototypeDetailsOpen());
 R.Tap(EKeys::Insert);TestTrue(TEXT("Real Insert opens details"),R.PC->IsPrototypeDetailsOpen());
 R.Tap(EKeys::Insert);TestFalse(TEXT("Real Insert closes details"),R.PC->IsPrototypeDetailsOpen());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeLocalPose,"TipRun.R4A2.Runtime.LocalPoseDriftSideActions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeLocalPose::RunTest(const FString&)
{
 for(int FPS:{30,60,120})for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;if(!R.Start(*this,FString::Printf(TEXT("R4A2_Local_%d_%d"),FPS,Port),FPS,Port)){return false;}
  auto SameBase=[&](const FTRRodSnapshot& A,const FTRRodSnapshot& B)
  {return A.RodRootLocal==B.RodRootLocal && A.BaseRodDirectionLocal==B.BaseRodDirectionLocal && A.BaseRodTipLocal==B.BaseRodTipLocal && A.LengthM==B.LengthM;};
  const auto Start=R.PC->GetDebugSnapshot();R.Frames(FPS*2);const auto Drift=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("Saved environment actually moves boat and world rod"),!Drift.Boat.PositionM.Equals(Start.Boat.PositionM,1.e-4) && !Drift.Rod.RootWorldPositionM.Equals(Start.Rod.RootWorldPositionM,1.e-4));
  TestTrue(TEXT("Drift cannot change local base"),SameBase(Start.Rod,Drift.Rod));
  TestTrue(TEXT("World tip follows boat translation"),(Drift.Rod.TipWorldPositionM-Start.Rod.TipWorldPositionM).Equals(Drift.Boat.PositionM-Start.Boat.PositionM,1.e-8));
  // R3 ready re-selection, then cancel the opposite tentative side.
  R.Tap(EKeys::C);R.Tap(Port?EKeys::D:EKeys::A);R.Tap(EKeys::Enter);
  const auto Side=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("Opposite station committed in Fishing"),Side.PlayerMode.Mode==ETRPlayerMode::Fishing && Side.PlayerMode.FishingSide==(Port?ETRFishingSide::Starboard:ETRFishingSide::Port));
  TestTrue(TEXT("New grip really uses selected station"),Side.Rod.RootWorldPositionM.Equals(Side.Station.RodRootWorldM,1.e-9));
  R.Tap(EKeys::C);R.Tap(Port?EKeys::A:EKeys::D);R.Tap(EKeys::BackSpace);
  TestTrue(TEXT("Cancel preserves committed station pose"),SameBase(Side.Rod,R.PC->GetDebugSnapshot().Rod));
  R.Key(EKeys::D,true);R.Frames(FPS/3);R.Key(EKeys::D,false);R.Frames(3);
  TestTrue(TEXT("New station also ignores camera-only changes"),SameBase(Side.Rod,R.PC->GetDebugSnapshot().Rod));
  // Real right click; unchanged base throughout the entire old Up/Return profile.
  R.Tap(EKeys::Enter);R.Frames(FPS);const auto Base=R.PC->GetDebugSnapshot();
  R.Key(EKeys::RightMouseButton,true);bool ActionMoved=false;
  for(int I=0;I<FPS*2;++I)
  {
   R.Frame();if(I==2){R.Key(EKeys::RightMouseButton,false);}
   const auto N=R.PC->GetDebugSnapshot();TestTrue(TEXT("Shakuri never mutates local base"),SameBase(Base.Rod,N.Rod));
   ActionMoved|=!N.Rod.FinalRodDirectionLocal.Equals(Base.Rod.BaseRodDirectionLocal,1.e-5);
   TestTrue(TEXT("Action and visual fixed length"),FMath::IsNearlyEqual((N.Rod.FinalRodTipLocal-N.Rod.RodRootLocal).Size(),Base.Rod.LengthM,1.e-9) && R.Observe().TipErrorM<1.e-8);
  }
  const auto Returned=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("Temporary profile moved and returned exactly to base"),ActionMoved && !Returned.Rod.bShakuriActive && Returned.Rod.FinalRodDirectionLocal==Returned.Rod.BaseRodDirectionLocal && Returned.Rod.FinalRodTipLocal==Returned.Rod.BaseRodTipLocal);
  if(R.PC->GetBoundSession()->HasResult() && R.PC->GetBoundSession()->GetLastResult().Outcome==ETRCastOutcome::Aborted)
  {
   AddError(FString::Printf(TEXT("R4A4 EVIDENCE after station change fps=%d initialPort=%d: %s\nBefore cleanup: %s"),FPS,Port,*R.PC->GetBoundSession()->GetRuntimeDiagnostics(),*R.PC->GetBoundSession()->DiagnosticAbortContext));
   continue; // Not a PASS or successful Quick; preserve this failure for A4.
  }
  // Quick action remains independent of root, base, physical length and camera look.
  R.Key(EKeys::Q,true);bool SawQuick=false;
  for(int I=0;I<FPS*3;++I)
  {
   R.Frame();if(I==2){R.Key(EKeys::Q,false);}
   const auto N=R.PC->GetDebugSnapshot();SawQuick|=N.Retrieval.bIsQuickRetrieving;
   TestTrue(TEXT("Quick keeps exact local base"),SameBase(Returned.Rod,N.Rod));
   TestTrue(TEXT("Quick cannot rotate camera"),N.FishingCamera.Rotation.Equals(Returned.FishingCamera.Rotation,1.e-9));
   if(SawQuick && R.PC->GetBoundSession()->CanChangeEquipment()){break;}
  }
  TestTrue(TEXT("Quick actually completed Ready Onboard Unlocked"),SawQuick && R.PC->GetBoundSession()->CanChangeEquipment() && R.PC->GetBoundSession()->IsEgiOnboard());
  AddInfo(FString::Printf(TEXT("Local invariants fps=%d port=%d drift=%.9g baseDelta=%.9g"),FPS,Port,(Drift.Boat.PositionM-Start.Boat.PositionM).Size(),(Drift.Rod.BaseRodDirectionLocal-Start.Rod.BaseRodDirectionLocal).Size()));
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeQuickLocalPose,"TipRun.R4A2.Runtime.QuickLocalPose",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeQuickLocalPose::RunTest(const FString&)
{
 for(int FPS:{30,60,120})for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;if(!R.Start(*this,FString::Printf(TEXT("R4A2_Quick_%d_%d"),FPS,Port),FPS,Port)){return false;}
  R.Mouse(FVector2D::ZeroVector);R.Mouse(FVector2D(3,2));R.Frames(4);
  R.Tap(EKeys::Enter);R.Frames(FPS);const auto Before=R.PC->GetDebugSnapshot();
  R.Key(EKeys::Q,true);bool SawQuick=false;
  for(int I=0;I<FPS*3;++I)
  {
   R.Frame();if(I==2){R.Key(EKeys::Q,false);}
   const auto N=R.PC->GetDebugSnapshot();SawQuick|=N.Retrieval.bIsQuickRetrieving;
   TestTrue(TEXT("Quick root/base/tip/length invariant"),N.Rod.RodRootLocal==Before.Rod.RodRootLocal && N.Rod.BaseRodDirectionLocal==Before.Rod.BaseRodDirectionLocal && N.Rod.BaseRodTipLocal==Before.Rod.BaseRodTipLocal && N.Rod.LengthM==Before.Rod.LengthM);
   TestTrue(TEXT("Quick camera boat-relative position and rotation invariant"),(N.FishingCamera.PositionM-N.Boat.PositionM).Equals(Before.FishingCamera.PositionM-Before.Boat.PositionM,1.e-9) && N.FishingCamera.Rotation.Equals(Before.FishingCamera.Rotation,1.e-9));
   TestTrue(TEXT("Quick actual mesh still matches snapshot"),R.Observe().TipErrorM<1.e-7);
   if(SawQuick && R.PC->GetBoundSession()->CanChangeEquipment()){break;}
  }
  TestTrue(TEXT("Independent Quick actually completes Ready Onboard Unlocked"),SawQuick && R.PC->GetBoundSession()->CanChangeEquipment() && R.PC->GetBoundSession()->IsEgiOnboard());
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeFrozenRodObservation,"TipRun.R4A2.Runtime.FrozenCommandCamera",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeFrozenRodObservation::RunTest(const FString&)
{
 FVector Reference=FVector::ZeroVector;
 for(bool ChangeView:{false,true})
 {
  FRuntimeRodWorld R;if(!R.Start(*this,ChangeView?TEXT("R4A2_DelayedLook"):TEXT("R4A2_DelayedReference"))){return false;}
  auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();const auto Before=R.PC->GetDebugSnapshot();
  const int64 Target=Sim->GetSimulationTime().TickIndex+12;
  TestTrue(TEXT("Mouse is queued with a captured view"),R.PC->SubmitMouseDelta(FVector2D(2,1),Target));
  if(ChangeView){R.Key(EKeys::W,true);}
  R.Frames(6);
  TestTrue(TEXT("Future command cannot change base early"),R.PC->GetDebugSnapshot().Rod.BaseRodDirectionLocal==Before.Rod.BaseRodDirectionLocal);
  if(ChangeView){R.Key(EKeys::W,false);}
  R.Frames(12);const auto After=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("Due mouse really changes local direction"),!After.Rod.BaseRodDirectionLocal.Equals(Before.Rod.BaseRodDirectionLocal,1.e-8));
  if(!ChangeView){Reference=After.Rod.BaseRodDirectionLocal;}
  else
  {
   TestTrue(TEXT("Camera actually moved after command observation"),!After.FishingCamera.Rotation.Equals(Before.FishingCamera.Rotation,1.e-4));
   TestTrue(TEXT("Consumption uses command camera, never later camera"),After.Rod.BaseRodDirectionLocal.Equals(Reference,1.e-12));
  }
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeShakuriBase,"TipRun.R4A2.Runtime.ShakuriBaseFinal",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeShakuriBase::RunTest(const FString&)
{
 for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;if(!R.Start(*this,Port?TEXT("R4A2_ShakuriPort"):TEXT("R4A2_ShakuriStarboard"),60,Port)){return false;}
  R.Tap(EKeys::Enter);R.Frames(60*12);
  const auto Base=R.PC->GetDebugSnapshot().Rod;bool Moved=false;int ActiveFrames=0;
  R.Key(EKeys::RightMouseButton,true);
  for(int I=0;I<120;++I)
  {
   R.Frame();if(I==2){R.Key(EKeys::RightMouseButton,false);}
   const auto N=R.PC->GetDebugSnapshot().Rod;
   if(N.bShakuriActive){++ActiveFrames;}
   Moved|=!N.FinalRodDirectionLocal.Equals(Base.BaseRodDirectionLocal,1.e-6);
   TestTrue(TEXT("Real Shakuri keeps exact base and grip"),N.RodRootLocal==Base.RodRootLocal && N.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && N.BaseRodTipLocal==Base.BaseRodTipLocal);
   TestTrue(TEXT("Shakuri physical length stays fixed"),FMath::IsNearlyEqual((N.FinalRodTipLocal-N.RodRootLocal).Size(),Base.LengthM,1.e-12));
  }
  const auto End=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("Real profile ran and returned to exact base"),Moved && ActiveFrames>2 && !End.Rod.bShakuriActive && End.Rod.FinalRodDirectionLocal==Base.BaseRodDirectionLocal && End.Rod.FinalRodTipLocal==Base.BaseRodTipLocal);
  if(R.PC->GetBoundSession()->HasResult())
  {AddError(TEXT("R4A4 action ended unexpectedly: ")+R.PC->GetBoundSession()->GetRuntimeDiagnostics());}
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeDeltaAxes,"TipRun.R4A3.Runtime.ProjectionSidesHeadingsResolutions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeDeltaAxes::RunTest(const FString&)
{
 double MaxCross=0,MaxTipError=0,MinLength=DBL_MAX,MaxLength=0;FVector2D Rate1080=FVector2D::ZeroVector;
 for(int Width:{1920,2560})for(bool Port:{true,false})for(double Heading:{0.,90.,180.,270.})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;
  if(!R.Start(*this,FString::Printf(TEXT("R4A3_Axes_%d_%d_%.0f"),Width,Port,Heading),60,Port,Width,Width*9/16,Heading)){return false;}
  // The existing focus contract intentionally discards the first nonzero device sample.
  const auto BoundaryPose=R.PC->GetDebugSnapshot().Rod;R.Mouse(FVector2D(1,0));R.Frames(2);
  TestTrue(TEXT("First relative mouse boundary sample is safely discarded"),R.PC->GetDebugSnapshot().Rod.BaseRodDirectionLocal==BoundaryPose.BaseRodDirectionLocal);
  R.Mouse(FVector2D(-1,0));R.Frames(3);const auto Initial=R.PC->GetDebugSnapshot().Rod;
  const auto Camera=R.Observe().CameraRotation;
  auto Sample=[&](const TCHAR* Label)
  {
   const auto O=R.Observe();const double L=(O.PixelTip-O.PixelRoot).Size();
   MinLength=FMath::Min(MinLength,L*1920/Width);MaxLength=FMath::Max(MaxLength,L*1920/Width);MaxTipError=FMath::Max(MaxTipError,O.TipErrorM);
   TestTrue(TEXT("Actual visual pair finite, fixed length and snapshot aligned"),O.bProjected && FMath::IsNearlyEqual(O.LengthM,R.ConfiguredLengthM,1.e-8) && O.TipErrorM<1.e-7 && O.RootErrorM<1.e-7);
   R.Trace+=FString::Printf(TEXT("%s width=%d heading=%.0f port=%d pixelTip=%s projectedLength=%.9g %s\n"),Label,Width,Heading,Port,*O.PixelTip.ToString(),L,*O.Describe());
  };
  Sample(TEXT("Center"));FVector2D Rate=FVector2D::ZeroVector;
  for(auto Delta:{FVector2D(20,0),FVector2D(-20,0),FVector2D(0,20),FVector2D(0,-20),FVector2D(20,20),FVector2D(-20,-20)})
  {
   for(int I=0;I<8;++I){R.Mouse(FVector2D::ZeroVector);}
   const auto B=R.Observe();R.Mouse(Delta);R.Frames(2);const auto A=R.Observe();
   const FVector2D Move=A.PixelTip-B.PixelTip;
   if(Delta.X==0){MaxCross=FMath::Max(MaxCross,FMath::Abs(Move.X));TestTrue(TEXT("Pure Y goes up/down on actual mesh"),Move.Y*Delta.Y<0);Rate.Y=FMath::Abs(Move.Y)/Width;}
   else if(Delta.Y==0){MaxCross=FMath::Max(MaxCross,FMath::Abs(Move.Y));TestTrue(TEXT("Pure X goes right/left on actual mesh"),Move.X*Delta.X>0);Rate.X=FMath::Abs(Move.X)/Width;}
   else{TestTrue(TEXT("Diagonal changes both axes with the correct signs"),Move.X*Delta.X>0 && Move.Y*Delta.Y<0);}
   Sample(*Delta.ToString());
  }
  if(Heading==0 && Port)
  {if(Width==1920){Rate1080=Rate;}else{TestTrue(TEXT("1080p/1440p real mouse normalized motion agrees"),Rate.Equals(Rate1080,1.e-7));}}
  TestTrue(TEXT("Mouse cannot change actual camera"),R.Observe().CameraRotation.Equals(Camera,1.e-9));
  // Camera-only after a non-default pose, followed by mouse in the new view.
  TestTrue(TEXT("Controller queues a non-default pose"),R.PC->SubmitMouseDelta(FVector2D(8,5)));R.Frames(2);
  const auto Base=R.PC->GetDebugSnapshot().Rod;R.Key(EKeys::D,true);R.Frames(20);R.Key(EKeys::D,false);R.Frames(3);
  TestTrue(TEXT("Look preserves persistent local pose exactly"),R.PC->GetDebugSnapshot().Rod.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && R.PC->GetDebugSnapshot().Rod.BaseRodTipLocal==Base.BaseRodTipLocal && R.PC->GetDebugSnapshot().Rod.RodRootLocal==Initial.RodRootLocal);
  Sample(TEXT("CameraLook"));
  for(FVector2D Delta:{FVector2D(2,0),FVector2D(0,2)})
  {
   const auto B=R.Observe();TestTrue(TEXT("Mouse command in current camera accepted"),R.PC->SubmitMouseDelta(Delta));R.Frames(2);
   const auto A=R.Observe();const FVector2D Move=A.PixelTip-B.PixelTip;
   const int Axis=Delta.X?0:1;MaxCross=FMath::Max(MaxCross,FMath::Abs(Move[1-Axis]));
   TestTrue(TEXT("After camera look mouse uses that view without a first-input jump"),Move[Axis]*(Axis?-1:1)>0 && FMath::Abs(Move[Axis])<Width*.03);
  }
  R.Trace+=R.PC->GetDebugSnapshot().RuntimeDiagnostics;
 }
 AddInfo(FString::Printf(TEXT("A3 actual mesh cross max=%.12g px tolerance=0.05 px; visual error=%.12g m; normalized-to-1080 projected length=%.9g..%.9g ratio=%.9g"),MaxCross,MaxTipError,MinLength,MaxLength,MaxLength/MinLength));
 TestTrue(TEXT("Real Mesh pure X/Y cross axis within 0.05 pixels"),MaxCross<.05);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeDeltaStress,"TipRun.R4A3.Runtime.RepeatedDeltaReach",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeDeltaStress::RunTest(const FString&)
{
 double MaxCross=0,MaxReturn=0,MaxLocal=0,MinReach=DBL_MAX,MinY=DBL_MAX,MaxY=-DBL_MAX,MinLength=DBL_MAX,MaxLength=0;
 for(int Width:{1920,2560})for(bool Port:{true,false})for(double Heading:{0.,90.,180.,270.})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R4A3_Stress_%d_%d_%.0f"),Width,Port,Heading),60,Port,Width,Width*9/16,Heading)){return false;}
  const auto Sensitivity=R.World->GetAuthGameMode<ATRGameModeBase>()->SessionConfig->Rod->Parameters.Screen.Sensitivity;
  auto Send=[&](FVector2D D){if(!R.PC->SubmitMouseDelta(D)){AddError(TEXT("Controller stress command rejected before queue"));}R.Frames(2);};
  const auto Initial=R.PC->GetDebugSnapshot().Rod;const auto Start=R.Observe();
  auto Measure=[&](const TCHAR* Label)
  {
   const auto O=R.Observe();const double Pixels=(O.PixelTip-O.PixelRoot).Size();
   MinLength=FMath::Min(MinLength,Pixels*1920/Width);MaxLength=FMath::Max(MaxLength,Pixels*1920/Width);
   const auto Text=FString::Printf(TEXT("REACH POSE %s width=%d port=%d heading=%.0f projectedLength=%.9g pixels=%s\n"),Label,Width,Port,Heading,Pixels,*O.PixelTip.ToString());R.Trace+=Text;AddInfo(Text);
  };
  Measure(TEXT("Center"));
  for(int Axis=0;Axis<2;++Axis)
  {
   const auto Before=R.Observe();
   for(int I=0;I<2000;++I)
   {
    FVector2D D=FVector2D::ZeroVector;D[Axis]=2;
    const auto B=R.Observe();Send(D);const auto A=R.Observe();Send(-D);const auto End=R.Observe();
    MaxCross=FMath::Max(MaxCross,FMath::Max(FMath::Abs((A.PixelTip-B.PixelTip)[1-Axis]),FMath::Abs((End.PixelTip-A.PixelTip)[1-Axis])));
    if(!End.bProjected || End.TipErrorM>1.e-7 || !FMath::IsNearlyEqual(End.LengthM,R.ConfiguredLengthM,1.e-8)){AddError(TEXT("Runtime stress visual/length became invalid"));return false;}
   }
   MaxReturn=FMath::Max(MaxReturn,(R.Observe().PixelTip-Before.PixelTip).Size());
  }
  MaxLocal=FMath::Max(MaxLocal,(R.PC->GetDebugSnapshot().Rod.BaseRodDirectionLocal-Initial.BaseRodDirectionLocal).Size());
  // Probe both horizontal boundaries through actual controller/queue/world/mesh.
  for(int I=0;I<100;++I){Send(FVector2D(-10,0));}const double Left=R.Observe().PixelTip.X;Measure(TEXT("Left"));
  for(int I=0;I<200;++I){Send(FVector2D(10,0));}const double Right=R.Observe().PixelTip.X;Measure(TEXT("Right"));
  const double Reach=(Right-Left)/Width;MinReach=FMath::Min(MinReach,Reach);
  for(int I=0;I<100;++I){Send(FVector2D(0,10));}const auto Upper=R.Observe();Measure(TEXT("RightUpDiagonal"));
  for(int I=0;I<200;++I){Send(FVector2D(0,-10));}const auto Lower=R.Observe();Measure(TEXT("RightDownDiagonal"));
  MinY=FMath::Min(MinY,Upper.PixelTip.Y/Width);MaxY=FMath::Max(MaxY,Lower.PixelTip.Y/Width);
  TestTrue(TEXT("Vertical axis still moves at the horizontal screen boundary"),Lower.PixelTip.Y-Upper.PixelTip.Y>Width*.15);
  const auto Boundary=R.PC->GetDebugSnapshot().Rod;Send(FVector2D(0,-100));
  const auto Limited=R.PC->GetDebugSnapshot().Rod;
  TestTrue(TEXT("Outside request stays finite on diagnosed valid ergonomic boundary"),!Limited.BaseRodDirectionLocal.ContainsNaN() && Limited.AimResult!=TEXT("Valid") && (Limited.bEnvelopeEnabled?Limited.bEnvelopeLimited:Limited.BaseRodDirectionLocal==Boundary.BaseRodDirectionLocal));
  AddInfo(FString::Printf(TEXT("A3 reach width=%d port=%d left=%.6f right=%.6f fraction=%.9g upperY=%.6f lowerY=%.6f reason=%s"),Width,Port,Left,Right,Reach,Upper.PixelTip.Y,Lower.PixelTip.Y,*Boundary.AimResult));
  // Also sample vertical reach about screen center via input, never by resetting the pose.
  for(int I=0;I<80;++I){const auto O=R.Observe();Send(FVector2D((Width*.5-O.PixelTip.X)/(Width*.5*Sensitivity.X),0));}
  for(int I=0;I<100;++I){Send(FVector2D(0,10));}Measure(TEXT("CenterUp"));
  for(int I=0;I<200;++I){Send(FVector2D(0,-10));}Measure(TEXT("CenterDown"));
  const auto CameraBase=R.PC->GetDebugSnapshot().Rod;R.Key(EKeys::D,true);R.Frames(20);R.Key(EKeys::D,false);R.Frames(3);Measure(TEXT("CameraLookAfterWidePose"));
  TestTrue(TEXT("Camera after wide reach still preserves local base"),R.PC->GetDebugSnapshot().Rod.BaseRodDirectionLocal==CameraBase.BaseRodDirectionLocal);
  R.Trace+=FString::Printf(TEXT("stress 2000 roundtrips per axis root=%s dir=%s projected=%s length=%.12g\n"),*Initial.RodRootLocal.ToString(),*Initial.BaseRodDirectionLocal.ToString(),*Start.PixelTip.ToString(),Initial.LengthM);
 }
 AddInfo(FString::Printf(TEXT("A3 stress maxCross=%.12g px maxReturn=%.12g px maxLocalDrift=%.12g minHorizontalFraction=%.9g vertical1080Pixels=%.6f..%.6f"),MaxCross,MaxReturn,MaxLocal,MinReach,MinY*1920,MaxY*1920));
 AddInfo(FString::Printf(TEXT("A3 wide reach projected length 1080-equivalent min=%.9g max=%.9g ratio=%.9g (measurement, no perceptual acceptance)"),MinLength,MaxLength,MaxLength/MinLength));
 TestTrue(TEXT("Cross axis and 2000 roundtrip return within 0.05px"),MaxCross<.05 && MaxReturn<.05);
 TestTrue(TEXT("No accumulated station local direction drift"),MaxLocal<1.e-9);
 TestTrue(TEXT("Reach measured, useful motion exists without forcing a screen percentage"),MinReach>0);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeDeltaSequence,"TipRun.R4A3.Runtime.SequenceAndDeterminism",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeDeltaSequence::RunTest(const FString&)
{
 FVector Reference=FVector::ZeroVector;
 for(int FPS:{30,60,120})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R4A3_Order_%d"),FPS),FPS)){return false;}
  // Keep initial camera/geometry equal; every scheduled command uses its own actual POV.
  auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
  const int64 Target=Sim->GetSimulationTime().TickIndex+120;
  TArray<FTRFishingCommand> Consumed;
  const auto Handle=R.PC->GetBoundSession()->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result)
  {if(C.Type==ETRFishingCommandType::RodAim && Result==ETRCommandResult::Accepted){Consumed.Add(C);}});
  TestTrue(TEXT("First captured command queues"),R.PC->SubmitMouseDelta(FVector2D(1,0),Target));
  R.Key(EKeys::D,true);R.Frames(FPS/3);R.Key(EKeys::D,false);R.Frames(2);
  TestTrue(TEXT("Second separately captured command queues at the same tick"),R.PC->SubmitMouseDelta(FVector2D(0,1),Target));
  R.Frames(FPS*3);const auto A=R.PC->GetDebugSnapshot().Rod;
  R.PC->GetBoundSession()->OnCommandProcessed.Remove(Handle);
  TestEqual(TEXT("Both same-tick mouse commands consumed once"),Consumed.Num(),2);
  if(Consumed.Num()==2)
  {
   TestTrue(TEXT("Each sequence keeps its own camera, not the last observation"),Consumed[0].Sequence<Consumed[1].Sequence && Consumed[0].TargetTick==Consumed[1].TargetTick && Consumed[0].RodView.CameraFrame<Consumed[1].RodView.CameraFrame && !Consumed[0].RodView.CameraRotation.Equals(Consumed[1].RodView.CameraRotation,1.e-3) && A.AimSequence==Consumed[1].Sequence && A.AimCameraFrame==Consumed[1].RodView.CameraFrame);
   AddInfo(FString::Printf(TEXT("Same tick fps=%d sequences=%lld,%lld cameraFrames=%lld,%lld rotations=%s / %s"),FPS,Consumed[0].Sequence,Consumed[1].Sequence,Consumed[0].RodView.CameraFrame,Consumed[1].RodView.CameraFrame,*Consumed[0].RodView.CameraRotation.ToString(),*Consumed[1].RodView.CameraRotation.ToString()));
  }
  TestTrue(TEXT("Last sequence and second camera are observable"),A.AimSequence>0 && A.AimCameraFrame>0 && A.AimResult==TEXT("Valid"));
  if(FPS==30){Reference=A.BaseRodDirectionLocal;}else{TestTrue(TEXT("30/60/120 fixed result identical for same ordered view observations"),A.BaseRodDirectionLocal.Equals(Reference,1.e-9));}
  R.Trace+=R.PC->GetDebugSnapshot().RuntimeDiagnostics;
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeDeltaActions,"TipRun.R4A3.Runtime.ActionPoseAndGuards",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeDeltaActions::RunTest(const FString&)
{
 for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,Port?TEXT("R4A3_ActionPort"):TEXT("R4A3_ActionStarboard"),60,Port)){return false;}
  TestTrue(TEXT("Non-default mouse pose queues"),R.PC->SubmitMouseDelta(FVector2D(4,1)));R.Frames(2);
  const auto Base=R.PC->GetDebugSnapshot().Rod;
  for(int Guard=0;Guard<3;++Guard)
  {
   if(Guard==0){R.PC->SetPauseRequested(true);}else if(Guard==1){R.PC->SetInputFocus(false);}else{R.PC->SetPrototypePanelOpen(true);}
   TestFalse(TEXT("Pause/focus/UI block rod mouse"),R.PC->SubmitMouseDelta(FVector2D(2,1)));
   TestFalse(TEXT("Pause/focus/UI block Shakuri"),R.PC->ActionStarted(ETRPlayerAction::Jerk));
   TestFalse(TEXT("Pause/focus/UI block Retrieve"),R.PC->ActionStarted(ETRPlayerAction::Retrieve));R.Frames(3);
   TestTrue(TEXT("Guard keeps permanent pose"),R.PC->GetDebugSnapshot().Rod.BaseRodDirectionLocal==Base.BaseRodDirectionLocal);
   if(Guard==0){R.PC->SetPauseRequested(false);}else if(Guard==1){R.PC->SetInputFocus(true);}else{R.PC->SetPrototypePanelOpen(false);}R.Frames(3);
   // Pair the direct guard API probes with releases after focus/pause restoration.
   // Otherwise the test itself leaves an intentionally blocked synthetic hold.
   R.PC->ActionReleased(ETRPlayerAction::Jerk);R.PC->ActionReleased(ETRPlayerAction::Retrieve);
  }
  R.Tap(EKeys::Enter);R.Frames(60*12);R.Key(EKeys::RightMouseButton,true);bool Moved=false;
  for(int I=0;I<120;++I)
  {
   R.Frame();const auto N=R.PC->GetDebugSnapshot();
   Moved|=!N.Rod.FinalRodDirectionLocal.Equals(Base.BaseRodDirectionLocal,1.e-6);
   TestTrue(TEXT("Mouse-based action keeps exact grip/base/physical length"),N.Rod.RodRootLocal==Base.RodRootLocal && N.Rod.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && FMath::IsNearlyEqual((N.Rod.FinalRodTipLocal-N.Rod.RodRootLocal).Size(),Base.LengthM,1.e-9));
   if(R.Visual->LineVisual->IsVisible())
   {
    const auto Box=R.Visual->LineVisual->GetStaticMesh()->GetBoundingBox();
    const FVector Start=R.Visual->LineVisual->GetComponentTransform().TransformPosition(FVector(Box.Min.X,0,0))*.01;
    TestTrue(TEXT("Actual line start is the same final tip as the actual rod mesh"),Start.Equals(R.Observe().VisualTipM,1.e-7));
   }
  }
  R.Key(EKeys::RightMouseButton,false);R.Frames(2);const auto End=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("Right hold produces exactly one Shakuri and returns to mouse base"),Moved && End.Egi.JerkCount==1 && !End.Rod.bShakuriActive && End.Rod.FinalRodDirectionLocal==Base.BaseRodDirectionLocal);
  if(R.PC->GetBoundSession()->HasResult()){AddError(TEXT("A4 evidence after mouse pose: ")+R.PC->GetBoundSession()->GetRuntimeDiagnostics());continue;}
  R.Key(EKeys::LeftMouseButton,true);R.Frames(3);TestTrue(TEXT("Left hold still retrieves"),R.PC->GetDebugSnapshot().Retrieval.bIsRetrieving);
  R.Key(EKeys::LeftMouseButton,false);R.Frames(3);TestFalse(TEXT("Left release stops normal winding"),R.PC->GetDebugSnapshot().Retrieval.bIsRetrieving);
  R.Tap(EKeys::Q);R.Frames(180);const auto Ready=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("Quick returns Ready/unlocked without altering mouse base/grip/length"),R.PC->GetBoundSession()->CanChangeEquipment() && Ready.Rod.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && Ready.Rod.RodRootLocal==Base.RodRootLocal && Ready.Rod.LengthM==Base.LengthM);
 }
 return true;
}

// R4A-4: unchanged saved runtime, action boundaries and actual safety/recovery.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimePending,"TipRun.R4A4.Runtime.PendingRetrieveReleaseContinuity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimePending::RunTest(const FString&)
{
 for(int FPS:{30,60,120})for(bool ReleaseEarly:{false,true})
 {
  FRuntimeRodWorld R;if(!R.Start(*this,FString::Printf(TEXT("R4A4_Pending_%d_%d"),FPS,ReleaseEarly),FPS)){return false;}
  auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
  R.Tap(EKeys::Enter);R.Frames(FPS*3);const auto Base=R.PC->GetDebugSnapshot().Rod;
  const int64 Start=Sim->GetSimulationTime().TickIndex+6;
  S->SubmitCommand(ETRFishingCommandType::Jerk,S->GetCastId(),Start);
  S->SubmitCommand(ETRFishingCommandType::RetrieveStarted,S->GetCastId(),Start+5);
  if(ReleaseEarly){S->SubmitCommand(ETRFishingCommandType::RetrieveStopped,S->GetCastId(),Start+8);}
  int64 PendingTick=-1,RetrieveTick=-1;double MaxTipDelta=0,MaxTipSpeed=0;
  const auto StateHandle=S->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState,ETRFishingState To)
  {if(To==ETRFishingState::Retrieving && RetrieveTick<0){RetrieveTick=Sim->GetSimulationTime().TickIndex;const auto Rod=S->RodControl->GetSnapshot();TestTrue(TEXT("Retrieve begins at exact Final==Base"),Rod.FinalRodTipLocal==Rod.BaseRodTipLocal);}});
  const auto Handle=S->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result)
  {if(C.Type==ETRFishingCommandType::RetrieveStarted){TestTrue(TEXT("Existing transient reports Pending, not lost"),Result==ETRCommandResult::Pending);}});
  auto Previous=Base;
  for(int I=0;I<FPS*2;++I)
  {
   R.Frame();const auto Now=R.PC->GetDebugSnapshot().Rod;
   MaxTipDelta=FMath::Max(MaxTipDelta,(Now.TipWorldPositionM-Previous.TipWorldPositionM).Size());
   if(Now.Tick>Previous.Tick){MaxTipSpeed=FMath::Max(MaxTipSpeed,(Now.TipWorldPositionM-Previous.TipWorldPositionM).Size()/((Now.Tick-Previous.Tick)/60.));}
   TestTrue(TEXT("Action boundaries keep fixed grip, base and length"),Now.RodRootLocal==Base.RodRootLocal && Now.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && Now.LengthM==Base.LengthM);
   if(S->Fishing->HasPendingRetrieve()){PendingTick=Now.Tick;TestTrue(TEXT("Pending keeps Shakuri state"),S->Fishing->GetState()==ETRFishingState::Jerking);}

   Previous=Now;
  }
  S->OnCommandProcessed.Remove(Handle);S->Fishing->OnFishingStateChanged.Remove(StateHandle);
  TestTrue(TEXT("Pending request was observed"),PendingTick>=Start+5);
  TestTrue(TEXT("No legal-action technical Abort"),!S->HasResult() || S->GetLastResult().Outcome!=ETRCastOutcome::Aborted);
  TestTrue(TEXT("Release cancels pending; holding begins after fixed profile end"),ReleaseEarly?RetrieveTick<0:RetrieveTick==Start+S->RodControl->GetJerkTicks());
  AddInfo(FString::Printf(TEXT("Continuity fps=%d release=%d maxFrameDelta=%.12g maxSampleSpeed=%.12g retrieveTick=%lld"),FPS,ReleaseEarly,MaxTipDelta,MaxTipSpeed,RetrieveTick));
  S->SubmitCommand(ETRFishingCommandType::RetrieveStopped,S->GetCastId());R.Frames(2);
  if(S->HasResult()) { TestTrue(TEXT("Normal completion retains Result until NextCast"),S->GetLastResult().Outcome==ETRCastOutcome::Retrieved && !S->GetLastResult().bQuickRetrieved && S->Fishing->GetState()==ETRFishingState::Result);R.Tap(EKeys::N); }
  else { S->SubmitCommand(ETRFishingCommandType::QuickRetrieve,S->GetCastId());R.Frames(FPS*3); }
  TestTrue(TEXT("Release and Quick remain Ready/Onboard/Unlocked"),S->CanChangeEquipment() && S->IsEgiOnboard());
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeRecovery,"TipRun.R4A4.Runtime.IntentionalAbortRecoveryAndStale",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeRecovery::RunTest(const FString&)
{
 for(int FPS:{30,60,120})for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;if(!R.Start(*this,FString::Printf(TEXT("R4A4_Recovery_%d_%d"),FPS,Port),FPS,Port)){return false;}
  auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
  R.Mouse(FVector2D(0,0));R.Mouse(FVector2D(2,1));R.Frames(3);R.Tap(EKeys::Enter);R.Frames(FPS);
  const auto Before=R.PC->GetDebugSnapshot();const auto Registration=S->GetRegistrationId();const auto Cast=S->GetCastId();const int64 Epoch=Before.PlayerMode.ModeEpoch;
  const int64 Future=Sim->GetSimulationTime().TickIndex+240;
  for(auto C:{ETRFishingCommandType::RetrieveStarted,ETRFishingCommandType::Jerk,ETRFishingCommandType::RodAim})
  {TestTrue(TEXT("Old registration future input queued before Abort"),Sim->EnqueueCommand(Registration,C,0,Future,Cast,FVector2D(1,1),Epoch));}
  int Ends=0;const auto EndHandle=S->OnCastCompleted.AddLambda([&](const FTRCatchResult&){++Ends;});
  R.Key(EKeys::RightMouseButton,true);R.Frames(2);R.Key(EKeys::RightMouseButton,false);
  R.Key(EKeys::LeftMouseButton,true);R.Frames(2);
  TestTrue(TEXT("Intentional fail interrupts a real Pending Retrieve"),S->Fishing->HasPendingRetrieve());
  // Intentional corruption, never a legitimate input nor a relaxed safety threshold.
  S->EgiSimulation->MotionVelocityMps=FVector(std::numeric_limits<double>::quiet_NaN(),0,0);
  R.Frames(2);
  TestTrue(TEXT("Real safety failure stays Aborted/offboard/locked"),S->HasResult() && S->GetLastResult().Outcome==ETRCastOutcome::Aborted && !S->IsEgiOnboard() && S->IsEquipmentLocked());
  TestTrue(TEXT("Specific NonFinite reason retained"),S->DiagnosticLastFailure.Contains(TEXT("NonFinite")));
  TestTrue(TEXT("Explicit recovery is offered"),S->IsRecoveryAvailable() && R.PC->GetDebugSnapshot().bRecoveryAvailable);
  TestTrue(TEXT("Japanese recovery operation visible"),UTRFishingHUDWidget::BuildCompactGuide(R.PC->GetDebugSnapshot()).ToString().Contains(TEXT("N：安全復旧")));
  TestTrue(TEXT("Abort invalidates old registration reservations"),Sim->GetDiagnosticQueue(Registration).StartsWith(TEXT("0 ")));
  const FString CleanupQueue=Sim->GetDiagnosticQueue(S->GetRegistrationId());
  TestTrue(TEXT("Only new-generation safety Release may remain, never old action reservations"),!CleanupQueue.Contains(TEXT("RetrieveStarted")) && !CleanupQueue.Contains(TEXT("Jerk")) && !CleanupQueue.Contains(TEXT("RodAim")));
  AddInfo(TEXT("Abort cleanup queue (current generation): ")+CleanupQueue);
  TestFalse(TEXT("Old registration cannot receive new input"),Sim->EnqueueCommand(Registration,ETRFishingCommandType::RetrieveStarted,0,-1,Cast,{},Epoch));
  const auto Terminal=S->GetLastResult();const auto RecoveryRegistration=S->GetRegistrationId();
  const auto BeforeRecovery=R.PC->GetDebugSnapshot();bool SawRecoveryCommand=false;
  const auto ResetHandle=S->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result)
  {if(C.Type==ETRFishingCommandType::NextCast){SawRecoveryCommand=true;const auto B=S->GetHUDSnapshot().Boat;
    TestTrue(TEXT("Recovery boundary does not reset Boat position/heading/velocity"),Result==ETRCommandResult::Accepted && B.PositionM==BeforeRecovery.Boat.PositionM && B.HeadingRad==BeforeRecovery.Boat.HeadingRad && B.VelocityMps==BeforeRecovery.Boat.VelocityMps);}});
  TestTrue(TEXT("Controller queues recovery, never writes state directly"),R.PC->ActionStarted(ETRPlayerAction::NextCast));R.Frame();R.PC->ActionReleased(ETRPlayerAction::NextCast);R.Frames(3);
  S->OnCommandProcessed.Remove(ResetHandle);TestTrue(TEXT("Recovery observed through real fixed command"),SawRecoveryCommand);
  R.Key(EKeys::LeftMouseButton,false);R.Frames(2);const auto Ready=R.PC->GetDebugSnapshot();
  TestTrue(TEXT("Recovery explicitly reaches Ready without rewriting Abort"),Ready.Egi.FishingState==ETRFishingState::Ready && S->GetLastResult().Outcome==ETRCastOutcome::Aborted && S->GetLastResult().CastId==Terminal.CastId && S->IsEgiOnboard() && S->CanChangeEquipment() && !S->GetCastId().IsValid());
  TestTrue(TEXT("Recovery leaves Side/Mode/Camera/Base/Length unchanged"),Ready.PlayerMode.Mode==ETRPlayerMode::Fishing && Ready.PlayerMode.FishingSide==Before.PlayerMode.FishingSide && Ready.FishingCamera.Rotation.Equals(Before.FishingCamera.Rotation,1.e-8) && (Ready.FishingCamera.PositionM-Ready.Boat.PositionM).Equals(Before.FishingCamera.PositionM-Before.Boat.PositionM,1.e-8) && Ready.Rod.RodRootLocal==Before.Rod.RodRootLocal && Ready.Rod.BaseRodDirectionLocal==Before.Rod.BaseRodDirectionLocal && Ready.Rod.LengthM==Before.Rod.LengthM);
  TestTrue(TEXT("Recovery clears transient/held/pending"),Ready.Egi.PendingJerkCount==0 && !S->Fishing->HasPendingRetrieve() && Ready.Rod.TemporaryShakuriOffsetRad==0 && !Ready.Rod.bShakuriActive && Ready.RuntimeDiagnostics.Contains(TEXT("RetrieveHeld=0")));
  TestEqual(TEXT("Recovery never emits a second terminal result"),Ends,1);
  for(auto C:{ETRFishingCommandType::RetrieveStarted,ETRFishingCommandType::Jerk,ETRFishingCommandType::RodAim})
  {
   TestTrue(TEXT("Current registration can queue an old identity for rejection"),Sim->EnqueueCommand(S->GetRegistrationId(),C,0,-1,Cast,FVector2D(1,1),Epoch));R.Frames(2);
   TestTrue(TEXT("Old Cast cannot mutate recovered Ready"),S->GetLastCommandResult()==ETRCommandResult::RejectedInvalidState && S->Fishing->GetState()==ETRFishingState::Ready);
   TestTrue(TEXT("Current identity wrong ModeEpoch queued for rejection"),Sim->EnqueueCommand(S->GetRegistrationId(),C,0,-1,S->GetCastId(),{},Epoch-1));R.Frames(2);
   TestTrue(TEXT("Wrong ModeEpoch rejected"),S->GetLastCommandResult()==ETRCommandResult::RejectedInvalidState);
  }
  TestFalse(TEXT("Recovery generation invalidated"),Sim->EnqueueCommand(RecoveryRegistration,ETRFishingCommandType::Jerk,0,-1,Cast,{},Epoch));
  R.Tap(EKeys::Enter);TestTrue(TEXT("Fresh Deploy relocks a newer Cast"),S->GetCastId().Value>Cast.Value && S->IsEquipmentLocked());
  for(auto C:{ETRFishingCommandType::RetrieveStarted,ETRFishingCommandType::Jerk,ETRFishingCommandType::RodAim})
  {Sim->EnqueueCommand(S->GetRegistrationId(),C,0,-1,Cast,FVector2D(1,1),Epoch);R.Frames(2);TestTrue(TEXT("Old cast cannot reach new cast"),S->GetLastCommandResult()==ETRCommandResult::RejectedInvalidState);}
  R.Tap(EKeys::Q);R.Frames(FPS*3);TestTrue(TEXT("Quick after recovery still uses normal Ready contract"),S->CanChangeEquipment() && S->GetLastResult().bQuickRetrieved && S->GetLastResult().Outcome==ETRCastOutcome::Retrieved);
  S->OnCastCompleted.Remove(EndHandle);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeActionReplay,"TipRun.R4A4.Runtime.ActionReplayPauseFocus",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeActionReplay::RunTest(const FString&)
{
 TArray<FString> Reference;
 for(int FPS:{30,60,120})
 {
  FRuntimeRodWorld R;if(!R.Start(*this,FString::Printf(TEXT("R4A4_Replay_%d"),FPS),FPS)){return false;}
  auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
  R.Tap(EKeys::Enter);R.Frames(4);
  const int64 Start=Sim->GetSimulationTime().TickIndex+60;TArray<FString> Events;
  const auto Handle=S->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result)
  {Events.Add(FString::Printf(TEXT("%lld:%d:%d"),C.TargetTick-Start,int32(C.Type),int32(Result)));});
  const auto StateHandle=S->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState From,ETRFishingState To)
  {Events.Add(FString::Printf(TEXT("state:%lld:%d:%d"),Sim->GetSimulationTime().TickIndex-Start,int32(From),int32(To)));});
    for(const auto& A:TArray<TPair<int64,ETRFishingCommandType>>{
   {1,ETRFishingCommandType::Jerk},{5,ETRFishingCommandType::RetrieveStarted},{8,ETRFishingCommandType::RetrieveStopped},
   {50,ETRFishingCommandType::Jerk},{55,ETRFishingCommandType::RetrieveStarted},{95,ETRFishingCommandType::RetrieveStopped},
   {110,ETRFishingCommandType::QuickRetrieve},{110,ETRFishingCommandType::Jerk}})
  {S->SubmitCommand(A.Value,FTRCastId(1),Start+A.Key);}
  while(Sim->GetSimulationTime().TickIndex<Start+260){R.Frame();}
  S->OnCommandProcessed.Remove(Handle);S->Fishing->OnFishingStateChanged.Remove(StateHandle);
  if(Reference.IsEmpty()){Reference=Events;}else{TestTrue(TEXT("30/60/120 exact command and state transition Tick/Sequence"),Events==Reference);}
  TestTrue(TEXT("Replay ends successful normal Result or Quick Ready, never Abort"),S->HasResult() && S->GetLastResult().Outcome==ETRCastOutcome::Retrieved &&
    (S->GetLastResult().bQuickRetrieved ? S->CanChangeEquipment() : S->Fishing->GetState()==ETRFishingState::Result));
  if(S->Fishing->GetState()==ETRFishingState::Result){R.Tap(EKeys::N);}
  R.Trace+=TEXT("Replay:\n")+FString::Join(Events,TEXT("\n"));
  // Safety stops erase only reservations/held retrieve, not a running rod profile.
  R.Tap(EKeys::Enter);R.Frames(FPS*4);R.Key(EKeys::RightMouseButton,true);R.Frames(2);R.Key(EKeys::RightMouseButton,false);
  R.Key(EKeys::LeftMouseButton,true);R.Frames(2);
  TestTrue(TEXT("Actual held Retrieve queues pending in Shakuri"),S->Fishing->HasPendingRetrieve());
  R.PC->SetPauseRequested(true);const auto Paused=R.PC->GetDebugSnapshot().Rod;const auto PauseTick=Sim->GetSimulationTime().TickIndex;R.Frames(FPS);
  TestTrue(TEXT("Pause cannot advance action/rod pose"),Sim->GetSimulationTime().TickIndex==PauseTick && S->RodControl->GetSnapshot().FinalRodTipLocal==Paused.FinalRodTipLocal);
  R.PC->SetPauseRequested(false);R.PC->SetInputFocus(false);R.Frames(2);R.Key(EKeys::LeftMouseButton,false);R.PC->SetInputFocus(true);R.PC->ActionReleased(ETRPlayerAction::Retrieve);R.Frames(FPS*2);
  TestTrue(TEXT("Pause/Focus leave no pending or held retrieve and no Abort"),!S->Fishing->HasPendingRetrieve() && S->Fishing->GetState()!=ETRFishingState::Retrieving && (!S->HasResult() || S->GetLastResult().Outcome!=ETRCastOutcome::Aborted) && R.PC->GetDebugSnapshot().RuntimeDiagnostics.Contains(TEXT("RetrieveHeld=0")));
  TestTrue(TEXT("Transient completes exact Base after safety input stop"),S->RodControl->GetSnapshot().FinalRodTipLocal==S->RodControl->GetSnapshot().BaseRodTipLocal);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeEnvelopeSaved,"TipRun.R4A5.Runtime.SavedEnvelope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeEnvelopeSaved::RunTest(const FString&)
{
 auto* C=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));
 if(!TestNotNull(TEXT("Saved Prototype config and rod"),C) || !TestNotNull(TEXT("Saved Rod Asset"),C->Rod.Get())){return false;}
 if(FParse::Param(FCommandLine::Get(),TEXT("TRMigrateR4A5")))
 {
  C->Rod->Parameters.Envelope=FTRRodEnvelopeParameters();C->Rod->Parameters.Envelope.bEnabled=true;
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  TestTrue(TEXT("Explicit Prototype envelope saved by UE"),UPackage::SavePackage(C->Rod->GetOutermost(),C->Rod,*FPackageName::LongPackageNameToFilename(C->Rod->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
 }
 TArray<FText> Errors;TestTrue(TEXT("Saved ergonomic tuning validates and is opt-in"),C->Rod->Parameters.Envelope.bEnabled && C->Rod->Parameters.Validate(Errors));
 TestEqual(TEXT("Physical rod still two meters"),C->Rod->Parameters.LengthM,2.);
 TestEqual(TEXT("Fishing FOV remains 80 degrees"),C->FishingStations->Parameters.FOV,80.);
 for(int I=0;I<5;++I)
 {auto Bad=C->Rod->Parameters.Envelope;if(I==0){Bad.MinYawDeg=Bad.MaxYawDeg;}if(I==1){Bad.MaxPitchDeg=90;}if(I==2){Bad.MinPitchDeg=std::numeric_limits<double>::quiet_NaN();}if(I==3){Bad.MaxYawDeg=120;}if(I==4){Bad.MinPitchDeg=Bad.MaxPitchDeg;}TestFalse(TEXT("Invalid envelope rejected"),Bad.Validate());}
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeEnvelopeSweep,"TipRun.R4A5.Runtime.ErgonomicSweep",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeEnvelopeSweep::RunTest(const FString&)
{
 double MinL=DBL_MAX,MaxL=0,MaxError=0,MaxCross=0,MinReachX=1,MinReachY=1,MinSeparation=180;
 FString Csv=TEXT("width,side,heading,cameraYaw,cameraPitch,pose,x,y,lengthPx,physicalM,visualErrorM,clamp\n");
 for(int Width:{1920,2560})for(bool Port:{true,false})for(double Heading:{0.,90.,180.,270.})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R4A5_Sweep_%d_%d_%.0f"),Width,Port,Heading),60,Port,Width,Width*9/16,Heading)){return false;}
  UStaticMeshComponent* ReelMesh=nullptr;TInlineComponentArray<UStaticMeshComponent*> Components(R.Visual);
  for(auto* Mesh:Components){if(Mesh->GetFName()==TEXT("ReelObservation")){ReelMesh=Mesh;break;}}
  if(!TestNotNull(TEXT("Actual private reel mesh discovered from saved visual actor"),ReelMesh)){return false;}
  const auto Initial=R.PC->GetDebugSnapshot().Rod;
  const auto P=R.World->GetAuthGameMode<ATRGameModeBase>()->SessionConfig->Rod->Parameters;
  auto Send=[&](FVector2D D){TestTrue(TEXT("Ergonomic mouse uses Controller queue"),R.PC->SubmitMouseDelta(D));R.Frames(2);};
  auto Look=[&](double Yaw,double Pitch)
  {
   const auto Before=R.PC->GetDebugSnapshot().Rod;
   for(int Axis=0;Axis<2;++Axis)
   {
    const auto S=R.PC->GetDebugSnapshot().FishingCamera;const double Difference=(Axis?Pitch-S.PitchDeg:Yaw-S.YawDeg);
    const FKey K=Axis?(Difference>0?EKeys::W:EKeys::S):(Difference>0?EKeys::D:EKeys::A);
    const auto CameraTuning=R.World->GetAuthGameMode<ATRGameModeBase>()->SessionConfig->FishingStations->Parameters;
    const double Rate=Axis?CameraTuning.FishingCameraPitchRateDegPerS:CameraTuning.FishingCameraYawRateDegPerS;R.Key(K,true);R.Frames(FMath::RoundToInt(FMath::Abs(Difference)*60/Rate));R.Key(K,false);R.Frames(3);
   }
   const auto After=R.PC->GetDebugSnapshot().Rod;
   TestTrue(TEXT("Camera-only exact local root/base/tip/length invariance"),Before.RodRootLocal==After.RodRootLocal && Before.BaseRodDirectionLocal==After.BaseRodDirectionLocal && Before.BaseRodTipLocal==After.BaseRodTipLocal && Before.LengthM==After.LengthM);
  };
  auto HomePose=[&]()
  {
   for(int I=0;I<70;++I)
   {
    const auto S=R.PC->GetDebugSnapshot();const auto O=R.Observe();
    const FQuat Basis(FVector::UpVector,FMath::DegreesToRadians(S.Station.FacingWorldDeg));
    const FVector Tip=S.Rod.RootWorldPositionM+Basis.RotateVector(Initial.BaseRodDirectionLocal)*P.LengthM;
    FVector2D Goal;R.PC->ProjectWorldLocationToScreen(Tip*100,Goal);
    const FVector2D Difference=Goal-O.PixelTip;if(Difference.Size()<.002){break;}
    Send(FVector2D(Difference.X,-Difference.Y)/(Width*.5*P.Screen.Sensitivity.X));
   }
  };
  for(FVector2D Camera:{FVector2D(0,-20),FVector2D(-15,-20),FVector2D(15,-20),FVector2D(0,-10),FVector2D(0,-30)})
  {
   Look(Camera.X,Camera.Y);HomePose();const auto Center=R.Observe();
   double Left=DBL_MAX,Right=-DBL_MAX,Top=DBL_MAX,Bottom=-DBL_MAX;
   auto Sample=[&](const TCHAR* Label)
   {
    const auto S=R.PC->GetDebugSnapshot();const auto O=R.Observe();
    const double L=(O.PixelTip-O.PixelRoot).Size();MinL=FMath::Min(MinL,L*1920/Width);MaxL=FMath::Max(MaxL,L*1920/Width);MaxError=FMath::Max(MaxError,FMath::Max(O.RootErrorM,O.TipErrorM));
    TestTrue(TEXT("Real active camera and unchanged physical mesh pair"),O.bProjected && O.FOV==80 && FMath::IsNearlyEqual(O.LengthM,2.,1.e-8) && O.RootErrorM<1.e-7 && O.TipErrorM<1.e-7);
    TestTrue(TEXT("Fixed station grip through all mouse targets"),S.Rod.RodRootLocal==Initial.RodRootLocal);
    const FVector D=S.Rod.BaseRodDirectionLocal;const double Yaw=FMath::RadiansToDegrees(FMath::Atan2(D.Y,D.X));const double Pitch=FMath::RadiansToDegrees(FMath::Asin(D.Z));
    TestTrue(TEXT("Direction is in station ergonomic envelope, not a screen rectangle"),Yaw>=P.Envelope.MinYawDeg-1.e-8 && Yaw<=P.Envelope.MaxYawDeg+1.e-8 && Pitch>=P.Envelope.MinPitchDeg-1.e-8 && Pitch<=P.Envelope.MaxPitchDeg+1.e-8);
    // Perspective collapse occurs when eye, grip and tip are nearly collinear.
    const FQuat B(FVector::UpVector,FMath::DegreesToRadians(S.Station.FacingWorldDeg));
    const FVector EyeRay=B.UnrotateVector(S.Rod.RootWorldPositionM-S.FishingCamera.PositionM).GetSafeNormal();
    const double Separation=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(D,EyeRay),-1.,1.)));MinSeparation=FMath::Min(MinSeparation,Separation);
    TestTrue(TEXT("Avoid eye-grip-ray foreshortening without changing length/FOV"),Separation>25);
    TestTrue(TEXT("Prototype visible length budget: at least ten percent of 1080-height in representative views"),L/(Width*9./16)>.10);
    TestTrue(TEXT("Rod/Reel share observer root, not camera attachment"),R.Visual->RodVisual->GetAttachParent()==R.Visual->GetRootComponent() && ReelMesh->GetAttachParent()==R.Visual->GetRootComponent());
    const FVector Reel=(S.Rod.RootWorldPositionM+S.Rod.TipDirection*.15-FVector(0,0,.09))*100;
    TestTrue(TEXT("Reel remains grip-side with fixed geometry"),ReelMesh->GetComponentLocation().Equals(Reel,1.e-6) && ReelMesh->GetComponentScale()==FVector(.17,.17,.13));
    Left=FMath::Min(Left,O.PixelTip.X);Right=FMath::Max(Right,O.PixelTip.X);Top=FMath::Min(Top,O.PixelTip.Y);Bottom=FMath::Max(Bottom,O.PixelTip.Y);
    Csv+=FString::Printf(TEXT("%d,%s,%.0f,%.3f,%.3f,%s,%.6f,%.6f,%.9g,%.12g,%.12g,%s\n"),Width,Port?TEXT("Port"):TEXT("Starboard"),Heading,S.FishingCamera.YawDeg,S.FishingCamera.PitchDeg,Label,O.PixelTip.X,O.PixelTip.Y,L,O.LengthM,O.TipErrorM,*S.Rod.EnvelopeReason);
   };
   Sample(TEXT("Center"));
   // Actual mesh projection is tested in the envelope interior after every view change.
   for(FVector2D D:{FVector2D(2,0),FVector2D(-2,0),FVector2D(0,2),FVector2D(0,-2)})
   {
    const auto Before=R.Observe();Send(D);const auto After=R.Observe();const auto S=R.PC->GetDebugSnapshot();const int Axis=D.X?0:1;
    if(!S.Rod.bEnvelopeLimited){MaxCross=FMath::Max(MaxCross,FMath::Abs((After.PixelTip-Before.PixelTip)[1-Axis]));TestTrue(TEXT("Mouse direction follows current actual camera"),(After.PixelTip-Before.PixelTip)[Axis]*(Axis?-D.Y:D.X)>0);}
   }
   for(const auto& Pair:TArray<TPair<FString,FVector2D>>{{FString(TEXT("Left")),FVector2D(-20,0)},{FString(TEXT("Right")),FVector2D(20,0)},{FString(TEXT("Up")),FVector2D(0,20)},{FString(TEXT("Down")),FVector2D(0,-20)},{FString(TEXT("LeftUp")),FVector2D(-20,20)},{FString(TEXT("RightUp")),FVector2D(20,20)},{FString(TEXT("LeftDown")),FVector2D(-20,-20)},{FString(TEXT("RightDown")),FVector2D(20,-20)}})
   {
    HomePose();auto Previous=R.PC->GetDebugSnapshot().Rod.BaseRodDirectionLocal;
    for(int I=0;I<65;++I)
    {Send(Pair.Value);const auto Now=R.PC->GetDebugSnapshot().Rod.BaseRodDirectionLocal;TestTrue(TEXT("Mouse boundary projection is bounded and continuous"),(Now-Previous).Size()<.15);Previous=Now;}
    Sample(*Pair.Key);
   }
   MinReachX=FMath::Min(MinReachX,(Right-Left)/Width);MinReachY=FMath::Min(MinReachY,(Bottom-Top)/(Width*9./16));
   AddInfo(FString::Printf(TEXT("A5 region width=%d side=%d heading=%.0f view=%s x=%.6f..%.6f (%.4f%%) y=%.6f..%.6f (%.4f%%)"),Width,Port,Heading,*Camera.ToString(),Left,Right,(Right-Left)*100/Width,Top,Bottom,(Bottom-Top)*100/(Width*9./16)));
  }
 }
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("Automation/R4A5");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Csv,*(Dir/TEXT("RuntimeSweep.csv")));
 AddInfo(FString::Printf(TEXT("A5 SUMMARY normalized1080 length=%.12g..%.12g ratio=%.12g minReachX=%.12g minReachY=%.12g cross=%.12g error=%.12g minEyeGripSeparationDeg=%.12g"),MinL,MaxL,MaxL/MinL,MinReachX,MinReachY,MaxCross,MaxError,MinSeparation));
 TestTrue(TEXT("Interior cross axis precision unchanged"),MaxCross<.05);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeEnvelopeBoundary,"TipRun.R4A5.Runtime.BoundaryNoAccumulation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeEnvelopeBoundary::RunTest(const FString&)
{
 for(int FPS:{30,60,120})for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R4A5_Boundary_%d_%d"),FPS,Port),FPS,Port)){return false;}
  auto Send=[&](FVector2D D){TestTrue(TEXT("Mouse boundary input queued"),R.PC->SubmitMouseDelta(D));R.Frames(FPS==120?2:1);};
  const auto Before=R.PC->GetDebugSnapshot().Rod;
  for(int Axis=0;Axis<2;++Axis)for(int I=0;I<2000;++I)
  {FVector2D D=FVector2D::ZeroVector;D[Axis]=1;Send(D);Send(-D);}
  const auto After=R.PC->GetDebugSnapshot().Rod;
  TestTrue(TEXT("2000 roundtrips per axis no local accumulation or length drift"),After.BaseRodDirectionLocal.Equals(Before.BaseRodDirectionLocal,1.e-9) && After.RodRootLocal==Before.RodRootLocal && After.LengthM==Before.LengthM);
  for(int Axis=0;Axis<2;++Axis)
  {
   for(int I=0;I<100;++I){FVector2D D=FVector2D::ZeroVector;D[Axis]=10;Send(D);}
   const auto Edge=R.PC->GetDebugSnapshot().Rod;
   TestTrue(TEXT("Explicit clamp diagnosis at boundary"),Edge.bEnvelopeLimited && !Edge.EnvelopeReason.IsEmpty());
   for(int I=0;I<100;++I){FVector2D D=FVector2D::ZeroVector;D[Axis]=10;Send(D);}
   // Nearest spherical projection can slide tangentially along a boundary.
   // Stored pose, not an unbounded screen target, must be the next command origin.
   const auto Settled=R.PC->GetDebugSnapshot().Rod;
   R.Frames(FPS);
   TestTrue(TEXT("No hidden clamp reservoir progresses after input stops"),R.PC->GetDebugSnapshot().Rod.BaseRodDirectionLocal==Settled.BaseRodDirectionLocal);
   AddInfo(FString::Printf(TEXT("Boundary tangent motion fps=%d side=%d axis=%d localDelta=%.12g reason=%s"),FPS,Port,Axis,(Settled.BaseRodDirectionLocal-Edge.BaseRodDirectionLocal).Size(),*Settled.EnvelopeReason));
   const auto BeforeReverse=R.Observe();FVector2D Return=FVector2D::ZeroVector;Return[Axis]=-1;Send(Return);
   const auto Reverse=R.Observe();
   TestTrue(TEXT("Opposite input immediately returns inward on actual camera axis"),!R.PC->GetDebugSnapshot().Rod.BaseRodDirectionLocal.Equals(Settled.BaseRodDirectionLocal,1.e-9) && (Reverse.PixelTip-BeforeReverse.PixelTip)[Axis]*(Axis?1.:-1.)>0);
  }
  TestTrue(TEXT("Boundary remains finite and aligned with actual visual"),R.Observe().bProjected && R.Observe().TipErrorM<1.e-7 && FMath::IsNearlyEqual(R.Observe().LengthM,2.,1.e-8));
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeEnvelopeActions,"TipRun.R4A5.Runtime.ActionAndQuickAtEnvelope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeEnvelopeActions::RunTest(const FString&)
{
 for(int FPS:{30,60,120})for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R4A5_EdgeActions_%d_%d"),FPS,Port),FPS,Port)){return false;}
  for(int I=0;I<80;++I){R.PC->SubmitMouseDelta(FVector2D(-10,-10));R.Frames(FPS==120?2:1);}
  const auto Base=R.PC->GetDebugSnapshot();auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
  TestTrue(TEXT("Edge action starts on saved ergonomic boundary"),Base.Rod.bEnvelopeEnabled && Base.Rod.bEnvelopeLimited);
  R.Tap(EKeys::Enter);R.Frames(FPS*4);
  bool SawPendingFall=false;int64 FallTick=-1;
  const auto H=S->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result)
  {if(C.Type==ETRFishingCommandType::Fall){SawPendingFall=Result==ETRCommandResult::Pending;}});
  const auto StateH=S->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState,ETRFishingState To)
  {if(To==ETRFishingState::FreeFall){FallTick=Sim->GetSimulationTime().TickIndex;TestTrue(TEXT("Pending Fall starts only with final returned to base"),S->RodControl->GetSnapshot().FinalRodTipLocal==S->RodControl->GetSnapshot().BaseRodTipLocal);}});
  R.Key(EKeys::RightMouseButton,true);R.Frames(2);R.Key(EKeys::RightMouseButton,false);
  const int64 Start=R.PC->GetDebugSnapshot().Egi.StateEnteredTick;
  R.Tap(EKeys::F);R.Frames(FPS);
  S->OnCommandProcessed.Remove(H);S->Fishing->OnFishingStateChanged.Remove(StateH);
  TestTrue(TEXT("Real F is retained Pending and starts on existing profile boundary"),SawPendingFall && FallTick==Start+S->RodControl->GetJerkTicks());
  TestTrue(TEXT("Envelope action never rewrites root/base/length"),S->RodControl->GetSnapshot().RodRootLocal==Base.Rod.RodRootLocal && S->RodControl->GetSnapshot().BaseRodDirectionLocal==Base.Rod.BaseRodDirectionLocal && S->RodControl->GetSnapshot().LengthM==Base.Rod.LengthM);
  TestTrue(TEXT("Fall at boundary never technically aborts"),!S->HasResult() || S->GetLastResult().Outcome!=ETRCastOutcome::Aborted);
  R.Key(EKeys::RightMouseButton,true);R.Frames(2);R.Key(EKeys::RightMouseButton,false);
  R.Tap(EKeys::Q);bool SawQuick=false;
  for(int I=0;I<FPS*3;++I)
  {
   const auto N=R.PC->GetDebugSnapshot();SawQuick|=N.Retrieval.bIsQuickRetrieving;
   TestTrue(TEXT("Quick at action/envelope boundary preserves exact base/grip/length and camera"),N.Rod.RodRootLocal==Base.Rod.RodRootLocal && N.Rod.BaseRodDirectionLocal==Base.Rod.BaseRodDirectionLocal && N.Rod.LengthM==Base.Rod.LengthM && N.FishingCamera.Rotation.Equals(Base.FishingCamera.Rotation,1.e-8) && (N.FishingCamera.PositionM-N.Boat.PositionM).Equals(Base.FishingCamera.PositionM-Base.Boat.PositionM,1.e-8));
   TestTrue(TEXT("Final and visual stay finite, length fixed"),R.Observe().bProjected && R.Observe().TipErrorM<1.e-7 && FMath::IsNearlyEqual(R.Observe().LengthM,2.,1.e-8));R.Frame();
  }
  TestTrue(TEXT("Edge Quick reaches Ready/unlocked and exact base without fake retrieval"),SawQuick && S->CanChangeEquipment() && S->GetLastResult().bQuickRetrieved && S->RodControl->GetSnapshot().FinalRodTipLocal==S->RodControl->GetSnapshot().BaseRodTipLocal);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeEnvelopeReplay,"TipRun.R4A5.Runtime.EnvelopeFixedTickReplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeEnvelopeReplay::RunTest(const FString&)
{
 for(bool Port:{true,false})
 {
  FVector Reference;TArray<FString> ReferenceEvents;
  for(int FPS:{30,60,120})
  {
   FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R4A5_EnvelopeReplay_%d_%d"),FPS,Port),FPS,Port)){return false;}
   auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
   const int64 Start=Sim->GetSimulationTime().TickIndex+120;TArray<FString> Events;bool SawLimit=false;
   const auto H=S->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result)
   {if(C.Type==ETRFishingCommandType::RodAim){Events.Add(FString::Printf(TEXT("%lld:%d:%s"),C.TargetTick-Start,int32(Result),*S->RodControl->GetSnapshot().AimResult));SawLimit|=S->RodControl->GetSnapshot().bEnvelopeLimited;}});
   // Immutable actual camera observations, queued at identical fixed ticks in each render condition.
   for(int I=0;I<120;++I)
   {const FVector2D D=I<40?FVector2D(20,-20):I<80?FVector2D(-20,20):FVector2D(20,20);TestTrue(TEXT("Boundary replay through real Controller"),R.PC->SubmitMouseDelta(D,Start+I*12));}
   while(Sim->GetSimulationTime().TickIndex<Start+120*12){R.Frame();}
   S->OnCommandProcessed.Remove(H);const auto Rod=S->RodControl->GetSnapshot();
   TestTrue(TEXT("Replay reaches diagnosed boundary and retains fixed finite geometry"),SawLimit && Events.Num()==120 && !Rod.BaseRodDirectionLocal.ContainsNaN() && Rod.LengthM==2 && R.Observe().TipErrorM<1.e-7);
   if(FPS==30){Reference=Rod.BaseRodDirectionLocal;ReferenceEvents=Events;}
   else{TestTrue(TEXT("Envelope pose and command Tick/Sequence results identical at 30/60/120fps"),Reference.Equals(Rod.BaseRodDirectionLocal,1.e-9) && Events==ReferenceEvents);}
   AddInfo(FString::Printf(TEXT("Envelope replay fps=%d side=%d final=%s events=%d"),FPS,Port,*Rod.BaseRodDirectionLocal.ToString(),Events.Num()));
  }
 }
 return true;
}

#endif
