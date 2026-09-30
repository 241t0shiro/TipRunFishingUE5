// R4A-1: actual Game World tick, saved map, LocalPlayer projection and mesh endpoints.
// KnownFailure.* intentionally stays red. Do not add expected-error suppression.
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
#include "HAL/FileManager.h"
#include "Tickable.h"
#include "Game/TRPlayerController.h"
#include "Game/TRPlayerCameraManager.h"
#include "Game/TRFishingSessionActor.h"
#include "Game/TRPrototypeViewActor.h"
#include "Game/TRGameModeBase.h"
#include "Fishing/TRFishingComponent.h"
#include "UI/TRFishingHUDWidget.h"
#include "Data/TRSessionConfigDataAsset.h"
#include "Data/TRRodTuningDataAsset.h"
#include "UObject/StrongObjectPtr.h"

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
  bool Start(FAutomationTestBase& Test,const FString& CaseName,int FPS=60,bool Port=true)
  {
   Name=CaseName;Dt=1./FPS;OldDelta=FApp::GetDeltaTime();PreviousWorld=GWorld;PreviousViewport=GEngine->GameViewport;
   Instance.Reset(NewObject<UGameInstance>(GEngine));Instance->InitializeStandalone();
   Client.Reset(NewObject<UGameViewportClient>(GEngine));
   auto& Context=*Instance->GetWorldContext();Client->Init(Context,Instance.Get(),false);Context.GameViewport=Client.Get();GEngine->GameViewport=Client.Get();
   // LoadMap's Game-world path requires an instance ID to load an independent
   // disk copy when the saved map is already open in the Editor. Never reuse it.
   static int32 RuntimeInstance=1000;Context.PIEInstance=++RuntimeInstance;
   Widget=SNew(SViewport);Viewport=FSceneViewport::Create(TStrongPtrVariant<FViewportClient>(Client.Get()),Widget);Widget->SetViewportInterface(Viewport.ToSharedRef());
   Viewport->SetInitialSize(FIntPoint(1920,1080));
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
   Frames(6);Tap(EKeys::Enter);Tap(Port?EKeys::A:EKeys::D);Tap(EKeys::Enter);Frames(6);
   const auto S=PC->GetDebugSnapshot();const auto O=Observe();
   Trace+=FString::Printf(TEXT("BOOT saved map=%s mode=%s worldType=%d begun=%d local=%d\n"),*World->GetPathName(),*World->GetAuthGameMode()->GetClass()->GetPathName(),int32(World->WorldType),World->HasBegunPlay(),PC->IsLocalController());
   return Test.TestTrue(TEXT("Saved world reaches Fishing Ready with real projection"),S.PlayerMode.Mode==ETRPlayerMode::Fishing && S.Egi.FishingState==ETRFishingState::Ready && O.bValid && O.bProjected && O.ViewRect.Width()==1920 && O.VisualFrame==O.CameraFrame);
  }
  void Frame()
  {
   ++GFrameCounter;FApp::SetDeltaTime(Dt);GWorld=World;
   World->Tick(LEVELTICK_All,float(Dt));
   // UGameEngine/UEditorEngine tick world-less services after their worlds.
   // Enhanced Input rebuilds pending mapping contexts here; a World tick alone
   // would leave every saved mapping pending forever in this synchronous test.
   FTickableGameObject::TickObjects(nullptr,LEVELTICK_All,false,float(Dt));
   if(PC && Visual){Trace+=FString::Printf(TEXT("\nFRAME %llu\n%s\n"),GFrameCounter,*PC->GetDebugSnapshot().RuntimeDiagnostics);}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeCameraFailure,"TipRun.R4A1.KnownFailure.CameraOnlyIndependence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeCameraFailure::RunTest(const FString&)
{
 for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;if(!R.Start(*this,Port?TEXT("Camera_Port"):TEXT("Camera_Starboard"),60,Port)){return false;}
  const auto Before=R.Observe();R.Key(EKeys::D,true);R.Frames(20);R.Key(EKeys::D,false);R.Frames(4);const auto After=R.Observe();
  TestTrue(TEXT("PASS fixed boat-local grip"),After.RootLocal.Equals(Before.RootLocal,1.e-8));
  TestTrue(TEXT("PASS camera actually changed"),!After.CameraRotation.Equals(Before.CameraRotation,1.e-4));
  AddInfo(FString::Printf(TEXT("LOCAL movement root=%.9g tip=%.9g direction=%.9g"),(After.RootLocal-Before.RootLocal).Size(),(After.TipLocal-Before.TipLocal).Size(),(After.DirectionLocal-Before.DirectionLocal).Size()));
  TestTrue(TEXT("EXPECTED FAIL R4: Camera-only must preserve local rod direction"),After.StationDirectionLocal.Equals(Before.StationDirectionLocal,1.e-8));
  TestTrue(TEXT("EXPECTED FAIL R4: Camera-only must preserve local rod tip"),After.StationTipLocal.Equals(Before.StationTipLocal,1.e-8));
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRuntimeMouse,"TipRun.R4A1.Runtime.MouseProjectionAndLength",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRuntimeMouse::RunTest(const FString&)
{
 for(int FPS:{30,60,120})for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;if(!R.Start(*this,FString::Printf(TEXT("Mouse_%d_%d"),FPS,Port),FPS,Port)){return false;}
  double Min=DBL_MAX,Max=0,Cross[2]={0,0};int Moved[2]={0,0};
  const auto Camera=R.Observe().CameraRotation;
  for(int Axis=0;Axis<2;++Axis)for(double Sign:{1.,-1.})
  {
   // Drain previous-axis device smoothing before isolating the next axis.
   for(int I=0;I<30;++I){R.Mouse(FVector2D::ZeroVector);}
   for(int I=0;I<180;++I)
   {
    const auto Before=R.Observe();FVector2D D=FVector2D::ZeroVector;D[Axis]=Sign*20;R.Mouse(D);const auto O=R.Observe();
    TestTrue(TEXT("Runtime mesh endpoints match snapshot"),O.RootErrorM<1.e-7 && O.TipErrorM<1.e-7);
    TestTrue(TEXT("World/visual fixed length and active projection"),O.bProjected && FMath::IsNearlyEqual(O.LengthM,R.ConfiguredLengthM,1.e-8));
    Cross[Axis]=FMath::Max(Cross[Axis],FMath::Abs((O.PixelTip-Before.PixelTip)[1-Axis]));
    const double Along=(O.PixelTip-Before.PixelTip)[Axis]*(Axis?-Sign:Sign);if(Along>.001){++Moved[Axis];}
    Min=FMath::Min(Min,(O.PixelTip-O.PixelRoot).Size());Max=FMath::Max(Max,(O.PixelTip-O.PixelRoot).Size());
   }
  }
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
  else{++Active;AddInfo(FString::Printf(TEXT("NO ABORT delay=%d state=%s"),Delay,*S->GetRuntimeDiagnostics()));}
 }
 AddInfo(FString::Printf(TEXT("Lock sweep aborted=%d normalResults=%d active=%d; zero abort means NOT REPRODUCED, not bug fixed"),Aborts,Results,Active));
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
 for(const TCHAR* Field:{TEXT("InputContext="),TEXT("Mouse raw="),TEXT("Smoothing="),TEXT("pendingJerk="),TEXT("RetrieveHeld="),TEXT("registration="),TEXT("Failure="),TEXT("StationRootLocal="),TEXT("cameraFrame="),TEXT("errorTip="),TEXT("Available=")})
 {TestTrue(Field,After.RuntimeDiagnostics.Contains(Field));}
 const auto Rows=UTRFishingHUDWidget::BuildReadout(After);
 TestTrue(TEXT("Detailed HUD receives runtime values"),Rows.Last().Value.ToString()==After.RuntimeDiagnostics);
 TestFalse(TEXT("Runtime detail row stays out of compact HUD"),UTRFishingHUDWidget::IsPrimaryReadout(Rows.Num()-1));
 TestFalse(TEXT("Details initially closed"),R.PC->IsPrototypeDetailsOpen());
 R.Tap(EKeys::Insert);TestTrue(TEXT("Real Insert opens details"),R.PC->IsPrototypeDetailsOpen());
 R.Tap(EKeys::Insert);TestFalse(TEXT("Real Insert closes details"),R.PC->IsPrototypeDetailsOpen());
 return true;
}
#endif
