#pragma once
#include "CoreMinimal.h"

// Shared authoritative R5C geometry, evaluated with the current substep endpoints.
// The surface span protects horizontal motion when sea and line constraints meet.
namespace TRLineGeometry
{
 inline double SurfaceSpan(const FVector& Rod, const FVector& Egi, double SurfaceM, double AllowedMotionM)
 {
  const double Radius=FMath::Max(0.,(Egi-Rod).Size2D()-AllowedMotionM);
  return FMath::Sqrt(FMath::Square(Rod.Z-SurfaceM)+Radius*Radius);
 }
 inline bool NeedsSurfaceSpan(const FVector& Rod,const FVector& Previous,const FVector& Trial,double SurfaceM,double LineM)
 {
  const FVector Delta=Trial-Rod;
  const FVector Constrained=Delta.Size()>LineM ? Rod+Delta.GetSafeNormal()*LineM : Trial;
  return Previous.Z>=SurfaceM-1.e-5 || Constrained.Z>=SurfaceM-1.e-5;
 }
 inline double Required(const FVector& Rod,const FVector& Previous,const FVector& Trial,double SurfaceM,double MinLineM,bool Surface)
 {
  return FMath::Max(FMath::Max(MinLineM,Rod.Z-SurfaceM),
   FMath::Max((Trial-Rod).Size(),Surface?SurfaceSpan(Rod,Previous,SurfaceM,0):0.));
 }
}

namespace TRReelResolver
{
 struct FResult { double LineM=0,SlackM=0,SlackConsumedM=0,TautAppliedM=0,ActualM=0,UnrealizedM=0; };
 // No endpoint mutation. The existing spatial solver responds to this line.
 inline FResult Resolve(double RequestedM,double LineM,double RequiredM,double MinLineM,double TautBudgetM,
  const FVector& Rod,const FVector& Previous,double SurfaceM,bool Surface)
 {
  FResult R;R.SlackM=FMath::Max(0.,LineM-RequiredM);
  const double Slack=FMath::Min(RequestedM,R.SlackM);
  const double Taut=FMath::Min(FMath::Max(0.,RequestedM-Slack),TautBudgetM);
  const double Proposed=Slack+Taut;
  const double Minimum=FMath::Max(MinLineM,Surface?TRLineGeometry::SurfaceSpan(Rod,Previous,SurfaceM,Taut):Rod.Z-SurfaceM);
  R.LineM=FMath::Max(Minimum,LineM-Proposed);
  R.ActualM=FMath::Clamp(LineM-R.LineM,0.,Proposed);
  R.SlackConsumedM=FMath::Min(Slack,R.ActualM);
  R.TautAppliedM=R.ActualM-R.SlackConsumedM;
  R.UnrealizedM=RequestedM-R.ActualM;
  return R;
 }
}
