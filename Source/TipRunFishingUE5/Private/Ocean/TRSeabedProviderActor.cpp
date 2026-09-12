#include "Ocean/TRSeabedProviderActor.h"
#include "Components/SceneComponent.h"
#include "Data/TROceanAreaDataAsset.h"

ATRSeabedProviderActor::ATRSeabedProviderActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

bool ATRSeabedProviderActor::InitializeFromArea(const UTROceanAreaDataAsset& Area, TArray<FText>& Errors)
{
	bConfigured = false;
	Settings = {};
	if (IsActorBeingDestroyed() || ConfigurationRevision == MAX_uint64 || !Area.Validate(Errors))
	{
		if (Errors.IsEmpty()) { Errors.Add(FText::FromString(TEXT("Seabed provider cannot be initialized"))); }
		return false;
	}
	Settings = Area.Settings;
	++ConfigurationRevision;
	bConfigured = true;
	return true;
}

FTRDepthSample ATRSeabedProviderActor::SampleBottomDepth(const FVector2D& PositionXYM) const
{
	FTRDepthSample Sample;
	if (!IsConfigured()) { return Sample; }
	if (!FMath::IsFinite(PositionXYM.X) || !FMath::IsFinite(PositionXYM.Y))
	{
		Sample.InvalidReason = ETRSampleError::InvalidQuery;
		return Sample;
	}
	if (!Settings.Contains(PositionXYM))
	{
		Sample.InvalidReason = ETRSampleError::OutsideArea;
		return Sample;
	}
	Sample.bValid = true;
	Sample.InvalidReason = ETRSampleError::None;
	Sample.BottomDepthM = Settings.FlatDepthM;
	Sample.BottomNormal = FVector::UpVector;
	return Sample;
}

void ATRSeabedProviderActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bConfigured = false;
	Settings = {};
	Super::EndPlay(EndPlayReason);
}
