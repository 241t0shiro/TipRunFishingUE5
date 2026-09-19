#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TRNavigationTuningDataAsset.h"
#include "TRBoatNavigationComponent.generated.h"
class ATRFishingSessionActor;
UCLASS()
class TIPRUNFISHINGUE5_API UTRBoatNavigationComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UTRBoatNavigationComponent();
 FTRNavigationSnapshot GetSnapshot() const {return State;}
 FTRNavigationParameters GetParameters() const {return Settings;}
private:
 friend class ATRFishingSessionActor;
 bool Configure(const FTRNavigationParameters& In);
 bool SetInput(FVector2D Input);
 void SetBoost(bool bRequested) { State.bBoostRequested=State.bConfigured && bRequested; }
 void Clear();
 void Step(double Dt, bool bAllowed);
 FTRNavigationParameters Settings;
 FTRNavigationSnapshot State;
};

