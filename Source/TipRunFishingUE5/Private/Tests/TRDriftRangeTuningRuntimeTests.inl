// R7B: offline candidate comparison on the saved Runtime startup, never angle feedback.
#include "Data/TRFishingTuningDataAsset.h"
namespace
{
 struct FTRR7BTuning
 {
  bool Profile=false;
  FVector Deep=FVector(.15,0,0);
  double Wind=.1,Current=1,Drag=1,Inertia=10,Stay=.2,Vertical=2;
 };
 struct FTRR7BCandidate
 {
  TStrongObjectPtr<UTRSessionConfigDataAsset> Config;
  TStrongObjectPtr<UTROceanAreaDataAsset> Ocean;
  TStrongObjectPtr<UTRBoatTuningDataAsset> Boat;
  TStrongObjectPtr<UTRFishingTuningDataAsset> Fishing;
  bool Load(FAutomationTestBase& T,const FTRR7BTuning& V,int Case=-1)
  {
   auto* Saved=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));
   if(!T.TestNotNull(TEXT("Candidate source is the saved Prototype config"),Saved)){return false;}
   Config.Reset(DuplicateObject<UTRSessionConfigDataAsset>(Saved,GetTransientPackage()));
   Ocean.Reset(DuplicateObject<UTROceanAreaDataAsset>(Saved->Ocean.Get(),GetTransientPackage()));
   Boat.Reset(DuplicateObject<UTRBoatTuningDataAsset>(Saved->Boat.Get(),GetTransientPackage()));
   Fishing.Reset(DuplicateObject<UTRFishingTuningDataAsset>(Saved->Fishing.Get(),GetTransientPackage()));
   Config->Ocean=Ocean.Get();Config->Boat=Boat.Get();Config->Fishing=Fishing.Get();
   auto& B=Boat->Parameters;B.WindResponseKgPerS=V.Wind;B.CurrentResponseKgPerS=V.Current;B.DragKgPerS=V.Drag;B.InertiaKg=V.Inertia;
   Fishing->Parameters.StaySinkScale=float(V.Stay);Fishing->Parameters.VerticalResponsePerS=V.Vertical;
   auto& E=Ocean->Settings;E.WindMps=FVector2D(0,2);E.CurrentMode=ETRCurrentFieldMode::Constant;E.CurrentMps=FVector(.7*1852./3600.,0,0);E.CurrentDepthProfile.Reset();
   FVector Surface=E.CurrentMps,Deep=V.Deep;
   if(Case>=0)
   {
    E.WindMps=FVector2D(2,0);Surface=FVector(.36,0,0);Deep=Surface;
    if(Case==1){Deep=FVector(.04,0,0);}if(Case==2){Deep=FVector(0,-.2,0);}
    if(Case==3){E.WindMps=FVector2D(-2,0);Deep=FVector(.15,0,0);}
    if(Case==4){E.WindMps=FVector2D::ZeroVector;Surface=Deep=FVector::ZeroVector;}
    if(Case==5){E.WindMps=FVector2D(0,5);Deep=V.Deep;}
    if(Case==6){E.WindMps=FVector2D(0,2);Deep=FVector(1,0,0);}
   }
   if(V.Profile || Case>=0)
   {E.CurrentMode=ETRCurrentFieldMode::DepthProfile;E.CurrentMps=FVector::ZeroVector;
    for(int I=0;I<3;++I){FTRCurrentDepthKey K;K.DepthM=15.f*I;K.CurrentMps=FMath::Lerp(Surface,Deep,I*.5);E.CurrentDepthProfile.Add(K);}}
   TArray<FText> Errors;const bool Valid=Config->ValidateStartup(Errors);for(const auto& EText:Errors){T.AddError(EText.ToString());}return Valid;
  }
 };
 struct FTRR7BMeasure
 {
  FTRHUDSnapshot Bottom,First,Ten,End;
  double BottomS=0,StayS=0,Residency=0,MaxLine=0,MaxStep=0;
  bool Valid=true;int StayTicks=0,GoodTicks=0;int64 BottomTick=-1,StayTick=-1;
  double Delta()const{return double(End.Egi.DepthM)-double(First.Egi.DepthM);}
  bool Good()const{return Bottom.RelativeDrift.LineAngleToSurfaceDeg>=40 && Bottom.RelativeDrift.LineAngleToSurfaceDeg<=55 && First.RelativeDrift.LineAngleToSurfaceDeg>=40 && First.RelativeDrift.LineAngleToSurfaceDeg<=55 && FMath::Abs(Delta())<=.5;}
 };
 FString R7BRow(const FString& Name,const FTRR7BMeasure& M)
 {
  return FString::Printf(TEXT("%s,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d\n"),*Name,M.BottomS,M.Bottom.RelativeDrift.BoatEgiHorizontalSeparationM,M.Bottom.RelativeDrift.LineAngleToSurfaceDeg,M.First.RelativeDrift.LineAngleToSurfaceDeg,M.Ten.RelativeDrift.LineAngleToSurfaceDeg,M.End.RelativeDrift.LineAngleToSurfaceDeg,M.Delta(),M.End.RelativeDrift.RelativeHorizontalVelocityMps.Size(),M.End.Boat.SpeedMps,FVector2D(M.End.Egi.VelocityMps.X,M.End.Egi.VelocityMps.Y).Size(),M.First.Egi.DepthM,M.End.Egi.DepthM,M.Residency,M.MaxLine,M.MaxStep,M.Good());
 }
 FString R7BHeader(){return TEXT("case,bottomSeconds,bottomBoatHorizontal,bottomAngle,tfStayAngle,stay10Angle,stay20Angle,depthDelta20,relativeSpeed,boatSpeed,egiHorizontalSpeed,firstDepth,endDepth,residency40to50,maxLine,maxStep,good\n");}
 bool R7BMeasure(FAutomationTestBase& T,const FString& Name,const FTRR7BTuning* V,FTRR7BMeasure& M,bool Port=true,FName Egi=FName(TEXT("Egi_3_5")),FName Sinker=NAME_None,int Case=-1,double Heading=0,UTRSessionConfigDataAsset* AuditConfig=nullptr)
 {
  FTRR7BCandidate C;if(V && !C.Load(T,*V,Case)){return false;}
  FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(T,Name,60,Port,1920,1080,Heading,AuditConfig?AuditConfig:(V?C.Config.Get():nullptr))){return false;}
  auto* S=R.PC->GetBoundSession();auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();TArray<FText> Errors;
  if(!T.TestTrue(TEXT("Candidate equipment uses saved tables and Ready contract"),S->TrySetEquipment(Egi,Sinker.IsNone()?TREquipment::NoSinkerId():Sinker,Errors)==ETRCommandResult::Accepted)){return false;}
  const auto Start=Sim->GetSimulationTime().TickIndex;FDriftAudit Audit;Audit.DeployTick=Start;FVector Previous;bool HavePrevious=false;
  TSet<ETRFishingState> EventsSeen;FString Events=TEXT("eventTick,state,publishedTick,depth,angleSurface\n");
  const auto StateHandle=S->Fishing->OnFishingStateChanged.AddLambda([&](ETRFishingState,ETRFishingState To)
  {EventsSeen.Add(To);const auto H=S->GetHUDSnapshot();Events+=FString::Printf(TEXT("%lld,%s,%lld,%.12g,%.12g\n"),Sim->GetSimulationTime().TickIndex,*StaticEnum<ETRFishingState>()->GetNameStringByValue(int64(To)),H.Egi.Tick,double(H.Egi.DepthM),H.RelativeDrift.LineAngleToSurfaceDeg);});
  auto* Observer=R.World->SpawnActor<AActor>();
  const auto Id=Sim->RegisterSession(Observer,FTRSimulationStep::CreateLambda([&](ETRSimulationPhase Phase,const FTRSimTime& Time)
  {
   if(Phase!=ETRSimulationPhase::Publish || !S->GetEgiActor() || S->HasResult()){return;}
   const auto H=S->GetHUDSnapshot();Audit.Record(H,Time);if(!H.RelativeDrift.bValid){return;}
   M.MaxLine=FMath::Max(M.MaxLine,double(H.Egi.LineLengthM));
   if(HavePrevious){M.MaxStep=FMath::Max(M.MaxStep,(H.Egi.WorldPositionM-Previous).Size());}Previous=H.Egi.WorldPositionM;HavePrevious=true;
   M.Valid &= H.Egi.DepthM>=0 && H.Egi.DepthM<=H.Ocean.BottomDepthM+1.e-5 && H.Egi.Reel.TotalActualRetrieveM<=H.Egi.Reel.TotalRequestedRetrieveM+1.e-8;
   if(H.Egi.bBottomContact && M.BottomTick<0){M.Bottom=H;M.BottomTick=Time.TickIndex;M.BottomS=(Time.TickIndex-Start)*Time.StepSeconds;}
   if(H.Egi.FishingState==ETRFishingState::Stay && M.StayTick<0){M.First=H;M.StayTick=Time.TickIndex;M.StayS=(Time.TickIndex-Start)*Time.StepSeconds;}
   if(M.StayTick>=0){const auto Age=Time.TickIndex-M.StayTick;if(Age==600){M.Ten=H;}if(Age<=1200){M.End=H;++M.StayTicks;if(H.RelativeDrift.LineAngleToSurfaceDeg>=40 && H.RelativeDrift.LineAngleToSurfaceDeg<=50){++M.GoodTicks;}}}
  }),FTRSimulationCommand::CreateLambda([](const FTRFishingCommand&){}));
  R.Tap(EKeys::Enter);for(int I=0;I<10800 && M.BottomTick<0 && !S->HasResult();++I){R.Frame();}
  R.Tap(EKeys::RightMouseButton);for(int I=0;I<240 && M.StayTick<0 && !S->HasResult();++I){R.Frame();}
  // The coordinator clock points at the NEXT tick after World::Tick returns.
  // Include the published tick at first Stay + 1200, not just the prior tick.
  while(M.StayTick>=0 && Sim->GetSimulationTime().TickIndex<=M.StayTick+1200 && !S->HasResult()){R.Frame();}
  Sim->Unregister(Id);Observer->Destroy();S->Fishing->OnFishingStateChanged.Remove(StateHandle);M.Residency=M.StayTicks?double(M.GoodTicks)/M.StayTicks:0;
  T.TestTrue(TEXT("Candidate reaches Bottom, Shakuri, TensionFall and Stay without technical Abort"),M.BottomTick>=0 && M.StayTick>=0 && !S->HasResult() && EventsSeen.Contains(ETRFishingState::Jerking) && EventsSeen.Contains(ETRFishingState::TensionFall));
  T.TestEqual(TEXT("Stay endpoint is exactly 1200 published fixed ticks later"),M.StayTicks,1201);
  T.TestTrue(TEXT("All candidate ticks finite, bounded, no penetration, consistent reel/line"),M.Valid && Audit.Finite && Audit.Geometry && Audit.Samples && M.MaxLine<80 && M.MaxStep<.17);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("Automation/R7BObservations");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Audit.CSV,*(Dir/(Name+TEXT(".csv"))));
  FFileHelper::SaveStringToFile(Events,*(Dir/(Name+TEXT(".events.csv"))));
  T.AddInfo(R7BRow(Name,M));return M.BottomTick>=0 && M.StayTick>=0 && !S->HasResult();
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7BSweep,"TipRun.R7B.Runtime.StageSweep",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7BSweep::RunTest(const FString&)
{
 FString Rows=R7BHeader();auto Run=[&](const FString& Name,const FTRR7BTuning& V){FTRR7BMeasure M;R7BMeasure(*this,Name,&V,M);Rows+=R7BRow(Name,M);};
 FTRR7BTuning V;FTRR7BMeasure Baseline;R7BMeasure(*this,TEXT("BaselineR7A"),&V,Baseline);Rows+=R7BRow(TEXT("BaselineR7A"),Baseline);
 TestTrue(TEXT("Frozen historical R7A baseline reproduced (exact 20s window)"),FMath::Abs(Baseline.BottomS-43.3666667)<1./60 && FMath::Abs(Baseline.Bottom.RelativeDrift.BoatEgiHorizontalSeparationM-13.7716133)<.001 && FMath::Abs(Baseline.First.RelativeDrift.LineAngleToSurfaceDeg-70.1528836)<.001 && FMath::Abs(Baseline.Delta()+.6462955475)<.001);
 V.Profile=true;
 for(FVector Deep:{FVector(.15,0,0),FVector(.1,-.1,0),FVector(.2,.05,0)}){V.Deep=Deep;Run(TEXT("S1_depth_")+Deep.ToString(),V);}
 V.Deep=FVector(.15,0,0);
 for(double W:{.3,.45,.6}){V.Wind=W;Run(TEXT("S2_wind_")+R7Number(W),V);}
 V.Wind=.45;
 for(double K:{.75,1.25}){V.Current=K;Run(TEXT("S2_current_")+R7Number(K),V);}V.Current=1;
 for(double D:{.75,1.,1.25})for(double I:{5.,10.,15.}){V.Drag=D;V.Inertia=I;Run(TEXT("S3_drag_")+R7Number(D)+TEXT("_inertia_")+R7Number(I),V);}
 // Stage 4 is deliberately a separate later invocation after stages 1-3 are measured.
 FFileHelper::SaveStringToFile(Rows,*(FPaths::ProjectSavedDir()/TEXT("Automation/R7BStages123.csv")));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7BWeightSweep,"TipRun.R7B.Runtime.WeightSweep",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7BWeightSweep::RunTest(const FString&)
{
 FString Rows=R7BHeader();FTRR7BTuning V;V.Profile=true;V.Wind=.45;V.Inertia=5;
 for(double Stay:{.5,.7,.9,1.1})
 {V.Stay=Stay;FTRR7BMeasure M;const FString Name=TEXT("S4_stay_")+R7Number(Stay);R7BMeasure(*this,Name,&V,M);Rows+=R7BRow(Name,M);}
 for(double Stay:{.65,.75,.85})for(double Vertical:{4.,6.})
 {V.Stay=Stay;V.Vertical=Vertical;FTRR7BMeasure M;const FString Name=TEXT("S4_stay_")+R7Number(Stay)+TEXT("_vertical_")+R7Number(Vertical);R7BMeasure(*this,Name,&V,M);Rows+=R7BRow(Name,M);}
 FFileHelper::SaveStringToFile(Rows,*(FPaths::ProjectSavedDir()/TEXT("Automation/R7BStage4.csv")));return true;
}

namespace
{
 FTRR7BTuning R7BSelected(){FTRR7BTuning V;V.Profile=true;V.Wind=.45;V.Inertia=5;V.Stay=.9;return V;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7BGood,"TipRun.R7B.Runtime.RepresentativeWeightsAndSides",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7BGood::RunTest(const FString&)
{
 const auto V=R7BSelected();const bool Saved=FParse::Param(FCommandLine::Get(),TEXT("TRR7BValidateSaved"));FString Rows=R7BHeader();
 FTRR7BMeasure Port,Starboard,Light,Heavy,Extreme;
 auto Run=[&](const FString& Name,FTRR7BMeasure& M,bool P,FName E,FName S=NAME_None){const bool OK=R7BMeasure(*this,Name,Saved?nullptr:&V,M,P,E,S);Rows+=R7BRow(Name,M);return OK;};
 if(!Run(TEXT("Good_Port35"),Port,true,TEXT("Egi_3_5")) || !Run(TEXT("Good_Starboard35"),Starboard,false,TEXT("Egi_3_5"))){return false;}
 TestTrue(TEXT("Both sides: Bottom/TF angle 40-55 and abs depth change20 <=.5"),Port.Good() && Starboard.Good());
 TestTrue(TEXT("Preferred .3m range candidate on both sides"),FMath::Abs(Port.Delta())<=.3 && FMath::Abs(Starboard.Delta())<=.3);
 TestTrue(TEXT("No artificial side asymmetry in angle/depth"),FMath::Abs(Port.Delta()-Starboard.Delta())<.2 && FMath::Abs(Port.End.RelativeDrift.LineAngleToSurfaceDeg-Starboard.End.RelativeDrift.LineAngleToSurfaceDeg)<1);
 TestTrue(TEXT("Meaningful increased drop separation, sustained Stay horizontal separation"),Port.Bottom.RelativeDrift.BoatEgiHorizontalSeparationM>25 && Port.End.RelativeDrift.BoatEgiHorizontalSeparationM>=Port.First.RelativeDrift.BoatEgiHorizontalSeparationM && Starboard.End.RelativeDrift.BoatEgiHorizontalSeparationM>=Starboard.First.RelativeDrift.BoatEgiHorizontalSeparationM);
 TestTrue(TEXT("Stay10/20 angle remains 40-55"),Port.Ten.RelativeDrift.LineAngleToSurfaceDeg>=40 && Port.Ten.RelativeDrift.LineAngleToSurfaceDeg<=55 && Port.End.RelativeDrift.LineAngleToSurfaceDeg>=40 && Port.End.RelativeDrift.LineAngleToSurfaceDeg<=55 && Starboard.End.RelativeDrift.LineAngleToSurfaceDeg>=40 && Starboard.End.RelativeDrift.LineAngleToSurfaceDeg<=55);
 if(!Run(TEXT("Weight30"),Light,true,TEXT("Egi_3")) || !Run(TEXT("Weight40"),Heavy,true,TEXT("Egi_4")) || !Run(TEXT("Weight90"),Extreme,true,TEXT("Egi_4"),TEXT("Sinker_50"))){return false;}
 TestTrue(TEXT("30 rise, 35 stable, 40 sink, 90 clearly sink"),Light.Delta()<0 && Light.Delta()<Port.Delta() && Port.Delta()<Heavy.Delta() && Heavy.Delta()>0 && Extreme.Delta()>.5 && FMath::Abs(Port.Delta())<FMath::Min(FMath::Abs(Light.Delta()),FMath::Abs(Heavy.Delta())));
 TestTrue(TEXT("Heavier drop earlier"),Light.BottomS>Port.BottomS && Port.BottomS>Heavy.BottomS && Heavy.BottomS>Extreme.BottomS);
 FTRR7BMeasure LightS,HeavyS,ExtremeS;
 Run(TEXT("StarboardWeight30"),LightS,false,TEXT("Egi_3"));Run(TEXT("StarboardWeight40"),HeavyS,false,TEXT("Egi_4"));Run(TEXT("StarboardWeight90"),ExtremeS,false,TEXT("Egi_4"),TEXT("Sinker_50"));
 TestTrue(TEXT("Starboard also keeps rise/stable/sink and drop order"),LightS.Delta()<0 && LightS.Delta()<Starboard.Delta() && Starboard.Delta()<HeavyS.Delta() && HeavyS.Delta()>0 && ExtremeS.Delta()>.5 && LightS.BottomS>Starboard.BottomS && Starboard.BottomS>HeavyS.BottomS && HeavyS.BottomS>ExtremeS.BottomS);
 // Only after the basic weights pass: representative sinkers use the same table.
 FTRR7BMeasure S5,S15;Run(TEXT("Sinker5"),S5,true,TEXT("Egi_3_5"),TEXT("Sinker_5"));Run(TEXT("Sinker15"),S15,true,TEXT("Egi_3_5"),TEXT("Sinker_15"));
 TestTrue(TEXT("Added sinker speeds Bottom and moves Stay toward sinking"),S5.BottomS<Port.BottomS && S15.BottomS<S5.BottomS && S5.Delta()>Port.Delta() && S15.Delta()>S5.Delta());
 FFileHelper::SaveStringToFile(Rows,*(FPaths::ProjectSavedDir()/TEXT("Automation/R7BFinalWeights.csv")));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7BBad,"TipRun.R7B.Runtime.BadDirectionsAndHeadings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7BBad::RunTest(const FString&)
{
 const auto V=R7BSelected();FString Rows=R7BHeader();int NotGood=0;
 for(int Case=0;Case<7;++Case){FTRR7BMeasure M;const FString Name=FString::Printf(TEXT("NewTuningCase%c"),TCHAR('A'+Case));R7BMeasure(*this,Name,&V,M,true,TEXT("Egi_3_5"),NAME_None,Case);Rows+=R7BRow(Name,M);if(Case<5 && !M.Good()){++NotGood;}}
 TestTrue(TEXT("At least two original A-E remain outside joint angle/range acceptance"),NotGood>=2);
 for(double Heading:{0.,90.,180.,270.}){FTRR7BMeasure M;const FString Name=TEXT("Heading_")+R7Number(Heading);R7BMeasure(*this,Name,&V,M,true,TEXT("Egi_3_5"),NAME_None,-1,Heading);Rows+=R7BRow(Name,M);}
 FFileHelper::SaveStringToFile(Rows,*(FPaths::ProjectSavedDir()/TEXT("Automation/R7BBadHeadings.csv")));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7BAssets,"TipRun.R7B.Runtime.SavedAssetProfileAndMigration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7BAssets::RunTest(const FString&)
{
 const auto V=R7BSelected();FTRR7BCandidate Candidate;if(!Candidate.Load(*this,V)){return false;}
 auto* Saved=LoadObject<UTRSessionConfigDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype"));
 if(FParse::Param(FCommandLine::Get(),TEXT("TRApplyR7BTuning")))
 {
  // Explicitly authorized Prototype migration, only these three packages.
  Saved->Ocean->Settings=Candidate.Ocean->Settings;Saved->Boat->Parameters=Candidate.Boat->Parameters;Saved->Fishing->Parameters=Candidate.Fishing->Parameters;
  TArray<FText> Errors;if(!TestTrue(TEXT("Selected saved startup validates before save"),Saved->ValidateStartup(Errors))){return false;}
  for(UObject* Asset:TArray<UObject*>{Saved->Ocean.Get(),Saved->Boat.Get(),Saved->Fishing.Get()})
  {const FString Filename=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;TestTrue(TEXT("UE SavePackage saves only selected Prototype tuning"),UPackage::SavePackage(Asset->GetOutermost(),Asset,*Filename,Args));}
 }
 FRuntimeRodWorld R;R.bTraceFrames=false;
 const bool CheckSaved=FParse::Param(FCommandLine::Get(),TEXT("TRR7BValidateSaved")) || FParse::Param(FCommandLine::Get(),TEXT("TRApplyR7BTuning"));
 if(!R.Start(*this,TEXT("R7BProfileSamples"),60,true,1920,1080,0,CheckSaved?nullptr:Candidate.Config.Get())){return false;}
 auto* Ocean=R.World->GetSubsystem<UTROceanWorldSubsystem>();FString Samples=TEXT("depth,currentX,currentY,currentZ\n");FVector Prior=FVector::ZeroVector;
 for(double Depth:{0.,.001,14.999,15.,15.001,29.999,30.})
 {FTROceanQuery Q;Q.PositionXYM=FVector2D(R.PC->GetDebugSnapshot().Boat.PositionM);Q.DepthM=float(Depth);Q.SimTick=R.World->GetSubsystem<UTRSimulationWorldSubsystem>()->GetSimulationTime().TickIndex;const auto At=Ocean->SampleOcean(Q);
  TestTrue(TEXT("Saved evaluated profile uses independent wind/surface/depth"),At.bValid && At.FieldRevision==2 && At.WindMps==FVector2D(0,2) && At.SurfaceCurrentMps==FVector(.7*1852./3600.,0,0) && At.CurrentMps.Equals(FMath::Lerp(At.SurfaceCurrentMps,V.Deep,Depth/30),1.e-7));
  if(Depth==15. || Depth==15.001 || Depth==30.){TestTrue(TEXT("Continuous profile boundaries: no jump"),(At.CurrentMps-Prior).Size()<.00002);}Prior=At.CurrentMps;
  Samples+=FString::Printf(TEXT("%.9g,%.12g,%.12g,%.12g\n"),Depth,At.CurrentMps.X,At.CurrentMps.Y,At.CurrentMps.Z);
 }
 FFileHelper::SaveStringToFile(Samples,*(FPaths::ProjectSavedDir()/TEXT("Automation/R7BProfileSamples.csv")));
 if(CheckSaved){const auto* C=R.World->GetAuthGameMode<ATRGameModeBase>()->SessionConfig.Get();TestTrue(TEXT("Saved Boat/Fishing selected values actually loaded"),C->Boat->Parameters.WindResponseKgPerS==V.Wind && C->Boat->Parameters.InertiaKg==V.Inertia && C->Fishing->Parameters.StaySinkScale==float(V.Stay));}
 TestEqual(TEXT("Line drag unchanged"),R.PC->GetBoundSession()->GetEquipmentSnapshot().Parameters.LineDragKgPerMS,.0001);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRR7BReplay,"TipRun.R7B.Runtime.FixedTickReplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRR7BReplay::RunTest(const FString&)
{
 FTRHUDSnapshot Reference;const auto V=R7BSelected();const bool Saved=FParse::Param(FCommandLine::Get(),TEXT("TRR7BValidateSaved"));
 for(int FPS:{30,60,120})
 {
  FTRR7BCandidate C;if(!C.Load(*this,V)){return false;}FRuntimeRodWorld R;R.bTraceFrames=false;if(!R.Start(*this,FString::Printf(TEXT("R7BReplay%d"),FPS),60,true,1920,1080,0,Saved?nullptr:C.Config.Get())){return false;}
  auto* Sim=R.World->GetSubsystem<UTRSimulationWorldSubsystem>();auto* S=R.PC->GetBoundSession();const int64 Start=Sim->GetSimulationTime().TickIndex+120;R.Dt=1./FPS;
  TestTrue(TEXT("Real controller queues Deploy on same fixed tick"),R.PC->ActionStarted(ETRPlayerAction::Deploy,Start));R.PC->ActionReleased(ETRPlayerAction::Deploy);
  while(Sim->GetSimulationTime().TickIndex<Start+2900){R.Frame();}
  TestTrue(TEXT("Real controller queues Jerk after Bottom"),R.PC->ActionStarted(ETRPlayerAction::Jerk,Start+3000));R.PC->ActionReleased(ETRPlayerAction::Jerk);
  while(Sim->GetSimulationTime().TickIndex<Start+4260){R.Frame();}const auto H=S->GetHUDSnapshot();
  TestTrue(TEXT("Replay active Stay, no technical Abort, finite"),!S->HasResult() && H.RelativeDrift.bValid && H.Egi.FishingState==ETRFishingState::Stay && !H.Egi.WorldPositionM.ContainsNaN());
  if(FPS==30){Reference=H;}else{TestTrue(TEXT("30/60/120 fixed result, drift and reel identical"),H.Egi.WorldPositionM.Equals(Reference.Egi.WorldPositionM,1.e-8) && H.Boat.PositionM.Equals(Reference.Boat.PositionM,1.e-8) && H.Egi.LineLengthM==Reference.Egi.LineLengthM && H.Egi.Reel.TotalActualRetrieveM==Reference.Egi.Reel.TotalActualRetrieveM && H.Shakuri.CompletedCount==Reference.Shakuri.CompletedCount);}
 }
 return true;
}
