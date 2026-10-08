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
#include "Fishing/TRShakuriSequenceComponent.h"
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
  // Bootstrap and deployment share identical fixed history before render FPS varies.
  FRuntimeRodWorld R;if(!R.Start(*this,FString::Printf(TEXT("R4A4_Replay_%d"),FPS),60)){return false;}
  auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
  R.Tap(EKeys::Enter);R.Frames(4);
  const int64 Start=Sim->GetSimulationTime().TickIndex+60;R.Dt=1./FPS;TArray<FString> Events;
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5Saved,"TipRun.R5.Runtime.SavedSequenceTuning",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5Saved::RunTest(const FString&)
{
 auto* C=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));
 if(!TestNotNull(TEXT("Saved config"),C) || !TestNotNull(TEXT("Saved Rod"),C->Rod.Get())){return false;}
 if(FParse::Param(FCommandLine::Get(),TEXT("TRMigrateR5")))
 {
  auto& P=C->Rod->Parameters;P.Sequence=FTRShakuriSequenceParameters();P.Sequence.bEnabled=true;P.ShakuriReelSpeedMps=P.ShakuriReelSeconds=0;
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  TestTrue(TEXT("Explicit R5 migration via UE SavePackage"),UPackage::SavePackage(C->Rod->GetOutermost(),C->Rod,*FPackageName::LongPackageNameToFilename(C->Rod->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
 }
 const auto& P=C->Rod->Parameters;TArray<FText> Errors;
 TestTrue(TEXT("Saved R5 validates and is enabled"),P.Sequence.bEnabled && P.Validate(Errors));
 TestEqual(TEXT("Nominal prototype m/turn"),P.Sequence.NominalRetrievePerHandleTurnM,.8);TestEqual(TEXT("One turn/action"),P.Sequence.HandleTurnsPerShakuri,1.);
 TestEqual(TEXT("Old two meter pulse removed"),P.ShakuriReelSpeedMps*P.ShakuriReelSeconds,0.);TestEqual(TEXT("Physical rod still 2m"),P.LengthM,2.);
 for(int I=0;I<5;++I){auto Bad=P.Sequence;if(I==0){Bad.HandleTurnsPerShakuri=1.01;}if(I==1){Bad.NominalRetrievePerHandleTurnM=std::numeric_limits<double>::quiet_NaN();}if(I==2){Bad.UpDemandFraction01=-.1;}if(I==3){Bad.HandleTurnsPerShakuri=0;}if(I==4){Bad.NominalRetrievePerHandleTurnM=-1;}TestFalse(TEXT("Invalid nominal demand rejected"),Bad.Validate());}
 FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,TEXT("R5_SavedBoot"))){return false;}
 return TestTrue(TEXT("Saved-map Runtime actually runs R5"),R.PC->GetBoundSession()->ShakuriSequence->IsEnabled());
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5Clicks,"TipRun.R5.Runtime.OneTwoThreeFiveClicks",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5Clicks::RunTest(const FString&)
{
 for(int FPS:{30,60,120})for(bool Port:{true,false})for(int Count:{1,2,3,5})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5_Clicks_%d_%d_%d"),FPS,Port,Count),FPS,Port)){return false;}
  R.Tap(EKeys::Enter);R.Frames(FPS*8);auto* S=R.PC->GetBoundSession();const auto Base=S->RodControl->GetSnapshot();
  int Requests=0,TF=0,Stay=0;bool Up=false,Recover=false,VisualMoved=false;
  auto H=S->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result){if(C.Type==ETRFishingCommandType::Jerk && (Result==ETRCommandResult::Accepted || Result==ETRCommandResult::Queued)){++Requests;}});
  auto SH=S->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState,ETRFishingState To){TF+=To==ETRFishingState::TensionFall;Stay+=To==ETRFishingState::Stay;});
  auto Check=[&]()
  {
   const auto N=R.PC->GetDebugSnapshot();const auto& Q=N.Shakuri;Up|=Q.Phase==ETRShakuriPhase::Up;Recover|=Q.Phase==ETRShakuriPhase::Recover;
   VisualMoved|=!R.Observe().StationDirectionLocal.Equals(Base.BaseRodDirectionLocal,1.e-5);
   TestTrue(TEXT("Every frame: Base/root/length and actual mesh stay consistent"),N.Rod.RodRootLocal==Base.RodRootLocal && N.Rod.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && N.Rod.LengthM==2 && R.Observe().TipErrorM<1.e-7);
   TestTrue(TEXT("Actual never exceeds action or total requested retrieve"),Q.ActualRetrieveM<=Q.RequestedRetrieveM+1.e-9 && Q.TotalActualRetrieveM<=Q.TotalRequestedRetrieveM+1.e-9);
   TestTrue(TEXT("Finite physical line and Egi"),!N.Egi.WorldPositionM.ContainsNaN() && FMath::IsFinite(N.Egi.LineLengthM) && N.Egi.LineLengthM>=0);
  };
  for(int I=0;I<Count;++I){R.Key(EKeys::RightMouseButton,true);R.Frames(2);Check();R.Key(EKeys::RightMouseButton,false);R.Frames(2);Check();}
  for(int I=0;I<FPS*5;++I){R.Frame();Check();}
  S->OnCommandProcessed.Remove(H);S->Fishing->OnFishingStateChanged.Remove(SH);const auto N=R.PC->GetDebugSnapshot();const auto Q=N.Shakuri;
  TestEqual(TEXT("Requests"),Requests,Count);TestEqual(TEXT("Executed sequences"),Q.SequenceIndex,int64(Count));TestEqual(TEXT("Completed recovers"),Q.CompletedCount,int64(Count));
  TestEqual(TEXT("Nominal turns"),Q.TotalRequestedHandleTurns,double(Count));TestTrue(TEXT("0.8m nominal per action"),FMath::IsNearlyEqual(Q.TotalRequestedRetrieveM,.8*Count,1.e-9));
  TestTrue(TEXT("Only final Recover enters TF then transient-complete Stay"),TF==1 && Stay==1 && N.Egi.FishingState==ETRFishingState::Stay);
  TestTrue(TEXT("Up/Recover visible and exact final Base"),Up && Recover && VisualMoved && N.Rod.FinalRodDirectionLocal==Base.BaseRodDirectionLocal);
  TestTrue(TEXT("No cast end/technical Abort or fabricated left hold"),!S->HasResult() && !R.PC->IsRetrieveHeld());
  AddInfo(FString::Printf(TEXT("fps=%d side=%d clicks=%d completed=%lld turns=%.9g requestM=%.9g actualM=%.9g TF=%d Stay=%d"),FPS,Port,Count,Q.CompletedCount,Q.TotalRequestedHandleTurns,Q.TotalRequestedRetrieveM,Q.TotalActualRetrieveM,TF,Stay));
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5Hold,"TipRun.R5.Runtime.LongHoldAndStates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5Hold::RunTest(const FString&)
{
 FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,TEXT("R5_Hold"))){return false;}auto* S=R.PC->GetBoundSession();
 R.Tap(EKeys::RightMouseButton);TestEqual(TEXT("Ready rejects"),S->GetHUDSnapshot().Shakuri.SequenceIndex,int64(0));
 R.Tap(EKeys::Enter);R.Frames(480);R.Key(EKeys::RightMouseButton,true);R.Frames(240);TestEqual(TEXT("Long hold exactly one"),S->GetHUDSnapshot().Shakuri.CompletedCount,int64(1));
 R.Key(EKeys::RightMouseButton,false);R.Frames(4);R.Tap(EKeys::RightMouseButton);R.Frames(120);TestEqual(TEXT("Release/new Started exactly next one"),S->GetHUDSnapshot().Shakuri.CompletedCount,int64(2));
 R.Tap(EKeys::Q);TestFalse(TEXT("Quick rejects Shakuri eligibility"),S->IsCommandAvailable(ETRFishingCommandType::Jerk));R.Tap(EKeys::RightMouseButton);R.Frames(120);
 TestTrue(TEXT("Quick remains Ready/onboard/unlocked"),S->CanChangeEquipment() && S->IsEgiOnboard());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5Bases,"TipRun.R5.Runtime.BaseAimAndMouseDuringSequence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5Bases::RunTest(const FString&)
{
 for(bool Port:{true,false})for(FVector2D Delta:{FVector2D(0,0),FVector2D(-20,0),FVector2D(20,0),FVector2D(0,20),FVector2D(0,-20)})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5_Base_%d_%g_%g"),Port,Delta.X,Delta.Y),60,Port)){return false;}
  for(int I=0;I<8;++I){R.PC->SubmitMouseDelta(Delta);R.Frames(2);}R.Tap(EKeys::Enter);R.Frames(480);
  auto* S=R.PC->GetBoundSession();const auto Base=S->RodControl->GetSnapshot();R.Tap(EKeys::RightMouseButton);R.Frames(120);const auto Done=S->RodControl->GetSnapshot();
  TestTrue(TEXT("Center/left/right/up/down returns exactly own base"),Done.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && Done.FinalRodDirectionLocal==Base.BaseRodDirectionLocal);
  R.Key(EKeys::RightMouseButton,true);R.Frames(2);R.Key(EKeys::RightMouseButton,false);R.PC->SubmitMouseDelta(FVector2D(2,-2));R.Frames(2);const auto Latest=S->RodControl->GetSnapshot();R.Frames(120);const auto Final=S->RodControl->GetSnapshot();
  TestTrue(TEXT("Mouse updates Base only and action returns latest Base"),Latest.BaseRodDirectionLocal!=Base.BaseRodDirectionLocal && Final.FinalRodDirectionLocal==Latest.BaseRodDirectionLocal && Final.BaseRodDirectionLocal==Latest.BaseRodDirectionLocal);
  TestTrue(TEXT("No root/length change or Abort"),Final.RodRootLocal==Base.RodRootLocal && Final.LengthM==2 && !S->HasResult());
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5Interrupt,"TipRun.R5.Runtime.ActionInterruptionsPauseFocus",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5Interrupt::RunTest(const FString&)
{
 for(bool Port:{true,false})for(int Action=0;Action<6;++Action)
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5_Interrupt_%d_%d"),Port,Action),60,Port)){return false;}
  R.Tap(EKeys::Enter);R.Frames(480);auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();const auto Base=S->RodControl->GetSnapshot();
  R.Tap(EKeys::RightMouseButton);R.Tap(EKeys::RightMouseButton);R.Key(EKeys::LeftMouseButton,true);R.Frames(2);
  TestTrue(TEXT("Left hold Pending at Base boundary"),S->Fishing->HasPendingRetrieve() && !S->GetHUDSnapshot().Retrieval.bIsRetrieving);
  if(Action==1){R.Key(EKeys::LeftMouseButton,false);R.Frames(2);TestFalse(TEXT("Early release clears pending"),S->Fishing->HasPendingRetrieve());}
  if(Action==2){R.Key(EKeys::LeftMouseButton,false);R.Tap(EKeys::F);TestTrue(TEXT("Re-Fall pending"),S->GetHUDSnapshot().Shakuri.bPendingReFall);}
  if(Action==3){R.Key(EKeys::LeftMouseButton,false);R.Tap(EKeys::Q);TestTrue(TEXT("Quick immediate separate action"),S->GetHUDSnapshot().Retrieval.bIsQuickRetrieving);}
  if(Action==4)
  {R.PC->SetPauseRequested(true);const auto Tick=Sim->GetSimulationTime().TickIndex;const auto Q=S->ShakuriSequence->GetSnapshot();const auto Tip=S->RodControl->GetSnapshot().FinalRodTipLocal;R.Frames(60);
   TestTrue(TEXT("Pause freezes sequence/time/pose/budget"),Sim->GetSimulationTime().TickIndex==Tick && S->ShakuriSequence->GetSnapshot().TickDemandM==Q.TickDemandM && S->RodControl->GetSnapshot().FinalRodTipLocal==Tip);R.PC->SetPauseRequested(false);R.Key(EKeys::LeftMouseButton,false);}
  if(Action==5){R.PC->SetInputFocus(false);R.Frames(2);R.Key(EKeys::LeftMouseButton,false);R.PC->SetInputFocus(true);R.PC->ActionReleased(ETRPlayerAction::Retrieve);}
  R.Frames(120);
  if(Action==0){TestTrue(TEXT("Pending left starts after base"),S->Fishing->GetState()==ETRFishingState::Retrieving);R.Key(EKeys::LeftMouseButton,false);R.Frames(4);}
  if(Action==2){TestTrue(TEXT("Pending Fall becomes FreeFall"),S->Fishing->GetState()==ETRFishingState::FreeFall);}
  if(Action==3){TestTrue(TEXT("Quick Ready/unlock"),S->CanChangeEquipment() && S->GetLastResult().bQuickRetrieved);}
  if(Action==1 || Action>=4){TestFalse(TEXT("Stopped held does not resume"),S->GetHUDSnapshot().Retrieval.bIsRetrieving);}
  const auto Final=S->RodControl->GetSnapshot();TestTrue(TEXT("Interrupt retains local base and no technical Abort"),Final.RodRootLocal==Base.RodRootLocal && Final.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && Final.FinalRodDirectionLocal==Base.BaseRodDirectionLocal && Final.LengthM==2 && (!S->HasResult() || S->GetLastResult().Outcome!=ETRCastOutcome::Aborted));
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5Replay,"TipRun.R5.Runtime.FixedTickSequenceReplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5Replay::RunTest(const FString&)
{
 for(bool Port:{true,false})
 {
  TArray<FString> Reference;FVector ReferenceEgi;double ReferenceActual=0;
  for(int FPS:{30,60,120})
  {
   FRuntimeRodWorld R;R.bTraceFrames=false;// Identical saved-world bootstrap before varying render FPS. Different boot
   // key frame counts would create different initial boat positions/inertia.
   if(!R.Start(*this,FString::Printf(TEXT("R5_Replay_%d_%d"),FPS,Port),60,Port)){return false;}R.Dt=1./FPS;
   auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();const int64 Start=120;
   TestTrue(TEXT("Controller target-tick Deploy"),R.PC->ActionStarted(ETRPlayerAction::Deploy,Start));R.PC->ActionReleased(ETRPlayerAction::Deploy);
   TArray<FString> Events;auto H=S->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result){Events.Add(FString::Printf(TEXT("%lld:%d:%d"),C.TargetTick,int32(C.Type),int32(Result)));});
   auto SH=S->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState,ETRFishingState To){Events.Add(FString::Printf(TEXT("state:%lld:%d"),Sim->GetSimulationTime().TickIndex,int32(To)));});
   while(Sim->GetSimulationTime().TickIndex<Start+60){R.Frame();}
   for(int I=0;I<5;++I){TestTrue(TEXT("Controller target-tick Jerk"),R.PC->ActionStarted(ETRPlayerAction::Jerk,Start+600+I*2));R.PC->ActionReleased(ETRPlayerAction::Jerk);}
   while(Sim->GetSimulationTime().TickIndex<Start+900){R.Frame();}
   S->OnCommandProcessed.Remove(H);S->Fishing->OnFishingStateChanged.Remove(SH);const auto N=S->GetHUDSnapshot();
   TestEqual(TEXT("Five recovers"),N.Shakuri.CompletedCount,int64(5));TestTrue(TEXT("Five turns/4m nominal cap"),N.Shakuri.TotalRequestedHandleTurns==5 && FMath::IsNearlyEqual(N.Shakuri.TotalRequestedRetrieveM,4.,1.e-9) && N.Shakuri.TotalActualRetrieveM<=4.);
   if(FPS==30){Reference=Events;ReferenceEgi=N.Egi.WorldPositionM;ReferenceActual=N.Shakuri.TotalActualRetrieveM;}
   else{TestTrue(TEXT("Exact Tick/Sequence/state and same Egi/actual at 30/60/120"),Events==Reference && ReferenceEgi.Equals(N.Egi.WorldPositionM,1.e-8) && ReferenceActual==N.Shakuri.TotalActualRetrieveM);}
   AddInfo(FString::Printf(TEXT("Replay fps=%d side=%d completed=%lld nominal=%.9g actual=%.9g"),FPS,Port,N.Shakuri.CompletedCount,N.Shakuri.TotalRequestedRetrieveM,N.Shakuri.TotalActualRetrieveM));
  }
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5States,"TipRun.R5.Runtime.StateSurfaceBudgetAndLifetime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5States::RunTest(const FString&)
{
 FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,TEXT("R5_StateBudget"))){return false;}
 auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
 const int64 Target=Sim->GetSimulationTime().TickIndex+2;ETRCommandResult DeployingJerk=ETRCommandResult::Accepted;
 const auto H=S->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result){if(C.Type==ETRFishingCommandType::Jerk){DeployingJerk=Result;}});
 TestTrue(TEXT("Real controller Deploy"),R.PC->ActionStarted(ETRPlayerAction::Deploy,Target));R.PC->ActionReleased(ETRPlayerAction::Deploy);
 TestTrue(TEXT("Same queue tick Jerk cannot preempt deployment"),R.PC->ActionStarted(ETRPlayerAction::Jerk,Target));R.PC->ActionReleased(ETRPlayerAction::Jerk);R.Frames(4);S->OnCommandProcessed.Remove(H);
 TestEqual(TEXT("Deploying Jerk rejected"),DeployingJerk,ETRCommandResult::RejectedInvalidState);
 TestTrue(TEXT("FreeFall eligibility"),S->Fishing->GetState()==ETRFishingState::FreeFall && S->IsCommandAvailable(ETRFishingCommandType::Jerk));
 R.Tap(EKeys::RightMouseButton);R.Frames(120);const auto Near=S->GetHUDSnapshot();
 TestTrue(TEXT("Near surface geometry may underfulfill nominal, never forces it"),Near.Shakuri.SequenceIndex==1 && Near.Shakuri.TotalActualRetrieveM<=Near.Shakuri.TotalRequestedRetrieveM && !S->HasResult());
 AddInfo(FString::Printf(TEXT("Near surface request=%.9g actual=%.9g (unused not forced/carried)"),Near.Shakuri.TotalRequestedRetrieveM,Near.Shakuri.TotalActualRetrieveM));
 R.Tap(EKeys::Q);R.Frames(120);R.Tap(EKeys::Enter);
 for(int I=0;I<6000 && S->Fishing->GetState()!=ETRFishingState::BottomContact && !S->HasResult();++I){R.Frame();}
 TestTrue(TEXT("Real saved ocean reaches Bottom, accepts Shakuri"),S->Fishing->GetState()==ETRFishingState::BottomContact && S->IsCommandAvailable(ETRFishingCommandType::Jerk));
 R.Tap(EKeys::RightMouseButton);R.Frames(120);TestTrue(TEXT("Bottom Shakuri keeps budget and safe lineage"),S->GetHUDSnapshot().Shakuri.CompletedCount==1 && !S->HasResult());
 R.Key(EKeys::LeftMouseButton,true);R.Frames(2);TestTrue(TEXT("Normal Retrieving permits replacement by Jerk"),S->Fishing->GetState()==ETRFishingState::Retrieving && S->IsCommandAvailable(ETRFishingCommandType::Jerk));
 R.Tap(EKeys::RightMouseButton);R.Key(EKeys::LeftMouseButton,false);R.Frames(120);TestTrue(TEXT("Jerk replaces held retrieval without summing reel demands"),S->GetHUDSnapshot().Shakuri.CompletedCount==2 && !S->HasResult());
 R.Tap(EKeys::Q);R.Frames(120);R.Tap(EKeys::Enter);R.Tap(EKeys::RightMouseButton);const auto Cast=S->GetCastId();
 S->EndFishing();const auto Stopped=S->ShakuriSequence->GetSnapshot();R.Frames(120);
 TestTrue(TEXT("Stopped session cannot advance sequence or retained demand"),S->ShakuriSequence->GetSnapshot().Tick==Stopped.Tick && Stopped.TickDemandM==0 && Stopped.QueuedCount==0 && !S->SubmitCommand(ETRFishingCommandType::Jerk,Cast));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5Distribution,"TipRun.R5.Runtime.PhaseDistributionAndFrozenTuning",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5Distribution::RunTest(const FString&)
{
 auto* C=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));
 if(!TestNotNull(TEXT("Loaded saved tuning for controlled phase variation"),C) || !TestNotNull(TEXT("Rod"),C->Rod.Get())){return false;}
 for(double Fraction:{0.,.5,1.})
 {
  // Controlled editor-like variation of the loaded asset only. No SavePackage.
  // The same saved-map GameMode/config path instantiates the runtime session.
  TGuardValue<FTRShakuriSequenceParameters> Restore(C->Rod->Parameters.Sequence,FTRShakuriSequenceParameters());
  C->Rod->Parameters.Sequence.bEnabled=true;C->Rod->Parameters.Sequence.UpDemandFraction01=Fraction;
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5_Distribution_%g"),Fraction))){return false;}
  auto* S=R.PC->GetBoundSession();R.Tap(EKeys::Enter);R.Frames(480);
  // Live asset changes cannot rewrite frozen demand/profile in this session/cast.
  C->Rod->Parameters.Sequence.NominalRetrievePerHandleTurnM=10;
  double UpDemand=0,RecoverDemand=0;int64 ObservedTick=-1;
  R.Key(EKeys::RightMouseButton,true);
  for(int I=0;I<120;++I)
  {
   R.Frame();if(I==1){R.Key(EKeys::RightMouseButton,false);}const auto N=R.PC->GetDebugSnapshot();const auto& Q=N.Shakuri;
   if(Q.Tick!=ObservedTick)
   {ObservedTick=Q.Tick;if(Q.Phase==ETRShakuriPhase::Up){UpDemand+=Q.TickDemandM;TestEqual(TEXT("Rod and Sequence share Up"),N.Rod.ShakuriPhase,uint8(1));}
    if(Q.Phase==ETRShakuriPhase::Recover){RecoverDemand+=Q.TickDemandM;TestEqual(TEXT("Rod and Sequence share Recover"),N.Rod.ShakuriPhase,uint8(2));}}
  }
  const auto Q=S->GetHUDSnapshot().Shakuri;
  if(S->HasResult()){AddInfo(TEXT("Distribution safety diagnostic: ")+S->DiagnosticAbortContext+TEXT(" endReason=")+S->DiagnosticLastFailure);}
  TestTrue(TEXT("Phase allocation follows asset fraction and frozen 0.8m demand"),FMath::IsNearlyEqual(UpDemand,.8*Fraction,1.e-9) && FMath::IsNearlyEqual(RecoverDemand,.8*(1-Fraction),1.e-9));
  TestTrue(TEXT("Actual physical shortening capped, session does not read live 10m"),Q.CompletedCount==1 && Q.TotalRequestedRetrieveM==.8 && Q.TotalActualRetrieveM<=.8+1.e-9 && !S->HasResult());
  AddInfo(FString::Printf(TEXT("Up fraction=%.2f demand Up=%.9g Recover=%.9g actual=%.9g"),Fraction,UpDemand,RecoverDemand,Q.TotalActualRetrieveM));
 }
 return true;
}
// R5A acceptance uses the saved runtime camera and device/controller queue.
// A tiny reversible aim sample refreshes the real last-mouse observation without
// changing the tested base. Camera-only and last-mouse-camera paths are distinct.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5ACamera,"TipRun.R5A.Runtime.CameraIndependentTrajectory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5ACamera::RunTest(const FString&)
{
 TArray<FVector> PortTrajectory;
 for(bool Port:{true,false})
 {
  TArray<FVector> Reference;
  for(FVector2D View:{FVector2D(0,-20),FVector2D(0,0),FVector2D(0,-35),FVector2D(-25,-20),FVector2D(25,-20)})
  {
   FRuntimeRodWorld R;R.bTraceFrames=false;
   if(!R.Start(*this,FString::Printf(TEXT("R5A_Camera_%d_%g_%g"),Port,View.X,View.Y),60,Port)){return false;}
   auto* S=R.PC->GetBoundSession();const auto Initial=S->RodControl->GetSnapshot();
   const auto CameraTuning=R.World->GetAuthGameMode<ATRGameModeBase>()->SessionConfig->FishingStations->Parameters;
   for(int Axis=0;Axis<2;++Axis)
   {
    const auto C=R.PC->GetDebugSnapshot().FishingCamera;const double Difference=View[Axis]-(Axis?C.PitchDeg:C.YawDeg);
    const FKey K=Axis?(Difference>0?EKeys::W:EKeys::S):(Difference>0?EKeys::D:EKeys::A);
    const double Rate=Axis?CameraTuning.FishingCameraPitchRateDegPerS:CameraTuning.FishingCameraYawRateDegPerS;
    R.Key(K,true);R.Frames(FMath::RoundToInt(FMath::Abs(Difference)*60/Rate));R.Key(K,false);R.Frames(3);
   }
   TestTrue(TEXT("Camera-only preserves exact base/root"),S->RodControl->GetSnapshot().BaseRodDirectionLocal==Initial.BaseRodDirectionLocal && S->RodControl->GetSnapshot().RodRootLocal==Initial.RodRootLocal);
   // Controller captures the current Active Camera, not a test-generated POV.
   TestTrue(TEXT("Refresh current observation through real mouse command"),R.PC->SubmitMouseDelta(FVector2D(.05,0)));R.Frames(2);
   TestTrue(TEXT("Reverse same camera sample"),R.PC->SubmitMouseDelta(FVector2D(-.05,0)));R.Frames(2);
   const auto Base=S->RodControl->GetSnapshot();
   TestTrue(TEXT("Reversible sample retains same station base within 1e-10"),Base.BaseRodDirectionLocal.Equals(Initial.BaseRodDirectionLocal,1.e-10));
   R.Tap(EKeys::Enter);R.Frames(480);R.Key(EKeys::RightMouseButton,true);
   TArray<FVector> Trajectory;double Peak=0,MaxTickMove=0,MaxError=0;FVector Previous=Base.BaseRodTipLocal,PeakDirection=Base.BaseRodDirectionLocal;
   for(int I=0;I<60;++I)
   {
    R.Frame();if(I==1){R.Key(EKeys::RightMouseButton,false);}const auto N=S->RodControl->GetSnapshot();const auto O=R.Observe();
    Trajectory.Add(N.FinalRodTipLocal);
    const double Displacement=(N.FinalRodTipLocal-Base.BaseRodTipLocal).Size();
    if(Displacement>Peak){Peak=Displacement;PeakDirection=N.FinalRodDirectionLocal;}
    MaxTickMove=FMath::Max(MaxTickMove,(N.FinalRodTipLocal-Previous).Size());Previous=N.FinalRodTipLocal;
    MaxError=FMath::Max(MaxError,FMath::Max(O.RootErrorM,O.TipErrorM));
    TestTrue(TEXT("Every action frame preserves grip, base and physical 2m"),N.RodRootLocal==Base.RodRootLocal && N.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && N.LengthM==2 && FMath::IsNearlyEqual((N.FinalRodTipLocal-N.RodRootLocal).Size(),2.,1.e-12));
   }
   const auto End=S->GetHUDSnapshot();const auto C=R.PC->GetDebugSnapshot().FishingCamera;
   TestTrue(TEXT("One action returns exactly to latest base without Abort"),End.Shakuri.CompletedCount==1 && End.Rod.FinalRodDirectionLocal==Base.BaseRodDirectionLocal && !S->HasResult());
   TestTrue(TEXT("Actual visual root/tip match action snapshot"),MaxError<1.e-7);
   double Difference=0;
   if(Reference.IsEmpty()){Reference=Trajectory;if(Port){for(const auto& Tip:Trajectory){PortTrajectory.Add(Tip-Base.RodRootLocal);}}}
   else{for(int I=0;I<Trajectory.Num();++I){Difference=FMath::Max(Difference,(Trajectory[I]-Reference[I]).Size());}}
   TestTrue(TEXT("All camera directions give same station-local action within 1e-9m"),Difference<1.e-9);
   if(!Port && View==FVector2D(0,-20)){for(int I=0;I<Trajectory.Num();++I){TestTrue(TEXT("Both side station-local action displacement agrees"),(Trajectory[I]-Base.RodRootLocal).Equals(PortTrajectory[I],1.e-9));}}
   AddInfo(FString::Printf(TEXT("R5A side=%s cameraYaw=%.9g pitch=%.9g base=%s peakDirection=%s peakDisplacementM=%.12g maxTickMoveM=%.12g cameraTrajectoryErrorM=%.12g returnErrorM=%.12g visualErrorM=%.12g nominal=%.9g"),Port?TEXT("Port"):TEXT("Starboard"),C.YawDeg,C.PitchDeg,*Base.BaseRodDirectionLocal.ToString(),*PeakDirection.ToString(),Peak,MaxTickMove,Difference,(End.Rod.FinalRodTipLocal-Base.BaseRodTipLocal).Size(),MaxError,End.Shakuri.TotalRequestedRetrieveM));
  }
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5ADown,"TipRun.R5A.Runtime.CameraDownReproduction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5ADown::RunTest(const FString&)
{
 for(bool Port:{true,false})for(bool LastMouse:{false,true})
 {
  double ReferencePeak=0;TArray<FVector> Reference;
  for(bool Down:{false,true})
  {
   FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5A_Down_%d_%d_%d"),Port,LastMouse,Down),60,Port)){return false;}
   auto* S=R.PC->GetBoundSession();const auto Initial=S->RodControl->GetSnapshot();
   if(Down){R.Key(EKeys::S,true);R.Frames(26);R.Key(EKeys::S,false);R.Frames(3);}
   if(LastMouse){R.PC->SubmitMouseDelta(FVector2D(.05,0));R.Frames(2);R.PC->SubmitMouseDelta(FVector2D(-.05,0));R.Frames(2);}
   const auto Base=S->RodControl->GetSnapshot();TestTrue(TEXT("Down reproduction retains same base"),Base.BaseRodDirectionLocal.Equals(Initial.BaseRodDirectionLocal,1.e-10));
   R.Tap(EKeys::Enter);R.Frames(480);R.Key(EKeys::RightMouseButton,true);
   double Peak=0,Difference=0;TArray<FVector> Trajectory;
   for(int I=0;I<60;++I){R.Frame();if(I==1){R.Key(EKeys::RightMouseButton,false);}const auto N=S->RodControl->GetSnapshot();Trajectory.Add(N.FinalRodTipLocal);Peak=FMath::Max(Peak,(N.FinalRodTipLocal-Base.BaseRodTipLocal).Size());}
   if(!Down){ReferencePeak=Peak;Reference=Trajectory;}else{for(int I=0;I<Trajectory.Num();++I){Difference=FMath::Max(Difference,(Trajectory[I]-Reference[I]).Size());}}
   TestTrue(TEXT("Downward view does not extinguish local Shakuri"),Peak>.5 && FMath::IsNearlyEqual(Peak,ReferencePeak,1.e-9) && Difference<1.e-9 && !S->HasResult());
   AddInfo(FString::Printf(TEXT("R5A Down side=%d currentMouseObservation=%d down=%d cameraPitch=%.9g peakM=%.12g referenceM=%.12g trajectoryErrorM=%.12g"),Port,LastMouse,Down,R.PC->GetDebugSnapshot().FishingCamera.PitchDeg,Peak,ReferencePeak,Difference));
  }
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5AEnvelope,"TipRun.R5A.Runtime.BaseEnvelopeActionSeparation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5AEnvelope::RunTest(const FString&)
{
 auto* Config=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));
 if(!TestNotNull(TEXT("Saved rod tuning"),Config)){return false;}
 const auto P=Config->Rod->Parameters;
 for(bool Port:{true,false})for(FVector2D Aim:{FVector2D(-35,5),FVector2D(35,5),FVector2D(0,35)})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5A_Envelope_%d_%g_%g"),Port,Aim.X,Aim.Y),60,Port)){return false;}
  // Move the camera through WASD so an upper base is reachable on the real screen.
  R.Key(EKeys::W,true);R.Frames(35);R.Key(EKeys::W,false);R.Frames(3);
  const FVector Desired=FRotator(Aim.Y,Aim.X,0).Vector();
  for(int I=0;I<180;++I)
  {
   const auto N=R.PC->GetDebugSnapshot();const FQuat Basis(FVector::UpVector,FMath::DegreesToRadians(N.Station.FacingWorldDeg));
   FVector2D Target;R.PC->ProjectWorldLocationToScreen((N.Rod.RootWorldPositionM+Basis.RotateVector(Desired)*2)*100,Target);
   const auto Difference=Target-R.Observe().PixelTip;if(Difference.Size()<.001){break;}
   TestTrue(TEXT("Upper/side base uses real Controller aim"),R.PC->SubmitMouseDelta(FVector2D(Difference.X,-Difference.Y)/(960*P.Screen.Sensitivity.X)));R.Frames(2);
  }
  auto* S=R.PC->GetBoundSession();const auto Base=S->RodControl->GetSnapshot();
  TestTrue(TEXT("Reached the specified ergonomic base through the runtime projection"),Base.BaseRodDirectionLocal.Equals(Desired,1.e-5));
  R.Tap(EKeys::Enter);R.Frames(480);R.Key(EKeys::RightMouseButton,true);double PeakAngle=0,MaxPitch=0;
  for(int I=0;I<60;++I)
  {
   R.Frame();if(I==1){R.Key(EKeys::RightMouseButton,false);}const auto N=S->RodControl->GetSnapshot();
   PeakAngle=FMath::Max(PeakAngle,FMath::Acos(FMath::Clamp(FVector::DotProduct(N.BaseRodDirectionLocal,N.FinalRodDirectionLocal),-1.,1.)));
   MaxPitch=FMath::Max(MaxPitch,FMath::RadiansToDegrees(N.FinalPitchRad));
   TestTrue(TEXT("Action preserves base/root/2m and outward safe hemisphere"),N.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && N.RodRootLocal==Base.RodRootLocal && N.LengthM==2 && N.FinalRodDirectionLocal.X>0 && N.FinalPitchRad<=FMath::DegreesToRadians(P.ActionSafety.MaxPitchDeg)+1.e-12);
   TestTrue(TEXT("Action is represented by the actual mesh"),R.Observe().TipErrorM<1.e-7);
  }
  const auto End=S->GetHUDSnapshot();TestTrue(TEXT("Full tuned angular amplitude is independent of base envelope ceiling"),FMath::IsNearlyEqual(PeakAngle,P.ShakuriAmplitudeRad,1.e-8));
  if(Aim.Y==35){TestTrue(TEXT("Temporary upper action may safely exceed base ceiling"),MaxPitch>P.Envelope.MaxPitchDeg+1);}
  TestTrue(TEXT("Latest exact base return, 0.8 nominal and no normal Abort"),End.Rod.FinalRodDirectionLocal==Base.BaseRodDirectionLocal && End.Shakuri.TotalRequestedRetrieveM==.8 && !S->HasResult());
  AddInfo(FString::Printf(TEXT("R5A envelope side=%d baseYaw=%.9g basePitch=%.9g peakAngleRad=%.12g maxActionPitchDeg=%.12g"),Port,Base.BaseYawRad,Base.BasePitchRad,PeakAngle,MaxPitch));
 }
 for(double Bad:{-1.,85.,std::numeric_limits<double>::quiet_NaN()}){auto A=P.ActionSafety;A.MaxPitchDeg=Bad;TestFalse(TEXT("Invalid action safety data rejected"),A.Validate());}
 auto Bad=P;Bad.ActionSafety.MaxPitchDeg=P.Envelope.MaxPitchDeg-1;TArray<FText> Errors;TestFalse(TEXT("Action safety cannot silently narrow the base envelope"),Bad.Validate(Errors));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5ALookReplay,"TipRun.R5A.Runtime.ActiveLookFixedTickTrajectory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5ALookReplay::RunTest(const FString&)
{
 for(bool Port:{true,false})
 {
  TArray<FVector> Reference;FVector ReferenceEgi;double ReferenceActual=0;
  for(int FPS:{30,60,120})for(bool Look:{false,true})
  {
   FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5A_Replay_%d_%d_%d"),Port,FPS,Look),60,Port)){return false;}
   R.Tap(EKeys::Enter);R.Frames(480);auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
   const auto Base=S->RodControl->GetSnapshot();const int64 Start=Sim->GetSimulationTime().TickIndex+10;
   for(int I=0;I<5;++I){TestTrue(TEXT("Five real controller target-tick starts"),R.PC->ActionStarted(ETRPlayerAction::Jerk,Start+I*2));R.PC->ActionReleased(ETRPlayerAction::Jerk);}
   TArray<FVector> Trajectory;
   // Read-only participant observes every publish tick, including both steps of
   // a 30fps frame. It does not inject a solver result or apply presentation.
   auto* Observer=R.World->SpawnActor<AActor>();
   const auto Id=Sim->RegisterSession(Observer,FTRSimulationStep::CreateLambda([&](ETRSimulationPhase Phase,const FTRSimTime& Time)
   {
    if(Phase!=ETRSimulationPhase::Publish || Time.TickIndex<Start || Time.TickIndex>=Start+150){return;}
    const auto N=S->RodControl->GetSnapshot();Trajectory.Add(N.FinalRodTipLocal);
    TestTrue(TEXT("Camera motion never owns base/root/length during an action"),N.RodRootLocal==Base.RodRootLocal && N.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && N.LengthM==2);
   }),FTRSimulationCommand::CreateLambda([](const FTRFishingCommand&){}));
   TestTrue(TEXT("Read-only fixed publish observer registered"),Id.IsValid());R.Dt=1./FPS;
   if(Look){R.Key(EKeys::S,true);R.Key(EKeys::D,true);}
   double VisualError=0;
   while(Sim->GetSimulationTime().TickIndex<Start+150){R.Frame();VisualError=FMath::Max(VisualError,R.Observe().TipErrorM);}
   if(Look){R.Key(EKeys::S,false);R.Key(EKeys::D,false);}
   Sim->Unregister(Id);Observer->Destroy();const auto End=S->GetHUDSnapshot();const auto Camera=R.PC->GetDebugSnapshot().FishingCamera;
   TestTrue(TEXT("Every fixed tick observed, five completed, exact base return, no technical Abort"),Trajectory.Num()==150 && End.Shakuri.CompletedCount==5 && End.Rod.FinalRodDirectionLocal==Base.BaseRodDirectionLocal && !S->HasResult());
   TestTrue(TEXT("0.8 per sequence unchanged and real mesh agrees"),End.Shakuri.TotalRequestedRetrieveM==4 && End.Shakuri.TotalActualRetrieveM<=4+1.e-9 && VisualError<1.e-7);
   double Difference=0;
   if(Reference.IsEmpty()){Reference=Trajectory;ReferenceEgi=End.Egi.WorldPositionM;ReferenceActual=End.Shakuri.TotalActualRetrieveM;}
   else{for(int I=0;I<FMath::Min(Trajectory.Num(),Reference.Num());++I){Difference=FMath::Max(Difference,(Trajectory[I]-Reference[I]).Size());}
    TestTrue(TEXT("Same Egi and reel actual through camera changes and render fps"),End.Egi.WorldPositionM.Equals(ReferenceEgi,1.e-9) && End.Shakuri.TotalActualRetrieveM==ReferenceActual);}
   TestTrue(TEXT("All fixed local action samples agree within 1e-12m"),Difference<1.e-12);
   if(Look){TestTrue(TEXT("Actual camera moved to deep down/right clamp while rod action continued"),Camera.PitchDeg<-60 && Camera.YawDeg>50);}
   AddInfo(FString::Printf(TEXT("R5A activeLook side=%d fps=%d look=%d samples=%d localTrajectoryErrorM=%.12g cameraYaw=%.9g pitch=%.9g actualRetrieveM=%.12g visualErrorM=%.12g"),Port,FPS,Look,Trajectory.Num(),Difference,Camera.YawDeg,Camera.PitchDeg,End.Shakuri.TotalActualRetrieveM,VisualError));
  }
 }
 return true;
}
namespace
{
 void R5BLook(FRuntimeRodWorld& R,FVector2D Desired)
 {
  const auto P=R.World->GetAuthGameMode<ATRGameModeBase>()->SessionConfig->FishingStations->Parameters;
  for(int Axis=0;Axis<2;++Axis)
  {
   const auto C=R.PC->GetDebugSnapshot().FishingCamera;const double Diff=Desired[Axis]-(Axis?C.PitchDeg:C.YawDeg);
   const FKey K=Axis?(Diff>0?EKeys::W:EKeys::S):(Diff>0?EKeys::D:EKeys::A);
   const double Rate=Axis?P.FishingCameraPitchRateDegPerS:P.FishingCameraYawRateDegPerS;
   if(FMath::Abs(Diff)>Rate/120){R.Key(K,true);R.Frames(FMath::RoundToInt(FMath::Abs(Diff)*60/Rate));R.Key(K,false);R.Frames(2);}
  }
 }
 FVector R5BBaseWorld(const FTRHUDSnapshot& S)
 {return S.Rod.RootWorldPositionM+FQuat(FVector::UpVector,FMath::DegreesToRadians(S.Station.FacingWorldDeg)).RotateVector(S.Rod.BaseRodDirectionLocal)*S.Rod.LengthM;}
 bool R5BProject(FRuntimeRodWorld& R,const FVector& WorldM,FVector2D& Pixel)
 {return R.PC->ProjectWorldLocationToScreen(WorldM*100,Pixel);}
 bool R5BInside(const FIntRect& Rect,FVector2D Pixel)
 {return Pixel.X>=Rect.Min.X && Pixel.X<=Rect.Max.X && Pixel.Y>=Rect.Min.Y && Pixel.Y<=Rect.Max.Y;}
 FTRRuntimeObservation R5BVisual(FRuntimeRodWorld& R)
 {
  auto O=R.Observe();
  // Project actual mesh tip independently: a behind-camera grip must not
  // short-circuit observation of a still-visible tip. This never applies a pose.
  O.bProjected=R5BProject(R,O.VisualTipM,O.PixelTip);return O;
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5BPitch,"TipRun.R5B.Runtime.PitchSweepAndMouseRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5BPitch::RunTest(const FString&)
{
 for(bool Port:{true,false})for(double Pitch:{-20.,-25.,-30.,-35.,-36.,-37.,-38.,-39.,-40.,-45.,-50.,-55.,-60.,-65.})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5B_Pitch_%d_%g"),Port,Pitch),60,Port)){return false;}
  R.Tap(EKeys::Enter);R.Frames(480);auto* S=R.PC->GetBoundSession();const auto Base=S->GetHUDSnapshot().Rod;
  R5BLook(R,FVector2D(0,Pitch));FVector2D BeforePixel=FVector2D::ZeroVector;const bool BeforeProjected=R5BProject(R,R5BBaseWorld(S->GetHUDSnapshot()),BeforePixel);
  TestTrue(TEXT("Camera look preserves physical base before Shakuri"),S->RodControl->GetSnapshot().BaseRodDirectionLocal==Base.BaseRodDirectionLocal);
  const auto BeforeVisual=R5BVisual(R);R.Key(EKeys::RightMouseButton,true);double YawError=0,Peak=0,VisualError=0;FVector2D PeakPixel=FVector2D::ZeroVector;bool PeakProjected=false;FTRRodSnapshot PeakPose;
  for(int I=0;I<60;++I)
  {
   R.Frame();if(I==1){R.Key(EKeys::RightMouseButton,false);}const auto N=S->RodControl->GetSnapshot();const auto O=R5BVisual(R);
   const double D=(N.FinalRodTipLocal-N.BaseRodTipLocal).Size();if(D>Peak){Peak=D;PeakPixel=O.PixelTip;PeakProjected=O.bProjected;PeakPose=N;}
   YawError=FMath::Max(YawError,FMath::Abs(FMath::UnwindRadians(N.FinalYawRad-N.BaseYawRad)));VisualError=FMath::Max(VisualError,O.TipErrorM);
   TestTrue(TEXT("Shakuri keeps base, fixed root and 2m length"),N.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && N.RodRootLocal==Base.RodRootLocal && N.LengthM==2);
  }
  const auto Recover=S->GetHUDSnapshot();const auto RecoverVisual=R5BVisual(R);
  AddInfo(FString::Printf(TEXT("R5B PEAK side=%d cameraPitch=%.9g finalDirection=%s finalYawRad=%.12g finalPitchRad=%.12g viewRect=%dx%d FOV=%.9g"),Port,R.PC->GetDebugSnapshot().FishingCamera.PitchDeg,*PeakPose.FinalRodDirectionLocal.ToString(),PeakPose.FinalYawRad,PeakPose.FinalPitchRad,BeforeVisual.ViewRect.Width(),BeforeVisual.ViewRect.Height(),BeforeVisual.FOV));
  TestTrue(TEXT("Shakuri is a vertical plane, not a camera-dependent yaw"),YawError<1.e-12 && Peak>.5);
  TestTrue(TEXT("Recover exact latest base, real mesh and no Abort"),Recover.Rod.FinalRodDirectionLocal==Base.BaseRodDirectionLocal && VisualError<1.e-7 && !S->HasResult());
  AddInfo(FString::Printf(TEXT("R5B PITCH side=%d actualPitch=%.9g root=%s baseDirection=%s baseTip=%s finalDirection=%s baseYaw=%.12g finalYaw=%.12g basePitch=%.12g finalPitch=%.12g projected=%d inside=%d basePixel=%s visualBefore=%s peakProjected=%d peakVisual=%s recoverVisual=%s yawErrorRad=%.12g peakM=%.12g"),Port,R.PC->GetDebugSnapshot().FishingCamera.PitchDeg,*Base.RodRootLocal.ToString(),*Base.BaseRodDirectionLocal.ToString(),*Base.BaseRodTipLocal.ToString(),*Recover.Rod.FinalRodDirectionLocal.ToString(),Base.BaseYawRad,Recover.Rod.FinalYawRad,Base.BasePitchRad,Recover.Rod.FinalPitchRad,BeforeProjected,BeforeProjected && R5BInside(BeforeVisual.ViewRect,BeforePixel),*BeforePixel.ToString(),*BeforeVisual.PixelTip.ToString(),PeakProjected,*PeakPixel.ToString(),*RecoverVisual.PixelTip.ToString(),YawError,Peak));
  for(int Axis=0;Axis<2;++Axis)
  {
   const auto Start=S->RodControl->GetSnapshot();
   for(double Sign:{1.,-1.})
   {
    const auto B=S->GetHUDSnapshot();FVector2D Pixel=FVector2D::ZeroVector;const bool Projected=R5BProject(R,R5BBaseWorld(B),Pixel);
    FVector2D Delta=FVector2D::ZeroVector;Delta[Axis]=Sign;
    TestTrue(TEXT("Mouse four directions queue through actual camera/controller"),R.PC->SubmitMouseDelta(Delta));R.Frames(2);
    const auto N=S->GetHUDSnapshot();FVector2D AfterPixel=FVector2D::ZeroVector;const bool AfterProjected=R5BProject(R,R5BBaseWorld(N),AfterPixel);
    const double Motion=(N.Rod.BaseRodDirectionLocal-B.Rod.BaseRodDirectionLocal).Size();
    TestTrue(TEXT("First off-screen mouse command is accepted and moves base"),N.Rod.AimResult==TEXT("Valid") && Motion>1.e-8);
    AddInfo(FString::Printf(TEXT("R5B DIAGNOSTIC mapping=%s projection=%s valid=%d outside=%d sphere=%s discriminant=%.12g"),*N.Rod.AimMapping,*N.Rod.AimProjectionStatus,N.Rod.bAimProjectionValid,N.Rod.bAimOutsideViewRect,*N.Rod.AimSphereResult,N.Rod.AimSphereDiscriminant));
    if(Projected && AfterProjected && !N.Rod.bEnvelopeLimited)
    {
     TestTrue(TEXT("Off-screen pure mouse preserves the other projected axis"),FMath::Abs((AfterPixel-Pixel)[1-Axis])<.05);
     TestTrue(TEXT("Off-screen delta has same pixel mapping as on-screen"),FMath::IsNearlyEqual((AfterPixel-Pixel)[Axis],Sign*(Axis?-1:1)*3.84,.05));
    }
    AddInfo(FString::Printf(TEXT("R5B MOUSE side=%d pitch=%.9g axis=%d sign=%.0f baseBefore=%s baseAfter=%s projected=%d beforePixel=%s afterPixel=%s current=%s target=%s result=%s limited=%d reason=%s motion=%.12g visualErrorM=%.12g"),Port,R.PC->GetDebugSnapshot().FishingCamera.PitchDeg,Axis,Sign,*B.Rod.BaseRodDirectionLocal.ToString(),*N.Rod.BaseRodDirectionLocal.ToString(),Projected,*Pixel.ToString(),*AfterPixel.ToString(),*N.Rod.AimCurrentPixel.ToString(),*N.Rod.AimTargetPixel.ToString(),*N.Rod.AimResult,N.Rod.bEnvelopeLimited,*N.Rod.EnvelopeReason,Motion,R5BVisual(R).TipErrorM));
   }
   TestTrue(TEXT("Opposite off-screen mouse is reversible without a viewport-edge latch"),S->RodControl->GetSnapshot().BaseRodDirectionLocal.Equals(Start.BaseRodDirectionLocal,1.e-8));
  }
 }
 return true;
}

namespace
{
 bool R5BAim(FRuntimeRodWorld& R,FVector2D Degrees)
 {
  R5BLook(R,FVector2D(0,15));
  const auto P=R.World->GetAuthGameMode<ATRGameModeBase>()->SessionConfig->Rod->Parameters;
  const FVector Desired=FRotator(Degrees.Y,Degrees.X,0).Vector();
  for(int I=0;I<180;++I)
  {
   const auto S=R.PC->GetDebugSnapshot();if(S.Rod.BaseRodDirectionLocal.Equals(Desired,1.e-7)){return true;}
   FVector2D Target,Current;
   const FQuat Basis(FVector::UpVector,FMath::DegreesToRadians(S.Station.FacingWorldDeg));
   if(!R5BProject(R,S.Rod.RootWorldPositionM+Basis.RotateVector(Desired)*S.Rod.LengthM,Target) || !R5BProject(R,R5BBaseWorld(S),Current)){return false;}
   const FVector2D Difference=Target-Current;
   if(!R.PC->SubmitMouseDelta(FVector2D(Difference.X,-Difference.Y)/(960*P.Screen.Sensitivity.X))){return false;}R.Frames(2);
  }
  return R.PC->GetDebugSnapshot().Rod.BaseRodDirectionLocal.Equals(Desired,1.e-7);
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5BExtreme,"TipRun.R5B.Runtime.ExtremeAimCameraPlaneAndRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5BExtreme::RunTest(const FString&)
{
 const TArray<FVector2D> Views={FVector2D(0,-20),FVector2D(0,20),FVector2D(0,-65),FVector2D(-55,-20),FVector2D(55,-20)};
 for(bool Port:{true,false})for(FVector2D Aim:{FVector2D(0,0),FVector2D(-39,0),FVector2D(39,0),FVector2D(0,34),FVector2D(0,-9)})
 {
  TArray<FVector> Reference;
  for(FVector2D View:Views)
  {
   FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5B_Extreme_%d_%g_%g_%g_%g"),Port,Aim.X,Aim.Y,View.X,View.Y),60,Port)){return false;}
   if(!TestTrue(TEXT("Extreme base reached only through actual Controller mouse"),R5BAim(R,Aim))){return false;}
   R.Tap(EKeys::Enter);R.Frames(480);auto* S=R.PC->GetBoundSession();const auto Base=S->GetHUDSnapshot().Rod;
   R5BLook(R,View);const auto Before=R5BVisual(R);R.Key(EKeys::RightMouseButton,true);
   double YawError=0,Peak=0,VisualError=0,Difference=0;FTRRodSnapshot PeakPose;FVector2D PeakPixel=FVector2D::ZeroVector;bool PeakProjected=false;TArray<FVector> Trajectory;
   for(int I=0;I<60;++I)
   {
    R.Frame();if(I==1){R.Key(EKeys::RightMouseButton,false);}const auto N=S->RodControl->GetSnapshot();const auto O=R5BVisual(R);Trajectory.Add(N.FinalRodTipLocal);
    const double D=(N.FinalRodTipLocal-N.BaseRodTipLocal).Size();if(D>Peak){Peak=D;PeakPose=N;PeakPixel=O.PixelTip;PeakProjected=O.bProjected;}
    YawError=FMath::Max(YawError,FMath::Abs(FMath::UnwindRadians(N.FinalYawRad-N.BaseYawRad)));VisualError=FMath::Max(VisualError,FMath::Max(O.TipErrorM,O.RootErrorM));
    TestTrue(TEXT("Vertical action preserves latest base/root/2m and 75 degree pitch-only safety"),N.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && N.RodRootLocal==Base.RodRootLocal && N.LengthM==2 && N.FinalPitchRad<=FMath::DegreesToRadians(75.)+1.e-12);
   }
   if(Reference.IsEmpty()){Reference=Trajectory;}else{for(int I=0;I<Trajectory.Num();++I){Difference=FMath::Max(Difference,(Trajectory[I]-Reference[I]).Size());}}
   const auto End=S->GetHUDSnapshot();const auto Recovered=R5BVisual(R);
   AddInfo(FString::Printf(TEXT("R5B VISIBILITY side=%d aim=%s camera=%s beforeProjected=%d peakProjected=%d recoveredProjected=%d beforeInside=%d peakInside=%d recoveredInside=%d"),Port,*Aim.ToString(),*View.ToString(),Before.bProjected,PeakProjected,Recovered.bProjected,Before.bProjected && R5BInside(Before.ViewRect,Before.PixelTip),PeakProjected && R5BInside(Before.ViewRect,PeakPixel),Recovered.bProjected && R5BInside(Recovered.ViewRect,Recovered.PixelTip)));
   TestTrue(TEXT("All five actual camera views keep identical local action and azimuth"),Difference<1.e-10 && YawError<1.e-12 && Peak>.5);
   TestTrue(TEXT("Latest base recovered, actual mesh matches and nominal remains 0.8m without Abort"),End.Rod.FinalRodDirectionLocal==Base.BaseRodDirectionLocal && VisualError<1.e-7 && End.Shakuri.CompletedCount==1 && End.Shakuri.TotalRequestedRetrieveM==.8 && !S->HasResult());
   AddInfo(FString::Printf(TEXT("R5B EXTREME side=%d aim=%s camera=%s baseYawRad=%.12g peakYawRad=%.12g yawErrorRad=%.12g peakPitchRad=%.12g peakM=%.12g trajectoryErrorM=%.12g visualBefore=%s peak=%s recover=%s meshErrorM=%.12g"),Port,*Aim.ToString(),*View.ToString(),Base.BaseYawRad,PeakPose.FinalYawRad,YawError,PeakPose.FinalPitchRad,Peak,Difference,*Before.PixelTip.ToString(),*PeakPixel.ToString(),*Recovered.PixelTip.ToString(),VisualError));
   if(View.Y!=-65){continue;}
   for(int Axis=0;Axis<2;++Axis)
   {
    const auto Start=S->RodControl->GetSnapshot();
    for(double Sign:{1.,-1.})
    {
     const auto B=S->GetHUDSnapshot();FVector2D Delta=FVector2D::ZeroVector;Delta[Axis]=Sign;
     TestTrue(TEXT("Down/extreme first mouse accepted by queue"),R.PC->SubmitMouseDelta(Delta));R.Frames(2);const auto N=S->GetHUDSnapshot();const auto O=R5BVisual(R);
     TestTrue(TEXT("Near boundary but not at envelope no mouse command lost"),N.Rod.AimResult==TEXT("Valid") && (N.Rod.BaseRodDirectionLocal-B.Rod.BaseRodDirectionLocal).Size()>1.e-8);
     TestTrue(TEXT("Mouse cannot mutate Camera/root/length; actual mesh stays exact"),N.Rod.RodRootLocal==Base.RodRootLocal && N.Rod.LengthM==2 && O.TipErrorM<1.e-7);
     AddInfo(FString::Printf(TEXT("R5B EXTREME_MOUSE aim=%s axis=%d sign=%.0f result=%s mapping=%s projection=%s sphere=%s meshPixel=%s base=%s"),*Aim.ToString(),Axis,Sign,*N.Rod.AimResult,*N.Rod.AimMapping,*N.Rod.AimProjectionStatus,*N.Rod.AimSphereResult,*O.PixelTip.ToString(),*N.Rod.BaseRodDirectionLocal.ToString()));
    }
    TestTrue(TEXT("Down/extreme opposite input reversible"),S->RodControl->GetSnapshot().BaseRodDirectionLocal.Equals(Start.BaseRodDirectionLocal,1.e-8));
   }
  }
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5BFallback,"TipRun.R5B.Runtime.BehindProjectionFallbackAndEnvelope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5BFallback::RunTest(const FString&)
{
 for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5B_Fallback_%d"),Port),60,Port)){return false;}
  if(!TestTrue(TEXT("High base via real mouse"),R5BAim(R,FVector2D(0,34)))){return false;}
  R5BLook(R,FVector2D(55,-65));auto* S=R.PC->GetBoundSession();const auto Base=S->GetHUDSnapshot().Rod;const auto Camera=R5BVisual(R).CameraRotation;
  FVector2D Pixel=FVector2D::ZeroVector;TestFalse(TEXT("Dedicated fallback uses genuinely behind Active Camera base"),R5BProject(R,R5BBaseWorld(S->GetHUDSnapshot()),Pixel));
  for(int Axis=0;Axis<2;++Axis)
  {
   const auto Start=S->RodControl->GetSnapshot();
   for(double Sign:{1.,-1.})
   {
    const auto Previous=S->RodControl->GetSnapshot();FVector2D Delta=FVector2D::ZeroVector;Delta[Axis]=Sign;TestTrue(TEXT("Behind mouse queue accepted"),R.PC->SubmitMouseDelta(Delta));R.Frames(2);const auto N=S->RodControl->GetSnapshot();
    TestTrue(TEXT("Behind uses camera-relative tangent without dead input"),N.AimMapping==TEXT("CameraTangent") && N.AimProjectionStatus==TEXT("BehindCamera") && N.AimResult==TEXT("Valid") && !N.BaseRodDirectionLocal.Equals(Previous.BaseRodDirectionLocal,1.e-9));
   }
   TestTrue(TEXT("Fallback inverse returns exact original base"),S->RodControl->GetSnapshot().BaseRodDirectionLocal.Equals(Start.BaseRodDirectionLocal,1.e-8));
  }
  R.Frames(30);TestTrue(TEXT("Fallback never owns pose on input-free ticks"),S->RodControl->GetSnapshot().BaseRodDirectionLocal.Equals(Base.BaseRodDirectionLocal,1.e-8) && R5BVisual(R).CameraRotation.Equals(Camera,1.e-8));
  // Fault only the projection matrix; retain real Active Camera, boat and queue.
  R5BLook(R,FVector2D(0,-20));const auto H=S->GetHUDSnapshot();const auto O=R5BVisual(R);FTRRodAimObservation View;
  View.bValid=true;View.CameraWorldM=O.CameraM;View.CameraRotation=O.CameraRotation;View.FOVDeg=O.FOV;View.ViewRect=O.ViewRect;View.CameraFrame=O.CameraFrame;
  View.BoatWorldM=H.Boat.PositionM;View.BoatHeadingRad=H.Boat.HeadingRad;View.Side=H.PlayerMode.FishingSide;View.bHasProjection=true;View.ProjectionScale=FVector2D::ZeroVector;
  TestTrue(TEXT("Invalid projection diagnostic still passes actual Session queue"),S->SubmitRodAim(FVector2D(.5,0),S->GetCastId(),S->GetRegistrationId(),-1,View));R.Frames(2);
  const auto N=S->RodControl->GetSnapshot();TestTrue(TEXT("Invalid projection falls back without rejecting physical aim"),N.AimMapping==TEXT("CameraTangent") && N.AimProjectionStatus==TEXT("ProjectionInvalid") && N.AimResult==TEXT("Valid") && !N.BaseRodDirectionLocal.Equals(Base.BaseRodDirectionLocal,1.e-8));
  AddInfo(FString::Printf(TEXT("R5B FALLBACK side=%d result=%s projection=%s mapping=%s direction=%s"),Port,*N.AimResult,*N.AimProjectionStatus,*N.AimMapping,*N.BaseRodDirectionLocal.ToString()));
  if(!TestTrue(TEXT("Axis-pole base via actual mouse"),R5BAim(R,FVector2D(-35,0)))){return false;}
  R5BLook(R,FVector2D(55,0));const auto Pole=S->GetHUDSnapshot();const auto PoleCamera=R5BVisual(R);
  View.CameraWorldM=PoleCamera.CameraM;View.CameraRotation=PoleCamera.CameraRotation;View.CameraFrame=PoleCamera.CameraFrame;
  View.BoatWorldM=Pole.Boat.PositionM;View.BoatHeadingRad=Pole.Boat.HeadingRad;
  TestTrue(TEXT("Invalid projection at camera-axis pole queued"),S->SubmitRodAim(FVector2D(.5,0),S->GetCastId(),S->GetRegistrationId(),-1,View));R.Frames(2);
  const auto PoleMoved=S->RodControl->GetSnapshot();
  TestTrue(TEXT("Degenerate camera-axis tangent has safe meridian recovery, not a permanent rejection"),PoleMoved.AimResult==TEXT("Valid") && PoleMoved.AimMapping==TEXT("CameraTangent") && !PoleMoved.BaseRodDirectionLocal.Equals(Pole.Rod.BaseRodDirectionLocal,1.e-8) && !PoleMoved.BaseRodDirectionLocal.ContainsNaN() && PoleMoved.RodRootLocal==Pole.Rod.RodRootLocal && PoleMoved.LengthM==2);
 }
 for(bool Port:{true,false})for(double CameraYaw:{-55.,55.})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5B_HorizontalOffscreen_%d_%g"),Port,CameraYaw),60,Port)){return false;}
  R5BLook(R,FVector2D(CameraYaw,-20));auto* S=R.PC->GetBoundSession();const auto Start=S->GetHUDSnapshot();FVector2D Pixel=FVector2D::ZeroVector;
  TestTrue(TEXT("Saved camera is inside physical grip sphere so offscreen ray cannot miss"),(R5BVisual(R).CameraM-Start.Rod.RootWorldPositionM).Size()<Start.Rod.LengthM);
  TestTrue(TEXT("Dedicated left/right offscreen is valid front-facing projection"),R5BProject(R,R5BBaseWorld(Start),Pixel) && (Pixel.X<0 || Pixel.X>1920));
  const double Inward=Pixel.X<0?1.:-1.;
  TestTrue(TEXT("First mouse from horizontal offscreen queues"),R.PC->SubmitMouseDelta(FVector2D(Inward,0)));R.Frames(2);const auto M=S->GetHUDSnapshot();FVector2D After=FVector2D::ZeroVector;R5BProject(R,R5BBaseWorld(M),After);
  TestTrue(TEXT("First horizontal offscreen input moves toward screen without cross-axis warp"),M.Rod.AimResult==TEXT("Valid") && M.Rod.AimMapping==TEXT("UnboundedProjection") && M.Rod.bAimOutsideViewRect && FMath::IsNearlyEqual((After-Pixel).X,Inward*3.84,.05) && FMath::Abs((After-Pixel).Y)<.05);
  R.PC->SubmitMouseDelta(FVector2D(-Inward,0));R.Frames(2);TestTrue(TEXT("Horizontal offscreen opposite reverses immediately"),S->RodControl->GetSnapshot().BaseRodDirectionLocal.Equals(Start.Rod.BaseRodDirectionLocal,1.e-8));
  AddInfo(FString::Printf(TEXT("R5B HORIZONTAL side=%d cameraYaw=%g before=%s after=%s"),Port,CameraYaw,*Pixel.ToString(),*After.ToString()));
 }
 for(bool Port:{true,false})for(FVector2D Aim:{FVector2D(-40,0),FVector2D(40,0),FVector2D(0,-10),FVector2D(0,35)})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5B_Boundary_%d_%g_%g"),Port,Aim.X,Aim.Y),60,Port)){return false;}
  if(!TestTrue(TEXT("Actual ergonomic boundary reached through mouse"),R5BAim(R,Aim))){return false;}
  auto* S=R.PC->GetBoundSession();const auto Boundary=S->RodControl->GetSnapshot();const int Axis=Aim.X==0?1:0;const double Sign=Aim[Axis]>0?1:-1;FVector2D Delta=FVector2D::ZeroVector;Delta[Axis]=Sign*20;
  R.PC->SubmitMouseDelta(Delta);R.Frames(2);const auto Out=S->RodControl->GetSnapshot();
  TestTrue(TEXT("True ergonomic boundary diagnosed, never ViewRect boundary"),Out.bEnvelopeLimited && Out.EnvelopeReason.StartsWith(TEXT("Envelope")) && Out.AimResult!=TEXT("ViewportBoundary"));
  Delta[Axis]=-Sign;R.PC->SubmitMouseDelta(Delta);R.Frames(2);const auto In=S->RodControl->GetSnapshot();
  TestTrue(TEXT("First opposite input immediately returns inside true envelope"),In.AimResult==TEXT("Valid") && (In.BaseRodDirectionLocal-Out.BaseRodDirectionLocal).Size()>1.e-8 && FMath::Abs(Axis?In.BasePitchRad:In.BaseYawRad)<FMath::Abs(Axis?Boundary.BasePitchRad:Boundary.BaseYawRad));
  AddInfo(FString::Printf(TEXT("R5B BOUNDARY side=%d aim=%s outward=%s inward=%s insideDirection=%s"),Port,*Aim.ToString(),*Out.EnvelopeReason,*In.AimResult,*In.BaseRodDirectionLocal.ToString()));
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5BReplay,"TipRun.R5B.Runtime.OffscreenFixedTickReplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5BReplay::RunTest(const FString&)
{
 for(bool Port:{true,false})
 {
  FVector Reference;TArray<FString> ReferenceEvents;
  for(int FPS:{30,60,120})
  {
   FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5B_Replay_%d_%d"),Port,FPS),60,Port)){return false;}
   R5BLook(R,FVector2D(0,-65));auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();const auto Base=S->RodControl->GetSnapshot();const int64 Start=Sim->GetSimulationTime().TickIndex+120;
   TArray<FString> Events;const auto Handler=S->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result)
   {if(C.Type==ETRFishingCommandType::RodAim){Events.Add(FString::Printf(TEXT("%lld:%d:%s:%s"),C.TargetTick-Start,int32(Result),*S->RodControl->GetSnapshot().AimMapping,*S->RodControl->GetSnapshot().AimResult));}});
   // Same actual camera observation and fixed Tick/Sequence across render fps.
   for(int I=0;I<400;++I)
   {const FVector2D D=I%4==0?FVector2D(1,0):I%4==1?FVector2D(-1,0):I%4==2?FVector2D(0,1):FVector2D(0,-1);TestTrue(TEXT("Offscreen replay actual Controller queues"),R.PC->SubmitMouseDelta(D,Start+I*2));}
   R.Dt=1./FPS;while(Sim->GetSimulationTime().TickIndex<Start+800){R.Frame();}S->OnCommandProcessed.Remove(Handler);const auto N=S->RodControl->GetSnapshot();
   TestTrue(TEXT("All inputs consumed, no accumulation, physical root/length and mesh agree"),Events.Num()==400 && N.BaseRodDirectionLocal.Equals(Base.BaseRodDirectionLocal,1.e-8) && N.RodRootLocal==Base.RodRootLocal && N.LengthM==2 && R5BVisual(R).TipErrorM<1.e-7);
   if(ReferenceEvents.IsEmpty()){Reference=N.BaseRodDirectionLocal;ReferenceEvents=Events;}else{TestTrue(TEXT("Offscreen pose and every command outcome deterministic at 30/60/120fps"),Reference.Equals(N.BaseRodDirectionLocal,1.e-12) && Events==ReferenceEvents);}
   AddInfo(FString::Printf(TEXT("R5B REPLAY side=%d fps=%d events=%d returnError=%.12g"),Port,FPS,Events.Num(),(N.BaseRodDirectionLocal-Base.BaseRodDirectionLocal).Size()));
  }
 }
 return true;
}

// R5C: saved Runtime, real off-screen camera and raw click events. No solver injection.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5CRepeat,"TipRun.R5C.Runtime.OffscreenRepeated",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5CRepeat::RunTest(const FString&)
{
 for(bool Port:{true,false})for(int DepthFrames:{4,60,480})for(int ViewIndex=0;ViewIndex<4;++ViewIndex)for(int Count:{1,2,3,5,10,20,50})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;const FString Case=FString::Printf(TEXT("R5C_%d_%d_%d_%d"),Port,DepthFrames,ViewIndex,Count);
  if(!R.Start(*this,Case,60,Port)){return false;}auto* S=R.PC->GetBoundSession();
  // High base, viewed from below, or opposite-side azimuth: tip demonstrably outside the actual ViewRect.
  const FVector2D Aim=ViewIndex==0?FVector2D(0,34):ViewIndex==3?FVector2D(0,-9):FVector2D(ViewIndex==1?-39:39,0);
  if(!TestTrue(TEXT("Saved control reaches base through real Controller mouse"),R5BAim(R,Aim))){return false;}
  const FVector2D View=ViewIndex==0?FVector2D(0,-65):ViewIndex==3?FVector2D(0,20):FVector2D(ViewIndex==1?55:-55,-20);
  R5BLook(R,View);const auto Base=S->RodControl->GetSnapshot();const auto O=R5BVisual(R);
  TestTrue(TEXT("Fixture actual visual tip is off-screen"),!O.bProjected || !R5BInside(O.ViewRect,O.PixelTip));
  R.Tap(EKeys::Enter);R.Frames(DepthFrames);int Requests=0;auto Handle=S->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result){if(C.Type==ETRFishingCommandType::Jerk && (Result==ETRCommandResult::Accepted || Result==ETRCommandResult::Queued)){++Requests;}});
  bool Finite=true,Fixed=true,Demand=true;double MaxCorrection=0,MaxSpeed=0;
  auto Capture=[&](){const auto N=S->GetHUDSnapshot();Finite &= !N.Egi.WorldPositionM.ContainsNaN() && !N.Egi.VelocityMps.ContainsNaN() && FMath::IsFinite(N.Egi.LineLengthM) && N.Egi.LineLengthM>=0;
   Fixed &= N.Rod.RodRootLocal==Base.RodRootLocal && N.Rod.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && N.Rod.LengthM==2 && R.Observe().TipErrorM<1.e-7;
   Demand &= N.Shakuri.TotalActualRetrieveM<=N.Shakuri.TotalRequestedRetrieveM+1.e-9;
   MaxCorrection=FMath::Max(MaxCorrection,N.Egi.LineConstraintCorrectionM);MaxSpeed=FMath::Max(MaxSpeed,N.Egi.VelocityMps.Size());
   if(N.Egi.Tick%24==0 || S->HasResult()){R.Trace+=S->GetRuntimeDiagnostics()+TEXT("\n")+S->DiagnosticAbortContext+TEXT("\n");}};
  for(int I=0;I<Count && !S->HasResult();++I){R.Key(EKeys::RightMouseButton,true);R.Frames(2);Capture();R.Key(EKeys::RightMouseButton,false);R.Frames(2);}
  for(int I=0;I<Count*24+120 && !S->HasResult();++I){R.Frame();Capture();}
  S->OnCommandProcessed.Remove(Handle);const auto H=S->GetHUDSnapshot();Capture();
  if(S->HasResult())
  {
   AddError(Case+TEXT(" reproduced stop: ")+S->DiagnosticAbortContext+TEXT("\nAFTER: ")+S->GetRuntimeDiagnostics());
   const bool Recover=S->IsRecoveryAvailable();R.Tap(EKeys::N);R.Frames(4);
   AddInfo(FString::Printf(TEXT("%s N recovery=%d ready=%d onboard=%d unlocked=%d retainedAborted=%d"),*Case,Recover,S->Fishing->GetState()==ETRFishingState::Ready,S->IsEgiOnboard(),S->CanChangeEquipment(),S->GetLastResult().Outcome==ETRCastOutcome::Aborted));continue;
  }
  TestTrue(TEXT("All observed frames finite/fixed/within nominal demand; no excessive motion"),Finite && Fixed && Demand && MaxSpeed<=10+1.e-8);
  TestEqual(TEXT("All click requests accepted/queued"),Requests,Count);TestEqual(TEXT("Exact completed sequence count"),H.Shakuri.CompletedCount,int64(Count));
  TestTrue(TEXT("Stay is operable with no pending action"),H.Egi.FishingState==ETRFishingState::Stay && S->IsCommandAvailable(ETRFishingCommandType::Jerk) && S->IsCommandAvailable(ETRFishingCommandType::QuickRetrieve) && !H.Shakuri.bPendingRetrieve && !H.Shakuri.bPendingReFall);
  TestTrue(TEXT("No residual queue/temporary or base/root/length drift"),H.Shakuri.QueuedCount==0 && H.Rod.TemporaryShakuriOffsetRad==0 && H.Rod.FinalRodDirectionLocal==Base.BaseRodDirectionLocal && H.Rod.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && H.Rod.RodRootLocal==Base.RodRootLocal && H.Rod.LengthM==2);
  TestTrue(TEXT("Retrieve demand is a ceiling, not a guaranteed amount"),H.Shakuri.TotalActualRetrieveM<=H.Shakuri.TotalRequestedRetrieveM+1.e-9);
  AddInfo(FString::Printf(TEXT("%s PASS completed=%lld depth=%.9g line=%.9g required=%.9g requested=%.12g actual=%.12g"),*Case,H.Shakuri.CompletedCount,double(H.Egi.DepthM),double(H.Egi.LineLengthM),(H.Egi.WorldPositionM-H.Rod.TipWorldPositionM).Size(),H.Shakuri.TotalRequestedRetrieveM,H.Shakuri.TotalActualRetrieveM));
 }
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5CProbe,"TipRun.R5C.Runtime.LowBaseCadenceProbe",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5CProbe::RunTest(const FString&)
{
 for(bool Port:{true,false})for(double Pitch:{-9.,0.,34.})for(int Wait:{4,480})for(int Cadence:{4,26,60})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;const FString Case=FString::Printf(TEXT("R5C_Probe_%d_%g_%d_%d"),Port,Pitch,Wait,Cadence);
  if(!R.Start(*this,Case,60,Port) || !R5BAim(R,FVector2D(0,Pitch))){return false;}auto* S=R.PC->GetBoundSession();R5BLook(R,FVector2D(0,-65));R.Tap(EKeys::Enter);R.Frames(Wait);
  int Click=0;
  for(;Click<50 && !S->HasResult();++Click)
  {
   R.Key(EKeys::RightMouseButton,true);R.Frames(2);R.Key(EKeys::RightMouseButton,false);R.Frames(Cadence-2);
   R.Trace+=S->GetRuntimeDiagnostics()+TEXT("\n")+S->DiagnosticAbortContext+TEXT("\n");
  }
  for(int I=0;I<50*24+120 && !S->HasResult();++I){R.Frame();if(I%24==0){R.Trace+=S->GetRuntimeDiagnostics()+TEXT("\n")+S->DiagnosticAbortContext+TEXT("\n");}}
  if(S->HasResult())
  {AddError(Case+FString::Printf(TEXT(" stopped after clicks=%d\n"),Click)+S->DiagnosticAbortContext+TEXT("\nAFTER: ")+S->GetRuntimeDiagnostics());R.Tap(EKeys::N);AddInfo(Case+TEXT(" N: ")+S->GetRuntimeDiagnostics());}
  else{const auto H=S->GetHUDSnapshot();TestEqual(TEXT("50 recover completions"),H.Shakuri.CompletedCount,int64(50));AddInfo(Case+FString::Printf(TEXT(" PASS depth=%.9g line=%.9g request=%.12g actual=%.12g"),double(H.Egi.DepthM),double(H.Egi.LineLengthM),H.Shakuri.TotalRequestedRetrieveM,H.Shakuri.TotalActualRetrieveM));}
 }
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5CSurface,"TipRun.R5C.Runtime.SurfaceLookAndMouse",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5CSurface::RunTest(const FString&)
{
 for(bool Port:{true,false})for(int Motion=0;Motion<4;++Motion)
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;const FString Case=FString::Printf(TEXT("R5C_Surface_%d_%d"),Port,Motion);
  if(!R.Start(*this,Case,60,Port)){return false;}auto* S=R.PC->GetBoundSession();R.Tap(EKeys::Enter);
  for(int I=0;I<3600 && S->GetHUDSnapshot().Egi.DepthM<5 && !S->HasResult();++I){R.Frame();}
  AddInfo(Case+FString::Printf(TEXT(" start depth=%.12g"),double(S->GetHUDSnapshot().Egi.DepthM)));
  for(int I=0;I<10 && !S->HasResult();++I){R.Tap(EKeys::RightMouseButton);}
  // Reproduce the reported sequence: look down and swing sideways while queued actions pull the Egi back to the surface.
  R.Key(EKeys::S,true);R.Key(EKeys::D,true);
  for(int I=0;I<600 && !S->HasResult();++I)
  {
   if(I==120){R.Key(EKeys::S,false);R.Key(EKeys::D,false);}
   if(I==200){R.Key(EKeys::A,true);}if(I==360){R.Key(EKeys::A,false);}
   if(Motion>0 && I>=90)
   {const double Sign=(I/30)%2?-1:1;const FVector2D Delta(Sign*(Motion==1?1:Motion==2?20:200),0);R.PC->SubmitMouseDelta(Delta);}
   R.Frame();R.Trace+=S->GetRuntimeDiagnostics()+TEXT("\n")+S->DiagnosticAbortContext+TEXT("\n");
  }
  R.Key(EKeys::S,false);R.Key(EKeys::D,false);R.Key(EKeys::A,false);
  if(S->HasResult()){AddError(Case+TEXT(" reproduced freeze\n")+S->DiagnosticAbortContext+TEXT("\nAFTER:\n")+S->GetRuntimeDiagnostics());R.Tap(EKeys::N);AddInfo(Case+TEXT(" N recovery: ")+S->GetRuntimeDiagnostics());}
  else
  {
   const auto N=S->GetHUDSnapshot();TestEqual(TEXT("Reported ten-click case completes all sequences"),N.Shakuri.CompletedCount,int64(10));
   TestTrue(TEXT("No stuck action and nominal actual ceiling"),N.Shakuri.QueuedCount==0 && N.Rod.TemporaryShakuriOffsetRad==0 && N.Rod.FinalRodDirectionLocal==N.Rod.BaseRodDirectionLocal && N.Shakuri.TotalActualRetrieveM<=8.+1.e-9 && S->IsCommandAvailable(ETRFishingCommandType::Jerk));
   AddInfo(Case+TEXT(" PASS ")+S->GetRuntimeDiagnostics());
   R.Tap(EKeys::Q);R.Frames(180);TestTrue(TEXT("After repeated surface actions Quick works, no N recovery needed"),S->CanChangeEquipment() && S->IsEgiOnboard() && S->GetLastResult().bQuickRetrieved);
  }
 }
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR5CReplay,"TipRun.R5C.Runtime.FiftyOffscreenFixedTickReplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR5CReplay::RunTest(const FString&)
{
 for(bool Port:{true,false})
 {
  TArray<FString> Reference;FVector ReferenceEgi;double ReferenceActual=0;
  for(int FPS:{30,60,120})
  {
   FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R5C_Replay_%d_%d"),FPS,Port),60,Port)){return false;}
   R5BLook(R,FVector2D(0,-65));auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();const auto Base=S->RodControl->GetSnapshot();
   const auto O=R5BVisual(R);TestTrue(TEXT("Replay starts with actual offscreen mesh"),!O.bProjected || !R5BInside(O.ViewRect,O.PixelTip));
   const int64 Start=Sim->GetSimulationTime().TickIndex+120;R.Dt=1./FPS;
   TestTrue(TEXT("Real controller queues Deploy"),R.PC->ActionStarted(ETRPlayerAction::Deploy,Start));R.PC->ActionReleased(ETRPlayerAction::Deploy);
   TArray<FString> Events;auto H=S->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& C,ETRCommandResult Result){Events.Add(FString::Printf(TEXT("%lld:%d:%d"),C.TargetTick,int32(C.Type),int32(Result)));});
   auto SH=S->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState,ETRFishingState To){Events.Add(FString::Printf(TEXT("state:%lld:%d"),Sim->GetSimulationTime().TickIndex,int32(To)));});
   while(Sim->GetSimulationTime().TickIndex<Start+60){R.Frame();}
   for(int I=0;I<50;++I){TestTrue(TEXT("One Started per queued Shakuri"),R.PC->ActionStarted(ETRPlayerAction::Jerk,Start+600+I*2));R.PC->ActionReleased(ETRPlayerAction::Jerk);}
   while(Sim->GetSimulationTime().TickIndex<Start+2000){R.Frame();}
   S->OnCommandProcessed.Remove(H);S->Fishing->OnFishingStateChanged.Remove(SH);const auto N=S->GetHUDSnapshot();
   TestTrue(TEXT("50 sequences without Abort/lock, queue0/temp0/base/root/length fixed"),!S->HasResult() && N.Shakuri.CompletedCount==50 && N.Shakuri.QueuedCount==0 && N.Rod.TemporaryShakuriOffsetRad==0 && N.Rod.BaseRodDirectionLocal==Base.BaseRodDirectionLocal && N.Rod.FinalRodDirectionLocal==Base.BaseRodDirectionLocal && N.Rod.RodRootLocal==Base.RodRootLocal && N.Rod.LengthM==2);
   TestTrue(TEXT("50 turns, 40m nominal is not forced"),N.Shakuri.TotalRequestedHandleTurns==50 && FMath::IsNearlyEqual(N.Shakuri.TotalRequestedRetrieveM,40.,1.e-9) && N.Shakuri.TotalActualRetrieveM<=40.);
   if(FPS==30){Reference=Events;ReferenceEgi=N.Egi.WorldPositionM;ReferenceActual=N.Shakuri.TotalActualRetrieveM;}
   else{TestTrue(TEXT("30/60/120 exact input/state events and same physical endpoint/reel"),Events==Reference && ReferenceEgi.Equals(N.Egi.WorldPositionM,1.e-8) && ReferenceActual==N.Shakuri.TotalActualRetrieveM);}
   AddInfo(FString::Printf(TEXT("R5C replay side=%d fps=%d count=%lld depth=%.12g line=%.12g required=%.12g requested=%.12g actual=%.12g"),Port,FPS,N.Shakuri.CompletedCount,double(N.Egi.DepthM),double(N.Egi.LineLengthM),N.Egi.RodToEgiDistanceM,N.Shakuri.TotalRequestedRetrieveM,N.Shakuri.TotalActualRetrieveM));
  }
 }
 return true;
}

#endif
