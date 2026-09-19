#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "TRInputConfigDataAsset.generated.h"

UENUM(BlueprintType)
enum class ETRPlayerAction : uint8 { Deploy, Jerk, Fall, TensionFall, Hook, Retrieve, Cancel, NextCast, Pause, RodAim, QuickRetrieve, FishingStart, ReturnNavigation, NavigationBoost };

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRInputBinding
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun") ETRPlayerAction Command = ETRPlayerAction::Deploy;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun") TObjectPtr<UInputAction> Action;
};

UCLASS(BlueprintType)
class TIPRUNFISHINGUE5_API UTRInputConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Navigation") TObjectPtr<UInputMappingContext> NavigationContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Navigation") TObjectPtr<UInputAction> NavigationThrottle;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Navigation") TObjectPtr<UInputAction> NavigationSteering;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Navigation") TObjectPtr<UInputAction> NavigationLook;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Navigation") TObjectPtr<UInputAction> NavigationZoom;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Navigation") TObjectPtr<UInputAction> NavigationFishingStart;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun|Navigation") TObjectPtr<UInputAction> NavigationBoost;
	void CreateNavigationPrototype();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun") TObjectPtr<UInputMappingContext> FishingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TipRun") TArray<FTRInputBinding> Bindings;
	bool Validate(TArray<FText>& Errors) const;
	// Explicit opt-in, temporary keyboard/gamepad mappings; never a product default.
	static UTRInputConfigDataAsset* CreatePrototype(UObject* Outer);
	static UTRInputConfigDataAsset* CreateMouseRodPrototype(UObject* Outer);
	static UTRInputConfigDataAsset* CreateMouseRetrievePrototype(UObject* Outer);
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
