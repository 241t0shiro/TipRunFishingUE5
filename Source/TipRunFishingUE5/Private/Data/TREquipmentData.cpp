#include "Data/TREquipmentData.h"
#include "Data/TRSimulationTypes.h"
#include "Engine/StaticMesh.h"
#include "Misc/DataValidation.h"

namespace
{
	bool Require(bool Condition, const FString& Message, TArray<FText>& Errors)
	{
		if (!Condition) { Errors.Add(FText::FromString(Message)); }
		return Condition;
	}

	template<typename RowType, typename IdGetter>
	bool ValidateRows(const UDataTable& Table, IdGetter GetId, TArray<FText>& Errors)
	{
		bool bValid = true;
		TSet<FName> Ids;
		for (const FName RowName : Table.GetRowNames())
		{
			const RowType* Row = Table.FindRow<RowType>(RowName, TEXT("M02 validation"), false);
			if (!Row) { bValid = Require(false, TEXT("Missing table row"), Errors); continue; }
			bValid &= Row->Validate(Errors);
			const FName Id = GetId(*Row);
			bValid &= Require(RowName == Id, TEXT("Row name must match ID: ") + RowName.ToString(), Errors);
			bValid &= Require(!Ids.Contains(Id), TEXT("Duplicate equipment ID: ") + Id.ToString(), Errors);
			Ids.Add(Id);
		}
		return bValid;
	}
}

FName TREquipment::InitialEgiId() { return FName(TEXT("Egi_3_5")); }
FName TREquipment::NoSinkerId() { return FName(TEXT("Sinker_None")); }

bool FTREgiSpecRow::Validate(TArray<FText>& Errors) const
{
	bool bValid = Require(!EgiId.IsNone() && !SimulationProfileId.IsNone(), TEXT("Egi ID/profile ID is missing"), Errors);
	bValid &= Require(FMath::IsFinite(SizeGo) && SizeGo > 0.0f, TEXT("Egi size must be finite and positive"), Errors);
	bValid &= Require(FMath::IsFinite(BaseMassG) && BaseMassG > 0.0f, TEXT("Egi mass must be finite and positive"), Errors);
	bValid &= Require(!DisplayName.IsEmpty(), TEXT("Egi display name is missing"), Errors);
	bValid &= Require(!Mesh.IsNull() && Mesh.LoadSynchronous() != nullptr, TEXT("Egi mesh reference is missing/unresolved"), Errors);
	return bValid;
}

bool FTRSinkerSpecRow::Validate(TArray<FText>& Errors) const
{
	bool bValid = Require(!SinkerId.IsNone(), TEXT("Sinker ID is missing"), Errors);
	bValid &= Require(FMath::IsFinite(MassG) && MassG >= 0.0f, TEXT("Sinker mass must be finite and nonnegative"), Errors);
	bValid &= Require((MassG == 0.0f) == (SinkerId == TREquipment::NoSinkerId()),
		TEXT("Only Sinker_None may represent zero grams"), Errors);
	bValid &= Require(!DisplayName.IsEmpty(), TEXT("Sinker display name is missing"), Errors);
	if (MassG != 0.0f || !VisualMesh.IsNull())
	{
		bValid &= Require(!VisualMesh.IsNull() && VisualMesh.LoadSynchronous() != nullptr,
			TEXT("Sinker mesh reference is missing/unresolved"), Errors);
	}
	return bValid;
}

bool TREquipment::ValidateTables(const UDataTable* Egis, const UDataTable* Sinkers,
	const UTRFishingTuningDataAsset* Tuning, TArray<FText>& Errors)
{
	bool bValid = Require(Egis && Egis->GetRowStruct() == FTREgiSpecRow::StaticStruct(), TEXT("Missing/wrong Egi DataTable"), Errors);
	bValid &= Require(Sinkers && Sinkers->GetRowStruct() == FTRSinkerSpecRow::StaticStruct(), TEXT("Missing/wrong Sinker DataTable"), Errors);
	bValid &= Require(Tuning != nullptr, TEXT("FishingTuning reference is missing"), Errors);
	if (!bValid) { return false; }
	bValid &= Require(!Egis->GetRowMap().IsEmpty() && !Sinkers->GetRowMap().IsEmpty(), TEXT("Equipment tables must not be empty"), Errors);
	bValid &= Tuning->Validate(Errors);
	bValid &= ValidateRows<FTREgiSpecRow>(*Egis, [](const FTREgiSpecRow& Row) { return Row.EgiId; }, Errors);
	bValid &= ValidateRows<FTRSinkerSpecRow>(*Sinkers, [](const FTRSinkerSpecRow& Row) { return Row.SinkerId; }, Errors);
	for (const FName RowName : Egis->GetRowNames())
	{
		const FTREgiSpecRow* Egi = Egis->FindRow<FTREgiSpecRow>(RowName, TEXT("M02 validation"), false);
		if (!Egi) { continue; }
		const FTREgiSimulationProfile* Profile = Tuning->FindProfile(Egi->SimulationProfileId);
		bValid &= Require(Profile != nullptr, TEXT("Missing simulation profile: ") + Egi->SimulationProfileId.ToString(), Errors);
		if (!Profile) { continue; }
		for (const FName SinkerName : Sinkers->GetRowNames())
		{
			const FTRSinkerSpecRow* Sinker = Sinkers->FindRow<FTRSinkerSpecRow>(SinkerName, TEXT("M02 validation"), false);
			if (!Sinker) { continue; }
			const float MassG = Egi->BaseMassG + Sinker->MassG;
			bValid &= Require(FMath::IsFinite(MassG) && MassG > 0.0f, TEXT("Invalid total equipment mass"), Errors);
			for (const FRichCurve* Curve : { Profile->SinkSpeedByTotalMass.GetRichCurveConst(), Profile->HorizontalResponseByTotalMass.GetRichCurveConst() })
			{
				const auto& Keys = Curve->GetConstRefOfKeys();
				bValid &= Require(Keys.Num() >= 2 && MassG >= Keys[0].Time && MassG <= Keys.Last().Time,
					TEXT("Equipment mass outside profile curve domain"), Errors);
			}
		}
	}
	return bValid;
}

