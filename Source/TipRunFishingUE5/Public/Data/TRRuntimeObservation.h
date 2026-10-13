#pragma once
#include "CoreMinimal.h"

// R7C presentation diagnostics; copied values never control simulation or camera.
struct FTRWorldPresentationObservation
{
 bool bValid=false,bCameraInsideBackground=false,bSeabedCoversBoat=false;
 int64 BoatTick=-1;uint64 Frame=0;
 FVector BoatWorldM=FVector::ZeroVector,StationWorldM=FVector::ZeroVector,CameraWorldM=FVector::ZeroVector;
 FVector BoatVisualWorldM=FVector::ZeroVector,WaterCenterM=FVector::ZeroVector;
 double CameraStationErrorM=0,BoatVisualErrorM=0,BackgroundDistanceM=0;
 FBox SeabedBoundsM=FBox(ForceInit),BackgroundBoundsM=FBox(ForceInit),WaterBoundsM=FBox(ForceInit);
 FString Describe() const;
};

// R4A-1 read-only observations. Never an input to the rod, camera, or physics.
struct FTRRuntimeObservation
{
 FTRWorldPresentationObservation Presentation;
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
