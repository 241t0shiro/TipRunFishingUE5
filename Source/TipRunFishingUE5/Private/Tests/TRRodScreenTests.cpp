#include "TRSessionTestFixture.h"
#include "Fishing/TRRodControlComponent.h"
#include "Data/TRFishingStationDataAsset.h"
#include "Data/TRHUDSnapshot.h"
#include "Game/TRPrototypeViewActor.h"
#include "Camera/CameraTypes.h"
#include "SceneView.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
 struct FScreenRod
 {
  TStrongObjectPtr<UTRRodControlComponent> Rod{NewObject<UTRRodControlComponent>()};
  FTRRodParameters P;
  FTRFishingStation Station;
  FTRFishingStationParameters Cameras;
  FTRBoatSnapshot Boat;
  FTRSimTime Time;
  FTREgiSnapshot Egi;
  FVector2D Look;
  bool Start(FAutomationTestBase& Test,ETRFishingSide Side,double Heading)
  {
   auto* Config=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));
   if(!Test.TestNotNull(TEXT("Saved configuration"),Config)){return false;}
   P=Config->Rod->Parameters;Cameras=Config->FishingStations->Parameters;Station=*Cameras.Find(Side);
   Time.StepSeconds=1./60;Boat.PositionM=FVector(10,20,.5);Boat.HeadingRad=FMath::DegreesToRadians(Heading);
   TArray<FText> Errors;if(!Test.TestTrue(TEXT("Initialize frozen tuning"),Rod->Initialize(P,Time.StepSeconds,Errors))){return false;}
   Rod->SetScreenStation(Station,Cameras);Look=FVector2D(0,Cameras.InitialPitchDeg);return Step();
  }
  bool Step(FVector2D Delta=FVector2D::ZeroVector)
  {++Time.TickIndex;Boat.Tick=Time.TickIndex;return Rod->ApplyAim(Delta,Time) && Rod->Step(Time,Boat,Egi,0);}
  bool View(FVector2D Angles)
  {Look=Angles;return Rod->ApplyView(Angles) && Step();}
  FVector2D Project(FVector WorldM,int Width=1920,int Height=1080) const
  {
   // Use UE's actual perspective projection, independent of the Rod inverse solver.
   FMinimalViewInfo POV;POV.Location=(Boat.PositionM+FQuat(FVector::UpVector,Boat.HeadingRad).RotateVector(Station.CameraM))*100;
   POV.Rotation=FRotator(Look.Y,FMath::RadiansToDegrees(double(Boat.HeadingRad))+Station.FacingDeg+Look.X,0);
   POV.FOV=Cameras.FOV;POV.AspectRatio=float(Width)/Height;POV.AspectRatioAxisConstraint=AspectRatio_MaintainXFOV;
   const FMatrix Axes(FPlane(0,0,1,0),FPlane(1,0,0,0),FPlane(0,1,0,0),FPlane(0,0,0,1));
   const FMatrix View=FTranslationMatrix(-POV.Location)*FInverseRotationMatrix(POV.Rotation)*Axes;
   FVector2D Pixel;
   const FIntRect Rect(0,0,Width,Height);FSceneViewProjectionData Data;Data.SetViewRectangle(Rect);
   FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(POV,AspectRatio_MaintainYFOV,Rect,Data);
   if(!FSceneView::ProjectWorldToScreen(WorldM*100,Rect,View*Data.ProjectionMatrix,Pixel)){return FVector2D(-1.e9,-1.e9);}
   return Pixel;
  }
 };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRScreenSaved,"TipRun.M105R4.Screen.SavedTuning",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRScreenSaved::RunTest(const FString&)
{
 auto* C=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));
 if(!TestNotNull(TEXT("Saved session"),C)){return false;}
 if(FParse::Param(FCommandLine::Get(),TEXT("TRMigrateR4Screen")))
 {
  C->Rod->Parameters.Screen=FTRRodScreenParameters();
  for(auto& Row:C->FishingStations->Parameters.Stations)
  {Row.RodMountM.X=Row.CameraM.X;Row.RodMountM.Z=1.1;}
  for(UObject* A:{static_cast<UObject*>(C->Rod),static_cast<UObject*>(C->FishingStations)})
  {FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;TestTrue(TEXT("Save explicit screen Prototype"),UPackage::SavePackage(A->GetOutermost(),A,*FPackageName::LongPackageNameToFilename(A->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));}
 }
 TArray<FText> Errors;TestTrue(TEXT("Saved rod and camera tuning valid"),C->Rod->Parameters.Validate(Errors) && C->FishingStations->Parameters.Validate());
 for(const auto& Row:C->FishingStations->Parameters.Stations){TestTrue(TEXT("Prototype fixed grip centered and below eye"),Row.RodMountM.X==Row.CameraM.X && Row.RodMountM.Z<Row.CameraM.Z);}
 for(int I=0;I<4;++I)
 {
  auto Bad=C->Rod->Parameters.Screen;
  if(I==0){Bad.Sensitivity.X=0;}if(I==1){Bad.Min.X=Bad.Max.X;}if(I==2){Bad.Initial.Y=2;}if(I==3){Bad.MaxRatePerS=std::numeric_limits<double>::infinity();}
  TestFalse(TEXT("Invalid screen tuning rejected"),Bad.Validate());
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRScreenProjection,"TipRun.M105R4.Screen.ProjectedAxesSidesHeadings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRScreenProjection::RunTest(const FString&)
{
 double MaxCrossDrift=0,MaxReturnError=0;
 for(auto Side:{ETRFishingSide::Port,ETRFishingSide::Starboard})for(double Heading:{0.,90.,180.,270.})for(auto Look:{FVector2D(0,-20),FVector2D(20,-10),FVector2D(-55,-65),FVector2D(55,10)})
 {
  FScreenRod R;if(!R.Start(*this,Side,Heading) || !R.View(Look)){return false;}
  auto Point=[&](){return R.Project(R.Rod->GetSnapshot().TipWorldPositionM);};
  const FVector2D Initial=Point();const auto Root=R.Rod->GetSnapshot().RootWorldPositionM;
  for(auto Delta:{FVector2D(1,0),FVector2D(-1,0),FVector2D(0,1),FVector2D(0,-1),FVector2D(1,1),FVector2D(-1,-1)})
  {
   const FVector2D Before=Point();TestTrue(TEXT("Mouse accepted"),R.Step(Delta));const auto After=Point();
   if(Delta==FVector2D(1,0)){TestTrue(TEXT("Screen sensitivity from frozen tuning"),FMath::IsNearlyEqual(R.Rod->GetSnapshot().ScreenControl.X,R.P.Screen.Initial.X+R.P.Screen.Sensitivity.X,1.e-12));}
   if(Delta.X){TestTrue(TEXT("Mouse right/left maps to screen right/left"),(After.X-Before.X)*Delta.X>0);}else{MaxCrossDrift=FMath::Max(MaxCrossDrift,FMath::Abs(After.X-Before.X));}
   if(Delta.Y){TestTrue(TEXT("Mouse up/down maps to screen up/down (pixels Y down)"),(After.Y-Before.Y)*Delta.Y<0);}else{MaxCrossDrift=FMath::Max(MaxCrossDrift,FMath::Abs(After.Y-Before.Y));}
  }
  for(int Axis=0;Axis<2;++Axis)
  {
   for(int I=0;I<2000;++I)
   {
    FVector2D Delta=FVector2D::ZeroVector;Delta[Axis]=I%2?-1:1;
    const auto Before=Point();if(!R.Step(Delta)){AddError(TEXT("Repeated screen input failed"));return false;}
    MaxCrossDrift=FMath::Max(MaxCrossDrift,FMath::Abs(Point()[1-Axis]-Before[1-Axis]));
   }
   MaxReturnError=FMath::Max(MaxReturnError,(Point()-Initial).Size());
  }
  for(int I=0;I<100;++I){R.Step(FVector2D(100,100));}
  auto S=R.Rod->GetSnapshot();TestTrue(TEXT("Independent screen maxima"),S.ScreenControl.Equals(R.P.Screen.Max,1.e-10));
  const auto MaxPoint=Point();R.Step(FVector2D(100,100));TestTrue(TEXT("Clamp stationary on screen"),Point().Equals(MaxPoint,1.e-6));
  for(int I=0;I<100;++I){R.Step(FVector2D(-100,-100));}
  S=R.Rod->GetSnapshot();TestTrue(TEXT("Screen minima and fixed grip/length"),S.ScreenControl.Equals(R.P.Screen.Min,1.e-10) && S.RootWorldPositionM==Root && FMath::IsNearlyEqual(FVector::Distance(Root,S.TipWorldPositionM),R.P.LengthM,1.e-9));
  TestTrue(TEXT("Entire working rectangle remains above sea with fixed length"),S.TipWorldPositionM.Z>=R.P.Screen.SurfaceClearanceM-1.e-9);
  const auto P1080=Point();const auto P1440=R.Project(S.TipWorldPositionM,2560,1440);
  TestTrue(TEXT("Same normalized projection at representative resolutions"),P1440.Equals(P1080*(4./3),1.e-3));
 }
 AddInfo(FString::Printf(TEXT("Max cross-axis drift %.12g pixels; max 2000-input return error %.12g pixels"),MaxCrossDrift,MaxReturnError));
 // FSceneView::ProjectWorldToScreen uses float RHW/NormalizedX/NormalizedY and
 // float pixel multiplication. Budget four normalized float ULPs at 1920 pixels,
 // rather than imposing double-coordinate precision on the real viewport path.
 const double PixelTolerance=4.*std::numeric_limits<float>::epsilon()*1920;
 TestTrue(TEXT("No cross-axis motion beyond viewport float precision"),MaxCrossDrift<PixelTolerance);
 TestTrue(TEXT("Repeated X/Y returns without accumulated drift"),MaxReturnError<1.e-5);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRScreenVisual,"TipRun.M105R4.Screen.VisualProfileAndSafety",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRScreenVisual::RunTest(const FString&)
{
 FTRTestSession W;auto* View=W.World->SpawnActor<ATRPrototypeViewActor>();
 double MinPixels=1.e9,MaxPixels=0,MaxLengthError=0;
 for(auto Side:{ETRFishingSide::Port,ETRFishingSide::Starboard})for(double Heading:{0.,90.,180.,270.})
 {
  FScreenRod R;if(!R.Start(*this,Side,Heading)){return false;}
  for(double X:{R.P.Screen.Min.X,0.,R.P.Screen.Max.X})for(double Y:{R.P.Screen.Min.Y,R.P.Screen.Initial.Y,R.P.Screen.Max.Y})
  {
   for(int I=0;I<80;++I)
   {const auto S=R.Rod->GetSnapshot();R.Step(FVector2D((X-S.ScreenControl.X)/R.P.Screen.Sensitivity.X,(Y-S.ScreenControl.Y)/R.P.Screen.Sensitivity.Y));}
   const auto S=R.Rod->GetSnapshot();const double Pixels=(R.Project(S.TipWorldPositionM)-R.Project(S.RootWorldPositionM)).Size();
   MinPixels=FMath::Min(MinPixels,Pixels);MaxPixels=FMath::Max(MaxPixels,Pixels);
   FTRHUDSnapshot HUD;HUD.bSessionValid=HUD.bEnvironmentValid=HUD.PlayerMode.bValid=true;HUD.PlayerMode.Mode=ETRPlayerMode::Fishing;HUD.Boat=R.Boat;HUD.Rod=S;
   View->ApplyObservation(HUD,R.P,0,30);
   const auto Box=View->RodVisual->GetStaticMesh()->GetBoundingBox();const auto T=View->RodVisual->GetComponentTransform();
   const FVector RootCm=T.TransformPosition(FVector(Box.Min.X,0,0)),TipCm=T.TransformPosition(FVector(Box.Max.X,0,0));
   MaxLengthError=FMath::Max(MaxLengthError,FMath::Abs((TipCm-RootCm).Size()-R.P.LengthM*100));
   TestTrue(TEXT("Visual uses fixed root and solved tip"),RootCm.Equals(S.RootWorldPositionM*100,1.e-6) && TipCm.Equals(S.TipWorldPositionM*100,1.e-6));
  }
  const auto Base=R.Rod->GetSnapshot();R.Egi.FishingState=ETRFishingState::Jerking;R.Egi.StateEnteredTick=R.Time.TickIndex+1;R.Egi.JerkCount=1;
  bool Moved=false;
  for(int I=0;I<R.Rod->GetJerkTicks();++I)
  {
   TestTrue(TEXT("Existing profile valid"),R.Step());const auto S=R.Rod->GetSnapshot();Moved|=(R.Project(S.TipWorldPositionM)-R.Project(Base.TipWorldPositionM)).Size()>1;
   TestTrue(TEXT("Shakuri keeps control/grip/length"),S.ScreenControl==Base.ScreenControl && S.RootWorldPositionM==Base.RootWorldPositionM && FMath::IsNearlyEqual((S.TipWorldPositionM-S.RootWorldPositionM).Size(),R.P.LengthM,1.e-9));
  }
  R.Egi.FishingState=ETRFishingState::Stay;R.Step();TestTrue(TEXT("Profile returns to precise base screen target"),Moved && R.Rod->GetSnapshot().TipWorldPositionM.Equals(Base.TipWorldPositionM,1.e-9));
  const auto Control=R.Rod->GetSnapshot().ScreenControl;R.View(FVector2D(R.Cameras.MaxYawDeg,R.Cameras.MinPitchDeg));
  TestTrue(TEXT("Camera limit does not mutate control or violate sea/fixed length"),R.Rod->GetSnapshot().ScreenControl==Control && R.Rod->GetSnapshot().TipWorldPositionM.Z>=0 && !R.Rod->GetSnapshot().TipWorldPositionM.ContainsNaN());
  TestFalse(TEXT("Nonfinite view rejected"),R.Rod->ApplyView(FVector2D(std::numeric_limits<double>::infinity(),0)));
 }
 AddInfo(FString::Printf(TEXT("1920x1080 base projected length %.3f..%.3f px, ratio %.4f; max visual length error %.12g cm"),MinPixels,MaxPixels,MaxPixels/MinPixels,MaxLengthError));
 TestTrue(TEXT("Prototype base projection avoids collapse or extreme magnification"),MinPixels>200 && MaxPixels/MinPixels<2.0);
 TestTrue(TEXT("World to visual physical length constant"),MaxLengthError<1.e-6);
 // A forward ray which misses the sphere still produces finite, fixed-length nearest geometry.
 FScreenRod Miss;if(!Miss.Start(*this,ETRFishingSide::Starboard,0)){return false;}
 Miss.Station.CameraM=Miss.Station.RodMountM+FVector(4,-5,.5);Miss.Rod->SetScreenStation(Miss.Station,Miss.Cameras);
 TestTrue(TEXT("Ray miss has safe nearest sphere point"),Miss.Step());
 const auto Fallback=Miss.Rod->GetSnapshot();
 TestTrue(TEXT("Miss fallback diagnosed, finite and fixed length"),Fallback.bScreenSafetyLimited && !Fallback.TipWorldPositionM.ContainsNaN() && FMath::IsNearlyEqual((Fallback.TipWorldPositionM-Fallback.RootWorldPositionM).Size(),Miss.P.LengthM,1.e-9));
 return true;
}
#endif
