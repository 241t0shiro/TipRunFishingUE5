// Shares the saved-map Runtime fixture; no copied production tuning or solver calls.
#include "Ocean/TROceanWorldSubsystem.h"
#include "Ocean/TRSeabedProviderActor.h"
#include "Data/TROceanAreaDataAsset.h"
#include "Data/TRBoatTuningDataAsset.h"

namespace
{
 FString R7Number(double V){return FString::Printf(TEXT("%.12g"),V);}
 void R7Vector(TArray<FString>& Cells,FVector V)
 {Cells.Add(R7Number(V.X));Cells.Add(R7Number(V.Y));Cells.Add(R7Number(V.Z));}
 struct FDriftAudit
 {
  FString CSV=TEXT("tick,time,state,window,boatX,boatY,boatZ,egiX,egiY,egiZ,boatVX,boatVY,boatVZ,egiVX,egiVY,egiVZ,relativeVX,relativeVY,separationRate,boatHorizontal,rodHorizontal,vertical,straight,angleSurface,depth,line,required,slack,weight,windX,windY,windZ,surfaceX,surfaceY,surfaceZ,depthCurrentX,depthCurrentY,depthCurrentZ,windDVX,windDVY,windDVZ,currentDVX,currentDVY,currentDVZ,dragDVX,dragDVY,dragDVZ,rodVX,rodVY,rodVZ,equilibriumX,equilibriumY,equilibriumZ,candidateVX,candidateVY,candidateVZ,motionVX,motionVY,motionVZ,lineForcingX,lineForcingY,lineForcingZ,wetLength,transfer,dragRate,endWeight,response,correction,tension,boatSampleX,boatSampleY,boatSampleZ,egiSampleX,egiSampleY,egiSampleZ,egiSampleDepth,egiSampleCurrentX,egiSampleCurrentY,egiSampleCurrentZ,line0Depth,line0CurrentX,line0CurrentY,line0CurrentZ,line1Depth,line1CurrentX,line1CurrentY,line1CurrentZ,line2Depth,line2CurrentX,line2CurrentY,line2CurrentZ\n");
  int64 DeployTick=0,BottomTick=-1,StayTick=-1;
  int Observations=0;bool Finite=true,Geometry=true,Samples=true;
  double MinAngle=90,MaxAngle=0;
  TSet<ETRFishingState> States;
  FTRHUDSnapshot FirstStay,Last;
  void Record(const FTRHUDSnapshot& H,const FTRSimTime& Time)
  {
   States.Add(H.Egi.FishingState);
   if(H.Egi.bBottomContact && BottomTick<0){BottomTick=Time.TickIndex;}
   if(H.Egi.FishingState==ETRFishingState::Stay && StayTick<0){StayTick=Time.TickIndex;FirstStay=H;}
   if(!H.RelativeDrift.bValid){return;}
   const auto& D=H.RelativeDrift;const auto& E=H.Egi;const auto& A=E.DriftObservation;
   ++Observations;Last=H;
   Finite &= !D.RelativeHorizontalVelocityMps.ContainsNaN() && FMath::IsFinite(D.LineAngleToSurfaceDeg) &&
    !A.CandidateVelocityMps.ContainsNaN() && !A.ConstrainedMotionVelocityMps.ContainsNaN();
   Geometry &= D.LineAngleToSurfaceDeg>=0 && D.LineAngleToSurfaceDeg<=90 &&
    D.StraightDistanceM<=double(E.LineLengthM)+1.e-5 &&
    FMath::IsNearlyEqual(E.SlackM,FMath::Max(0.,double(E.LineLengthM)-E.Reel.RequiredLineLengthM),1.e-10) &&
    FMath::IsNearlyEqual(D.LineAngleToSurfaceDeg+FMath::RadiansToDegrees(double(E.LineAngleRad)),90.,1.e-4);
   Samples &= A.Tick==H.Boat.Tick && A.Tick==H.Rod.Tick && A.Tick==Time.TickIndex &&
    A.LineSampleDepthsM.Num()==A.LineSampleCurrentsMps.Num();
   MinAngle=FMath::Min(MinAngle,D.LineAngleToSurfaceDeg);MaxAngle=FMath::Max(MaxAngle,D.LineAngleToSurfaceDeg);
   TArray<FString> C{FString::Printf(TEXT("%lld"),Time.TickIndex),R7Number((Time.TickIndex-DeployTick)*Time.StepSeconds),
    StaticEnum<ETRFishingState>()->GetNameStringByValue(int64(E.FishingState)),StayTick<0?TEXT("DropAction"):TEXT("StayObservation")};
   R7Vector(C,H.Boat.PositionM);R7Vector(C,E.WorldPositionM);R7Vector(C,H.Boat.VelocityMps);R7Vector(C,E.VelocityMps);
   for(double V:{D.RelativeHorizontalVelocityMps.X,D.RelativeHorizontalVelocityMps.Y,D.SeparationRateMps,
    D.BoatEgiHorizontalSeparationM,D.RodEgiHorizontalSeparationM,D.VerticalSeparationM,D.StraightDistanceM,D.LineAngleToSurfaceDeg,
    double(E.DepthM),double(E.LineLengthM),E.Reel.RequiredLineLengthM,E.SlackM,double(E.TotalMassG)}){C.Add(R7Number(V));}
   R7Vector(C,FVector(H.Boat.WindMps.X,H.Boat.WindMps.Y,0));R7Vector(C,H.Boat.SurfaceCurrentMps);R7Vector(C,E.CurrentAtEgiDepthMps);
   R7Vector(C,H.Boat.WindDeltaVelocityMps);R7Vector(C,H.Boat.CurrentDeltaVelocityMps);R7Vector(C,H.Boat.DragDeltaVelocityMps);
   R7Vector(C,A.RodVelocityMps);R7Vector(C,A.EquilibriumVelocityMps);R7Vector(C,A.CandidateVelocityMps);
   R7Vector(C,A.ConstrainedMotionVelocityMps);R7Vector(C,A.LineForcingMps);
   for(double V:{A.WetLengthM,A.Transfer01,A.LineDragRatePerS,A.LineEndWeight,A.HorizontalResponsePerS,E.LineConstraintCorrectionM,double(E.Tension01)}){C.Add(R7Number(V));}
   R7Vector(C,H.Boat.EnvironmentSamplePositionM);R7Vector(C,A.EgiSamplePositionM);C.Add(R7Number(A.EgiSampleDepthM));R7Vector(C,A.EgiSampleCurrentMps);
   for(int I=0;I<3;++I)
   {C.Add(I<A.LineSampleDepthsM.Num()?R7Number(A.LineSampleDepthsM[I]):TEXT(""));
    if(I<A.LineSampleCurrentsMps.Num()){R7Vector(C,A.LineSampleCurrentsMps[I]);}else{C.Append({TEXT(""),TEXT(""),TEXT("")});}}
   CSV+=FString::Join(C,TEXT(","))+TEXT("\n");
  }
 };
 bool R7Run(FAutomationTestBase& T,const FString& Name,bool Port,int Case,FName EgiId=NAME_None,FName SinkerId=NAME_None)
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(T,Name,60,Port)){return false;}
  auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
  auto* Config=R.World->GetAuthGameMode<ATRGameModeBase>()->SessionConfig.Get();
  TStrongObjectPtr<UTROceanAreaDataAsset> Variant;
  // TEST ONLY: transient copy of saved environment. Retain depth/bounds/provider;
  // vary directions/profile only. Never mutate or save the original asset.
  if(Case>=0)
  {
   Variant.Reset(DuplicateObject<UTROceanAreaDataAsset>(Config->Ocean.Get(),GetTransientPackage()));
   auto& P=Variant->Settings;P.WindMps=FVector2D(2,0);P.CurrentMode=ETRCurrentFieldMode::DepthProfile;
   FVector Surface(.36,0,0),Deep(.36,0,0);
   if(Case==1){Deep=FVector(.04,0,0);}
   if(Case==2){Deep=FVector(0,-.20,0);}
   if(Case==3){P.WindMps=FVector2D(-2,0);Deep=FVector(.15,0,0);}
   if(Case==4){P.WindMps=FVector2D::ZeroVector;Surface=Deep=FVector::ZeroVector;}
   if(Case==5){P.WindMps=FVector2D(0,8);Deep=FVector(.15,0,0);}
   if(Case==6){P.WindMps=FVector2D(0,2);Deep=FVector(1,0,0);}
   P.CurrentMps=FVector::ZeroVector;P.CurrentDepthProfile.Reset();
   FTRCurrentDepthKey K;K.DepthM=0;K.CurrentMps=Surface;P.CurrentDepthProfile.Add(K);
   K.DepthM=P.FlatDepthM;K.CurrentMps=Deep;P.CurrentDepthProfile.Add(K);
   ATRSeabedProviderActor* Provider=nullptr;
   for(TActorIterator<ATRSeabedProviderActor> It(R.World);It;++It){Provider=*It;break;}
   TArray<FText> Errors;
   if(!T.TestTrue(TEXT("Transient audit environment initialized through real Ocean/provider"),R.World->GetSubsystem<UTROceanWorldSubsystem>()->InitializeArea(Variant.Get(),Provider,Errors))){return false;}
   // Existing explicit mode flow resets drift normally; no test velocity setter.
   R.Tap(EKeys::E);R.Tap(EKeys::Enter);R.Tap(Port?EKeys::A:EKeys::D);R.Tap(EKeys::Enter);
  }
  if(!EgiId.IsNone())
  {TArray<FText> Errors;if(!T.TestTrue(TEXT("Weight selected from saved table through Session"),S->TrySetEquipment(EgiId,SinkerId.IsNone()?TREquipment::NoSinkerId():SinkerId,Errors)==ETRCommandResult::Accepted)){return false;}}
  const auto Equipment=S->GetEquipmentSnapshot();const auto& B=Config->Boat->Parameters;const auto& P=Equipment.Parameters;
  const auto& Env=Case<0?Config->Ocean->Settings:Variant->Settings;
  FString Settings=FString::Printf(TEXT("savedConfig=%s savedOcean=%s savedBoat=%s savedFishing=%s testCase=%d side=%d\nDepth=%.9g Wind=%s CurrentMode=%d\nEquipment=%s sinker=%s total=%.9g Sink=%.9g HorizontalResponse=%.9g\nBoat wind=%.9g current=%.9g drag=%.9g inertia=%.9g bow=%.9g stern=%.9g side=%.9g heading=%.9g\nEgi lineDrag=%.9g taut=%.9g slack=%.9g allowance=%.9g verticalRate=%.9g FreeFallScale=%.9g TFScale=%.9g StayScale=%.9g\n"),
   *Config->GetPathName(),*Config->Ocean->GetPathName(),*Config->Boat->GetPathName(),*Config->Fishing->GetPathName(),Case,Port,
   double(Env.FlatDepthM),*Env.WindMps.ToString(),int32(Env.CurrentMode),*Equipment.EgiId.ToString(),*Equipment.SinkerId.ToString(),
   double(Equipment.TotalMassG),double(Equipment.SinkSpeedMps),double(Equipment.HorizontalResponsePerS),
   B.WindResponseKgPerS,B.CurrentResponseKgPerS,B.DragKgPerS,B.InertiaKg,B.BowWindScale,B.SternWindScale,B.SideWindScale,double(S->GetHUDSnapshot().Boat.HeadingRad),
   P.LineDragKgPerMS,P.TautLineTransfer01,P.SlackLineTransfer01,P.LineSlackAllowanceM,P.VerticalResponsePerS,double(P.FreeFallSinkScale),double(P.TensionFallSinkScale),double(P.StaySinkScale));
  for(const auto& K:Env.CurrentDepthProfile){Settings+=FString::Printf(TEXT("DepthKey %.9g %s\n"),double(K.DepthM),*K.CurrentMps.ToString());}
  FDriftAudit Audit;Audit.DeployTick=Sim->GetSimulationTime().TickIndex;
  FString Events=TEXT("eventTick,state,lastPublishedTick,boatVX,boatVY,egiVX,egiVY,depth,boatHorizontal,rodHorizontal,vertical,angleSurface,line,required,slack\n");
  const auto StateHandle=S->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState,ETRFishingState To)
  {
   Audit.States.Add(To);const auto H=S->GetHUDSnapshot();const auto& D=H.RelativeDrift;
   // Event instant can precede physical integration. Explicitly mark the age of
   // the available published measurement; never invent a separate TF step.
   Events+=FString::Printf(TEXT("%lld,%s,%lld"),Sim->GetSimulationTime().TickIndex,*StaticEnum<ETRFishingState>()->GetNameStringByValue(int64(To)),H.Egi.Tick);
   for(double V:{H.Boat.VelocityMps.X,H.Boat.VelocityMps.Y,H.Egi.VelocityMps.X,H.Egi.VelocityMps.Y,double(H.Egi.DepthM),
    D.BoatEgiHorizontalSeparationM,D.RodEgiHorizontalSeparationM,D.VerticalSeparationM,D.LineAngleToSurfaceDeg,double(H.Egi.LineLengthM),H.Egi.Reel.RequiredLineLengthM,H.Egi.SlackM}){Events+=TEXT(",")+R7Number(V);}
   Events+=TEXT("\n");
  });
  bool ActualSamplesMatch=true;int SampleChecks=0;
  auto* Ocean=R.World->GetSubsystem<UTROceanWorldSubsystem>();
  auto* Observer=R.World->SpawnActor<AActor>();
  const auto Id=Sim->RegisterSession(Observer,FTRSimulationStep::CreateLambda([&](ETRSimulationPhase Phase,const FTRSimTime& Time)
  {if(Phase==ETRSimulationPhase::Publish && S->GetEgiActor() && !S->HasResult()){const auto H=S->GetHUDSnapshot();Audit.Record(H,Time);
    if(H.RelativeDrift.bValid && Time.TickIndex%60==0)
    {
     ++SampleChecks;const auto& A=H.Egi.DriftObservation;FTROceanQuery Q;Q.SimTick=Time.TickIndex;
     Q.PositionXYM=FVector2D(H.Boat.EnvironmentSamplePositionM.X,H.Boat.EnvironmentSamplePositionM.Y);
     const auto AtBoat=Ocean->SampleOcean(Q);
     ActualSamplesMatch &= AtBoat.bValid && AtBoat.WindMps==H.Boat.WindMps && AtBoat.SurfaceCurrentMps==H.Boat.SurfaceCurrentMps;
     Q.PositionXYM=FVector2D(A.EgiSamplePositionM.X,A.EgiSamplePositionM.Y);Q.DepthM=float(A.EgiSampleDepthM);
     ActualSamplesMatch &= Ocean->SampleOcean(Q).CurrentMps==A.EgiSampleCurrentMps;
     for(int I=0;I<A.LineSampleDepthsM.Num();++I)
     {Q.PositionXYM=FVector2D(A.LineSamplePositionsM[I].X,A.LineSamplePositionsM[I].Y);Q.DepthM=float(A.LineSampleDepthsM[I]);
      ActualSamplesMatch &= Ocean->SampleOcean(Q).CurrentMps==A.LineSampleCurrentsMps[I];}
    }}}),FTRSimulationCommand::CreateLambda([](const FTRFishingCommand&){}));
  R.Tap(EKeys::Enter);
  // Bounded observation, not force-bottom. Failure to reach depth is an audit
  // finding, never repaired by a fixture position/physics override.
  for(int I=0;I<60*180 && Audit.BottomTick<0 && !S->HasResult();++I){R.Frame();}
  R.Tap(EKeys::RightMouseButton);
  for(int I=0;I<180 && Audit.StayTick<0 && !S->HasResult();++I){R.Frame();}
  R.Frames(60*20);
  Sim->Unregister(Id);Observer->Destroy();S->Fishing->OnFishingStateChanged.Remove(StateHandle);
  T.TestTrue(TEXT("Actual boat/egi/three line point samples match their own XY/depth/tick queries"),SampleChecks>20 && ActualSamplesMatch);
  T.TestTrue(TEXT("Audit observations finite, geometrically consistent and same-tick"),Audit.Observations>1200 && Audit.Finite && Audit.Geometry && Audit.Samples);
  T.TestTrue(TEXT("Audit never ends cast through technical Abort"),!S->HasResult());
  T.TestTrue(TEXT("Real Right click completed and entered Stay"),Audit.StayTick>=0 && Audit.States.Contains(ETRFishingState::Jerking) && Audit.States.Contains(ETRFishingState::TensionFall));
  Settings+=FString::Printf(TEXT("DeployTick=%lld BottomTick=%lld StayTick=%lld observations=%d angleMin=%.12g angleMax=%.12g\n"),Audit.DeployTick,Audit.BottomTick,Audit.StayTick,Audit.Observations,Audit.MinAngle,Audit.MaxAngle);
  const auto& H=Audit.Last;
  T.AddInfo(Name+TEXT(" ")+Settings+FString::Printf(TEXT("End depth=%.9g boatH=%.9g rodH=%.9g angleSurface=%.9g relativeSpeed=%.9g depthChangeAfterFirstStay=%.9g"),double(H.Egi.DepthM),H.RelativeDrift.BoatEgiHorizontalSeparationM,H.RelativeDrift.RodEgiHorizontalSeparationM,H.RelativeDrift.LineAngleToSurfaceDeg,H.RelativeDrift.RelativeHorizontalVelocityMps.Size(),double(H.Egi.DepthM-Audit.FirstStay.Egi.DepthM)));
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("Automation/R7AObservations");IFileManager::Get().MakeDirectory(*Dir,true);
  T.TestTrue(TEXT("Audit CSV evidence written"),FFileHelper::SaveStringToFile(Audit.CSV,*(Dir/(Name+TEXT(".csv")))));
  T.TestTrue(TEXT("Transient state event evidence written"),FFileHelper::SaveStringToFile(Events,*(Dir/(Name+TEXT(".events.csv")))));
  T.TestTrue(TEXT("Audit settings evidence written"),FFileHelper::SaveStringToFile(Settings,*(Dir/(Name+TEXT(".txt")))));
  return true;
 }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7Baseline,"TipRun.R7A.Runtime.SavedPrototypeDropToStay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7Baseline::RunTest(const FString&)
{
 for(bool Port:{true,false}){if(!R7Run(*this,Port?TEXT("Baseline_Port"):TEXT("Baseline_Starboard"),Port,-1)){return false;}}
 for(FName Egi:{FName(TEXT("Egi_3")),FName(TEXT("Egi_4"))})
 {if(!R7Run(*this,TEXT("Baseline_")+Egi.ToString(),true,-1,Egi)){return false;}}
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7Matrix,"TipRun.R7A.Runtime.DirectionAndWeightAudit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7Matrix::RunTest(const FString&)
{for(int Case=0;Case<5;++Case)for(FName Egi:{FName(TEXT("Egi_3")),FName(TEXT("Egi_3_5")),FName(TEXT("Egi_4"))})
 {if(!R7Run(*this,FString::Printf(TEXT("Case%c_%s"),TCHAR('A'+Case),*Egi.ToString()),true,Case,Egi)){return false;}}return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7Observation,"TipRun.R7A.Runtime.ReadOnlyAndFixedTickReplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7Observation::RunTest(const FString&)
{
 FTREgiSnapshot E;E.bWorldPositionValid=true;E.DriftObservation.bValid=true;E.WorldPositionM=FVector(1,0,-1);
 FTRRodSnapshot Rod;Rod.bValid=true;FTRBoatSnapshot Boat;
 TestTrue(TEXT("Independent horizontal-plane angle definition 45 degrees"),FMath::IsNearlyEqual(FTRRelativeDriftSnapshot::Observe(Boat,E,Rod).LineAngleToSurfaceDeg,45.,1.e-12));
 Rod.Tick=1;TestFalse(TEXT("Mismatched fixed ticks are invalid, not relative motion"),FTRRelativeDriftSnapshot::Observe(Boat,E,Rod).bValid);
 FVector Reference;FTRRelativeDriftSnapshot Ref;FTREgiDriftObservation RefAudit;
 for(int FPS:{30,60,120})
 {
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R7A_Replay%d"),FPS))){return false;}
  auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();
  const int64 Start=Sim->GetSimulationTime().TickIndex+120;R.Dt=1./FPS;
  TestTrue(TEXT("Controller queues identical fixed-tick Deploy"),R.PC->ActionStarted(ETRPlayerAction::Deploy,Start));R.PC->ActionReleased(ETRPlayerAction::Deploy);
  while(Sim->GetSimulationTime().TickIndex<Start+60){R.Frame();}
  TestTrue(TEXT("Controller queues identical fixed-tick Jerk"),R.PC->ActionStarted(ETRPlayerAction::Jerk,Start+120));R.PC->ActionReleased(ETRPlayerAction::Jerk);
  while(Sim->GetSimulationTime().TickIndex<Start+1500){R.Frame();}
  TestTrue(TEXT("Replay remains active with valid same-tick drift, no hidden Abort"),!S->HasResult() && S->GetHUDSnapshot().RelativeDrift.bValid);
  const auto Before=S->GetHUDSnapshot();const auto Tick=Sim->GetSimulationTime().TickIndex;const auto Queue=Sim->GetQueuedCommandCount();
  for(int I=0;I<100;++I){UTRFishingHUDWidget::BuildReadout(S->GetHUDSnapshot());S->GetHUDSnapshot();}
  const auto After=S->GetHUDSnapshot();
  TestTrue(TEXT("Audit/HUD reads leave physics, tick, queue and reel unchanged"),Before.Egi.WorldPositionM==After.Egi.WorldPositionM && Before.Egi.Reel.TotalActualRetrieveM==After.Egi.Reel.TotalActualRetrieveM && Sim->GetSimulationTime().TickIndex==Tick && Sim->GetQueuedCommandCount()==Queue);
  TestTrue(TEXT("Audit row stays in details, not compact display"),!UTRFishingHUDWidget::IsPrimaryReadout(UTRFishingHUDWidget::BuildReadout(After).Num()-2));
  if(FPS==30){Reference=After.Egi.WorldPositionM;Ref=After.RelativeDrift;RefAudit=After.Egi.DriftObservation;}
  else{TestTrue(TEXT("30/60/120 same fixed-tick audit/physical results"),Reference.Equals(After.Egi.WorldPositionM,1.e-8) && Ref.RelativeHorizontalVelocityMps.Equals(After.RelativeDrift.RelativeHorizontalVelocityMps,1.e-8) && FMath::IsNearlyEqual(Ref.LineAngleToSurfaceDeg,After.RelativeDrift.LineAngleToSurfaceDeg,1.e-8) && RefAudit.LineSampleCurrentsMps==After.Egi.DriftObservation.LineSampleCurrentsMps);}
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7Limits,"TipRun.R7A.Runtime.NonIdealDriftAudit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7Limits::RunTest(const FString&)
{
 if(!R7Run(*this,TEXT("FastBoat_TestOnly"),true,5)){return false;}
 if(!R7Run(*this,TEXT("StrongDepthCurrent_TestOnly"),true,6)){return false;}
 return R7Run(*this,TEXT("Baseline_90g_TestEquipment"),true,-1,FName(TEXT("Egi_4")),FName(TEXT("Sinker_50")));
}