bool TREquipment::TryBuildSnapshot(const UDataTable* Egis, const UDataTable* Sinkers,
	const UTRFishingTuningDataAsset* Tuning, FName EgiId, FName SinkerId,
	double StepSeconds, FTREquipmentSnapshot& OutSnapshot, TArray<FText>& Errors)
{
	if (!ValidateTables(Egis, Sinkers, Tuning, Errors)) { return false; }
	const FTREgiSpecRow* Egi = Egis->FindRow<FTREgiSpecRow>(EgiId, TEXT("Equipment snapshot"), false);
	const FTRSinkerSpecRow* Sinker = Sinkers->FindRow<FTRSinkerSpecRow>(SinkerId, TEXT("Equipment snapshot"), false);
	if (!Require(Egi && Sinker, TEXT("Selected equipment ID is missing"), Errors)) { return false; }
	int64 OpenTicks = 0, CloseTicks = 0, OtherTicks = 0;
	const FTRFishingParameters& P = Tuning->Parameters;
	if (!Require(TRTime::TrySecondsToTicks(P.HookOpenDelayS, StepSeconds, OpenTicks) &&
		TRTime::TrySecondsToTicks(P.HookCloseDelayS, StepSeconds, CloseTicks) && CloseTicks > OpenTicks &&
		TRTime::TrySecondsToTicks(P.JerkDurationS, StepSeconds, OtherTicks) && OtherTicks > 0 &&
		TRTime::TrySecondsToTicks(P.OverTensionDurationS, StepSeconds, OtherTicks) && OtherTicks > 0 &&
		(P.QuickRetrieveDurationS == 0.0 ||
			(TRTime::TrySecondsToTicks(P.QuickRetrieveDurationS, StepSeconds, OtherTicks) && OtherTicks > 0)),
		TEXT("Invalid fixed step or unrepresentable timing window"), Errors)) { return false; }
	const FTREgiSimulationProfile* Profile = Tuning->FindProfile(Egi->SimulationProfileId);
	FTREquipmentSnapshot Snapshot;
	Snapshot.EgiId = EgiId;
	Snapshot.SinkerId = SinkerId;
	Snapshot.SimulationProfileId = Egi->SimulationProfileId;
	Snapshot.BaseMassG = Egi->BaseMassG;
	Snapshot.SinkerMassG = Sinker->MassG;
	Snapshot.TotalMassG = Snapshot.BaseMassG + Snapshot.SinkerMassG;
	Snapshot.SinkSpeedMps = Profile->SinkSpeedByTotalMass.GetRichCurveConst()->Eval(Snapshot.TotalMassG);
	Snapshot.HorizontalResponsePerS = Profile->HorizontalResponseByTotalMass.GetRichCurveConst()->Eval(Snapshot.TotalMassG);
	if (!Require(FMath::IsFinite(Snapshot.SinkSpeedMps) && Snapshot.SinkSpeedMps > 0.0f &&
		FMath::IsFinite(Snapshot.HorizontalResponsePerS) && Snapshot.HorizontalResponsePerS > 0.0f,
		TEXT("Invalid evaluated profile coefficients"), Errors)) { return false; }
	Snapshot.Parameters = P;
	OutSnapshot = Snapshot;
	return true;
}

#if WITH_EDITOR
EDataValidationResult FTREgiSpecRow::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	const bool bValid = Validate(Errors);
	for (const FText& Error : Errors) { Context.AddError(Error); }
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
EDataValidationResult FTRSinkerSpecRow::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	const bool bValid = Validate(Errors);
	for (const FText& Error : Errors) { Context.AddError(Error); }
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
