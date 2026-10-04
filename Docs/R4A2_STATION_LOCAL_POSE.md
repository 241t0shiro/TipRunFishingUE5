# R4A-2 Station-local Rod Pose移行・検証記録

実装/最終ビルド/試験: 2026-09-30。中断後の証跡再確認・文書反映: 2026-10-04。R4はユーザー手動PIEで正式不合格を維持する。今回は正本移行とCamera独立性だけを実装した。R4A-3完成版Mouse変換、R4A-4のLock修正、R5以降/H/M11は未着手。

## 旧正本からの変更

旧方式はScreenControlと現在Cameraから毎固定Tickで竿を解き直していた。R4A-1では両舷ともCamera-onlyのTipLocal変化0.675294027mを再現した。

新しい永続正本はUTRRodControlComponentのRootLocal、正規化BaseDirectionLocal、凍結LengthM。Station-localの原点はPlayer anchor、基底は選択舷Facing。GripはRodMountMをこの基底へ変換して固定する。Boat/Station→CameraAnchorとBoat/Station→RodGripを論理的な兄弟系統として扱う。表示Componentは従来のObservationRoot配下でWorld Transformを適用し、Cameraの子にはしない。

BaseTipLocalはRootLocal＋BaseDirectionLocal×LengthM。World位置はBoat位置/HeadingとStation Transformで導出し、物理長2mを維持する。Camera LookだけではLocal正本を一切更新しない。Side確定時だけ新Stationの既存初期設定から姿勢を初期化する。

具体的な変更経路:

- TRPlayerController.cpp:279のSubmitMouseDeltaはRodView＋RodAimの2コマンドを廃止し、凍結Camera観測付きRodAimだけを既存Queueへ送る。580のAdvanceFishingCameraから毎FrameのSubmitRodViewを撤去した。
- TRRodControlComponent.cpp:139のApplyViewは互換APIの検証だけでposeを変更しない。62のStepはBaseDirectionからLocal/Worldの派生値を作る。Cameraが変わるだけでBase用ResolveScreenPoseを呼ばない。
- 15のApplyAimは非ゼロMouse Commandの固定消費時だけLocal方向を変更する。129のSetScreenStationで新Grip/初期姿勢を準備し、最初の固定更新で一度初期化する。162のPublishLocalPoseでBase/Finalを明示する。

## 暫定MouseとAction

FTRRodAimObservationに実Camera location/rotation/FOV/ViewRect、選択Side、Camera観測frame、同時点のBoat位置/Headingを保存する。CastId/ModeEpoch/Sequenceは既存FTRFishingCommand、登録世代は既存配送先契約を使用する。消費後の新Cameraを参照しないことは、未来Tickへ送ったMouseの後でCameraだけ変更するRuntime試験で確認した。

Mouse互換処理は前回の凍結入力観測へLocal Baseを投影して旧Control読取値を得て、既存delta/感度/Clamp/固定予算と今回コマンドの観測で旧球面Solverを実行する。ScreenControlは永続正本ではない。今回のCameraへ現在Tipを投影してdeltaを加える正式方式はR4A-3に残す。Camera Look後の最初のMouseが旧絶対画面目標へ寄る可能性も残る。

Shakuriは既存Up/Return、振幅、Reel Pulseを保持。Baseと凍結入力観測のStation-local平面から一時Finalだけを作る。Baseを変更せず、終了時にFinalDirection/FinalTipをBaseへ完全一致させる。同Tick内の開始姿勢記録もSnapshotの前Tick Eulerではなく現在Base方向から導出し、Sequence契約を維持した。係数・入力域・安全閾値は変更していない。

Presentationは同一SnapshotのFinal RootWorld/TipWorldからMesh端点を生成し、Line Startも同じTipを使用する。Station設定済みでSnapshot無効の場合はFishing表示を隠す。旧初期姿勢の黙示Fallbackを使用しない。m→cmは表示境界で一度だけ行う。最大Snapshot/実Mesh Tip差は2.80866677486e-15m。

Snapshot/Insert詳細にはRodRootLocal、BaseRodDirectionLocal、BaseRodTipLocal、FinalRodDirectionLocal、FinalRodTipLocal、LengthMを追加。World Root/Tip、Camera/実Mesh/frame/入力/終了理由のR4A-1診断は維持する。

