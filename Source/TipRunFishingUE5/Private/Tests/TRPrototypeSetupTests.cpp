#include "TRSessionTestFixture.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Game/TRGameModeBase.h"
#include "Game/TRPlayerController.h"
#include "Game/TRPrototypeViewActor.h"
#include "Data/TRRodTuningDataAsset.h"
#include "UI/TRHUD.h"
#include "UI/TRFishingHUDWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "GameFramework/WorldSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "AssetCompilingManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	const FString Root = TEXT("/Game/TipRun/Prototype/M105/");
	const FString MapPath = Root + TEXT("L_TR_M105_Prototype");
	const FString ModePath = Root + TEXT("BP_TR_M105GameMode_Prototype");
	FString DataPath(const TCHAR* Kind) { return Root + TEXT("Data/DA_TR_M105") + Kind + TEXT("_Prototype"); }
	template<class T> T* NewAsset(const FString& Path)
	{ return NewObject<T>(CreatePackage(*Path), *FPackageName::GetLongPackageAssetName(Path), RF_Public|RF_Standalone); }
	bool SaveAsset(UObject* Asset, bool bMap=false)
	{
		const FString Filename=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),
			bMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
		FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
		return UPackage::SavePackage(Asset->GetOutermost(),Asset,*Filename,Args);
	}
	bool CreateMap(FAutomationTestBase& Test, UBlueprint* BP)
	{
		if (FPaths::FileExists(FPackageName::LongPackageNameToFilename(MapPath,TEXT(".umap"))))
		{ Test.AddError(TEXT("G map creation refuses to overwrite an existing level")); return false; }
		const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
			.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		auto* World=UWorld::CreateWorld(EWorldType::Editor,false,*FPackageName::GetLongPackageAssetName(MapPath),CreatePackage(*MapPath),true,ERHIFeatureLevel::Num,&Values);
		World->SetFlags(RF_Public|RF_Standalone);
		World->GetWorldSettings()->DefaultGameMode=BP->GeneratedClass;
		World->SpawnActor<ATRPrototypeViewActor>()->SetActorLabel(TEXT("Prototype Snapshot Observer (m to cm display only)"));
		World->GetOutermost()->SetPackageFlags(PKG_ContainsMap);
		const bool Saved=SaveAsset(World,true); World->DestroyWorld(false);
		return Test.TestTrue(TEXT("Saved G observation map"),Saved);
	}
	bool CreatePrototype(FAutomationTestBase& Test)
	{
		TArray<FString> Paths={MapPath,ModePath};
		for (const TCHAR* Kind:{TEXT("Ocean"),TEXT("Boat"),TEXT("Fishing"),TEXT("Rod"),TEXT("Input"),TEXT("Session")}) { Paths.Add(DataPath(Kind)); }
		for (const FString& Path:Paths)
		{
			if (FPaths::FileExists(FPackageName::LongPackageNameToFilename(Path,Path==MapPath ? TEXT(".umap") : TEXT(".uasset"))))
			{ Test.AddError(TEXT("G creation refuses to overwrite existing prototype packages")); return false; }
		}
		auto* Legacy=LoadObject<UTRFishingTuningDataAsset>(nullptr,TEXT("/Game/TipRun/Prototype/Data/DA_TR_FishingTuning_Prototype"));
		if (!Test.TestNotNull(TEXT("Legacy curves available for explicit copy"),Legacy)) { return false; }
		auto* Ocean=NewAsset<UTROceanAreaDataAsset>(DataPath(TEXT("Ocean")));
		auto& O=Ocean->Settings; O.AreaId=TEXT("M105G_Prototype30m"); O.BoundsMinXYM=FVector2D(-1000,-1000);
		O.BoundsMaxXYM=FVector2D(1000,1000); O.FlatDepthM=30; O.SurfaceZ_M=0; O.FieldRevision=2;
		O.CurrentMps=FVector(.7*1852.0/3600.0,0,0); O.WindMps=FVector2D(0,2);
		auto* Boat=NewAsset<UTRBoatTuningDataAsset>(DataPath(TEXT("Boat")));
		auto& B=Boat->Parameters; B.ModelRevision=2; B.WindResponseKgPerS=.1; B.CurrentResponseKgPerS=1;
		B.DragKgPerS=1; B.InertiaKg=10; B.BowWindScale=1; B.SternWindScale=.5; B.SideWindScale=2;
		B.MaxDriftSpeedMps=1; B.HullHeightOffsetM=.5; B.RodAnchorOffsetM=FVector(2,1,1);
		const FString FishingPath=DataPath(TEXT("Fishing"));
		auto* Fishing=DuplicateObject<UTRFishingTuningDataAsset>(Legacy,CreatePackage(*FishingPath),*FPackageName::GetLongPackageAssetName(FishingPath));
		Fishing->SetFlags(RF_Public|RF_Standalone);
		// Explicit G Prototype values based on C/D/E fixtures, not measured/product balance.
		auto& P=Fishing->Parameters; P.EgiModelRevision=2; P.VerticalResponsePerS=2; P.LineDragKgPerMS=.0001;
		P.TautLineTransfer01=.05; P.SlackLineTransfer01=0; P.LineSlackAllowanceM=.1; P.MaxStepTravelM=20;
		P.MaxEgiSpeedMps=10; P.PayoutMps=4; P.MaxLineLengthM=200; P.ReelMps=1; P.QuickRetrieveDurationS=1.5;
		auto* Rod=NewAsset<UTRRodTuningDataAsset>(DataPath(TEXT("Rod")));
		auto& R=Rod->Parameters; R.MinPitchRad=0; R.MaxPitchRad=1.2; R.MinYawRad=-.6; R.MaxYawRad=.6; R.InitialPitchRad=.1;
		R.SensitivityXRad=.01; R.SensitivityYRad=.02; R.MaxMouseDelta=50; R.MaxAimRateRadPerS=1.2;
		R.LengthM=2; R.MountOffsetM=FVector(0,0,1); R.ShakuriAmplitudeRad=.3; R.ShakuriUpSeconds=.15; R.ShakuriReturnSeconds=.25;
		const FString InputPath=DataPath(TEXT("Input"));
		auto* Input=UTRInputConfigDataAsset::CreateMouseRetrievePrototype(CreatePackage(*InputPath));
		Input->Rename(*FPackageName::GetLongPackageAssetName(InputPath)); Input->SetFlags(RF_Public|RF_Standalone);
		auto* Config=NewAsset<UTRSessionConfigDataAsset>(DataPath(TEXT("Session")));
		Config->StepSeconds=1.0/60; Config->MaxCatchUpSteps=8; Config->SessionSeed=20260916;
		Config->Ocean=Ocean; Config->Boat=Boat; Config->Fishing=Fishing; Config->Rod=Rod; Config->Input=Input;
		Config->Egis=LoadObject<UDataTable>(nullptr,TEXT("/Game/TipRun/Prototype/Data/DT_TR_Egi_Prototype"));
		Config->Sinkers=LoadObject<UDataTable>(nullptr,TEXT("/Game/TipRun/Prototype/Data/DT_TR_Sinker_Prototype"));
		Config->InitialSinkerId=TREquipment::NoSinkerId();
		TArray<FText> Errors; const bool bValid=Config->ValidateStartup(Errors);
		for (const auto& Error:Errors) { Test.AddError(Error.ToString()); }
		if (!bValid) { return false; }
		for (UObject* Asset:TArray<UObject*>{Ocean,Boat,Fishing,Rod,Input,Config})
		{ if (!Test.TestTrue(TEXT("Saved new G data package"),SaveAsset(Asset))) { return false; } }
		auto* BP=FKismetEditorUtilities::CreateBlueprint(ATRGameModeBase::StaticClass(),CreatePackage(*ModePath),
			*FPackageName::GetLongPackageAssetName(ModePath),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
		FKismetEditorUtilities::CompileBlueprint(BP);
		CastChecked<ATRGameModeBase>(BP->GeneratedClass->GetDefaultObject())->SessionConfig=Config;
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP); FKismetEditorUtilities::CompileBlueprint(BP);
		if (!Test.TestTrue(TEXT("Saved G GameMode"),SaveAsset(BP))) { return false; }
		return CreateMap(Test,BP);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRPrototypeAssetsGTest,"TipRun.M105G.SavedAssetReferences",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRPrototypeAssetsGTest::RunTest(const FString& Parameters)
{
	if (FParse::Param(FCommandLine::Get(),TEXT("TRCreateM105Prototype")) && !CreatePrototype(*this)) { return false; }
	if (FParse::Param(FCommandLine::Get(),TEXT("TRCreateM105Map")))
	{
		auto* SavedBP=LoadObject<UBlueprint>(nullptr,*ModePath);
		if (!TestNotNull(TEXT("Existing G GameMode for map creation"),SavedBP) || !CreateMap(*this,SavedBP)) { return false; }
	}
	auto* Config=LoadObject<UTRSessionConfigDataAsset>(nullptr,*DataPath(TEXT("Session")));
	auto* BP=LoadObject<UBlueprint>(nullptr,*ModePath);
	auto* World=LoadObject<UWorld>(nullptr,*MapPath);
	if (!TestNotNull(TEXT("Stored config loads"),Config) || !TestNotNull(TEXT("Stored GameMode loads"),BP) || !TestNotNull(TEXT("Stored Level loads"),World)) { return false; }
	if(FParse::Param(FCommandLine::Get(),TEXT("TRMigrateM105GView")))
	{
		const FString MaterialPath=Root+TEXT("M_TR_Observation_Prototype");
		UMaterial* Material=nullptr;
		if(FPaths::FileExists(FPackageName::LongPackageNameToFilename(MaterialPath,TEXT(".uasset"))))
		{Material=LoadObject<UMaterial>(nullptr,*MaterialPath);}
		else
		{
			Material=NewAsset<UMaterial>(MaterialPath); Material->SetShadingModel(MSM_Unlit);
			auto* Color=NewObject<UMaterialExpressionVectorParameter>(Material); Color->ParameterName=TEXT("Color"); Color->DefaultValue=FLinearColor(.6f,.7f,.8f);
			Material->GetExpressionCollection().AddExpression(Color); Material->GetEditorOnlyData()->EmissiveColor.Connect(0,Color);
			Material->PostEditChange(); FAssetCompilingManager::Get().FinishAllCompilation();
			if(!TestTrue(TEXT("Saved unlit observation material"),SaveAsset(Material))){return false;}
		}
		if(!TestNotNull(TEXT("Observation material"),Material)){return false;}
		for(const auto& Actor:World->PersistentLevel->Actors)
		{if(auto* View=Cast<ATRPrototypeViewActor>(Actor)){View->ObservationMaterial=Material;View->CameraOffsetM=FVector(-8,-12,7);View->LookAtOffsetM=FVector(1,0,0);View->FieldOfView=60;}}
		if(!TestTrue(TEXT("Saved G visualization migration"),SaveAsset(World,true))){return false;}
	}
	if(FParse::Param(FCommandLine::Get(),TEXT("TRMigrateM105GInteraction")))
	{
		auto& R=Config->Rod->Parameters;
		R.MinPitchRad=-.35;R.MaxPitchRad=1.4;R.MinYawRad=-1.2;R.MaxYawRad=1.2;R.MaxAimRateRadPerS=2;
		R.ShakuriReelSpeedMps=8;R.ShakuriReelSeconds=.25;
		if(!TestTrue(TEXT("Saved explicit rod interaction revision"),SaveAsset(Config->Rod)) || !TestTrue(TEXT("Saved fixed reference meshes"),SaveAsset(World,true))){return false;}
	}
	TArray<FText> Errors; TestTrue(TEXT("All startup references validate"),Config->ValidateStartup(Errors));
	for (const auto& Error:Errors) { AddError(Error.ToString()); }
	TestEqual(TEXT("Environment explicit revision 2"),Config->Ocean->Settings.FieldRevision,2);
	TestEqual(TEXT("Boat explicit revision 2"),Config->Boat->Parameters.ModelRevision,2);
	TestEqual(TEXT("Egi explicit revision 2"),Config->Fishing->Parameters.EgiModelRevision,2);
	TestNotNull(TEXT("Rod enabled"),Config->Rod.Get());
	TestTrue(TEXT("Quick configured"),Config->Fishing->Parameters.QuickRetrieveDurationS>0);
	TestTrue(TEXT("Map overrides GameMode"),World->GetWorldSettings()->DefaultGameMode==BP->GeneratedClass);
	const auto* Mode=CastChecked<ATRGameModeBase>(BP->GeneratedClass->GetDefaultObject());
	TestTrue(TEXT("GameMode references rev2 session and F HUD/controller"),Mode->SessionConfig==Config && Mode->HUDClass==ATRHUD::StaticClass() && Mode->PlayerControllerClass==ATRPlayerController::StaticClass());
	bool Observer=false; for (const auto& Actor:World->PersistentLevel->Actors)
	{
		if(auto* View=Cast<ATRPrototypeViewActor>(Actor))
		{
			Observer=true; TestNotNull(TEXT("Saved unlit material"),View->ObservationMaterial.Get());
			TestTrue(TEXT("Persistent view meshes and tick"),View->BoatVisual->GetStaticMesh() && View->RodVisual->GetStaticMesh() && View->LineVisual->GetStaticMesh() && View->EgiVisual->GetStaticMesh() && View->PrimaryActorTick.bCanEverTick);
		}
	}
	TestTrue(TEXT("Stored observation actor"),Observer);
	for (FKey Key:{EKeys::Mouse2D,EKeys::RightMouseButton,EKeys::LeftMouseButton,EKeys::F,EKeys::Q,EKeys::Enter})
	{
		bool Found=false; for (const auto& M:Config->Input->FishingContext->GetMappings()) { Found|=M.Key==Key; }
		TestTrue(*FString::Printf(TEXT("Saved mapping %s"),*Key.ToString()),Found);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRPrototypeBootGTest,"TipRun.M105G.SavedSetupInputAndEquipmentSmoke",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRPrototypeBootGTest::RunTest(const FString& Parameters)
{
	auto* BP=LoadObject<UBlueprint>(nullptr,*ModePath); if (!TestNotNull(TEXT("G GameMode loads"),BP)) { return false; }
	FTRTestSession F;
	auto* Mode=F.World->SpawnActor<ATRGameModeBase>(BP->GeneratedClass);
	TArray<FText> Errors; if (!TestTrue(TEXT("Stored GameMode starts revision2 Session"),Mode->InitializeSession(Errors)))
	{ for (const auto& Error:Errors) { AddError(Error.ToString()); } return false; }
	auto* PC=F.World->SpawnActor<ATRPlayerController>(); PC->InputConfig=Mode->SessionConfig->Input;
	TStrongObjectPtr<UEnhancedInputComponent> Input(NewObject<UEnhancedInputComponent>(PC)); PC->InputComponent=Input.Get();
	TStrongObjectPtr<UEnhancedPlayerInput> PlayerInput(NewObject<UEnhancedPlayerInput>(PC)); PC->PlayerInput=PlayerInput.Get();
	TestTrue(TEXT("Stored input installed"),PC->InstallInputBindings(Input.Get())); TestTrue(TEXT("Session binds"),PC->BindSession(Mode->GetSession()));
	TestTrue(TEXT("R1 explicit Fishing start"),PC->RequestPlayerMode(ETRPlayerMode::Fishing)); F.Step();
	TStrongObjectPtr<UTRFishingHUDWidget> UI(CreateWidget<UTRFishingHUDWidget>(F.World,UTRFishingHUDWidget::StaticClass()));
	UI->TakeWidget(); UI->BindController(PC);
	TestFalse(TEXT("Initial panel closed"),PC->IsPrototypePanelOpen()); PC->TogglePrototypePanel(); UI->RefreshFromController();
	TestEqual(TEXT("Three table egis"),UI->GetEgiOptionCount(),3); TestEqual(TEXT("Nine table sinkers"),UI->GetSinkerOptionCount(),9);
	auto Inject=[&](FKey Key,FInputActionValue Value)
	{
		for (const auto& M:PC->InputConfig->FishingContext->GetMappings())
		{ if (M.Key==Key) { PlayerInput->InjectInputForAction(M.Action,Value); PlayerInput->ProcessInputStack({Input.Get()},1.f/60,false); return; } }
	};
	// UI Enter uses the same authoritative Deploy request while the Ready panel owns focus.
	TestTrue(TEXT("Ready panel deploy"),UI->RequestDeploy()); F.Step(); UI->RefreshFromController();
	TestTrue(TEXT("FreeFall and live spatial snapshot"),PC->GetDebugSnapshot().bEgiValid && PC->GetDebugSnapshot().Egi.bWorldPositionValid);
	F.Step(240);
	const auto Before=PC->GetDebugSnapshot(); Inject(EKeys::Mouse2D,FInputActionValue(FVector2D(1,1))); F.Step();
	TestTrue(TEXT("Saved Mouse reaches rod"),PC->GetDebugSnapshot().Rod.BaseYawRad!=Before.Rod.BaseYawRad);
	Inject(EKeys::RightMouseButton,FInputActionValue(true)); F.Step();
	TestTrue(TEXT("Right click starts Shakuri"),PC->GetDebugSnapshot().Egi.FishingState==ETRFishingState::Jerking);
	Inject(EKeys::RightMouseButton,FInputActionValue(false)); F.Step(60);
	Inject(EKeys::LeftMouseButton,FInputActionValue(true)); F.Step(5);
	TestTrue(TEXT("Saved Left hold retrieves"),PC->GetDebugSnapshot().Retrieval.bIsRetrieving);
	Inject(EKeys::LeftMouseButton,FInputActionValue(false)); F.Step();
	TestTrue(TEXT("Saved Left release returns Stay"),PC->GetDebugSnapshot().Egi.FishingState==ETRFishingState::Stay);
	Inject(EKeys::F,FInputActionValue(true)); F.Step(); Inject(EKeys::F,FInputActionValue(false));
	TestTrue(TEXT("Saved F re-fall"),PC->GetDebugSnapshot().Egi.FishingState==ETRFishingState::FreeFall);
	Inject(EKeys::Q,FInputActionValue(true)); F.Step(); Inject(EKeys::Q,FInputActionValue(false));
	TestTrue(TEXT("Saved Q quick"),PC->GetDebugSnapshot().Retrieval.bIsQuickRetrieving); F.Step(100); UI->RefreshFromController();
	TestTrue(TEXT("Quick unlocks but does not open equipment"),!PC->IsPrototypePanelOpen() && PC->GetDebugSnapshot().bCanChangeEquipment);
	PC->TogglePrototypePanel(); UI->RefreshFromController();
	TestTrue(TEXT("Saved table choice applies"),UI->SelectEquipment(TEXT("Egi_4"),TEXT("Sinker_10")) && UI->ApplyEquipmentSelection());
	TestTrue(TEXT("Next deploy"),UI->RequestDeploy()); F.Step(); UI->RefreshFromController();
	TestTrue(TEXT("Next cast locks selected 50g"),PC->GetDebugSnapshot().bEquipmentLocked && PC->GetDebugSnapshot().Equipment.TotalMassG==50);
	const auto Snapshot=PC->GetDebugSnapshot();
	TestTrue(TEXT("No spatial NaN"),!Snapshot.Egi.WorldPositionM.ContainsNaN() && !Snapshot.Boat.VelocityMps.ContainsNaN());
	auto* View=F.World->SpawnActor<ATRPrototypeViewActor>();
	const auto Tick=F.Sim()->GetSimulationTime().TickIndex;
	View->ApplyObservation(Snapshot,Mode->SessionConfig->Rod->Parameters,0,30);
	TestTrue(TEXT("Persistent Egi mesh follows sole world position authority"),View->EgiVisual->GetComponentLocation().Equals(Snapshot.Egi.WorldPositionM*100,1.e-6));
	TestTrue(TEXT("Tip mesh follows published rod transform"),View->TipVisual->GetComponentLocation().Equals(Snapshot.Rod.TipWorldPositionM*100,1.e-6));
	TestTrue(TEXT("Line midpoint joins actual endpoints"),View->LineVisual->GetComponentLocation().Equals((Snapshot.Rod.TipWorldPositionM+Snapshot.Egi.WorldPositionM)*50,1.e-6));
	TestTrue(TEXT("Boat visible and collisionless"),View->BoatVisual->IsVisible() && View->BoatVisual->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
	FMinimalViewInfo Camera;View->CalcCamera(0,Camera);
	TestTrue(TEXT("Near perspective observer camera"),Camera.ProjectionMode==ECameraProjectionMode::Perspective && (Camera.Location-Snapshot.Boat.PositionM*100).Size()<2000);
	TestEqual(TEXT("Render projection never advances simulation"),F.Sim()->GetSimulationTime().TickIndex,Tick);
	PC->PlayerCameraManager=F.World->SpawnActor<APlayerCameraManager>();PC->PlayerCameraManager->InitializeFor(PC);PC->SetViewTarget(View);
	const auto OldRod=PC->GetDebugSnapshot().Rod;
	TestTrue(TEXT("Modifier mouse routed to camera"),PC->RoutePrototypeMouse(FVector2D(30,20),true));
	FMinimalViewInfo Turned;View->CalcCamera(0,Turned);
	TestFalse(TEXT("Orbit changes observation"),Turned.Location.Equals(Camera.Location,1.e-6));
	F.Step();TestEqual(TEXT("Camera never queues rod yaw"),PC->GetDebugSnapshot().Rod.BaseYawRad,OldRod.BaseYawRad);
	PC->ResetPrototypeCamera();View->CalcCamera(0,Turned);TestTrue(TEXT("Home restores view"),Turned.Location.Equals(Camera.Location,1.e-6));
	const auto Fixed=View->GetReferenceBuoy()->GetComponentLocation();auto Moved=Snapshot;Moved.Boat.PositionM+=FVector(4,5,0);
	View->ApplyObservation(Moved,Mode->SessionConfig->Rod->Parameters,0,30);
	TestTrue(TEXT("World reference mesh does not follow drifting boat"),View->GetReferenceBuoy()->GetComponentLocation().Equals(Fixed,0));
	TestTrue(TEXT("Boat moves relative to world"),View->BoatVisual->GetComponentLocation().Equals(Moved.Boat.PositionM*100,1.e-6));
	PC->SetInputFocus(false);TestFalse(TEXT("Focus blocks camera too"),PC->RoutePrototypeMouse(FVector2D(30,20),true));PC->SetInputFocus(true);
	PC->SetPauseRequested(true);TestFalse(TEXT("Pause blocks camera input"),PC->RoutePrototypeMouse(FVector2D(30,20),true));PC->SetPauseRequested(false);
	PC->TogglePrototypePanel();TestFalse(TEXT("UI blocks camera input"),PC->RoutePrototypeMouse(FVector2D(30,20),true));PC->TogglePrototypePanel();
	View->ApplyObservation(Snapshot,Mode->SessionConfig->Rod->Parameters,0,30);
	const FVector OldTip=View->TipVisual->GetComponentLocation();
	PC->ActionStarted(ETRPlayerAction::Jerk);F.Step(8);PC->ActionReleased(ETRPlayerAction::Jerk);
	View->ApplyObservation(PC->GetDebugSnapshot(),Mode->SessionConfig->Rod->Parameters,0,30);
	TestFalse(TEXT("Shakuri moves rendered tip"),View->TipVisual->GetComponentLocation().Equals(OldTip,1.e-3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTRPrototypeCompactGTest,"TipRun.M105G.CompactHUDRevisionsAndPanel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTRPrototypeCompactGTest::RunTest(const FString& Parameters)
{
	FTRTestSession F;if(!F.Start(*this)){return false;}
	auto* PC=F.World->SpawnActor<ATRPlayerController>(); PC->BindSession(F.Session.Get());
	TStrongObjectPtr<UTRFishingHUDWidget> UI(CreateWidget<UTRFishingHUDWidget>(F.World,UTRFishingHUDWidget::StaticClass()));
	UI->TakeWidget();UI->BindController(PC);
	TestFalse(TEXT("Initial Closed"),PC->IsPrototypePanelOpen());
	UI->RefreshFromController();TestFalse(TEXT("Refresh never auto opens Ready"),PC->IsPrototypePanelOpen());
	PC->TogglePrototypePanel();UI->RefreshFromController();TestTrue(TEXT("Tab opens"),PC->IsPrototypePanelOpen());
	TestFalse(TEXT("Panel blocks fishing mouse"),PC->SubmitMouseDelta(FVector2D(1,1)));
	PC->TogglePrototypePanel();UI->RefreshFromController();TestFalse(TEXT("Tab closes"),PC->IsPrototypePanelOpen());
	TestFalse(TEXT("No held retrieve after close"),PC->IsRetrieveHeld());
	FTRHUDSnapshot S=PC->GetDebugSnapshot();S.bEnvironmentValid=true;S.Ocean.FieldRevision=2;S.Boat.ModelRevision=2;S.Equipment.Parameters.EgiModelRevision=2;
	UI->ApplySnapshot(S);
	TestTrue(TEXT("All configured revisions displayed before deploy"),UTRFishingHUDWidget::FormatRevisions(S).ToString().Contains(TEXT("Env 2 / Boat 2 / Egi 2")));
	S.bEgiValid=true;S.Egi.EgiModelRevision=1;
	TestTrue(TEXT("Live revision1 mismatch visibly warned"),UTRFishingHUDWidget::HasRevisionMismatch(S) && UTRFishingHUDWidget::FormatRevisions(S).ToString().Contains(TEXT("警告")));
	S.Egi.EgiModelRevision=2;UI->ApplySnapshot(S);
	int32 Primary=0,GuideCount=0;
	for(int32 I=0;I<UTRFishingHUDWidget::BuildReadout(S).Num();++I){Primary+=UTRFishingHUDWidget::IsPrimaryReadout(I)?1:0;}
	for(int32 I=0;I<UTRFishingHUDWidget::BuildGuide(S).Num();++I){GuideCount+=UTRFishingHUDWidget::IsPrimaryGuide(I)?1:0;}
	TestEqual(TEXT("Eight compact readings"),Primary,8);TestEqual(TEXT("Long guide rows only in details"),GuideCount,0);
	TArray<UWidget*> Widgets;UI->WidgetTree->GetAllWidgets(Widgets);
	auto DetailVisibility=[&]()
	{
		for(auto* W:Widgets){if(auto* T=Cast<UTextBlock>(W);T && T->GetText().ToString()==TEXT("キャストID"))
		{return T->GetParent()->GetParent()->GetVisibility();}}
		return ESlateVisibility::Hidden;
	};
	TestTrue(TEXT("Actual detail row initially collapsed"),DetailVisibility()==ESlateVisibility::Collapsed);
	PC->TogglePrototypeDetails();UI->ApplySnapshot(S);
	TestTrue(TEXT("F1 expands actual detail row"),DetailVisibility()==ESlateVisibility::Visible);
	TestFalse(TEXT("F1 does not open equipment or capture mouse"),PC->IsPrototypePanelOpen());
	PC->TogglePrototypeDetails();UI->ApplySnapshot(S);TestTrue(TEXT("F1 collapses"),DetailVisibility()==ESlateVisibility::Collapsed);
	return true;
}
#endif
