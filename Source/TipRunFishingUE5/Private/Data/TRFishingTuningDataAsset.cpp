#include "Data/TRFishingTuningDataAsset.h"
#include "Misc/DataValidation.h"

namespace
{
	bool RequireFishingTuning(bool Condition, const FString& Message, TArray<FText>& Errors)
	{
		if (!Condition) { Errors.Add(FText::FromString(Message)); }
		return Condition;
	}

	bool ValidateCurve(const FRichCurve& Curve, const FString& Name, TArray<FText>& Errors)
	{
		bool bValid = true;
		const TArray<FRichCurveKey>& Keys = Curve.GetConstRefOfKeys();
		bValid &= RequireFishingTuning(Keys.Num() >= 2, Name + TEXT(": at least two keys required"), Errors);
		if (Keys.Num() < 2) { return false; }
		bValid &= RequireFishingTuning(Keys[0].Time <= 30.0f && Keys.Last().Time >= 90.0f,
			Name + TEXT(": must cover 30..90 g"), Errors);
		float PreviousTime = -TNumericLimits<float>::Max();
		for (const FRichCurveKey& Key : Keys)
		{
			bValid &= RequireFishingTuning(FMath::IsFinite(Key.Time) && Key.Time > PreviousTime &&
				FMath::IsFinite(Key.Value) && Key.Value > 0.0f &&
				FMath::IsFinite(Key.ArriveTangent) && FMath::IsFinite(Key.LeaveTangent) &&
				FMath::IsFinite(Key.ArriveTangentWeight) && FMath::IsFinite(Key.LeaveTangentWeight),
				Name + TEXT(": keys must be finite, ordered, with positive values"), Errors);
			bValid &= RequireFishingTuning(Key.InterpMode == RCIM_Linear,
				Name + TEXT(": use linear interpolation to avoid negative spline overshoot"), Errors);
			PreviousTime = Key.Time;
		}
		return bValid;
	}
}

bool FTRFishingParameters::Validate(TArray<FText>& Errors) const
{
	bool bValid = true;
	bValid &= RequireFishingTuning(FMath::IsFinite(MaxLineLengthM) && MaxLineLengthM > 0, TEXT("MaxLineLengthM: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(MinLineM) && MinLineM > 0, TEXT("MinLineM: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(PayoutMps) && PayoutMps > 0, TEXT("PayoutMps: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(TensionPayoutMps) && TensionPayoutMps > 0, TEXT("TensionPayoutMps: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(ReelMps) && ReelMps > 0, TEXT("ReelMps: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(FreeFallSinkScale) && FreeFallSinkScale > 0, TEXT("FreeFallSinkScale: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(TensionFallSinkScale) && TensionFallSinkScale > 0, TEXT("TensionFallSinkScale: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(StaySinkScale) && StaySinkScale > 0, TEXT("StaySinkScale: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(JerkDurationS) && JerkDurationS > 0, TEXT("JerkDurationS: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(JerkLiftMps) && JerkLiftMps > 0, TEXT("JerkLiftMps: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(JerkReelMps) && JerkReelMps > 0, TEXT("JerkReelMps: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(TensionReferenceM) && TensionReferenceM > 0, TEXT("TensionReferenceM: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(MaxEgiSpeedMps) && MaxEgiSpeedMps > 0, TEXT("MaxEgiSpeedMps: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(HookCloseDelayS) && HookCloseDelayS > 0, TEXT("HookCloseDelayS: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(ReelTensionRisePerS) && ReelTensionRisePerS > 0, TEXT("ReelTensionRisePerS: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(TensionRecoveryPerS) && TensionRecoveryPerS > 0, TEXT("TensionRecoveryPerS: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(OverTensionDurationS) && OverTensionDurationS > 0, TEXT("OverTensionDurationS: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(ReelProgressPerSecond) && ReelProgressPerSecond > 0, TEXT("ReelProgressPerSecond: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(RetrievalToleranceM) && RetrievalToleranceM > 0, TEXT("RetrievalToleranceM: must be finite and positive"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(AutoStayDelayS) && AutoStayDelayS >= 0.0,
		TEXT("AutoStayDelayS: must be finite and nonnegative"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(HookOpenDelayS) && HookOpenDelayS >= 0.0 && HookOpenDelayS < HookCloseDelayS,
		TEXT("Hook window: require 0 <= open < close"), Errors);
	bValid &= RequireFishingTuning(MinLineM <= MaxLineLengthM, TEXT("MinLineM exceeds MaxLineLengthM"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(OverTensionThreshold01) && OverTensionThreshold01 > 0.0f && OverTensionThreshold01 < 1.0f,
		TEXT("OverTensionThreshold01: must be within (0, 1)"), Errors);
	bValid &= RequireFishingTuning(FMath::IsFinite(InitialFightTension01) && InitialFightTension01 >= 0.0f && InitialFightTension01 <= OverTensionThreshold01,
		TEXT("InitialFightTension01: must be within [0, threshold]"), Errors);
	return bValid;
}

bool FTREgiSimulationProfile::Validate(TArray<FText>& Errors) const
{
	bool bValid = RequireFishingTuning(!ProfileId.IsNone(), TEXT("ProfileId is missing"), Errors);
	const FString Prefix = ProfileId.ToString();
	bValid &= ValidateCurve(*SinkSpeedByTotalMass.GetRichCurveConst(), Prefix + TEXT(".SinkSpeed"), Errors);
	bValid &= ValidateCurve(*HorizontalResponseByTotalMass.GetRichCurveConst(), Prefix + TEXT(".HorizontalResponse"), Errors);
	return bValid;
}

bool UTRFishingTuningDataAsset::Validate(TArray<FText>& Errors) const
{
	bool bValid = Parameters.Validate(Errors);
	bValid &= RequireFishingTuning(!Profiles.IsEmpty(), TEXT("Simulation profiles are missing"), Errors);
	TSet<FName> Ids;
	for (const FTREgiSimulationProfile& Profile : Profiles)
	{
		bValid &= Profile.Validate(Errors);
		bValid &= RequireFishingTuning(!Ids.Contains(Profile.ProfileId), TEXT("Duplicate ProfileId: ") + Profile.ProfileId.ToString(), Errors);
		Ids.Add(Profile.ProfileId);
	}
	return bValid;
}

const FTREgiSimulationProfile* UTRFishingTuningDataAsset::FindProfile(FName ProfileId) const
{
	return Profiles.FindByPredicate([ProfileId](const FTREgiSimulationProfile& Profile) { return Profile.ProfileId == ProfileId; });
}

#if WITH_EDITOR
EDataValidationResult UTRFishingTuningDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	const bool bValid = Validate(Errors);
	for (const FText& Error : Errors) { Context.AddError(Error); }
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
