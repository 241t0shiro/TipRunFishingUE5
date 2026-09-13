#include "Data/TROceanAreaDataAsset.h"
#include "Misc/DataValidation.h"

bool FTROceanAreaSettings::Validate(TArray<FText>& Errors) const
{
	bool bValid = true;
	const auto Require = [&Errors, &bValid](bool Condition, const TCHAR* Message)
	{
		if (!Condition) { bValid = false; Errors.Add(FText::FromString(Message)); }
	};
	Require(!AreaId.IsNone(), TEXT("Ocean AreaId is missing"));
	Require(FMath::IsFinite(BoundsMinXYM.X) && FMath::IsFinite(BoundsMinXYM.Y) &&
		FMath::IsFinite(BoundsMaxXYM.X) && FMath::IsFinite(BoundsMaxXYM.Y) &&
		BoundsMinXYM.X < BoundsMaxXYM.X && BoundsMinXYM.Y < BoundsMaxXYM.Y,
		TEXT("Ocean bounds must be finite with min < max on both axes"));
	Require(FMath::IsFinite(SurfaceZ_M), TEXT("Ocean surface Z must be finite"));
	Require(FMath::IsFinite(FlatDepthM) && FlatDepthM > 0.0f &&
		FMath::IsFinite(SurfaceZ_M - FlatDepthM), TEXT("Ocean bottom depth/world Z must be finite and depth positive"));
	Require(SeabedMode == ETRSeabedMode::Flat, TEXT("M03 supports only Flat seabed"));
	Require(FMath::IsFinite(CurrentMps.X) && FMath::IsFinite(CurrentMps.Y) &&
		FMath::IsFinite(CurrentMps.Z) && CurrentMps.Z == 0.0,
		TEXT("MVP current must be finite and horizontal (Z = 0)"));
	Require(FMath::IsFinite(CurrentMps.SizeSquared()) && FMath::IsFinite(WindMps.SizeSquared()),
		TEXT("Environment vectors and their squared magnitudes must be finite"));
	Require(FieldRevision == 1 || FieldRevision == 2, TEXT("Unknown ocean field revision"));
	Require(CurrentMode == ETRCurrentFieldMode::Constant || CurrentMode == ETRCurrentFieldMode::DepthProfile,
		TEXT("Unknown current field mode"));
	Require(FieldRevision != 1 || (CurrentMode == ETRCurrentFieldMode::Constant &&
		WindMps == FVector2D::ZeroVector && CurrentDepthProfile.IsEmpty()),
		TEXT("Legacy field revision requires constant current and no wind/profile; select revision 2 explicitly"));
	Require(CurrentMode != ETRCurrentFieldMode::Constant || CurrentDepthProfile.IsEmpty(),
		TEXT("Constant field must not contain an unused depth profile"));
	if (CurrentMode == ETRCurrentFieldMode::DepthProfile)
	{
		Require(!CurrentDepthProfile.IsEmpty() && CurrentDepthProfile[0].DepthM == 0.0f,
			TEXT("Depth profile requires its first key at depth zero"));
		float PreviousDepthM = -1.0f;
		for (const FTRCurrentDepthKey& Key : CurrentDepthProfile)
		{
			Require(FMath::IsFinite(Key.DepthM) && Key.DepthM >= 0.0f && Key.DepthM > PreviousDepthM,
				TEXT("Current depths must be finite, nonnegative and strictly increasing"));
			Require(!Key.CurrentMps.ContainsNaN() && Key.CurrentMps.Z == 0.0 &&
				FMath::IsFinite(Key.CurrentMps.SizeSquared()), TEXT("Profile velocities must be finite and horizontal"));
			PreviousDepthM = Key.DepthM;
		}
	}
	return bValid;
}

bool FTROceanAreaSettings::Contains(const FVector2D& PositionXYM) const
{
	return FMath::IsFinite(PositionXYM.X) && FMath::IsFinite(PositionXYM.Y) &&
		PositionXYM.X >= BoundsMinXYM.X && PositionXYM.X <= BoundsMaxXYM.X &&
		PositionXYM.Y >= BoundsMinXYM.Y && PositionXYM.Y <= BoundsMaxXYM.Y;
}

#if WITH_EDITOR
EDataValidationResult UTROceanAreaDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	const bool bValid = Validate(Errors);
	for (const FText& Error : Errors) { Context.AddError(Error); }
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
