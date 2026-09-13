#include "Ocean/TROceanWorldSubsystem.h"
#include "Data/TROceanAreaDataAsset.h"
#include "Ocean/TRSeabedProviderActor.h"
#include "Ocean/TREnvironmentField.h"

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
	Field = MakeShared<FTREnvironmentField>();
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
	FTROceanQuery FieldQuery = Query;
	FieldQuery.DepthM = FMath::Min(Query.DepthM, Bottom.BottomDepthM);
	const FVector Current = Field->CurrentAtLocationAndDepth(Settings, FieldQuery);
	FieldQuery.DepthM = 0.0f;
	const FVector SurfaceCurrent = Field->CurrentAtLocationAndDepth(Settings, FieldQuery);
	const FVector2D Wind = Field->WindAtLocation(Settings, Query.PositionXYM, Query.SimTick);
	if (Current.ContainsNaN() || SurfaceCurrent.ContainsNaN() || Wind.ContainsNaN() ||
		Current.Z != 0.0 || SurfaceCurrent.Z != 0.0 || !FMath::IsFinite(Current.SizeSquared()) ||
		!FMath::IsFinite(SurfaceCurrent.SizeSquared()) || !FMath::IsFinite(Wind.SizeSquared()))
	{
		Sample.InvalidReason = ETRSampleError::InvalidQuery;
		return Sample;
	}
	Sample.bValid = true;
	Sample.InvalidReason = ETRSampleError::None;
	Sample.AreaId = Settings.AreaId;
	Sample.SurfaceZ_M = Settings.SurfaceZ_M;
	Sample.BottomDepthM = Bottom.BottomDepthM;
	Sample.BottomNormal = Bottom.BottomNormal;
	Sample.CurrentMps = Current;
	Sample.SurfaceCurrentMps = SurfaceCurrent;
	Sample.WindMps = Wind;
	Sample.FieldRevision = Settings.FieldRevision;
	return Sample;
}

void UTROceanWorldSubsystem::ShutdownArea()
{
	Field.Reset();
	Provider.Reset();
	AreaData = nullptr;
	Settings = {};
	ProviderRevision = 0;
	EnvironmentTimeS = 0.0;
	OceanState = ETROceanState::Unloaded;
}

bool UTROceanWorldSubsystem::InitializeAreaWithField(UTROceanAreaDataAsset* InArea,
	ATRSeabedProviderActor* InProvider, TSharedRef<const FTREnvironmentField> InField, TArray<FText>& Errors)
{
	if (!IsValid(InArea) || InArea->Settings.FieldRevision != 2)
	{
		ShutdownArea(); OceanState = ETROceanState::Error;
		Errors.Add(FText::FromString(TEXT("Spatial field requires explicit revision 2 settings")));
		return false;
	}
	if (!InitializeArea(InArea, InProvider, Errors)) { return false; }
	Field = InField;
	return true;
}

FTROceanSample UTROceanWorldSubsystem::SampleSurfaceCurrent(const FVector2D& PositionXYM, int64 SimTick) const
{
	FTROceanQuery Query; Query.PositionXYM = PositionXYM; Query.SimTick = SimTick;
	return SampleOcean(Query);
}

FTROceanSample UTROceanWorldSubsystem::SampleWindAtLocation(const FVector2D& PositionXYM, int64 SimTick) const
{
	return SampleSurfaceCurrent(PositionXYM, SimTick);
}

void UTROceanWorldSubsystem::Deinitialize()
{
	ShutdownArea();
	Super::Deinitialize();
}
