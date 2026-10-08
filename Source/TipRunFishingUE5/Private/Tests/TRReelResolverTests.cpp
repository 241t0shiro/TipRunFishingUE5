#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Fishing/TRReelResolver.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRReelResolverTest,"TipRun.R6.Unit.GeometryAndAllocation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRReelResolverTest::RunTest(const FString&)
{
 const FVector Rod(0,0,1),Deep(0,0,-9),Surface(2,0,0);
 const double Required=TRLineGeometry::Required(Rod,Deep,Deep,0,1,false);
 const auto Rich=TRReelResolver::Resolve(.8,12,Required,1,0,Rod,Deep,0,false);
 TestTrue(TEXT("Slack >= .8 winds exactly .8 with zero endpoint budget"),Rich.ActualM==.8 && Rich.SlackConsumedM==.8 && Rich.TautAppliedM==0 && FMath::IsNearlyEqual(Rich.LineM,11.2,1.e-12));
 const auto Taut=TRReelResolver::Resolve(.8,10,Required,1,.2,Rod,Deep,0,false);
 TestTrue(TEXT("Taut motion budget bounds shortening, unmet is normal"),FMath::IsNearlyEqual(Taut.ActualM,.2,1.e-12) && Taut.SlackConsumedM==0 && FMath::IsNearlyEqual(Taut.UnrealizedM,.6,1.e-12));
 const double Span=TRLineGeometry::Required(Rod,Surface,Surface,0,1,true);
 const auto S=TRReelResolver::Resolve(.8,Span,Span,1,.2,Rod,Surface,0,true);
 TestTrue(TEXT("R5C surface geometry never collapses circle to force a request"),S.LineM>=TRLineGeometry::SurfaceSpan(Rod,Surface,0,.2) && S.ActualM<.2 && S.UnrealizedM>.6);
 TestTrue(TEXT("Geometry growth and reel are independently measurable"),S.ActualM>0 && S.LineM>1 && S.SlackConsumedM+S.TautAppliedM==S.ActualM);
 const auto None=TRReelResolver::Resolve(0,Span,Span,1,.2,Rod,Surface,0,true);
 TestTrue(TEXT("Hold and payout generate no negative reel"),None.ActualM==0 && None.UnrealizedM==0 && None.LineM==Span);
 return true;
}
#endif
