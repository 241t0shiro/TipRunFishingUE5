#include "Game/TRGameModeBase.h"
#include "Game/TRPlayerController.h"
#include "Game/TRFishingSessionActor.h"
#include "Data/TRSessionConfigDataAsset.h"
#include "Data/TROceanAreaDataAsset.h"
#include "Data/TRBoatTuningDataAsset.h"
#include "Ocean/TROceanWorldSubsystem.h"
#include "Ocean/TRSeabedProviderActor.h"
#include "Boat/TRBoatPawn.h"
#include "UI/TRHUD.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "Misc/ScopeExit.h"

ATRGameModeBase::ATRGameModeBase()
{
	PlayerControllerClass = ATRPlayerController::StaticClass(); HUDClass = ATRHUD::StaticClass(); DefaultPawnClass = nullptr;
}
bool ATRGameModeBase::InitializeSession(TArray<FText>& Errors)
{
	if (Session || !IsValid(SessionConfig) || !SessionConfig->ValidateStartup(Errors))
	{
		Errors.Add(FText::FromString(TEXT("M09 startup requires configured session, ocean, boat and input assets"))); return false;
	}
	auto* Sim = GetWorld()->GetSubsystem<UTRSimulationWorldSubsystem>();
	auto* Sea = GetWorld()->GetSubsystem<UTROceanWorldSubsystem>();
	if (!Sim || !Sea || !Sim->Configure(SessionConfig, Errors)) { return false; }
	bool bStarted = false;
	ON_SCOPE_EXIT
	{
		if (!bStarted)
		{
			if (Session) { Session->Destroy(); Session = nullptr; }
			if (BoatActor) { BoatActor->Destroy(); BoatActor = nullptr; }
			Sea->ShutdownArea();
			if (Provider) { Provider->Destroy(); Provider = nullptr; }
			Sim->SetSimulationPaused(true);
			Errors.Add(FText::FromString(TEXT("Startup stopped; correct the configuration and restart the world")));
		}
	};
	Provider = GetWorld()->SpawnActor<ATRSeabedProviderActor>();
	if (!Sea->InitializeArea(SessionConfig->Ocean, Provider, Errors)) { return false; }
	BoatActor = GetWorld()->SpawnActor<ATRBoatPawn>();
	if (!BoatActor) { return false; }
	BoatActor->Tuning = SessionConfig->Boat;
	const FTRActorSimId BoatId = Sim->RegisterBoat(BoatActor, SessionConfig->InitialBoatXYM, SessionConfig->InitialHeadingRad, Errors);
	if (!BoatId.IsValid()) { return false; }
	Session = GetWorld()->SpawnActor<ATRFishingSessionActor>();
	if(Session){Session->RodTuning=SessionConfig->Rod;}
	bStarted = Session && Session->Initialize(Sim, BoatId, SessionConfig->Egis, SessionConfig->Sinkers, SessionConfig->Fishing,
		SessionConfig->InitialSinkerId, Errors) && Session->StartFishing(Errors) == ETRCommandResult::Accepted;
	return bStarted;
}
void ATRGameModeBase::ConnectPlayer(APlayerController* Player)
{
	if (auto* PC = Cast<ATRPlayerController>(Player); PC && Session && Session->IsAcceptingPlayerInput())
	{
		PC->InputConfig = SessionConfig->Input;
		PC->InstallInputBindings(Cast<UEnhancedInputComponent>(PC->InputComponent));
		PC->BindSession(Session); PC->SetViewTarget(BoatActor);
	}
}
void ATRGameModeBase::StartPlay()
{
	TArray<FText> Errors;
	if (!InitializeSession(Errors))
	{
		FString Message; for (const auto& Error : Errors) { Message += Error.ToString() + TEXT("\n"); }
		StartupError = FText::FromString(Message.IsEmpty() ? TEXT("M09 startup failed") : Message);
	}
	Super::StartPlay();
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It) { ConnectPlayer(It->Get()); }
}
void ATRGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer); ConnectPlayer(NewPlayer);
}