## Runtime結果

保存Map、保存GameMode/設定、LocalPlayer、Controller、CameraManager、Session、実Rod Mesh、1920×1080 ViewRect、FOV80°を使うR4A-1の通常World Tick経路を継続した。GUI PIE/実OS Mouse capture/GPU描画は実施していない。

| 検証 | 結果 |
|---|---|
| Camera-only独立性 | PASS。両舷で従来D20frameと追加W/A/S/D各90frame。永続Root/BaseDirection/BaseTip/Lengthは完全一致。再変換したLocal Tip差1.11022302e-16m、Direction差0。旧FAILの誤差基準1e-8mは緩めていない。 |
| Boat Drift | 両舷×30/60/120fpsでBoatと竿World位置が実際に移動し、Local Baseは完全一致。World Tipの変位はBoatの変位と1e-8m以内で一致。 |
| Boat Heading | 別の座標Unit試験で両舷×0/90/180/270°。Local値完全一致、World pairがStation basisと1e-12m以内で一致。Runtime Driftと区別する。 |
| 舷切替/取消 | Runtimeの両方向で新Station Gripを使用、Fishing維持、取消で確定済みpose維持。確定後Camera-onlyでもBase不変。 |
| Shakuri Base/Final | 両舷の投入12秒後RuntimeでProfileを通し、Base完全不変、Finalの一時変化と終了後の完全復帰を確認。舷変更直後の短いライン条件の安全停止は下記別FAIL。 |
| Quick | 両舷×30/60/120fpsで前/中/完了ReadyのRoot/Base/Length不変。CameraのBoat相対位置/回転不変、実Mesh整合、Onboard/Unlock成功。 |
| Command観測凍結 | Mouseを12Tick先へ送り、その後Cameraを変更しても、変更しない対照とのBaseDirection差は1e-12以内。消費前Base不変。 |
| Pure Mouse X/Y | 条件内PASS。両舷×30/60/120fpsの実Controller/実Mesh投影で交差軸差0px。以前の手動PIEの軸ずれは未解決、原因未特定。 |

旧KnownFailure.CameraOnlyIndependenceはAcceptance.CameraOnlyIndependenceへ名称変更した。ExpectedError等でFAILを成功に変換していない。旧Math試験はUnitとして残し、視線だけでposeが変わる旧fixture準備を非ゼロ互換Aimの明示準備へ変更した。Runtime受入の代用にはしない。

投影長は測定のみ。物理長/Camera/FOV/Gripを調整して見かけを補正していない。

| 条件 | R4A-1 Min / Max / Ratio(px) | R4A-2 Min / Max / Ratio(px) |
|---|---|---|
| Mouseのみ・30/60/120fps | 508.139 / 731.964 / 1.44048 | 508.139 / 731.964 / 1.44048 |
| Camera込み・30fps | 508.139 / 923.783 / 1.81797 | 507.056 / 731.964 / 1.44356 |
| Camera込み・60fps | 508.139 / 923.783 / 1.81797 | 507.015 / 731.964 / 1.44367 |
| Camera込み・120fps | 508.139 / 921.397 / 1.81328 | 507.015 / 731.964 / 1.44367 |

同じ操作列での観測。投影長の最終受入はR4A-3/A-5で判断する。Camera-onlyでLocalが固定されても透視投影の長さは変化し得る。

## Lock probeと残るFAIL

従来と同じ42ケース（初期/上下端×Deploy1/4frame×Jerk後0/2/5/9/15/24/32frame）は全てRetrieved→Result/Onboard→NでUnlock。Abort0、Active残留0。元の申告Lockはこの条件では未再現であり、解消とは扱わない。

追加のLocalPoseDriftSideActionsでは、初期Starboard→CでPort確定→反対舷仮選択Cancel→DでCameraを約1/3秒変更→Deploy後約1秒→Shakuriで安全停止を再現した。30/60/120fpsの3条件すべて同じ詳細理由。これは新しく観測した失敗で、今回変更が発生原因か、旧Camera追従が症状を隠していたかは比較未確定。

