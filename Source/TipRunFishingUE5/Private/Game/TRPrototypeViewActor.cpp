#include "Game/TRPrototypeViewActor.h"
#include "Game/TRPlayerController.h"
#include "Game/TRGameModeBase.h"
#include "Data/TRSessionConfigDataAsset.h"
#include "Data/TRRodTuningDataAsset.h"
#include "Data/TRSimulationTypes.h"
#include "Data/TROceanAreaDataAsset.h"
#include "Data/TRHUDSnapshot.h"
#include "Camera/CameraTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "SceneView.h"
#include "Game/TRPlayerCameraManager.h"

namespace
{
 void Segment(UStaticMeshComponent* Mesh, FVector A, FVector B, double RadiusM)
 {
  const FVector Delta=B-A;
  Mesh->SetWorldLocation(TRUnits::MetersToCentimeters((A+B)*.5));
  Mesh->SetWorldRotation(Delta.Rotation());
  // Engine cube is 1m on each axis. Geometry thickness is display only.
  Mesh->SetWorldScale3D(FVector(FMath::Max(.001,Delta.Size()),RadiusM,RadiusM));
 }
}
ATRPrototypeViewActor::ATRPrototypeViewActor()
{
 PrimaryActorTick.bCanEverTick=true;
 PrimaryActorTick.TickGroup=TG_PostUpdateWork;
 PrimaryActorTick.bTickEvenWhenPaused=true;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ObservationRoot")));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 auto Make=[&](const TCHAR* Name, bool Ball=false)
 {
  auto* M=CreateDefaultSubobject<UStaticMeshComponent>(Name); M->SetupAttachment(RootComponent);
  M->SetStaticMesh(Ball?Sphere.Object:Cube.Object); M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  M->SetCastShadow(false); M->SetGenerateOverlapEvents(false); return M;
 };
 ReelVisual=Make(TEXT("ReelObservation"),true);PortGunwale=Make(TEXT("PortGunwale"));StarboardGunwale=Make(TEXT("StarboardGunwale"));
 BoatVisual=Make(TEXT("BoatObservation")); RodVisual=Make(TEXT("RodObservation"));
	BoatBow=Make(TEXT("BoatBow"),true); BoatCabin=Make(TEXT("BoatCabin")); ReferenceBuoy=Make(TEXT("WorldOriginBuoy"),true);
	for(int32 I=0;I<42;++I){WorldGrid.Add(Make(*FString::Printf(TEXT("FixedWaterGrid%d"),I)));}
 TipVisual=Make(TEXT("TipObservation"),true); LineVisual=Make(TEXT("LineObservation")); EgiVisual=Make(TEXT("EgiObservation"),true);
 SeabedVisual=Make(TEXT("Seabed30m")); BackgroundVisual=Make(TEXT("ObservationBackground"));
 SurfaceEdges.Add(Make(TEXT("SurfaceNorth"))); SurfaceEdges.Add(Make(TEXT("SurfaceSouth")));
 SurfaceEdges.Add(Make(TEXT("SurfaceEast"))); SurfaceEdges.Add(Make(TEXT("SurfaceWest")));
 ReelVisual->SetVisibility(false);RodVisual->SetVisibility(false); TipVisual->SetVisibility(false); LineVisual->SetVisibility(false); EgiVisual->SetVisibility(false);
 BoatVisual->SetRelativeScale3D(FVector(5,2,.6));
 SeabedVisual->SetRelativeLocation(FVector(0,0,-3020)); SeabedVisual->SetRelativeScale3D(FVector(200,200,.4));
 BackgroundVisual->SetRelativeLocation(FVector(0,10000,0)); BackgroundVisual->SetRelativeScale3D(FVector(400,1,400));
}
void ATRPrototypeViewActor::ConfigureMaterials()
{
 if(!ObservationMaterial){return;}
 auto Tint=[&](UStaticMeshComponent* M,FLinearColor Color)
 { auto* MID=UMaterialInstanceDynamic::Create(ObservationMaterial,this); MID->SetVectorParameterValue(TEXT("Color"),Color); M->SetMaterial(0,MID); };
 Tint(ReelVisual,FLinearColor(.1f,.3f,.8f));Tint(PortGunwale,FLinearColor(.8f,.15f,.12f));Tint(StarboardGunwale,FLinearColor(.12f,.7f,.3f));
 Tint(BoatVisual,FLinearColor(.85f,.88f,.9f)); Tint(RodVisual,FLinearColor(1,.4f,.02f)); Tint(TipVisual,FLinearColor(1,.8f,.1f));
	Tint(BoatBow,FLinearColor(.2f,.6f,1)); Tint(BoatCabin,FLinearColor(.35f,.45f,.6f)); Tint(ReferenceBuoy,FLinearColor(1,.2f,.6f));
	for(const auto& M:WorldGrid){Tint(M,FLinearColor(.06f,.26f,.32f));}
 Tint(LineVisual,FLinearColor(1,.85f,.1f)); Tint(EgiVisual,FLinearColor(1,.08f,.06f));
 Tint(SeabedVisual,FLinearColor(.16f,.20f,.19f)); Tint(BackgroundVisual,FLinearColor(.04f,.12f,.19f));
 for(const auto& M:SurfaceEdges){Tint(M,FLinearColor(.08f,.65f,.8f));}
}
void ATRPrototypeViewActor::BeginPlay()
{
 Super::BeginPlay(); ConfigureMaterials();
}
void ATRPrototypeViewActor::CalcCamera(float DeltaTime,FMinimalViewInfo& OutResult)
{
 const FVector Relative=CameraOffsetM-LookAtOffsetM;
 FRotator Orbit=Relative.Rotation(); Orbit.Yaw+=CameraRotationOffset.X; Orbit.Pitch=FMath::Clamp(Orbit.Pitch+CameraRotationOffset.Y,-75.0,80.0);
 const FVector Offset=LookAtOffsetM+Orbit.Vector()*Relative.Size();
 OutResult.Location=TRUnits::MetersToCentimeters(CameraCenterM+Offset);
 OutResult.Rotation=(LookAtOffsetM-Offset).Rotation();
 OutResult.ProjectionMode=ECameraProjectionMode::Perspective;
 OutResult.FOV=FMath::Clamp(FieldOfView,30.f,100.f); OutResult.bConstrainAspectRatio=false;
}
void ATRPrototypeViewActor::ApplyCameraLook(FVector2D Delta)
{
 if(Delta.ContainsNaN() || !FMath::IsFinite(LookSensitivityDeg)){return;}
 CameraRotationOffset.X=FMath::UnwindDegrees(CameraRotationOffset.X+FMath::Clamp(Delta.X,-50.0,50.0)*FMath::Clamp(LookSensitivityDeg,0.0,2.0));
 CameraRotationOffset.Y=FMath::Clamp(CameraRotationOffset.Y+FMath::Clamp(Delta.Y,-50.0,50.0)*FMath::Clamp(LookSensitivityDeg,0.0,2.0),-100.0,55.0);
}
void ATRPrototypeViewActor::ApplyObservation(const FTRHUDSnapshot& S,const FTRRodParameters& Rod,double SurfaceM,double DepthM)
{
 const bool FishingVisible=S.bSessionValid && S.PlayerMode.bValid && S.PlayerMode.Mode==ETRPlayerMode::Fishing && S.bEnvironmentValid;
 ReelVisual->SetVisibility(false);
 RodVisual->SetVisibility(false);TipVisual->SetVisibility(false);LineVisual->SetVisibility(false);EgiVisual->SetVisibility(false);
 if(!S.bEnvironmentValid || S.Boat.PositionM.ContainsNaN() || !FMath::IsFinite(SurfaceM) || !FMath::IsFinite(DepthM)){return;}
 CameraCenterM=S.Boat.PositionM;
 BoatVisual->SetWorldLocation(TRUnits::MetersToCentimeters(S.Boat.PositionM));
 BoatVisual->SetWorldRotation(FQuat(FVector::UpVector,S.Boat.HeadingRad));
 const FQuat Heading(FVector::UpVector,S.Boat.HeadingRad);
 BoatBow->SetWorldLocation((S.Boat.PositionM+Heading.RotateVector(FVector(2.1,0,0)))*100); BoatBow->SetWorldRotation(Heading);BoatBow->SetWorldScale3D(FVector(1.3,1.9,.6));
 BoatCabin->SetWorldLocation((S.Boat.PositionM+Heading.RotateVector(FVector(-1,0,.6)))*100);BoatCabin->SetWorldRotation(Heading);BoatCabin->SetWorldScale3D(FVector(1,1.3,.8));
 for(int Side:{-1,1}){auto* Rail=Side<0?PortGunwale.Get():StarboardGunwale.Get();Rail->SetWorldLocation((S.Boat.PositionM+Heading.RotateVector(FVector(0,Side*1.,1.05)))*100);Rail->SetWorldRotation(Heading);Rail->SetWorldScale3D(FVector(5,.1,.12));}
 const FVector O(0,0,SurfaceM); ReferenceSurfaceM=SurfaceM;
 ReferenceBuoy->SetWorldLocation((O+FVector(0,-4,.35))*100);ReferenceBuoy->SetWorldScale3D(FVector(.5,.5,.7));
 for(int32 I=0;I<21;++I)
 {
  const double V=(I-10)*5.0;
  Segment(WorldGrid[I*2],O+FVector(-50,V,0),O+FVector(50,V,0),.025);
  Segment(WorldGrid[I*2+1],O+FVector(V,-50,0),O+FVector(V,50,0),.025);
 }
 // Open water frame, not an opaque surface that conceals the underwater egi.
 Segment(SurfaceEdges[0],O+FVector(-12,-8,0),O+FVector(12,-8,0),.04);
 Segment(SurfaceEdges[1],O+FVector(-12,8,0),O+FVector(12,8,0),.04);
 Segment(SurfaceEdges[2],O+FVector(-12,-8,0),O+FVector(-12,8,0),.04);
 Segment(SurfaceEdges[3],O+FVector(12,-8,0),O+FVector(12,8,0),.04);
 SeabedVisual->SetWorldLocation(TRUnits::MetersToCentimeters(FVector(O.X,O.Y,SurfaceM-DepthM-.2)));
 BackgroundVisual->SetWorldLocation(TRUnits::MetersToCentimeters(O+FVector(0,100,0)));
 if(!FishingVisible){return;}
 // Both endpoints must belong to the same fixed-step snapshot. Mixing a live station
 // root with an older rod tip stretches the mesh when boat/station transforms differ.
 const FVector Mount=S.Rod.bValid?S.Rod.RootWorldPositionM:(S.Station.bValid?S.Station.RodRootWorldM:S.Boat.PositionM+FQuat(FVector::UpVector,S.Boat.HeadingRad).RotateVector(Rod.MountOffsetM));
 // Before first Deploy the configured base pose is a labelled onboard display only.
 const FVector InitialDirection=FRotator(FMath::RadiansToDegrees(Rod.InitialPitchRad),FMath::RadiansToDegrees(S.Boat.HeadingRad+Rod.InitialYawRad),0).Vector();
 const FVector Tip=S.Rod.bValid?S.Rod.TipWorldPositionM:Mount+(S.Station.bValid?FRotator(0,S.Station.FacingWorldDeg,0).Vector():InitialDirection)*Rod.LengthM;
 Segment(RodVisual,Mount,Tip,.045); TipVisual->SetWorldLocation(TRUnits::MetersToCentimeters(Tip)); TipVisual->SetWorldScale3D(FVector(.14));
 RodVisual->SetVisibility(true); TipVisual->SetVisibility(true);
 ReelVisual->SetVisibility(S.Station.bValid);ReelVisual->SetWorldLocation((Mount+(Tip-Mount).GetSafeNormal()*.15-FVector(0,0,.09))*100);ReelVisual->SetWorldScale3D(FVector(.17,.17,.13));
 const bool Live=S.bEgiValid && S.Egi.bWorldPositionValid && !S.Egi.WorldPositionM.ContainsNaN();
 LineVisual->SetVisibility(Live); EgiVisual->SetVisibility(Live);
 if(Live)
 {
  Segment(LineVisual,Tip,S.Egi.WorldPositionM,.025);
  EgiVisual->SetWorldLocation(TRUnits::MetersToCentimeters(S.Egi.WorldPositionM)); EgiVisual->SetWorldScale3D(FVector(.36,.14,.14));
 }
}
void ATRPrototypeViewActor::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if(!Controller.IsValid()){Controller=Cast<ATRPlayerController>(GetWorld()->GetFirstPlayerController());}
 if(!Controller.IsValid()){return;}
 // GameMode may select the boat after the observer first binds: ensure the observation camera remains active.
 Controller->SetPrototypeObserver(this); // Registers once; the camera manager owns Navigation POV.
 const auto* Mode=GetWorld()->GetAuthGameMode<ATRGameModeBase>();
 const auto* Config=Mode?Mode->SessionConfig.Get():nullptr;
 if(Config && Config->Ocean && Config->Rod)
 {const auto S=Controller->GetDebugSnapshot();ApplyObservation(S,Config->Rod->Parameters,Config->Ocean->Settings.SurfaceZ_M,Config->Ocean->Settings.FlatDepthM);CaptureRuntimeObservation(S,Controller.Get());}
}

