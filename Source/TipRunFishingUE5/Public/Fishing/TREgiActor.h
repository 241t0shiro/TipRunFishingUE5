#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/TRSnapshots.h"
#include "TREgiActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
UCLASS()
class TIPRUNFISHINGUE5_API ATREgiActor : public AActor
{
	GENERATED_BODY()
public:
	ATREgiActor();
	bool InitializeVisual(FTRCastId CastId, UStaticMesh* Mesh);
	bool ApplySimulationSnapshot(const FTREgiSnapshot& Snapshot, float SurfaceZ_M);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TipRun|Egi")
	TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TipRun|Egi")
	TObjectPtr<UStaticMeshComponent> EgiMesh;
private:
	FTRCastId VisualCastId;
	int64 LastSnapshotTick = -1;
};