| FPS | 実Step速度 / 既存上限(m/s) | 移動量(m) | Rod速度(m/s) |
|---|---|---|---|
| 30 | 12.9406839 / 10 | 0.215678064 | 5.86880694 |
| 60 | 12.1521742 / 10 | 0.202536237 | 7.05499832 |
| 120 | 11.3149647 / 10 | 0.188582744 | 7.05361924 |

詳細理由はSpatial.ExcessiveMotionOrNonFinite（TREgiSpatialSimulation.cppの既存guard、診断site164）。End=Aborted、State=Result、Onboard=false、EquipmentLocked=true、PendingJerk=0、Reeling=0、Queue=0。通常回収成功ではない。安全閾値を緩めず、Abort/帰還/装備契約を変更せず、当該試験は実FAILのまま残す。R4A-4で原因と復旧を扱う。

## ビルド・回帰・証跡

- 最終81試験: **80 Success / 1 Fail（上記3assert）、notRun0、試験内警告0**。A1は4件全成功、A2は5件中4成功/1失敗。
- 直接関連72件全成功: R1=4、R2=14、R3=6、R4=11、M04=6、M09=7、D=5、E=7、F=6、G=6。A〜C全物理回帰/R9/Hは今回実行していない。
- ForceHeaderGeneration付き通常ビルド。UHT生成TREvents.gen.cppにFTRRodAimObservation/RodView、TRSnapshots.gen.cppに新Local propertyを実際に確認（9/30生成）。変更Runtime/試験.cpp実コンパイル、Link、Development Editor Win64成功。UE5.8.2、MSVC14.51.36257、Windows SDK10.0.22621.0。
- 初回のunity配置による既存テスト定数Rootの名前隠蔽エラーは、TRPrototypeSetupTests.cppの定数をPrototypeAssetRootへ変更して解消。互換逆投影の数値誤差と同Tickの開始姿勢記録も修正後に再試験済み。
- 残存通知はMSVC推奨14.50.35717との差、IncludeOrder Unreal5_6、試験開始前Editor/非Win64環境診断、旧bScreenControl/ScreenControl Python名衝突。今回試験内警告0。git diff --check成功。
- 最終証跡: Saved/Logs/R4A2BuildFinal.log、Saved/Logs/R4A2Final.log、Saved/Automation/R4A2Final/index.json。変更反射コードの実コンパイル履歴はR4A2Build.log〜R4A2Build4.log。再試験Focusedの結果だけでは全体合格にしない。
- 各Frame観測はSaved/Automation/R4A1ObservationsのR4A2_*および再実行したCamera_*/Mouse_*/Lock_*。このフォルダの同名ファイルは再実行結果で更新される。修正前数値の証跡はR4A1_RUNTIME_OBSERVATION.mdとSaved/Automation/R4A1Final*/index.json。Saved生成物はソースへコミットしない。

## 今回変更したファイル

Source/TipRunFishingUE5配下の15ファイル:

- Public/Data/TREvents.h、TRSnapshots.h。
- Public/Fishing/TRRodControlComponent.h、Private/Fishing/TRRodControlComponent.cpp。
- Public/Game/TRFishingSessionActor.h、TRPlayerCameraManager.h、TRSimulationWorldSubsystem.h。
- Private/Game/TRFishingSessionActor.cpp、TRPlayerCameraManager.cpp、TRPlayerController.cpp、TRPrototypeViewActor.cpp、TRSimulationWorldSubsystem.cpp。
- Private/Tests/TRRuntimeRodIntegrationTests.cpp、TRRodScreenTests.cpp、TRPrototypeSetupTests.cpp。

文書: AGENTS.md、Docs/FISHING_SYSTEM.md、UI_SPEC.md、GAME_DESIGN.md、ROADMAP.md、本書。

作業前から存在したDA_TR_M105Input_Prototype.uassetとDA_TR_R3FishingStations_Prototype.uassetの差分を保持。今回のContent/Config/調整DataAsset追加変更なし。R4A-2の正本/Camera独立性は確認でき、R4A-3へ接続する基盤はある。全体試験は未全成功であり、R4正式完了は宣言しない。次段階は別の明示依頼で開始する。
