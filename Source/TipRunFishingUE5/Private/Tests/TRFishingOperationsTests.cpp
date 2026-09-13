#include "TRSessionTestFixture.h"
#include "Game/TRPlayerController.h"
#include "Fishing/TREgiSimulationComponent.h"
#include "Fishing/TREgiActor.h"
#include "Components/StaticMeshComponent.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
	void SendM10(FTRTestSession& F, ETRFishingCommandType Type)
	{
		F.Session->SubmitCommand(Type, F.Session->GetCastId());
	}
	bool WaitM10(FTRTestSession& F, ETRFishingState State, int32 Limit = 2400)
	{
		for (int32 I = 0; I < Limit && F.Session->Fishing->GetState() != State; ++I) { F.Step(); }
		return F.Session->Fishing->GetState() == State;
	}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM10Migration, "TipRun.M10.TuningMigration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM10Migration::RunTest(const FString& Parameters)
{
	auto* Tuning = LoadObject<UTRFishingTuningDataAsset>(nullptr, TEXT("/Game/TipRun/Prototype/Data/DA_TR_FishingTuning_Prototype"));
	if (!TestNotNull(TEXT("Existing prototype tuning"), Tuning)) { return false; }
	if (FParse::Param(FCommandLine::Get(), TEXT("TRMigrateM10Prototype")))
	{
		// Explicit one-time schema migration, preserving all existing balance/profile values.
		Tuning->Parameters.TensionLiftDecayPerS = 12.0;
		Tuning->Parameters.TensionLiftCompletionMps = 0.05f;
		Tuning->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Tuning->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		TestTrue(TEXT("UE resaves schema without removed field"), UPackage::SavePackage(Tuning->GetOutermost(), Tuning, *Filename, Args));
	}
	TestNull(TEXT("Removed delay has no reflection property"), FindFProperty<FProperty>(FTRFishingParameters::StaticStruct(), TEXT("AutoStayDelayS")));
	TArray<FText> Errors; TestTrue(TEXT("Migrated prototype valid"), Tuning->Validate(Errors));
	FTRFishingParameters Invalid = Tuning->Parameters; Invalid.TensionLiftDecayPerS = 0.0;
	TestFalse(TEXT("Missing decay rejected"), Invalid.Validate(Errors));
	Invalid = Tuning->Parameters; Invalid.TensionLiftCompletionMps = Invalid.JerkLiftMps;
	TestFalse(TEXT("Completion must be below jerk lift"), Invalid.Validate(Errors));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM10Jerks, "TipRun.M10.F06OneInputOneJerk", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM10Jerks::RunTest(const FString& Parameters)
{
	for (int32 Count : {1, 10, 11, 20})
	{
		FTRTestSession F; F.Area->Settings.CurrentMps = FVector::ZeroVector;
		if (!F.Start(*this)) { return false; } F.Deploy(); F.Step(300);
		const float Before = F.Session->Fishing->GetSnapshot().DepthM;
		int32 Started = 0, Falls = 0, Stays = 0;
		F.Session->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState, ETRFishingState To)
		{
			Started += To == ETRFishingState::Jerking; Falls += To == ETRFishingState::TensionFall; Stays += To == ETRFishingState::Stay;
		});
		for (int32 I = 0; I < Count; ++I) { SendM10(F, ETRFishingCommandType::Jerk); }
		F.Step(); auto S = F.Session->Fishing->GetSnapshot();
		TestEqual(TEXT("Only first operation started"), S.JerkCount, int64(1));
		TestEqual(TEXT("Others queued"), S.PendingJerkCount, int64(Count - 1));
		TestTrue(TEXT("Jerk lifts depth and velocity"), S.DepthM < Before && S.VelocityMps.Z > 0.0);
		if (!TestTrue(TEXT("Finite transient reaches Stay"), WaitM10(F, ETRFishingState::Stay))) { return false; }
		S = F.Session->Fishing->GetSnapshot();
		TestEqual(TEXT("One action per input"), Started, Count); TestEqual(TEXT("Each action ends in TF"), Falls, Count);
		TestEqual(TEXT("No intermediate Stay between pending actions"), Stays, 1);
		TestEqual(TEXT("Series count"), S.SeriesJerkCount, int64(Count)); TestEqual(TEXT("Stay count"), S.StayPenaltyJerkCount, int64(Count));
		TestEqual(TEXT("No remaining reservation"), S.PendingJerkCount, int64(0));
		F.Step(60); TestEqual(TEXT("No automatic jerks"), F.Session->Fishing->GetSnapshot().JerkCount, int64(Count));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM10ReFall, "TipRun.M10.F07F18ReFallAndSeries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM10ReFall::RunTest(const FString& Parameters)
{
	FTRTestSession F; F.Area->Settings.CurrentMps = FVector::ZeroVector; F.Area->Settings.FlatDepthM = 3.0f;
	if (!F.Start(*this)) { return false; } F.Deploy();
	TestTrue(TEXT("FreeFall reaches Bottom"), WaitM10(F, ETRFishingState::BottomContact));
	F.Step(120); TestTrue(TEXT("No-input Bottom never becomes Stay"), F.Session->Fishing->GetState() == ETRFishingState::BottomContact);
	SendM10(F, ETRFishingCommandType::TensionFall); F.Step();
	TestTrue(TEXT("Persistent bottom contact wins over transient completion"), F.Session->Fishing->GetState() == ETRFishingState::BottomContact);
	for (int32 I = 0; I < 11; ++I) { SendM10(F, ETRFishingCommandType::Jerk); }
	TestTrue(TEXT("Eleven to Stay"), WaitM10(F, ETRFishingState::Stay));
	SendM10(F, ETRFishingCommandType::Fall); F.Step();
	TestTrue(TEXT("Re-Fall starts FreeFall"), F.Session->Fishing->GetState() == ETRFishingState::FreeFall);
	TestEqual(TEXT("Fall preserves applied count"), F.Session->Fishing->GetSnapshot().StayPenaltyJerkCount, int64(11));
	SendM10(F, ETRFishingCommandType::TensionFall); F.Step();
	TestTrue(TEXT("No residual lift means immediately complete"), F.Session->Fishing->GetState() == ETRFishingState::Stay);
	TestEqual(TEXT("No new jerk preserves previous series"), F.Session->Fishing->GetSnapshot().StayPenaltyJerkCount, int64(11));
	SendM10(F, ETRFishingCommandType::Jerk); F.Step();
	TestTrue(TEXT("Stay accepts new jerk"), F.Session->Fishing->GetState() == ETRFishingState::Jerking);
	TestTrue(TEXT("New series to Stay"), WaitM10(F, ETRFishingState::Stay));
	TestEqual(TEXT("New series replaces applied count"), F.Session->Fishing->GetSnapshot().StayPenaltyJerkCount, int64(1));
	SendM10(F, ETRFishingCommandType::Fall); F.Step(); TestTrue(TEXT("Re-Fall still reaches Bottom"), WaitM10(F, ETRFishingState::BottomContact));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM10Priority, "TipRun.M10.F08InputPriorityAndCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM10Priority::RunTest(const FString& Parameters)
{
	FTRTestSession F; F.Area->Settings.CurrentMps = FVector::ZeroVector;
	if (!F.Start(*this)) { return false; } F.Deploy(); F.Step(120);
	for (int32 I = 0; I < 20; ++I) { SendM10(F, ETRFishingCommandType::Jerk); }
	F.Step(); SendM10(F, ETRFishingCommandType::Fall); F.Step();
	TestEqual(TEXT("Cancelled reservations never count"), F.Session->Fishing->GetSnapshot().JerkCount, int64(1));
	TestEqual(TEXT("Fall cancels all pending"), F.Session->Fishing->GetSnapshot().PendingJerkCount, int64(0));
	SendM10(F, ETRFishingCommandType::Jerk); const int64 Started = F.Sim()->GetSimulationTime().TickIndex;
	int64 Duration = 0; TRTime::TrySecondsToTicks(F.Tuning->Parameters.JerkDurationS, 1.0 / 60.0, Duration);
	F.Session->SubmitCommand(ETRFishingCommandType::Fall, F.Session->GetCastId(), Started + Duration);
	int32 StayEvents = 0; F.Session->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState, ETRFishingState To) { StayEvents += To == ETRFishingState::Stay; });
	F.Step(int32(Duration + 2)); TestEqual(TEXT("Input wins over completed jerk"), StayEvents, 0);
	TestTrue(TEXT("Explicit Fall retained"), F.Session->Fishing->GetState() == ETRFishingState::FreeFall);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM10Retrieve, "TipRun.M10.F16RetrieveEquipmentLoop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM10Retrieve::RunTest(const FString& Parameters)
{
	FTRTestSession F;
	F.Egis.Reset(DuplicateObject<UDataTable>(F.Egis.Get(), GetTransientPackage()));
	auto* NextRow = F.Egis->FindRow<FTREgiSpecRow>(TEXT("Egi_4"), TEXT("M10 test mesh"), false);
	NextRow->Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	if (!F.Start(*this)) { return false; }
	TArray<FText> Errors; int32 Results = 0;
	F.Session->OnCastCompleted.AddLambda([&](const FTRCatchResult&) { ++Results; });
	TestTrue(TEXT("Initial onboard Ready is editable"), F.Session->CanChangeEquipment());
	ETRCommandResult SameTickChange = ETRCommandResult::Accepted;
	F.Session->OnCommandProcessed.AddLambda([&](const FTRFishingCommand& Command, ETRCommandResult Result)
	{
		if (Command.Type == ETRFishingCommandType::Deploy && Result == ETRCommandResult::Accepted)
		{
			SameTickChange = F.Session->TrySetEquipment(TEXT("Egi_4"), TEXT("Sinker_5"), Errors);
		}
	});
	F.Deploy(); F.Step(180); const auto Old = F.Session->GetCastId();
	TestTrue(TEXT("Deploy and lock are atomic in one tick"), SameTickChange == ETRCommandResult::RejectedBusy);
	SendM10(F, ETRFishingCommandType::RetrieveStarted); F.Step();
	TestTrue(TEXT("Retrieving is locked"), F.Session->Fishing->GetState() == ETRFishingState::Retrieving && F.Session->IsEquipmentLocked());
	TestTrue(TEXT("Early change rejected"), F.Session->TrySetEquipment(TEXT("Egi_4"), TEXT("Sinker_5"), Errors) == ETRCommandResult::RejectedBusy);
	SendM10(F, ETRFishingCommandType::RetrieveStopped); F.Step(); const float Line = F.Session->Fishing->GetSnapshot().LineLengthM;
	F.Step(30); TestEqual(TEXT("Released reel does not shorten line"), F.Session->Fishing->GetSnapshot().LineLengthM, Line);
	SendM10(F, ETRFishingCommandType::RetrieveStarted);
	if (!TestTrue(TEXT("Retrieve completes"), WaitM10(F, ETRFishingState::Result))) { return false; }
	F.Step(); TestEqual(TEXT("One result notification"), Results, 1);
	TestTrue(TEXT("Retrieved, onboard, no catch"), F.Session->IsEgiOnboard() && F.Session->GetLastResult().Outcome == ETRCastOutcome::Retrieved && F.Session->GetLastResult().WeightKg == 0.0f);
	TestNull(TEXT("Visual released on completion"), F.Session->GetEgiActor());
	SendM10(F, ETRFishingCommandType::NextCast); F.Step();
	TestTrue(TEXT("Next Ready unlocks without EndFishing"), F.Session->CanChangeEquipment() && !F.Session->IsEquipmentLocked());
	TestTrue(TEXT("Next weights selected"), F.Session->TrySetEquipment(TEXT("Egi_4"), TEXT("Sinker_50"), Errors) == ETRCommandResult::Accepted);
	TestEqual(TEXT("Past equipment unchanged"), F.Session->GetLastResultEquipment().TotalMassG, 35.0f);
	F.Deploy(); TestTrue(TEXT("Next cast relocks"), F.Session->IsEquipmentLocked() && F.Session->GetCastId().Value > Old.Value);
	TestEqual(TEXT("Next cast uses changed mass"), F.Session->GetEquipmentSnapshot().TotalMassG, 90.0f);
	TestTrue(TEXT("Next cast uses prepared mesh"), F.Session->GetEgiActor()->EgiMesh->GetStaticMesh() == NextRow->Mesh.Get());
	F.Session->SubmitCommand(ETRFishingCommandType::Fall, Old); F.Step();
	TestTrue(TEXT("Old CastId rejected"), F.Session->GetLastCommandResult() == ETRCommandResult::RejectedInvalidState);
	F.Session->AbortCast(F.Session->GetCastId()); F.Session->ResetCast();
	TestFalse(TEXT("Aborted Ready lacks onboard confirmation"), F.Session->CanChangeEquipment());
	F.Deploy(); TestTrue(TEXT("Cannot deploy missing onboard egi"), F.Session->Fishing->GetState() == ETRFishingState::Ready);
	F.Session->EndFishing(); F.Session->EndFishing();
	TestEqual(TEXT("Repeated ending cannot duplicate terminal result"), Results, 2);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM10Pause, "TipRun.M10.F17PauseFocusAndSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM10Pause::RunTest(const FString& Parameters)
{
	FTRTestSession F; if (!F.Start(*this)) { return false; } F.Deploy(); F.Step(120);
	auto* PC = F.World->SpawnActor<ATRPlayerController>(); PC->BindSession(F.Session.Get());
	PC->ActionStarted(ETRPlayerAction::Retrieve); F.Step(); PC->SetPauseRequested(true);
	const auto Before = F.Session->Fishing->GetSnapshot(); F.Step(120);
	TestEqual(TEXT("Pause freezes state tick"), F.Session->Fishing->GetSnapshot().Tick, Before.Tick);
	PC->SetPauseRequested(false); F.Step(); const float Line = F.Session->Fishing->GetSnapshot().LineLengthM;
	F.Step(10); TestEqual(TEXT("Resume never restarts held reel"), F.Session->Fishing->GetSnapshot().LineLengthM, Line);
	PC->ActionReleased(ETRPlayerAction::Retrieve); PC->ActionStarted(ETRPlayerAction::Retrieve); F.Step();
	PC->SetInputFocus(false); F.Step(); const float FocusLine = F.Session->Fishing->GetSnapshot().LineLengthM;
	F.Step(10); TestEqual(TEXT("Focus loss stops reel"), F.Session->Fishing->GetSnapshot().LineLengthM, FocusLine);
	const auto S = F.Session->Fishing->GetSnapshot(); const auto T = F.Sim()->GetSimulationTime();
	for (int32 I = 0; I < 100; ++I) { auto Copy = F.Session->GetHUDSnapshot(); Copy.Egi.DepthVelocityMps = 999.0f; }
	TestEqual(TEXT("Range data reads do not tick"), F.Sim()->GetSimulationTime().TickIndex, T.TickIndex);
	TestEqual(TEXT("Range data reads do not modify speed"), F.Session->Fishing->GetSnapshot().DepthVelocityMps, S.DepthVelocityMps);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM10FPS, "TipRun.M10.FixedFrameRatesAndResults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM10FPS::RunTest(const FString& Parameters)
{
	TArray<FString> Reference; FTRCatchResult ReferenceResult;
	for (int32 FPS : {30, 60, 120})
	{
		FTRTestSession F; F.Area->Settings.CurrentMps = FVector::ZeroVector;
		if (!F.Start(*this)) { return false; } F.Deploy();
		TArray<FString> Seen;
		F.Session->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState From, ETRFishingState To)
		{ Seen.Add(FString::Printf(TEXT("%lld:%d:%d"), F.Sim()->GetSimulationTime().TickIndex, int32(From), int32(To))); });
		const auto Cast = F.Session->GetCastId();
		F.Session->SubmitCommand(ETRFishingCommandType::Jerk, Cast, 120);
		F.Session->SubmitCommand(ETRFishingCommandType::Jerk, Cast, 120);
		F.Session->SubmitCommand(ETRFishingCommandType::Fall, Cast, 180);
		F.Session->SubmitCommand(ETRFishingCommandType::RetrieveStarted, Cast, 240);
		for (int32 I = 0; I < FPS * 15; ++I) { F.Sim()->Tick(1.0f / FPS); }
		TestTrue(TEXT("Replay completes retrieval"), F.Session->GetLastResult().Outcome == ETRCastOutcome::Retrieved && F.Session->HasResult());
		if (FPS == 30) { Reference = Seen; ReferenceResult = F.Session->GetLastResult(); }
		else { TestTrue(TEXT("State event sequence identical"), Seen == Reference); TestEqual(TEXT("Result time identical"), F.Session->GetLastResult().ElapsedSimSeconds, ReferenceResult.ElapsedSimSeconds); }
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM10Range, "TipRun.M10.F19DepthVelocityAndContact", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM10Range::RunTest(const FString& Parameters)
{
	for (const FName Sinker : {TREquipment::NoSinkerId(), FName(TEXT("Sinker_50"))})
	{
		FTRTestSession F; if (!F.Start(*this)) { return false; } TArray<FText> Errors;
		F.Session->TrySetEquipment(TREquipment::InitialEgiId(), Sinker, Errors); F.Deploy(); F.Step(120);
		SendM10(F, ETRFishingCommandType::Jerk); F.Step();
		bool SawTransient = false;
		for (int32 I = 0; I < 120; ++I)
		{
			const auto Before = F.Session->Fishing->GetSnapshot(); F.Step(); const auto After = F.Session->Fishing->GetSnapshot();
			TestTrue(TEXT("Depth velocity is corrected depth derivative"), FMath::IsNearlyEqual(After.DepthVelocityMps, float((double(After.DepthM) - Before.DepthM) * 60.0), 0.00001f));
			SawTransient |= After.FishingState == ETRFishingState::TensionFall;
		}
		TestTrue(TEXT("Transient processed for both weights"), SawTransient);
		TestTrue(TEXT("Stay independent of net vertical speed"), F.Session->Fishing->GetState() == ETRFishingState::Stay);
		TestTrue(TEXT("Observation duration available"), F.Session->Fishing->GetSnapshot().RangeObservationSeconds > 0.0);
		SendM10(F, ETRFishingCommandType::Fall); F.Step();
		TestTrue(TEXT("Observation clears outside TF/Stay"), F.Session->Fishing->GetSnapshot().RangeObservationSeconds == 0.0);
		TestTrue(TEXT("Bottom reachable"), WaitM10(F, ETRFishingState::BottomContact, 12000));
		TestTrue(TEXT("Bottom contact exposed"), F.Session->Fishing->GetSnapshot().bBottomContact);
	}
	return true;
}
#endif
