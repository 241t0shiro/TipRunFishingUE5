#include "Fishing/TREgiActor.h"
#include "Data/TRSimulationTypes.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

ATREgiActor::ATREgiActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
	EgiMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EgiMesh")); EgiMesh->SetupAttachment(SceneRoot);
	EgiMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); EgiMesh->SetSimulatePhysics(false);
}
bool ATREgiActor::InitializeVisual(FTRCastId CastId, UStaticMesh* Mesh)
{
	if (!CastId.IsValid() || VisualCastId.IsValid() || !IsValid(Mesh) || IsActorBeingDestroyed()) { return false; }
	VisualCastId = CastId; EgiMesh->SetStaticMesh(Mesh); return true;
}
bool ATREgiActor::ApplySimulationSnapshot(const FTREgiSnapshot& Snapshot, float SurfaceZ_M)
{
	if (IsActorBeingDestroyed() || !VisualCastId.IsValid() || Snapshot.CastId != VisualCastId ||
		Snapshot.Tick < 0 || Snapshot.Tick < LastSnapshotTick || !FMath::IsFinite(SurfaceZ_M) ||
		!FMath::IsFinite(Snapshot.DepthM) || Snapshot.DepthM < 0.0f) { return false; }
	const FVector PositionCm = TRUnits::MetersToCentimeters(Snapshot.bWorldPositionValid ? Snapshot.WorldPositionM :
		FVector(Snapshot.PositionXYM.X, Snapshot.PositionXYM.Y, double(SurfaceZ_M) - Snapshot.DepthM));
	if (PositionCm.ContainsNaN()) { return false; }
	SetActorLocation(PositionCm, false, nullptr, ETeleportType::TeleportPhysics);
	LastSnapshotTick = Snapshot.Tick;
	return true;
}
