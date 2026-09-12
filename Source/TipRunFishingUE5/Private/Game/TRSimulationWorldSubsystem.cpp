#include "Game/TRSimulationWorldSubsystem.h"
#include "Data/TRSessionConfigDataAsset.h"
#include "Ocean/TROceanWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Boat/TRBoatPawn.h"

bool UTRSimulationWorldSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UTRSimulationWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTRSimulationWorldSubsystem, STATGROUP_Tickables);
}

bool UTRSimulationWorldSubsystem::Configure(const UTRSessionConfigDataAsset* Config, TArray<FText>& Errors)
{
	if (!IsInitialized() || bConfigured || !IsValid(Config))
	{
		Errors.Add(FText::FromString(TEXT("Clock configuration requires an initialized, unconfigured world and a valid config")));
		return false;
	}
	if (!Config->Validate(Errors)) { return false; }
	Time.StepSeconds = Config->StepSeconds;
	MaxCatchUpSteps = Config->MaxCatchUpSteps;
	SessionSeed = Config->SessionSeed;
	bConfigured = true;
	return true;
}

const UTRSimulationWorldSubsystem::FRegistration* UTRSimulationWorldSubsystem::FindLive(FTRActorSimId Id) const
{
	return Registrations.FindByPredicate([Id](const FRegistration& Entry)
	{
		return Entry.Id == Id && Entry.Owner.IsValid() && !Entry.Owner->IsActorBeingDestroyed();
	});
}

bool UTRSimulationWorldSubsystem::IsRegistered(FTRActorSimId Id) const { return FindLive(Id) != nullptr; }

FTRActorSimId UTRSimulationWorldSubsystem::Register(AActor* Owner, bool bSquid,
	FTRSimulationStep Step, FTRSimulationCommand Command, bool bBoat)
{
	if (!bConfigured || !IsValid(Owner) || Owner->IsActorBeingDestroyed() || Owner->GetWorld() != GetWorld() ||
		!Step.IsBound() || (!bSquid && !bBoat && !Command.IsBound()) || NextId == MAX_int64 ||
		Registrations.ContainsByPredicate([Owner](const FRegistration& Entry) { return Entry.Owner.Get() == Owner; }))
	{
		return {};
	}
	FRegistration Entry;
	Entry.Id = FTRActorSimId(NextId++);
	Entry.Owner = Owner;
	Entry.bSquid = bSquid;
	Entry.bBoat = bBoat;
	Entry.Step = MoveTemp(Step);
	Entry.Command = MoveTemp(Command);
	Registrations.Add(MoveTemp(Entry));
	return Registrations.Last().Id;
}

FTRActorSimId UTRSimulationWorldSubsystem::RegisterSession(AActor* Owner, FTRSimulationStep Step, FTRSimulationCommand Command)
{
	return Register(Owner, false, MoveTemp(Step), MoveTemp(Command));
}

FTRActorSimId UTRSimulationWorldSubsystem::RegisterSquid(AActor* Owner, FTRSimulationStep Step)
{
	return Register(Owner, true, MoveTemp(Step), {});
}

void UTRSimulationWorldSubsystem::Unregister(FTRActorSimId Id)
{
	for (const FRegistration& Entry : Registrations)
	{
		if (Entry.Id == Id && Entry.bBoat)
		{
			if (ATRBoatPawn* Boat = Cast<ATRBoatPawn>(Entry.Owner.Get()))
			{
				Boat->OnEndPlay.RemoveDynamic(this, &UTRSimulationWorldSubsystem::HandleBoatEndPlay);
				Boat->DisableBoat();
			}
		}
	}
	Registrations.RemoveAll([Id](const FRegistration& Entry) { return Entry.Id == Id; });
	Commands.RemoveAll([Id](const FQueuedCommand& Entry) { return Entry.SessionId == Id; });
}

bool UTRSimulationWorldSubsystem::EnqueueCommand(FTRActorSimId SessionId, ETRFishingCommandType Type,
	float AxisValue, int64 TargetTick)
{
	const FRegistration* Entry = FindLive(SessionId);
	if (!bConfigured || IsSimulationPaused() || !Entry || Entry->bSquid || Entry->bBoat || !FMath::IsFinite(AxisValue) ||
		!StaticEnum<ETRFishingCommandType>()->IsValidEnumValue(int64(Type)) ||
		NextSequence == MAX_int64 || Time.TickIndex == MAX_int64) { return false; }
	const int64 EarliestTick = Time.TickIndex + (bAdvancing ? 1 : 0);
	if (TargetTick == -1) { TargetTick = EarliestTick; }
	if (TargetTick < EarliestTick || TargetTick == MAX_int64) { return false; }
	FQueuedCommand Queued;
	Queued.SessionId = SessionId;
	Queued.Value.Type = Type;
	Queued.Value.AxisValue = AxisValue;
	Queued.Value.TargetTick = TargetTick;
	Queued.Value.Sequence = NextSequence++;
	Commands.Add(Queued);
	return true;
}

