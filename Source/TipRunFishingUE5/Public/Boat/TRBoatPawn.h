#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Data/TRBoatTuningDataAsset.h"
#include "Data/TRSnapshots.h"
#include "Data/TRSimulationTypes.h"
#include "TRBoatPawn.generated.h"

class UTRBoatDriftComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
DECLARE_MULTICAST_DELEGATE_OneParam(FTRBoatEnvironmentInvalid, ETRSampleError);

UCLASS()
class TIPRUNFISHINGUE5_API ATRBoatPawn : public APawn
{
	GENERATED_BODY()
public:
	ATRBoatPawn();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TipRun|Boat")
	TObjectPtr<UTRBoatTuningDataAsset> Tuning;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	TObjectPtr<UStaticMeshComponent> BoatMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	TObjectPtr<USceneComponent> RodAnchor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	TObjectPtr<USpringArmComponent> SpringArm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TipRun|Boat")
	TObjectPtr<UTRBoatDriftComponent> DriftComponent;

	bool InitializeBoat(const FVector2D& InitialXYM, float HeadingRad, const FTROceanSample& Ocean, TArray<FText>& Errors);
	void StepBoat(const FTRSimTime& Time, const FTROceanSample& Ocean, TFunctionRef<bool(const FVector2D&)> IsDestinationValid);
	void DisableBoat();
	UFUNCTION(BlueprintPure, Category = "TipRun|Boat")
	FTRBoatSnapshot GetBoatSnapshot() const;
	UFUNCTION(BlueprintPure, Category = "TipRun|Boat")
	FVector GetRodAnchorWorldM() const { return GetBoatSnapshot().RodTipM; }
	UFUNCTION(BlueprintPure, Category = "TipRun|Boat")
	ETRBoatMode GetBoatMode() const { return BoatMode; }
	FTRBoatEnvironmentInvalid OnEnvironmentInvalid;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void ApplySimTransform();
	UPROPERTY(BlueprintReadOnly, Category = "TipRun|Boat", meta = (AllowPrivateAccess = "true"))
	ETRBoatMode BoatMode = ETRBoatMode::Uninitialized;
	FVector FrozenRodOffsetM = FVector::ZeroVector;
};
