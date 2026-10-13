// R7C uses the saved Runtime path. Depth changes the seabed only, not the current keys.
namespace
{
 UStaticMeshComponent* R7CMesh(ATRPrototypeViewActor* View,FName Name)
 {TArray<UStaticMeshComponent*> Components;View->GetComponents(Components);for(auto* C:Components){if(C->GetFName()==Name){return C;}}return nullptr;}
 bool R7CBackgroundBlocks(UStaticMeshComponent* Background,FVector CameraM,FVector TargetM)
 {
  const auto Transform=Background->GetComponentTransform();const auto Box=Background->GetStaticMesh()->GetBoundingBox();
  const FVector A=Transform.InverseTransformPosition(CameraM*100),B=Transform.InverseTransformPosition(TargetM*100),D=B-A;
  double Enter=0,Exit=1;
  for(int Axis=0;Axis<3;++Axis)
  {if(FMath::Abs(D[Axis])<1.e-12){if(A[Axis]<Box.Min[Axis] || A[Axis]>Box.Max[Axis]){return false;}}
   else{double Lo=(Box.Min[Axis]-A[Axis])/D[Axis],Hi=(Box.Max[Axis]-A[Axis])/D[Axis];if(Lo>Hi){Swap(Lo,Hi);}Enter=FMath::Max(Enter,Lo);Exit=FMath::Min(Exit,Hi);if(Enter>Exit){return false;}}}
  return Enter<1-1.e-6 && Exit>1.e-6;
 }
 struct FTRR7CVisualProbe
 {
  int InView=0,Blocked=0;
  bool Drawable()const{return InView>Blocked;}
 };
 FTRR7CVisualProbe R7CProbe(FRuntimeRodWorld& R,UStaticMeshComponent* Mesh,UStaticMeshComponent* Background)
 {
  FTRR7CVisualProbe P;if(!Mesh || !Mesh->IsVisible() || !Mesh->GetStaticMesh()){return P;}
  auto* LP=R.PC->GetLocalPlayer();FSceneViewProjectionData Data;if(!LP->GetProjectionData(LP->ViewportClient->Viewport,Data)){return P;}
  const auto Rect=Data.GetConstrainedViewRect();const FMatrix VP=Data.ComputeViewProjectionMatrix();
  const auto Box=Mesh->GetStaticMesh()->GetBoundingBox();const auto T=Mesh->GetComponentTransform();const FVector Camera=R.PC->PlayerCameraManager->GetCameraCacheView().Location*.01;
  for(int I=0;I<63;++I)
  {
   // Long gunwales cross the view even when all box corners are outside.
   const FVector Local(FMath::Lerp(Box.Min.X,Box.Max.X,double(I/3)/20.),Box.GetCenter().Y,FMath::Lerp(Box.Min.Z,Box.Max.Z,double(I%3)/2.));
   const FVector Point=T.TransformPosition(Local);FVector2D Pixel;
   if(FSceneView::ProjectWorldToScreen(Point,Rect,VP,Pixel) && Pixel.X>=Rect.Min.X && Pixel.X<=Rect.Max.X && Pixel.Y>=Rect.Min.Y && Pixel.Y<=Rect.Max.Y)
   {++P.InView;if(R7CBackgroundBlocks(Background,Camera,Point*.01)){++P.Blocked;}}
  }
  return P;
 }
 FString R7CTransform(USceneComponent* C)
 {return FString::Printf(TEXT("%s locationCM=%s rotation=%s scale=%s boundsOriginCM=%s boundsExtentCM=%s"),*C->GetName(),*C->GetComponentLocation().ToString(),*C->GetComponentRotation().ToString(),*C->GetComponentScale().ToString(),*C->Bounds.Origin.ToString(),*C->Bounds.BoxExtent.ToString());}
 FString R7CDescribe(FRuntimeRodWorld& R)
 {
  const auto H=R.PC->GetDebugSnapshot();const auto O=R.Observe();
  FString Out=FString::Printf(TEXT("tick=%lld frame=%llu mode=%d side=%d boatM=%s heading=%.9g velocity=%s stationPlayerM=%s stationCameraM=%s activeCameraM=%s activeRotation=%s rodRootM=%s observerWorld=%s envValid=%d\n"),H.Boat.Tick,GFrameCounter,int(H.PlayerMode.Mode),int(H.Station.Side),*H.Boat.PositionM.ToString(),H.Boat.HeadingRad,*H.Boat.VelocityMps.ToString(),*H.Station.PlayerWorldM.ToString(),*H.Station.CameraWorldM.ToString(),*O.CameraM.ToString(),*O.CameraRotation.ToString(),*H.Rod.RootWorldPositionM.ToString(),*R.Visual->GetActorTransform().ToString(),H.bEnvironmentValid);
  for(FName Name:{TEXT("BoatObservation"),TEXT("PortGunwale"),TEXT("StarboardGunwale"),TEXT("ObservationBackground"),TEXT("Seabed30m"),TEXT("SurfaceNorth")}){if(auto* M=R7CMesh(R.Visual,Name)){Out+=R7CTransform(M)+TEXT("\n");}}
  return Out;
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7CDepth,"TipRun.R7C.Runtime.DepthAndWeightAudit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7CDepth::RunTest(const FString&)
{
 FString Rows=R7BHeader().Replace(TEXT("\n"),TEXT(",bottomRodHorizontal,bottomVertical,depth\n"));
 for(double Depth:{10.,20.,30.,40.,50.})
 {
  FTRR7BMeasure Measures[3];
  for(int I=0;I<3;++I)
  {
   if(I!=1 && Depth!=10 && Depth!=30 && Depth!=50){continue;}
   auto* Saved=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));
   TStrongObjectPtr<UTRSessionConfigDataAsset> C(DuplicateObject<UTRSessionConfigDataAsset>(Saved,GetTransientPackage()));
   TStrongObjectPtr<UTROceanAreaDataAsset> O(DuplicateObject<UTROceanAreaDataAsset>(Saved->Ocean.Get(),GetTransientPackage()));
   C->Ocean=O.Get();O->Settings.FlatDepthM=float(Depth);
   TestTrue(TEXT("Depth fixture keeps saved profile keys and wind, boat and fishing references"),O->Settings.CurrentDepthProfile.Num()==3 && O->Settings.CurrentDepthProfile.Last().DepthM==30 && O->Settings.WindMps==Saved->Ocean->Settings.WindMps && C->Boat==Saved->Boat && C->Fishing==Saved->Fishing);
   FName Egi=I==0?FName(TEXT("Egi_3")):I==1?FName(TEXT("Egi_3_5")):FName(TEXT("Egi_4"));const FString Name=TEXT("R7C_Depth_")+R7Number(Depth)+TEXT("_")+Egi.ToString();auto& M=Measures[I];
   if(!R7BMeasure(*this,Name,nullptr,M,true,Egi,NAME_None,-1,0,C.Get())){return false;}
   Rows+=R7BRow(Name,M).Replace(TEXT("\n"),*(TEXT(",")+R7Number(M.Bottom.RelativeDrift.RodEgiHorizontalSeparationM)+TEXT(",")+R7Number(M.Bottom.RelativeDrift.VerticalSeparationM)+TEXT(",")+R7Number(Depth)+TEXT("\n")));
   TestTrue(TEXT("Angle equals measured atan2, never a depth mapping"),FMath::IsNearlyEqual(M.Bottom.RelativeDrift.LineAngleToSurfaceDeg,FMath::RadiansToDegrees(FMath::Atan2(M.Bottom.RelativeDrift.VerticalSeparationM,M.Bottom.RelativeDrift.RodEgiHorizontalSeparationM)),1.e-10));
   if(Depth==30 && I==1){TestTrue(TEXT("30m saved R7B acceptance retained"),M.Good() && FMath::Abs(M.Delta())<=.3);}
  }
  if(Depth==10 || Depth==30 || Depth==50){TestTrue(TEXT("Same-depth weight ordering in drop and Stay is retained"),Measures[0].BottomS>Measures[1].BottomS && Measures[1].BottomS>Measures[2].BottomS && Measures[0].Delta()<Measures[1].Delta() && Measures[1].Delta()<Measures[2].Delta());}
 }
 FFileHelper::SaveStringToFile(Rows,*(FPaths::ProjectSavedDir()/TEXT("Automation/R7CDepthWeights.csv")));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7CRepro,"TipRun.R7C.Runtime.LongDriftReproduction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7CRepro::RunTest(const FString&)
{
 for(bool Port:{true,false})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,Port?TEXT("R7C_Long_Port"):TEXT("R7C_Long_Starboard"),60,Port)){return false;}
  auto* Background=R7CMesh(R.Visual,TEXT("ObservationBackground"));auto* Rail=R7CMesh(R.Visual,Port?TEXT("PortGunwale"):TEXT("StarboardGunwale"));
  FString Evidence=TEXT("REAL saved Runtime, natural Fishing drift, no position setter\n");int FullyBlocked=0;bool First=true;double FirstDistance=0;
  const auto Start=R.PC->GetDebugSnapshot().Boat.PositionM;
  for(int I=0;I<60*210;++I)
  {
   R.Frame();const auto P=R7CProbe(R,Rail,Background);
   if(I%600==0){Evidence+=R7CDescribe(R);}
   if(P.InView>0 && !P.Drawable())
   {++FullyBlocked;if(First){First=false;FirstDistance=(R.PC->GetDebugSnapshot().Boat.PositionM-Start).Size();Evidence+=TEXT("FIRST FULL GUNWALE BACKGROUND OCCLUSION\n")+R7CDescribe(R);}}
  }
  Evidence+=FString::Printf(TEXT("FullyOccludedFrames=%d firstDistanceM=%.12g\n"),FullyBlocked,FirstDistance);
  AddInfo(Evidence.Right(1600));FFileHelper::SaveStringToFile(Evidence,*(FPaths::ProjectSavedDir()/TEXT("Automation/")+(Port?TEXT("R7C_Repro_Port.txt"):TEXT("R7C_Repro_Starboard.txt"))));
  TestEqual(TEXT("Background must never cover every in-view gunwale sample during natural drift"),FullyBlocked,0);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7CStress,"TipRun.R7C.Runtime.LongDriftStressModesAndHeadings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7CStress::RunTest(const FString&)
{
 FString Evidence=TEXT("Saved Runtime; natural drift; .1s World frames, unchanged 1/60 fixed steps (6 <= catchup8). No teleport.\n");
 for(bool Port:{true,false})for(double Heading:{0.,90.,180.,270.})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R7C_Stress_%d_%g"),Port,Heading),60,Port,1920,1080,Heading)){return false;}
  auto* Session=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
  const FVector Start=Session->GetHUDSnapshot().Boat.PositionM;const auto Root=Session->GetHUDSnapshot().Rod.RodRootLocal;
  TArray<UActorComponent*> Components;R.Visual->GetComponents(Components);const int Count=Components.Num();
  auto* Background=R7CMesh(R.Visual,TEXT("ObservationBackground"));auto* Rail=R7CMesh(R.Visual,Port?TEXT("PortGunwale"):TEXT("StarboardGunwale"));
  R.Dt=.1;
  R.Tap(EKeys::Enter);for(int I=0;I<1800 && !Session->GetHUDSnapshot().Egi.bBottomContact && !Session->HasResult();++I){R.Frame();}
  R.Tap(EKeys::RightMouseButton);R.Frames(5);
  // SynchronizeSession releases inputs again on Deploy's CastId change. Prime
  // AFTER that boundary through real device input above the action threshold.
  R.Mouse(FVector2D(20,0));R.Frames(2);
  TestTrue(TEXT("Post-Deploy capture priming reaches Enhanced Input"),R.PC->DiagnosticEnhancedSamples>0);
  for(double Distance:{50.,100.,250.,500.})
  {
   int Frames=0;while((Session->GetHUDSnapshot().Boat.PositionM-Start).Size()<Distance && Frames++<30000 && !Session->HasResult()){R.Frame();}
   const auto H=Session->GetHUDSnapshot();const auto Obs=R.Observe();const auto& P=Obs.Presentation;
   TestTrue(TEXT("Natural drift reaches checkpoint within ocean, active cast not aborted"),Frames<30000 && H.bEnvironmentValid && !Session->HasResult() && (H.Boat.PositionM-Start).Size()>=Distance);
   TestTrue(TEXT("Camera station / boat visual remain on snapshots"),P.bValid && P.CameraStationErrorM<1.e-6 && P.BoatVisualErrorM<1.e-8 && !P.bCameraInsideBackground);
   TestTrue(TEXT("Actual gunwale samples remain in frustum and unobstructed"),R7CProbe(R,Rail,Background).Drawable());
   TestTrue(TEXT("Boat geometry still enabled, rod visual/snapshot endpoints match"),R.Visual->BoatVisual->IsVisible() && R.Visual->RodVisual->IsVisible() && Obs.RootErrorM<1.e-7 && Obs.TipErrorM<1.e-7 && H.Rod.RodRootLocal==Root && H.Rod.LengthM==2);
   TestTrue(TEXT("Surface window and seabed cover actual boat XY"),P.bSeabedCoversBoat && FMath::Abs(H.Boat.PositionM.X-P.WaterCenterM.X)<5.001 && FMath::Abs(H.Boat.PositionM.Y-P.WaterCenterM.Y)<5.001 && FMath::IsNearlyEqual(P.BackgroundDistanceM,100.,1.e-8));
   Evidence+=FString::Printf(TEXT("CHECKPOINT side=%d requestedHeading=%g distance=%g simulationSeconds=%.9g\n"),Port,Heading,Distance,Sim->GetSimulationTime().TickIndex*Sim->GetSimulationTime().StepSeconds)+R7CDescribe(R)+P.Describe()+TEXT("\n");
   const auto Before=H.Rod.BaseRodDirectionLocal;R.Mouse(FVector2D(2,0));R.Frames(2);
   TestTrue(TEXT("Real mouse remains usable at remote checkpoint, camera remains independent"),!Session->GetHUDSnapshot().Rod.BaseRodDirectionLocal.Equals(Before,1.e-12));
   Evidence+=FString::Printf(TEXT("MOUSE rawCount=%lld enhancedCount=%lld queuedCount=%lld last=%s\n"),R.PC->DiagnosticRawSamples,R.PC->DiagnosticEnhancedSamples,R.PC->DiagnosticQueuedSamples,*R.PC->DiagnosticLastQueue);
   R.Mouse(FVector2D(-2,0));R.Frames(2);
   const int64 CompletedBefore=Session->GetHUDSnapshot().Shakuri.CompletedCount;
   R.Tap(EKeys::RightMouseButton);R.Frames(4);TestTrue(TEXT("Each remote right click completes exactly one new Shakuri"),!Session->HasResult() && Session->GetHUDSnapshot().Shakuri.CompletedCount==CompletedBefore+1);
   Evidence+=FString::Printf(TEXT("REMOTE INPUT completedBefore=%lld after=%lld lastAction=%s\n"),CompletedBefore,Session->GetHUDSnapshot().Shakuri.CompletedCount,*Session->DiagnosticLastAction);
  }
  R.Tap(EKeys::Q);R.Frames(20);TestTrue(TEXT("Remote Quick ends cast in Ready/Unlock"),Session->CanChangeEquipment() && Session->IsEgiOnboard());
  R.Tap(EKeys::E);R.Frames(2);TestTrue(TEXT("Remote Navigation camera/context restored"),Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Navigation && R.PC->GetDebugSnapshot().NavigationCamera.bActive && !R.Visual->RodVisual->IsVisible() && R7CProbe(R,R.Visual->BoatVisual,Background).Drawable());
  R.Tap(EKeys::Enter);R.Tap(Port?EKeys::D:EKeys::A);R.Tap(EKeys::Enter);R.Frames(2);
  TestTrue(TEXT("Remote opposite side FPS restored"),Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Fishing && R.Visual->RodVisual->IsVisible() && R.Observe().Presentation.CameraStationErrorM<1.e-6);
  R.Tap(EKeys::Enter);R.Frames(3);R.Tap(EKeys::Q);R.Frames(20);R.Tap(EKeys::E);R.Frames(2);
  TestTrue(TEXT("Navigation -> Fishing -> Quick -> Navigation repeated without camera/input lock"),Session->GetPlayerModeSnapshot().Mode==ETRPlayerMode::Navigation && R.PC->GetDebugSnapshot().NavigationCamera.bActive && !R.Visual->RodVisual->IsVisible() && R7CProbe(R,R.Visual->BoatVisual,Background).Drawable());
  R.Key(EKeys::D,true);R.Frames(4);R.Key(EKeys::D,false);R.Frames(2);
  const auto Changed=Session->GetHUDSnapshot().Boat.HeadingRad;
  R.Tap(EKeys::Enter);R.Tap(Port?EKeys::A:EKeys::D);R.Tap(EKeys::Enter);R.Frames(2);
  TestTrue(TEXT("Heading change at remote world position keeps FPS gunwale composition"),!FMath::IsNearlyEqual(Changed,FMath::DegreesToRadians(Heading),.01) && R7CProbe(R,Rail,Background).Drawable());
  Components.Reset();R.Visual->GetComponents(Components);TestEqual(TEXT("No camera/visual component duplication across modes"),Components.Num(),Count);
 }
 FFileHelper::SaveStringToFile(Evidence,*(FPaths::ProjectSavedDir()/TEXT("Automation/R7CStress.txt")));return true;
}