void UTRSimulationWorldSubsystem::ClearCommands() { Commands.Reset(); ++CommandEpoch; }
void UTRSimulationWorldSubsystem::SetSimulationPaused(bool bInPaused)
{
	bPaused = bInPaused;
	if (bPaused) { ClearCommands(); AccumulatorSeconds = 0.0; }
}
bool UTRSimulationWorldSubsystem::IsSimulationPaused() const
{
	return bPaused || (GetWorld() && GetWorld()->IsPaused());
}

void UTRSimulationWorldSubsystem::Tick(float DeltaTime) { AdvanceFrame(double(DeltaTime)); }

void UTRSimulationWorldSubsystem::AdvanceFrame(double DeltaSeconds)
{
	if (!bConfigured || bAdvancing) { return; }
	if (IsSimulationPaused()) { ClearCommands(); AccumulatorSeconds = 0.0; return; }
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0) { return; }
	const double Budget = Time.StepSeconds * MaxCatchUpSteps;
	// Drop all time beyond this frame's budget, including its fractional excess.
	if (DeltaSeconds > Budget - AccumulatorSeconds)
	{
		AccumulatorSeconds = Budget;
		if (CatchUpDropCount < MAX_int64) { ++CatchUpDropCount; }
	}
	else { AccumulatorSeconds += DeltaSeconds; }
	for (int32 Step = 0; Step < MaxCatchUpSteps && bConfigured && !IsSimulationPaused(); ++Step)
	{
		if (AccumulatorSeconds / Time.StepSeconds < 1.0 - 1.e-9) { break; }
		if (Time.TickIndex == MAX_int64 || !FMath::IsFinite(double(Time.TickIndex + 1) * Time.StepSeconds))
		{
			SetSimulationPaused(true); // technical overflow protection, never wrap the clock
			break;
		}
		AccumulatorSeconds = FMath::Max(0.0, AccumulatorSeconds - Time.StepSeconds);
		AdvanceFixedStep();
	}
}

void UTRSimulationWorldSubsystem::Dispatch(ETRSimulationPhase Phase, const TArray<FTRActorSimId>& Participants)
{
	for (FTRActorSimId Id : Participants)
	{
		const FRegistration* Entry = FindLive(Id);
		if (!bConfigured || !Entry) { continue; }
		if (Entry->bBoat)
		{
			if (Phase != ETRSimulationPhase::Boat) { continue; }
		}
		else if (Phase != ETRSimulationPhase::Timers && ((Phase == ETRSimulationPhase::Squid) != Entry->bSquid)) { continue; }
		// Copy before invoking: callbacks may mutate/reallocate the registration array.
		FTRSimulationStep Callback = Entry->Step;
		Callback.ExecuteIfBound(Phase, Time);
	}
}

void UTRSimulationWorldSubsystem::AdvanceFixedStep()
{
	TGuardValue<bool> Guard(bAdvancing, true);
	Registrations.RemoveAll([](const FRegistration& Entry)
	{
		return !Entry.Owner.IsValid() || Entry.Owner->IsActorBeingDestroyed();
	});
	Commands.RemoveAll([this](const FQueuedCommand& Entry) { return !FindLive(Entry.SessionId); });
	TArray<FTRActorSimId> Participants;
	for (const FRegistration& Entry : Registrations) { Participants.Add(Entry.Id); }
	Commands.Sort([](const FQueuedCommand& A, const FQueuedCommand& B)
	{
		return A.Value.TargetTick != B.Value.TargetTick ? A.Value.TargetTick < B.Value.TargetTick : A.Value.Sequence < B.Value.Sequence;
	});
	TArray<FQueuedCommand> Due;
	for (const FQueuedCommand& Entry : Commands)
	{
		if (Entry.Value.TargetTick <= Time.TickIndex) { Due.Add(Entry); }
	}
	Commands.RemoveAll([this](const FQueuedCommand& Entry) { return Entry.Value.TargetTick <= Time.TickIndex; });
	const uint64 DispatchEpoch = CommandEpoch;
	for (const FQueuedCommand& Entry : Due)
	{
		if (!bConfigured || DispatchEpoch != CommandEpoch) { break; }
		if (const FRegistration* Registration = FindLive(Entry.SessionId))
		{
			FTRSimulationCommand Callback = Registration->Command;
			Callback.ExecuteIfBound(Entry.Value);
		}
	}
	Dispatch(ETRSimulationPhase::Timers, Participants);
	if (bConfigured)
	{
		if (UTROceanWorldSubsystem* Ocean = GetWorld()->GetSubsystem<UTROceanWorldSubsystem>())
		{
			Ocean->SetSimulationTime(double(Time.TickIndex) * Time.StepSeconds);
		}
	}
	for (ETRSimulationPhase Phase : { ETRSimulationPhase::Boat, ETRSimulationPhase::Fishing,
		ETRSimulationPhase::Squid, ETRSimulationPhase::BiteResolution, ETRSimulationPhase::Fight, ETRSimulationPhase::Publish })
	{
		Dispatch(Phase, Participants);
	}
	++Time.TickIndex;
}

