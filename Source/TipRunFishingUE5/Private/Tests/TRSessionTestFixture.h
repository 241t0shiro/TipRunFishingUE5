#pragma once
#include "Game/TRFishingSessionActor.h"
#include "Fishing/TRFishingComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Boat/TRBoatPawn.h"
#include "Data/TROceanAreaDataAsset.h"
#include "Data/TRSessionConfigDataAsset.h"
#include "Ocean/TROceanWorldSubsystem.h"
#include "Ocean/TRSeabedProviderActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/AutomationTest.h"

namespace
{
	struct FTRTestSession
	{
		UWorld* World = nullptr;
		TStrongObjectPtr<UDataTable> Egis{LoadObject<UDataTable>(nullptr, TEXT("/Game/TipRun/Prototype/Data/DT_TR_Egi_Prototype"))};
		TStrongObjectPtr<UDataTable> Sinkers{LoadObject<UDataTable>(nullptr, TEXT("/Game/TipRun/Prototype/Data/DT_TR_Sinker_Prototype"))};
		TStrongObjectPtr<UTRFishingTuningDataAsset> Tuning;
		TStrongObjectPtr<UTROceanAreaDataAsset> Area{NewObject<UTROceanAreaDataAsset>()};
		TStrongObjectPtr<UTRBoatTuningDataAsset> BoatTuning{NewObject<UTRBoatTuningDataAsset>()};
		TStrongObjectPtr<UTRSessionConfigDataAsset> Clock{NewObject<UTRSessionConfigDataAsset>()};
		TWeakObjectPtr<ATRBoatPawn> Boat;
		TWeakObjectPtr<ATRSeabedProviderActor> Provider;
		TWeakObjectPtr<ATRFishingSessionActor> Session;
		FTRActorSimId BoatId;
		FTRTestSession()
		{
			if (auto* Source = LoadObject<UTRFishingTuningDataAsset>(nullptr, TEXT("/Game/TipRun/Prototype/Data/DA_TR_FishingTuning_Prototype")))
			{
				Tuning.Reset(DuplicateObject<UTRFishingTuningDataAsset>(Source, GetTransientPackage()));
			}
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("TR_Session_TestWorld")), nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			// Temporary test world/coefficients; no saved content or product defaults.
			Area->Settings.AreaId = TEXT("TestM06Ocean");
			Area->Settings.BoundsMinXYM = FVector2D(-1000.0, -1000.0);
			Area->Settings.BoundsMaxXYM = FVector2D(1000.0, 1000.0);
			Area->Settings.FlatDepthM = 30.0f;
			Area->Settings.SurfaceZ_M = 2.0f;
			Area->Settings.CurrentMps = FVector(1.0, 0.0, 0.0);
			BoatTuning->Parameters.CurrentResponse01 = 0.5;
			BoatTuning->Parameters.VelocityResponsePerS = 2.0;
			BoatTuning->Parameters.MaxDriftSpeedMps = 3.0;
			BoatTuning->Parameters.HullHeightOffsetM = 0.5;
			BoatTuning->Parameters.RodAnchorOffsetM = FVector(2.0, 1.0, 1.0);
			Clock->StepSeconds = 1.0 / 60.0; Clock->MaxCatchUpSteps = 8;
			if (World)
			{
				Provider = World->SpawnActor<ATRSeabedProviderActor>();
				Boat = World->SpawnActor<ATRBoatPawn>(); Boat->Tuning = BoatTuning.Get();
				Session = World->SpawnActor<ATRFishingSessionActor>();
			}
		}
		~FTRTestSession() { Shutdown(); }
		void Shutdown()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } World = nullptr; }
		}
		FTRTestSession(const FTRTestSession&) = delete;
		FTRTestSession& operator=(const FTRTestSession&) = delete;
		UTRSimulationWorldSubsystem* Sim() const { return World->GetSubsystem<UTRSimulationWorldSubsystem>(); }
		bool Environment(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Test tuning loaded"), Tuning.Get()) || !Test.TestNotNull(TEXT("Session spawned"), Session.Get())) { return false; }
			TArray<FText> Errors;
			bool bValid = Sim()->Configure(Clock.Get(), Errors) && World->GetSubsystem<UTROceanWorldSubsystem>()->InitializeArea(Area.Get(), Provider.Get(), Errors);
			if (bValid) { BoatId = Sim()->RegisterBoat(Boat.Get(), FVector2D(10.0, 20.0), 0.0f, Errors); bValid = BoatId.IsValid(); }
			for (const FText& Error : Errors) { Test.AddError(Error.ToString()); }
			return Test.TestTrue(TEXT("Environment initialized"), bValid);
		}
		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Environment(Test)) { return false; }
			TArray<FText> Errors;
			const bool bValid = Session->Initialize(Sim(), BoatId, Egis.Get(), Sinkers.Get(), Tuning.Get(), TREquipment::NoSinkerId(), Errors);
			for (const FText& Error : Errors) { Test.AddError(Error.ToString()); }
			return Test.TestTrue(TEXT("Session initialized"), bValid);
		}
		bool Start(FAutomationTestBase& Test)
		{
			if (!Initialize(Test)) { return false; }
			TArray<FText> Errors;
			const ETRCommandResult Result = Session->StartFishing(Errors);
			for (const FText& Error : Errors) { Test.AddError(Error.ToString()); }
			return Test.TestTrue(TEXT("Fishing started"), Result == ETRCommandResult::Accepted);
		}
		void Step(int32 Count = 1) { for (int32 I = 0; I < Count; ++I) { Sim()->AdvanceFrame(1.0 / 60.0); } }
		void Deploy() { Session->SubmitCommand(ETRFishingCommandType::Deploy, Session->GetCastId()); Step(); }
		bool ReturnToReady()
		{
			Session->SubmitCommand(ETRFishingCommandType::RetrieveStarted, Session->GetCastId());
			for (int32 I = 0; I < 12000 && Session->Fishing->GetState() != ETRFishingState::Result; ++I) { Step(); }
			if (!Session->IsEgiOnboard()) { return false; }
			return Session->ResetCast() == ETRCommandResult::Accepted;
		}
	};
}

#endif
