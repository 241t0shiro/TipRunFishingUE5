#include "Data/TREquipmentData.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/DataValidation.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
	constexpr double TestStep = 1.0 / 60.0;
	const TCHAR* EgiPackage = TEXT("/Game/TipRun/Prototype/Data/DT_TR_Egi_Prototype");
	const TCHAR* SinkerPackage = TEXT("/Game/TipRun/Prototype/Data/DT_TR_Sinker_Prototype");
	const TCHAR* TuningPackage = TEXT("/Game/TipRun/Prototype/Data/DA_TR_FishingTuning_Prototype");

	void SetTestCurve(FRuntimeFloatCurve& Curve, float ValueAt30G, float ValueAt90G)
	{
		Curve.ExternalCurve = nullptr;
		Curve.EditorCurveData.Reset();
		for (const auto& Pair : { TPair<float, float>(30.0f, ValueAt30G), TPair<float, float>(90.0f, ValueAt90G) })
		{
			const FKeyHandle Key = Curve.EditorCurveData.AddKey(Pair.Key, Pair.Value);
			Curve.EditorCurveData.SetKeyInterpMode(Key, RCIM_Linear);
		}
	}

	// Synthetic game approximation coefficients, not measured fishing data or production defaults.
	struct FTRTestEquipment
	{
		TStrongObjectPtr<UDataTable> Egis{NewObject<UDataTable>()};
		TStrongObjectPtr<UDataTable> Sinkers{NewObject<UDataTable>()};
		TStrongObjectPtr<UTRFishingTuningDataAsset> Tuning{NewObject<UTRFishingTuningDataAsset>()};
		TStrongObjectPtr<UStaticMesh> Mesh{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};

		FTRTestEquipment()
		{
			Egis->RowStruct = FTREgiSpecRow::StaticStruct();
			Sinkers->RowStruct = FTRSinkerSpecRow::StaticStruct();
			const TCHAR* EgiIds[] = { TEXT("Egi_3"), TEXT("Egi_3_5"), TEXT("Egi_4") };
			for (int32 Index = 0; Index < 3; ++Index)
			{
				FTREgiSpecRow Row;
				Row.EgiId = FName(EgiIds[Index]);
				Row.DisplayName = FText::FromString(FString::Printf(TEXT("Prototype Egi %.1f"), 3.0f + 0.5f * Index));
				Row.SizeGo = 3.0f + 0.5f * Index;
				Row.BaseMassG = 30.0f + 5.0f * Index;
				Row.SimulationProfileId = FName(TEXT("TestProfile"));
				Row.Mesh = Mesh.Get();
				Egis->AddRow(Row.EgiId, Row);
			}
			for (int32 Mass : { 0, 5, 10, 15, 20, 25, 30, 40, 50 })
			{
				FTRSinkerSpecRow Row;
				Row.SinkerId = Mass == 0 ? FName(TEXT("Sinker_None")) : FName(*FString::Printf(TEXT("Sinker_%d"), Mass));
				Row.MassG = static_cast<float>(Mass);
				Row.DisplayName = FText::FromString(FString::Printf(TEXT("Prototype Sinker %d g"), Mass));
				if (Mass != 0) { Row.VisualMesh = Mesh.Get(); }
				Sinkers->AddRow(Row.SinkerId, Row);
			}
			FTRFishingParameters& P = Tuning->Parameters;
			P.MaxLineLengthM = 100.0f;
			P.MinLineM = 1.0f;
			P.PayoutMps = 3.0f;
			P.TensionPayoutMps = 0.1f;
			P.ReelMps = 2.0f;
			P.FreeFallSinkScale = 1.0f;
			P.TensionFallSinkScale = 0.5f;
			P.StaySinkScale = 0.2f;
			P.JerkDurationS = 0.2; P.TensionLiftDecayPerS = 12.0; P.TensionLiftCompletionMps = 0.05f;
			P.JerkLiftMps = 2.0f;
			P.JerkReelMps = 1.0f;
			P.TensionReferenceM = 0.1f;
			P.MaxEgiSpeedMps = 10.0f;
			P.InitialFightTension01 = 0.2f;
			P.ReelTensionRisePerS = 0.4f;
			P.TensionRecoveryPerS = 0.5f;
			P.OverTensionThreshold01 = 0.8f;
			P.OverTensionDurationS = 0.5;
			P.ReelProgressPerSecond = 0.1f;
			P.RetrievalToleranceM = 0.1f;
			FTREgiSimulationProfile Profile;
			Profile.ProfileId = FName(TEXT("TestProfile"));
			SetTestCurve(Profile.SinkSpeedByTotalMass, 0.6f, 1.8f);
			SetTestCurve(Profile.HorizontalResponseByTotalMass, 2.0f, 0.5f);
			Tuning->Profiles.Add(Profile);
		}

		bool Build(FTREquipmentSnapshot& Out, TArray<FText>& Errors, FName EgiId = FName(TEXT("Egi_3_5")),
			FName SinkerId = FName(TEXT("Sinker_None")), double Step = TestStep) const
		{
			return TREquipment::TryBuildSnapshot(Egis.Get(), Sinkers.Get(), Tuning.Get(), EgiId, SinkerId, Step, Out, Errors);
		}
	};

	bool CheckCombinations(FAutomationTestBase& Test, const UDataTable* Egis, const UDataTable* Sinkers,
		const UTRFishingTuningDataAsset* Tuning)
	{
		TArray<FText> Errors;
		if (!Test.TestTrue(TEXT("Valid equipment data"), TREquipment::ValidateTables(Egis, Sinkers, Tuning, Errors)))
		{
			for (const FText& Error : Errors) { Test.AddError(Error.ToString()); }
			return false;
		}
		Test.TestEqual(TEXT("Three egis"), Egis->GetRowNames().Num(), 3);
		Test.TestEqual(TEXT("Nine sinkers"), Sinkers->GetRowNames().Num(), 9);
		const TCHAR* EgiIds[] = { TEXT("Egi_3"), TEXT("Egi_3_5"), TEXT("Egi_4") };
		int32 Combinations = 0;
		float MinMass = 1000.0f, MaxMass = 0.0f;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FName EgiId(EgiIds[Index]);
			const FTREgiSpecRow* Egi = Egis->FindRow<FTREgiSpecRow>(EgiId, TEXT("F01"), false);
			if (!Test.TestNotNull(TEXT("Specified egi exists"), Egi)) { return false; }
			Test.TestEqual(TEXT("Specified size"), Egi->SizeGo, 3.0f + Index * 0.5f);
			for (int32 Mass : { 0, 5, 10, 15, 20, 25, 30, 40, 50 })
			{
				const FName SinkerId = Mass == 0 ? FName(TEXT("Sinker_None")) : FName(*FString::Printf(TEXT("Sinker_%d"), Mass));
				FTREquipmentSnapshot Snapshot;
				if (!Test.TestTrue(TEXT("F01 combination builds"), TREquipment::TryBuildSnapshot(
					Egis, Sinkers, Tuning, EgiId, SinkerId, TestStep, Snapshot, Errors))) { return false; }
				Test.TestEqual(TEXT("Specified base mass"), Snapshot.BaseMassG, 30.0f + 5.0f * Index);
				Test.TestEqual(TEXT("Specified sinker mass"), Snapshot.SinkerMassG, static_cast<float>(Mass));
				Test.TestEqual(TEXT("F01 expected total"), Snapshot.TotalMassG, 30.0f + 5.0f * Index + Mass);
				MinMass = FMath::Min(MinMass, Snapshot.TotalMassG);
				MaxMass = FMath::Max(MaxMass, Snapshot.TotalMassG);
				++Combinations;
			}
		}
		Test.TestEqual(TEXT("27 combinations"), Combinations, 27);
		Test.TestEqual(TEXT("Minimum 30 g"), MinMass, 30.0f);
		Test.TestEqual(TEXT("Maximum 90 g"), MaxMass, 90.0f);
		Test.TestTrue(TEXT("Approved initial egi"), TREquipment::InitialEgiId() == FName(TEXT("Egi_3_5")));
		return true;
	}

	bool SavePrototype(UObject* Source, const TCHAR* LongPackageName)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(LongPackageName, FPackageName::GetAssetPackageExtension());
		// Explicit generation mode only; never overwrite an edited asset.
		if (FPaths::FileExists(Filename)) { return false; }
		UPackage* Package = CreatePackage(LongPackageName);
		UObject* Asset = StaticDuplicateObject(Source, Package, FName(*FPackageName::GetLongPackageAssetName(LongPackageName)));
		Asset->ClearFlags(RF_Transient);
		Asset->SetFlags(RF_Public | RF_Standalone);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Asset, *Filename, Args);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM02Combinations, "TipRun.M02.F01Combinations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM02Combinations::RunTest(const FString& Parameters)
{
	FTRTestEquipment Data;
	CheckCombinations(*this, Data.Egis.Get(), Data.Sinkers.Get(), Data.Tuning.Get());
	FTREquipmentSnapshot Snapshot;
	TArray<FText> Errors;
	TestTrue(TEXT("Initial egi and test zero sinker accepted"), Data.Build(Snapshot, Errors));
	TestEqual(TEXT("Initial mass 35 g"), Snapshot.TotalMassG, 35.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM02InvalidRows, "TipRun.M02.InvalidRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM02InvalidRows::RunTest(const FString& Parameters)
{
	FTRTestEquipment Data;
	const FTREgiSpecRow GoodEgi = *Data.Egis->FindRow<FTREgiSpecRow>(TREquipment::InitialEgiId(), TEXT("Test"));
	const FTRSinkerSpecRow GoodSinker = *Data.Sinkers->FindRow<FTRSinkerSpecRow>(FName(TEXT("Sinker_5")), TEXT("Test"));
	for (float BadMass : { -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
	{
		TArray<FText> Errors;
		FTREgiSpecRow Egi = GoodEgi; Egi.BaseMassG = BadMass;
		FTRSinkerSpecRow Sinker = GoodSinker; Sinker.MassG = BadMass;
		TestFalse(TEXT("Invalid egi mass rejected"), Egi.Validate(Errors));
		TestFalse(TEXT("Invalid sinker mass rejected"), Sinker.Validate(Errors));
		TestTrue(TEXT("Diagnostic errors supplied"), !Errors.IsEmpty());
	}
	TArray<FText> Errors;
	FTREgiSpecRow Egi = GoodEgi;
	Egi.BaseMassG = 0;
	TestFalse(TEXT("Zero egi rejected"), Egi.Validate(Errors));
	Egi = GoodEgi; Egi.SizeGo = -1;
	TestFalse(TEXT("Negative size rejected"), Egi.Validate(Errors));
	Egi = GoodEgi; Egi.Mesh.Reset();
	TestFalse(TEXT("Missing required mesh rejected"), Egi.Validate(Errors));
	FTRSinkerSpecRow Sinker = GoodSinker;
	Sinker.MassG = 0;
	TestFalse(TEXT("Zero grams with ordinary ID rejected"), Sinker.Validate(Errors));
	Sinker = GoodSinker; Sinker.SinkerId = TREquipment::NoSinkerId();
	TestFalse(TEXT("None ID with positive grams rejected"), Sinker.Validate(Errors));
	Sinker.MassG = 0; Sinker.VisualMesh.Reset();
	TestTrue(TEXT("None ID zero grams with no mesh accepted"), Sinker.Validate(Errors));
	Sinker.SinkerId = NAME_None;
	TestFalse(TEXT("Missing ID is not no-sinker"), Sinker.Validate(Errors));
	FDataValidationContext Context;
	TestTrue(TEXT("Row Editor validation reports invalid"), Sinker.IsDataValid(Context) == EDataValidationResult::Invalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM02References, "TipRun.M02.ReferencesAndDuplicates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM02References::RunTest(const FString& Parameters)
{
	FTRTestEquipment Data;
	TArray<FText> Errors;
	FTREquipmentSnapshot Snapshot; Snapshot.TotalMassG = 123.0f;
	TestFalse(TEXT("Missing table rejected"), TREquipment::ValidateTables(nullptr, Data.Sinkers.Get(), Data.Tuning.Get(), Errors));
	TestFalse(TEXT("Missing tuning rejected"), TREquipment::ValidateTables(Data.Egis.Get(), Data.Sinkers.Get(), nullptr, Errors));
	TestFalse(TEXT("Wrong row type rejected"), TREquipment::ValidateTables(Data.Sinkers.Get(), Data.Egis.Get(), Data.Tuning.Get(), Errors));
	TestFalse(TEXT("Missing selected egi rejected"), Data.Build(Snapshot, Errors, FName(TEXT("Missing"))));
	TestFalse(TEXT("Missing selected sinker rejected"), Data.Build(Snapshot, Errors, TREquipment::InitialEgiId(), NAME_None));
	TestEqual(TEXT("Failure leaves output unchanged"), Snapshot.TotalMassG, 123.0f);
	FTREgiSpecRow* Egi = Data.Egis->FindRow<FTREgiSpecRow>(TREquipment::InitialEgiId(), TEXT("Test"));
	Egi->SimulationProfileId = FName(TEXT("Missing"));
	TestFalse(TEXT("Missing profile rejected"), Data.Build(Snapshot, Errors));
	Egi->SimulationProfileId = FName(TEXT("TestProfile"));
	Data.Egis->AddRow(FName(TEXT("Duplicate")), *Egi);
	TestFalse(TEXT("Duplicate logical ID and mismatched row name rejected"), Data.Build(Snapshot, Errors));
	Data.Egis->RemoveRow(FName(TEXT("Duplicate")));
	const FTRSinkerSpecRow Row = *Data.Sinkers->FindRow<FTRSinkerSpecRow>(TREquipment::NoSinkerId(), TEXT("Test"));
	Data.Sinkers->AddRow(FName(TEXT("Duplicate")), Row);
	TestFalse(TEXT("Duplicate sinker ID rejected"), Data.Build(Snapshot, Errors));
	Data.Sinkers->RemoveRow(FName(TEXT("Duplicate")));
	const FTREgiSimulationProfile DuplicateProfile = Data.Tuning->Profiles[0];
	Data.Tuning->Profiles.Add(DuplicateProfile);
	TestFalse(TEXT("Duplicate profile ID rejected"), Data.Build(Snapshot, Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM02Tuning, "TipRun.M02.TuningValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM02Tuning::RunTest(const FString& Parameters)
{
	FTRTestEquipment Data;
	TArray<FText> Errors;
	const FTRFishingParameters Good = Data.Tuning->Parameters;
	TestEqual(TEXT("Approved Hook open default"), FTRFishingParameters().HookOpenDelayS, 0.10);
	TestEqual(TEXT("Approved Hook close default"), FTRFishingParameters().HookCloseDelayS, 0.55);
	TestFalse(TEXT("Unconfigured coefficients are not valid product defaults"), FTRFishingParameters().Validate(Errors));
	// Every numeric configuration field rejects non-finite/negative values.
	for (TFieldIterator<FNumericProperty> It(FTRFishingParameters::StaticStruct()); It; ++It)
	{
		for (double Bad : { -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() })
		{
			Data.Tuning->Parameters = Good;
			It->SetFloatingPointPropertyValue(It->ContainerPtrToValuePtr<void>(&Data.Tuning->Parameters), Bad);
			TestFalse(It->GetName() + TEXT(" rejects bad numeric value"), Data.Tuning->Validate(Errors));
		}
	}
	Data.Tuning->Parameters = Good;
	Data.Tuning->Parameters.MinLineM = Good.MaxLineLengthM + 1.0f;
	TestFalse(TEXT("Inverted line bounds rejected"), Data.Tuning->Validate(Errors));
	Data.Tuning->Parameters = Good;
	Data.Tuning->Parameters.HookOpenDelayS = Good.HookCloseDelayS;
	TestFalse(TEXT("Closed timing window rejected"), Data.Tuning->Validate(Errors));
	Data.Tuning->Parameters = Good;
	Data.Tuning->Parameters.OverTensionThreshold01 = 1.0f;
	TestFalse(TEXT("Unreachable threshold rejected"), Data.Tuning->Validate(Errors));
	Data.Tuning->Parameters = Good;
	Data.Tuning->Parameters.InitialFightTension01 = 1.0f;
	TestFalse(TEXT("Initial tension exceeds threshold"), Data.Tuning->Validate(Errors));
	Data.Tuning->Parameters = Good;
	const FTREgiSimulationProfile Profile = Data.Tuning->Profiles[0];
	Data.Tuning->Profiles[0].SinkSpeedByTotalMass.EditorCurveData.Reset();
	TestFalse(TEXT("Empty curve rejected"), Data.Tuning->Validate(Errors));
	Data.Tuning->Profiles[0] = Profile;
	SetTestCurve(Data.Tuning->Profiles[0].SinkSpeedByTotalMass, -1.0f, 1.0f);
	TestFalse(TEXT("Negative curve values rejected"), Data.Tuning->Validate(Errors));
	Data.Tuning->Profiles[0] = Profile;
	auto* Curve = Data.Tuning->Profiles[0].SinkSpeedByTotalMass.GetRichCurve();
	Curve->SetKeyInterpMode(Curve->GetFirstKeyHandle(), RCIM_Cubic);
	TestFalse(TEXT("Unvalidated spline interpolation rejected"), Data.Tuning->Validate(Errors));
	Data.Tuning->Profiles[0] = Profile;
	Curve = Data.Tuning->Profiles[0].SinkSpeedByTotalMass.GetRichCurve();
	Curve->SetKeyTime(Curve->GetLastKeyHandle(), 80.0f);
	TestFalse(TEXT("Insufficient curve domain rejected"), Data.Tuning->Validate(Errors));
	Data.Tuning->Profiles[0] = Profile;
	FDataValidationContext Context;
	TestTrue(TEXT("Editor tuning validation succeeds"), Data.Tuning->IsDataValid(Context) == EDataValidationResult::Valid);
	FTREquipmentSnapshot Snapshot;
	TestFalse(TEXT("Invalid fixed step rejected"), Data.Build(Snapshot, Errors, TREquipment::InitialEgiId(), TREquipment::NoSinkerId(), 0.0));
	Data.Tuning->Parameters.HookOpenDelayS = 0.101;
	Data.Tuning->Parameters.HookCloseDelayS = 0.102;
	TestFalse(TEXT("Distinct seconds collapsing to same tick rejected"), Data.Build(Snapshot, Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM02Snapshot, "TipRun.M02.FrozenSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM02Snapshot::RunTest(const FString& Parameters)
{
	FTRTestEquipment Data;
	TArray<FText> Errors;
	FTREquipmentSnapshot Before;
	if (!TestTrue(TEXT("Snapshot builds"), Data.Build(Before, Errors))) { return false; }
	const float FrozenSpeed = Before.SinkSpeedMps;
	Data.Tuning->Parameters.ReelMps = 7.0f;
	SetTestCurve(Data.Tuning->Profiles[0].SinkSpeedByTotalMass, 2.0f, 3.0f);
	Data.Egis->FindRow<FTREgiSpecRow>(TREquipment::InitialEgiId(), TEXT("Test"))->BaseMassG = 40.0f;
	TestEqual(TEXT("Frozen mass unaffected by table edit"), Before.TotalMassG, 35.0f);
	TestEqual(TEXT("Frozen coefficient unaffected by curve edit"), Before.SinkSpeedMps, FrozenSpeed);
	TestEqual(TEXT("Frozen parameters unaffected by tuning edit"), Before.Parameters.ReelMps, 2.0f);
	FTREquipmentSnapshot After;
	TestTrue(TEXT("New snapshot sees edits"), Data.Build(After, Errors));
	TestEqual(TEXT("Updated snapshot mass"), After.TotalMassG, 40.0f);
	TestEqual(TEXT("Updated snapshot parameters"), After.Parameters.ReelMps, 7.0f);
	TestTrue(TEXT("Updated coefficient"), After.SinkSpeedMps != FrozenSpeed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRM02PrototypeAssets, "TipRun.M02.PrototypeAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTRM02PrototypeAssets::RunTest(const FString& Parameters)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("TRWriteM02PrototypeAssets")))
	{
		FTRTestEquipment Data;
		if (!CheckCombinations(*this, Data.Egis.Get(), Data.Sinkers.Get(), Data.Tuning.Get())) { return false; }
		TestTrue(TEXT("Save Egi Prototype with UE serialization"), SavePrototype(Data.Egis.Get(), EgiPackage));
		TestTrue(TEXT("Save Sinker Prototype with UE serialization"), SavePrototype(Data.Sinkers.Get(), SinkerPackage));
		TestTrue(TEXT("Save Tuning Prototype with UE serialization"), SavePrototype(Data.Tuning.Get(), TuningPackage));
	}
	else
	{
		// Run in a fresh Editor process after generation to verify the on-disk assets.
		TStrongObjectPtr<UDataTable> Egis(LoadObject<UDataTable>(nullptr, EgiPackage));
		TStrongObjectPtr<UDataTable> Sinkers(LoadObject<UDataTable>(nullptr, SinkerPackage));
		TStrongObjectPtr<UTRFishingTuningDataAsset> Tuning(LoadObject<UTRFishingTuningDataAsset>(nullptr, TuningPackage));
		CheckCombinations(*this, Egis.Get(), Sinkers.Get(), Tuning.Get());
		if (Egis.IsValid() && Sinkers.IsValid() && Tuning.IsValid())
		{
			FDataValidationContext EgiContext, SinkerContext, TuningContext;
			TestTrue(TEXT("Persisted Egi rows validate"), Egis->IsDataValid(EgiContext) == EDataValidationResult::Valid);
			TestTrue(TEXT("Persisted Sinker rows validate"), Sinkers->IsDataValid(SinkerContext) == EDataValidationResult::Valid);
			TestTrue(TEXT("Persisted Tuning validates"), Tuning->IsDataValid(TuningContext) == EDataValidationResult::Valid);
		}
	}
	return true;
}

#endif