bool UTRSimulationWorldSubsystem::CreateRandomStream(FTRActorSimId Id, uint32 PurposeId, FRandomStream& OutStream) const
{
	if (!bConfigured || !FindLive(Id)) { return false; }
	// Defined unsigned 32-bit arithmetic; independent of addresses, FName hashes and global RNG.
	const auto Mix = [](uint32 Value)
	{
		Value ^= Value >> 16; Value *= 0x7feb352dU;
		Value ^= Value >> 15; Value *= 0x846ca68bU;
		return Value ^ (Value >> 16);
	};
	uint32 Seed = Mix(uint32(SessionSeed));
	Seed = Mix(Seed ^ uint32(uint64(Id.Value)));
	Seed = Mix(Seed ^ uint32(uint64(Id.Value) >> 32));
	Seed = Mix(Seed ^ PurposeId);
	int32 SignedSeed;
	FMemory::Memcpy(&SignedSeed, &Seed, sizeof(Seed));
	OutStream.Initialize(SignedSeed);
	return true;
}

void UTRSimulationWorldSubsystem::Deinitialize()
{
	bConfigured = false;
	ClearCommands();
	TArray<FTRActorSimId> Ids;
	for (const FRegistration& Entry : Registrations) { Ids.Add(Entry.Id); }
	for (FTRActorSimId Id : Ids) { Unregister(Id); }
	Registrations.Reset();
	AccumulatorSeconds = 0.0;
	Super::Deinitialize();
}

FTRActorSimId UTRSimulationWorldSubsystem::RegisterBoat(ATRBoatPawn* Boat, const FVector2D& InitialXYM,
	float HeadingRad, TArray<FText>& Errors)
{
	if (!bConfigured || !IsValid(Boat) || Boat->IsActorBeingDestroyed() || Boat->GetWorld() != GetWorld() ||
		NextId == MAX_int64 || Time.TickIndex == MAX_int64 ||
		Registrations.ContainsByPredicate([Boat](const FRegistration& Entry) { return Entry.Owner.Get() == Boat; }))
	{
		Errors.Add(FText::FromString(TEXT("Boat registration requires a configured clock and a unique live boat in this world")));
		return {};
	}
	UTROceanWorldSubsystem* Ocean = GetWorld()->GetSubsystem<UTROceanWorldSubsystem>();
	FTROceanQuery Query;
	Query.PositionXYM = InitialXYM;
	Query.SimTick = Time.TickIndex + (bAdvancing ? 1 : 0);
	if (!Boat->InitializeBoat(InitialXYM, HeadingRad, Ocean ? Ocean->SampleOcean(Query) : FTROceanSample(), Errors)) { return {}; }
	const FTRActorSimId Id = Register(Boat, false,
		FTRSimulationStep::CreateUObject(this, &UTRSimulationWorldSubsystem::StepRegisteredBoat, TWeakObjectPtr<ATRBoatPawn>(Boat)), {}, true);
	if (Id.IsValid()) { Boat->OnEndPlay.AddUniqueDynamic(this, &UTRSimulationWorldSubsystem::HandleBoatEndPlay); }
	else { Boat->DisableBoat(); }
	return Id;
}

void UTRSimulationWorldSubsystem::StepRegisteredBoat(ETRSimulationPhase Phase, const FTRSimTime& StepTime,
	TWeakObjectPtr<ATRBoatPawn> Boat)
{
	if (!Boat.IsValid() || Boat->IsActorBeingDestroyed() || Boat->GetBoatMode() != ETRBoatMode::DriftOnly) { return; }
	UTROceanWorldSubsystem* Ocean = GetWorld()->GetSubsystem<UTROceanWorldSubsystem>();
	const FVector PositionM = Boat->GetBoatSnapshot().PositionM;
	FTROceanQuery Query;
	Query.PositionXYM = FVector2D(PositionM.X, PositionM.Y);
	Query.SimTick = StepTime.TickIndex;
	const FTROceanSample Sample = Ocean ? Ocean->SampleOcean(Query) : FTROceanSample();
	Boat->StepBoat(StepTime, Sample, [Ocean, Query](const FVector2D& Destination) mutable
	{
		Query.PositionXYM = Destination;
		return Ocean && Ocean->SampleOcean(Query).bValid;
	});
}

bool UTRSimulationWorldSubsystem::GetBoatSnapshot(FTRActorSimId BoatId, FTRBoatSnapshot& OutSnapshot) const
{
	const FRegistration* Entry = FindLive(BoatId);
	const ATRBoatPawn* Boat = Entry && Entry->bBoat ? Cast<ATRBoatPawn>(Entry->Owner.Get()) : nullptr;
	if (!Boat || Boat->GetBoatMode() != ETRBoatMode::DriftOnly) { return false; }
	OutSnapshot = Boat->GetBoatSnapshot();
	return true;
}

void UTRSimulationWorldSubsystem::HandleBoatEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	const FRegistration* Entry = Registrations.FindByPredicate([Actor](const FRegistration& Value) { return Value.bBoat && Value.Owner.Get() == Actor; });
	if (Entry) { const FTRActorSimId Id = Entry->Id; Unregister(Id); }
}
