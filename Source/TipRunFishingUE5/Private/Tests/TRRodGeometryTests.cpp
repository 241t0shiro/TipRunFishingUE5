#include "TRSessionTestFixture.h"
#include "Fishing/TRRodControlComponent.h"
#include "Data/TRFishingStationDataAsset.h"
#include "Data/TRHUDSnapshot.h"
#include "Game/TRPrototypeViewActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
 struct FRodGeometry
 {
  TStrongObjectPtr<UTRRodControlComponent> Rod{NewObject<UTRRodControlComponent>()};
  FTRRodParameters P;
  FTRBoatSnapshot Boat;
  FTRSimTime Time;
  FTREgiSnapshot Egi;
  FQuat Basis;
  bool Start(FAutomationTestBase& Test, ETRFishingSide Side, double Heading)
  {
   const auto* Config=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));
   if(!Test.TestNotNull(TEXT("Saved session"),Config) || !Test.TestNotNull(TEXT("Saved rod"),Config->Rod.Get()) || !Test.TestNotNull(TEXT("Saved stations"),Config->FishingStations.Get())){return false;}
   P=Config->Rod->Parameters;
   const auto* Station=Config->FishingStations->Parameters.Find(Side);
   if(!Test.TestNotNull(TEXT("Saved side"),Station)){return false;}
   Time.StepSeconds=1./60;
   TArray<FText> Errors;
   if(!Test.TestTrue(TEXT("Saved physical rod initializes"),Rod->Initialize(P,Time.StepSeconds,Errors))){return false;}
   Boat.PositionM=FVector(12,-7,.5);Boat.HeadingRad=FMath::DegreesToRadians(Heading);
   Rod->SetStation(Station->RodMountM,FMath::DegreesToRadians(Station->FacingDeg));
   Basis=FQuat(FVector::UpVector,double(Boat.HeadingRad)+FMath::DegreesToRadians(Station->FacingDeg));
   return Step();
  }
  bool Step(FVector2D Delta=FVector2D::ZeroVector)
  {
   ++Time.TickIndex;Boat.Tick=Time.TickIndex;
   return Rod->ApplyAim(Delta,Time) && Rod->Step(Time,Boat,Egi,0);
  }
  bool Aim(double Yaw,double Pitch)
  {
   for(int I=0;I<300;++I)
   {
    const auto R=Rod->GetSnapshot();
    if(FMath::IsNearlyEqual(R.BaseYawRad,Yaw,1.e-10) && FMath::IsNearlyEqual(R.BasePitchRad,Pitch,1.e-10)){return true;}
    const FVector2D Delta((Yaw-R.BaseYawRad)/(P.SensitivityXRad*(P.bInvertX?-1:1)),(Pitch-R.BasePitchRad)/(P.SensitivityYRad*(P.bInvertY?-1:1)));
    if(!Step(Delta)){return false;}
   }
   return false;
  }
 };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRodGeometrySweep,"TipRun.M105R4.Geometry.FixedLengthPureYawPitch",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRodGeometrySweep::RunTest(const FString&)
{
 double MaxLengthError=0,MaxHeightError=0;
 int32 Samples=0;
 for(auto Side:{ETRFishingSide::Port,ETRFishingSide::Starboard}) for(double Heading:{0.,90.,180.,270.})
 {
  FRodGeometry G;if(!G.Start(*this,Side,Heading)){return false;}
  const FVector Root=G.Rod->GetSnapshot().RootWorldPositionM;
  for(double Pitch:{G.P.MinPitchRad,0.,.3,.7,G.P.MaxPitchRad})
  {
   if(!TestTrue(TEXT("Set fixed pitch"),G.Aim(0,Pitch))){return false;}
   const double LocalZ=G.Basis.UnrotateVector(G.Rod->GetSnapshot().TipWorldPositionM-Root).Z;
   for(double Yaw:{0.,-.3,.3,G.P.MinYawRad,G.P.MaxYawRad})
   {
    if(!TestTrue(TEXT("Pure yaw reaches target"),G.Aim(Yaw,Pitch))){return false;}
    const auto R=G.Rod->GetSnapshot();++Samples;
    MaxLengthError=FMath::Max(MaxLengthError,FMath::Abs(FVector::Distance(R.TipWorldPositionM,Root)-G.P.LengthM));
    MaxHeightError=FMath::Max(MaxHeightError,FMath::Abs(G.Basis.UnrotateVector(R.TipWorldPositionM-Root).Z-LocalZ));
    TestTrue(TEXT("Pure yaw preserves pitch and root"),FMath::IsNearlyEqual(R.BasePitchRad,Pitch,1.e-10) && R.RootWorldPositionM==Root);
    TestEqual(TEXT("Frozen length published"),R.LengthM,G.P.LengthM);
    TestTrue(TEXT("Normalized direction agrees with rotation"),R.TipDirection.IsUnit(1.e-10) && R.TipWorldRotation.GetForwardVector().Equals(R.TipDirection,1.e-10));
   }
  }
  for(double Yaw:{G.P.MinYawRad,0.,G.P.MaxYawRad}) for(double Pitch:{G.P.MinPitchRad,.2,G.P.MaxPitchRad})
  {
   TestTrue(TEXT("Pure pitch reaches target"),G.Aim(Yaw,Pitch));const auto R=G.Rod->GetSnapshot();
   TestTrue(TEXT("Pure pitch preserves yaw and physical length"),FMath::IsNearlyEqual(R.BaseYawRad,Yaw,1.e-10) && FMath::IsNearlyEqual(FVector::Distance(R.TipWorldPositionM,Root),G.P.LengthM,1.e-10));
  }
 }
 AddInfo(FString::Printf(TEXT("%d pure-yaw samples; max length error %.12g m; max station-local Z error %.12g m"),Samples,MaxLengthError,MaxHeightError));
 TestTrue(TEXT("All lengths fixed within 1 nanometer"),MaxLengthError<1.e-9);
 TestTrue(TEXT("All yaw arcs horizontal within 1 nanometer"),MaxHeightError<1.e-9);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRodGeometryRepeated,"TipRun.M105R4.Geometry.RepeatedAimAndShakuri",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRodGeometryRepeated::RunTest(const FString&)
{
 for(auto Side:{ETRFishingSide::Port,ETRFishingSide::Starboard}) for(double Heading:{0.,90.,180.,270.})
 {
  FRodGeometry G;if(!G.Start(*this,Side,Heading) || !G.Aim(0,.25)){return false;}
  const auto Initial=G.Rod->GetSnapshot();double MaxLengthError=0;
  for(int I=0;I<2000;++I)
  {
   if(!TestTrue(TEXT("Repeated independent mouse X accepted"),G.Step(FVector2D(I%2?-1:1,0)))){return false;}
   const auto R=G.Rod->GetSnapshot();MaxLengthError=FMath::Max(MaxLengthError,FMath::Abs(FVector::Distance(R.RootWorldPositionM,R.TipWorldPositionM)-G.P.LengthM));
   if(R.BasePitchRad!=Initial.BasePitchRad){AddError(TEXT("Pure mouse X accumulated pitch"));return false;}
  }
  auto R=G.Rod->GetSnapshot();TestTrue(TEXT("Repeated mouse cancels without numerical position drift"),R.TipWorldPositionM.Equals(Initial.TipWorldPositionM,1.e-10) && FMath::Abs(R.BaseYawRad-Initial.BaseYawRad)<1.e-10 && MaxLengthError<1.e-9);
  G.Egi.FishingState=ETRFishingState::Jerking;G.Egi.StateEnteredTick=G.Time.TickIndex+1;G.Egi.JerkCount=1;
  bool Moved=false;
  for(int I=0;I<G.Rod->GetJerkTicks();++I)
  {
   TestTrue(TEXT("Shakuri step valid"),G.Step());R=G.Rod->GetSnapshot();Moved|=R.FinalPitchRad>R.BasePitchRad;
   TestTrue(TEXT("Temporary offset preserves fixed length and base pose"),FMath::IsNearlyEqual(FVector::Distance(R.RootWorldPositionM,R.TipWorldPositionM),G.P.LengthM,1.e-9) && R.BasePitchRad==Initial.BasePitchRad && R.BaseYawRad==Initial.BaseYawRad);
  }
  G.Egi.FishingState=ETRFishingState::Stay;G.Step();R=G.Rod->GetSnapshot();
  TestTrue(TEXT("Shakuri returns to same base and fixed tip geometry"),Moved && R.FinalPitchRad==R.BasePitchRad && R.TipWorldPositionM.Equals(Initial.TipWorldPositionM,1.e-9));
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRRodGeometryVisual,"TipRun.M105R4.Geometry.PresentationAndUnits",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRRodGeometryVisual::RunTest(const FString&)
{
 FTRTestSession World;
 auto* View=World.World->SpawnActor<ATRPrototypeViewActor>();
 if(!TestNotNull(TEXT("Observation actor"),View)){return false;}
 const int32 Components=View->GetComponents().Num();
 for(auto Side:{ETRFishingSide::Port,ETRFishingSide::Starboard}) for(double Heading:{0.,90.,180.,270.})
 {
  FRodGeometry G;if(!G.Start(*this,Side,Heading)){return false;}
  for(double Pitch:{G.P.MinPitchRad,0.,.7,G.P.MaxPitchRad}) for(double Yaw:{G.P.MinYawRad,0.,G.P.MaxYawRad})
  {
   if(!TestTrue(TEXT("Visual pose reached"),G.Aim(Yaw,Pitch))){return false;}
   FTRHUDSnapshot S;S.bSessionValid=true;S.bEnvironmentValid=true;S.PlayerMode.bValid=true;S.PlayerMode.Mode=ETRPlayerMode::Fishing;
   S.Boat=G.Boat;S.Rod=G.Rod->GetSnapshot();S.Station.bValid=true;
   // Deliberately different live station/config: never stretch a frozen rod to either.
   S.Station.RodRootWorldM=S.Rod.RootWorldPositionM+FVector(1,2,3);
   auto LiveConfig=G.P;LiveConfig.LengthM*=2;
   View->ApplyObservation(S,LiveConfig,0,30);
   const auto Bounds=View->RodVisual->GetStaticMesh()->GetBoundingBox();
   const auto Transform=View->RodVisual->GetComponentTransform();
   const FVector A=Transform.TransformPosition(FVector(Bounds.Min.X,0,0)),B=Transform.TransformPosition(FVector(Bounds.Max.X,0,0));
   TestTrue(TEXT("Mesh endpoints use one physical snapshot, single meters to cm conversion"),A.Equals(S.Rod.RootWorldPositionM*100,1.e-6) && B.Equals(S.Rod.TipWorldPositionM*100,1.e-6));
   TestTrue(TEXT("Mesh longitudinal length fixed in centimeters"),FMath::IsNearlyEqual(FVector::Distance(A,B),G.P.LengthM*100,1.e-6));
   const FVector Scale=View->RodVisual->GetComponentScale();
   TestTrue(TEXT("Diameter independent of pitch/yaw/length"),FMath::IsNearlyEqual(Scale.Y,.045,1.e-9) && FMath::IsNearlyEqual(Scale.Z,.045,1.e-9));
   TestTrue(TEXT("Tip marker uses same endpoint"),View->TipVisual->GetComponentLocation().Equals(B,1.e-6));
  }
 }
 TestEqual(TEXT("No additional visual components"),View->GetComponents().Num(),Components);
 return true;
}
#endif
