#include "Data/TRRuntimeObservation.h"

FString FTRRuntimeObservation::Describe() const
{
 return FString::Printf(TEXT("valid=%d projected=%d rodTick=%lld worldFrame=%llu visualFrame=%llu cameraFrame=%llu\nBoatRootLocal=%s BoatTipLocal=%s BoatDirLocal=%s Length=%.9g m\nSimRoot=%s SimTip=%s\nMeshRoot=%s MeshTip=%s errorRoot=%.9g errorTip=%.9g m\nActiveCamera=%s rotation=%s FOV=%.6g ViewRect=%s\nScreenRoot=%s ScreenTip=%s projectedLength=%.6g px"),
 bValid,bProjected,RodTick,WorldFrame,VisualFrame,CameraFrame,*RootLocal.ToString(),*TipLocal.ToString(),*DirectionLocal.ToString(),LengthM,
 *SimulationRootM.ToString(),*SimulationTipM.ToString(),*VisualRootM.ToString(),*VisualTipM.ToString(),RootErrorM,TipErrorM,
 *CameraM.ToString(),*CameraRotation.ToString(),FOV,*ViewRect.ToString(),*PixelRoot.ToString(),*PixelTip.ToString(),(PixelTip-PixelRoot).Size())+FString::Printf(TEXT("\nStationRootLocal=%s TipLocal=%s DirectionLocal=%s"),*StationRootLocal.ToString(),*StationTipLocal.ToString(),*StationDirectionLocal.ToString());
}
