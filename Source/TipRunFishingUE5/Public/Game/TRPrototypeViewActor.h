#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRPrototypeViewActor.generated.h"
class ATRPlayerController;
class UStaticMeshComponent;
class UMaterialInterface;
struct FTRHUDSnapshot;
struct FTRRodParameters;
// G observation only: follows published snapshots, never advances or corrects simulation.
UCLASS()
class TIPRUNFISHINGUE5_API ATRPrototypeViewActor : public AActor
{
	GENERATED_BODY()
public:
	ATRPrototypeViewActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, Category="Prototype|View") FVector CameraOffsetM = FVector(-8,-12,7);
	UPROPERTY(EditAnywhere, Category="Prototype|View") FVector LookAtOffsetM = FVector(1,0,0);
	UPROPERTY(EditAnywhere, Category="Prototype|View") float FieldOfView = 60;
	UPROPERTY(EditAnywhere, Category="Prototype|View") double LookSensitivityDeg = .3;
	void ApplyCameraLook(FVector2D Delta);
	void ResetCameraLook() { CameraRotationOffset=FVector2D::ZeroVector; }
	FVector GetWorldReferenceM() const { return FVector(0,0,ReferenceSurfaceM); }
	UStaticMeshComponent* GetReferenceBuoy() const { return ReferenceBuoy; }
	UPROPERTY(EditAnywhere, Category="Prototype|View") TObjectPtr<UMaterialInterface> ObservationMaterial;
	// Render-only projection: exposed to tests, no writes to Session/Simulation.
	void ApplyObservation(const FTRHUDSnapshot& Snapshot, const FTRRodParameters& Rod, double SurfaceM, double DepthM);
	UPROPERTY(VisibleAnywhere, Category="Prototype|View") TObjectPtr<UStaticMeshComponent> BoatVisual;
	UPROPERTY(VisibleAnywhere, Category="Prototype|View") TObjectPtr<UStaticMeshComponent> RodVisual;
	UPROPERTY(VisibleAnywhere, Category="Prototype|View") TObjectPtr<UStaticMeshComponent> TipVisual;
	UPROPERTY(VisibleAnywhere, Category="Prototype|View") TObjectPtr<UStaticMeshComponent> LineVisual;
	UPROPERTY(VisibleAnywhere, Category="Prototype|View") TObjectPtr<UStaticMeshComponent> EgiVisual;
private:
	void ConfigureMaterials();
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SurfaceEdges;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> WorldGrid;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BoatBow;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BoatCabin;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ReferenceBuoy;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> SeabedVisual;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BackgroundVisual;
	TWeakObjectPtr<ATRPlayerController> Controller;
	FVector CameraCenterM = FVector::ZeroVector;
	FVector2D CameraRotationOffset=FVector2D::ZeroVector;
	double ReferenceSurfaceM=0;
};
