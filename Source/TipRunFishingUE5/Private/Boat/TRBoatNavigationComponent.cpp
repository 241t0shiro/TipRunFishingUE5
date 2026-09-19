#include "Boat/TRBoatNavigationComponent.h"
UTRBoatNavigationComponent::UTRBoatNavigationComponent(){PrimaryComponentTick.bCanEverTick=false;}
bool UTRBoatNavigationComponent::Configure(const FTRNavigationParameters& In)
{if(!In.Validate()){return false;}Settings=In;State={};State.bConfigured=true;return true;}
bool UTRBoatNavigationComponent::SetInput(FVector2D Input)
{if(!State.bConfigured||Input.ContainsNaN()||FMath::Abs(Input.X)>1||FMath::Abs(Input.Y)>1){return false;}State.Throttle=Input.X;State.Steering=Input.Y;return true;}
void UTRBoatNavigationComponent::Clear()
{const bool Valid=State.bConfigured;State={};State.bConfigured=Valid;}
void UTRBoatNavigationComponent::Step(double Dt,bool bAllowed)
{
 if(!bAllowed||!State.bConfigured||!FMath::IsFinite(Dt)||Dt<=0){Clear();return;}
 const double BoostTarget=State.bBoostRequested && State.Throttle!=0 ? 1.0 : 0.0;
 State.BoostBlend+=(BoostTarget-State.BoostBlend)*(1-FMath::Exp(-Settings.BoostResponsePerS*Dt));
 const double Target=State.Throttle*Settings.EngineForceN*(State.Throttle<0?Settings.ReverseScale:1)*(1+(Settings.BoostThrustMultiplier-1)*State.BoostBlend);
 State.EngineForceN+=(Target-State.EngineForceN)*(1-FMath::Exp(-Settings.EngineResponsePerS*Dt));
 State.YawRateRadPerS+=(State.Steering*Settings.SteeringRateRadPerS-State.YawRateRadPerS)*(1-FMath::Exp(-Settings.SteeringResponsePerS*Dt));
 State.bEngineActive=FMath::Abs(State.EngineForceN)>1.e-8;
 // Release stops assist immediately, even while engine response decays.
 State.LateralResponsePerS=State.bEngineActive ? Settings.NavigationLateralResponsePerS*FMath::Abs(State.Throttle) : 0;
}
