#include "Ocean/TROceanWorldSubsystem.h"
#include "Data/TROceanAreaDataAsset.h"
#include "Ocean/TRSeabedProviderActor.h"

bool UTROceanWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UTROceanWorldSubsystem::InitializeArea(UTROceanAreaDataAsset* InArea,
	ATRSeabedProviderActor* InProvider, TArray<FText>& Errors)
{
	ShutdownArea();
	OceanState = ETROceanState::Error;
	if (!IsValid(InArea) || !IsValid(InProvider) || InProvider->GetWorld() != GetWorld())
	{
		Errors.Add(FText::FromString(TEXT("Ocean requires an AreaData and a valid provider from the same world")));
		return false;
	}
	if (!InProvider->InitializeFromArea(*InArea, Errors)) { return false; }
	AreaData = InArea;
	Settings = InArea->Settings;
	Provider = InProvider;
	ProviderRevision = InProvider->GetConfigurationRevision();
	OceanState = ETROceanState::Ready;
	return true;
}

bool UTROceanWorldSubsystem::SetSimulationTime(double InEnvironmentTimeS)
{
	if (OceanState != ETROceanState::Ready || !FMath::IsFinite(InEnvironmentTimeS) || InEnvironmentTimeS < 0.0)
	{
		return false;
	}
	EnvironmentTimeS = InEnvironmentTimeS;
	return true;
}

FTROceanSample UTROceanWorldSubsystem::SampleOcean(const FTROceanQuery& Query) const
{
	FTROceanSample Sample;
	Sample.SampleTick = Query.SimTick;
	if (OceanState != ETROceanState::Ready) { return Sample; }
	const ATRSeabedProviderActor* ActiveProvider = Provider.Get();
	if (!IsValid(ActiveProvider) || !ActiveProvider->IsConfigured() ||
		ActiveProvider->GetConfigurationRevision() != ProviderRevision)
	{
		Sample.InvalidReason = ETRSampleError::MissingProvider;
		return Sample;
	}
	if (!FMath::IsFinite(Query.DepthM) || Query.DepthM < 0.0f)
	{
		Sample.InvalidReason = ETRSampleError::InvalidDepth;
		return Sample;
	}
	if (Query.SimTick < 0 || !FMath::IsFinite(Query.PositionXYM.X) || !FMath::IsFinite(Query.PositionXYM.Y))
	{
		Sample.InvalidReason = ETRSampleError::InvalidQuery;
		return Sample;
	}
	const FTRDepthSample Bottom = ActiveProvider->SampleBottomDepth(Query.PositionXYM);
	if (!Bottom.bValid)
	{
		Sample.InvalidReason = Bottom.InvalidReason;
		return Sample;
	}
	// Constant MVP current has no depth-dependent evaluation. Finite depths below the
	// bottom remain valid so the caller can project a trial position back to the seabed.
	Sample.bValid = true;
	Sample.InvalidReason = ETRSampleError::None;
	Sample.AreaId = Settings.AreaId;
	Sample.SurfaceZ_M = Settings.SurfaceZ_M;
	Sample.BottomDepthM = Bottom.BottomDepthM;
	Sample.BottomNormal = Bottom.BottomNormal;
	Sample.CurrentMps = Settings.CurrentMps;
	// SeasonId=None and WindMps=0 remain the common type defaults.
	return Sample;
}

void UTROceanWorldSubsystem::ShutdownArea()
{
	Provider.Reset();
	AreaData = nullptr;
	Settings = {};
	ProviderRevision = 0;
	EnvironmentTimeS = 0.0;
	OceanState = ETROceanState::Unloaded;
}

void UTROceanWorldSubsystem::Deinitialize()
{
	ShutdownArea();
	Super::Deinitialize();
}
