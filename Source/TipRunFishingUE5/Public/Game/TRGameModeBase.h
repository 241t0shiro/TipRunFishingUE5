#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TRGameModeBase.generated.h"
class UTRSessionConfigDataAsset;
class ATRFishingSessionActor;
class ATRBoatPawn;
class ATRSeabedProviderActor;
UCLASS()
class TIPRUNFISHINGUE5_API ATRGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
public:
	ATRGameModeBase();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TipRun") TObjectPtr<UTRSessionConfigDataAsset> SessionConfig;
	UPROPERTY(BlueprintReadOnly, Category="TipRun") FText StartupError;
	bool InitializeSession(TArray<FText>& Errors);
	ATRFishingSessionActor* GetSession() const { return Session.Get(); }
protected:
	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
private:
	void ConnectPlayer(APlayerController* Player);
	UPROPERTY() TObjectPtr<ATRFishingSessionActor> Session;
	UPROPERTY() TObjectPtr<ATRBoatPawn> BoatActor;
	UPROPERTY() TObjectPtr<ATRSeabedProviderActor> Provider;
};