void ATRPrototypeViewActor::CaptureRuntimeObservation(const FTRHUDSnapshot& S,ATRPlayerController* PC)
{
 RuntimeObservation={};auto& O=RuntimeObservation;O.WorldFrame=GFrameCounter;O.VisualFrame=GFrameCounter;O.RodTick=S.Rod.Tick;
 if(!S.Rod.bValid || !RodVisual || !RodVisual->GetStaticMesh() || !PC || !PC->PlayerCameraManager){return;}
 O.bValid=true;O.SimulationRootM=S.Rod.RootWorldPositionM;O.SimulationTipM=S.Rod.TipWorldPositionM;
 const FBox Box=RodVisual->GetStaticMesh()->GetBoundingBox();const FTransform T=RodVisual->GetComponentTransform();
 O.VisualRootM=T.TransformPosition(FVector(Box.Min.X,0,0))*.01;O.VisualTipM=T.TransformPosition(FVector(Box.Max.X,0,0))*.01;
 O.RootErrorM=(O.VisualRootM-O.SimulationRootM).Size();O.TipErrorM=(O.VisualTipM-O.SimulationTipM).Size();O.LengthM=(O.VisualTipM-O.VisualRootM).Size();
 const FQuat H(FVector::UpVector,S.Boat.HeadingRad);
 O.RootLocal=H.UnrotateVector(O.SimulationRootM-S.Boat.PositionM);O.TipLocal=H.UnrotateVector(O.SimulationTipM-S.Boat.PositionM);O.DirectionLocal=(O.TipLocal-O.RootLocal).GetSafeNormal();
 if(S.Station.bValid)
 {
  const FQuat StationRotation(FVector::UpVector,FMath::DegreesToRadians(S.Station.FacingWorldDeg));
  O.StationRootLocal=StationRotation.UnrotateVector(O.SimulationRootM-S.Station.PlayerWorldM);
  O.StationTipLocal=StationRotation.UnrotateVector(O.SimulationTipM-S.Station.PlayerWorldM);
  O.StationDirectionLocal=(O.StationTipLocal-O.StationRootLocal).GetSafeNormal();
 }
 const auto& POV=PC->PlayerCameraManager->GetCameraCacheView();O.CameraM=POV.Location*.01;O.CameraRotation=POV.Rotation;O.FOV=POV.FOV;
 if(const auto* Camera=Cast<ATRPlayerCameraManager>(PC->PlayerCameraManager)){O.CameraFrame=Camera->DiagnosticCameraFrame;}
 if(auto* LP=PC->GetLocalPlayer();LP && LP->ViewportClient && LP->ViewportClient->Viewport)
 {
  FSceneViewProjectionData Data;
  if(LP->GetProjectionData(LP->ViewportClient->Viewport,Data))
  {
   O.ViewRect=Data.GetConstrainedViewRect();const FMatrix VP=Data.ComputeViewProjectionMatrix();
   O.bProjected=FSceneView::ProjectWorldToScreen(O.VisualRootM*100,O.ViewRect,VP,O.PixelRoot) && FSceneView::ProjectWorldToScreen(O.VisualTipM*100,O.ViewRect,VP,O.PixelTip);
  }
 }
}
