#pragma once
#include "CoreMinimal.h"

// R4A-1 read-only observations. Never an input to the rod, camera, or physics.
struct FTRRuntimeObservation
{
 bool bValid=false;
 bool bProjected=false;
 uint64 WorldFrame=0, VisualFrame=0, CameraFrame=0;
 int64 RodTick=-1;
 FVector SimulationRootM=FVector::ZeroVector, SimulationTipM=FVector::ZeroVector;
 FVector VisualRootM=FVector::ZeroVector, VisualTipM=FVector::ZeroVector;
 FVector RootLocal=FVector::ZeroVector, TipLocal=FVector::ZeroVector, DirectionLocal=FVector::ZeroVector;
 FVector StationRootLocal=FVector::ZeroVector, StationTipLocal=FVector::ZeroVector, StationDirectionLocal=FVector::ZeroVector;
 FVector CameraM=FVector::ZeroVector;
 FRotator CameraRotation=FRotator::ZeroRotator;
 double FOV=0, RootErrorM=0, TipErrorM=0, LengthM=0;
 FIntRect ViewRect=FIntRect(0,0,0,0);
 FVector2D PixelRoot=FVector2D::ZeroVector, PixelTip=FVector2D::ZeroVector;
 FString Describe() const;
};
