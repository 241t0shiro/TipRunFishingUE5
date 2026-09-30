# R4A-1 Runtime観測・失敗再現記録

2026-09-30。R4はユーザーの手動PIEにより正式不合格。今回は監査後の第1段階だけを実施した。Rod方式、Camera設定、Shakuri/Reel量、Egi安全閾値、Abortの意味、Environment、Config、保存Contentは今回変更していない。Station-local正本移行（R4A-2）、R5以降/H/M11は未着手。

## 試験構成と観測境界

- 保存Map `/Game/TipRun/Prototype/M105/L_TR_M105_Prototype` を `UEngine::LoadMap` で独立したGame Worldへロードする。保存BP GameMode `BP_TR_M105GameMode_Prototype_C` とそのSessionConfig/参照DataAssetを使用。調整値を試験側へコピーしない。Editorで開かれている元Worldは破棄しない。
- 本物のGameInstance、LocalPlayer、GameViewportClient、1920×1080 SceneViewport、PlayerController、PlayerCameraManager、GameMode生成Session、Station、RodControl、保存PrototypeViewActor、StaticMeshComponentを使用する。
- 入力はViewportClientのInputKey/InputAxis→実Controller→Enhanced Input→Session Queue。Worldの通常Tickで固定Simulation、Camera更新、PostUpdateWorkのPresentationまで進める。World後のEngine共通Tickable更新も実行し、Enhanced Inputの保留Context再構築を通す。Solver直呼び、Action直接注入、手動ApplyObservationを受入経路に使わない。
- NullRHIのheadless World試験である。実OSのMouse capture、Slateの物理カーソル入力、GPU描画・画面の印象は再現していない。rawイベント境界より前を検証済みとはしない。自動GUI PIEは起動していない。
- RodVisualの実StaticMesh BoundingBoxの長軸両端をComponent TransformでWorldへ変換。Snapshot Tipの代用ではない。LineやSolverの値を書き換えない。
- そのFrameのLocalPlayer::GetProjectionDataからViewRect/Projection Matrixを取得し、実Mesh Root/TipをProjectWorldToScreenする。CameraManagerのCache POVから位置/回転/FOVを採取。今回の実FOV=80°、ViewRect=(0,0)-(1920,1080)。
- Snapshot Root/Tip、実Mesh Root/Tip、両者の誤差、Boat-localとStation-localのRoot/Tip/Direction、実長、Camera情報、Rod Tick、World/Visual/Camera frameを同時に記録する。Station-localはPlayer anchor原点・Station Facing basisを使う。

## 観測結果と分類

| 項目 | 分類 | 結果 |
|---|---|---|
| Camera-only independence | **Expected FAIL** | 両舷でDを20frame保持。Mouseなし、RootLocal変化0mだがTipLocalは0.675294027m、Directionは0.337647014変化。Station-local不変assertが両舷計4失敗。 |
| Pure Mouse X/Y | 条件内PASS／申告症状は**未再現** | 両舷×30/60/120fps、各軸正負で実入力・実Mesh投影。交差軸の最大frame差0px（試験許容0.05px）。Camera角度は不変。OS実マウスPIEを合格扱いにしない。 |
| 実長・表示整合 | PASS | World/Visual長2m。Snapshot TipとVisual Tipの最大誤差2.084e-15m。 |
| 投影長 | **測定のみ** | 下表。合格閾値は未設定。見かけの伸縮が直ったとはしない。 |
| Deploy→Shakuri→Retrieve Lock | **未再現** | Rod初期/上下端×投入後1/4frame×Shakuri後0/2/5/9/15/24/32frame＝42ケース。すべてJerk受理を確認、通常Retrieved→Result/Onboard→NでUnlock。Abort0件。 |
| 診断読取とInsert | PASS | 読取20回でTick/Queue/Rod不変。実Insertで詳細開閉、診断行は通常HUD非表示。 |

投影長（px、両舷ほぼ同値、同じ操作列）:

| 描画条件 | MouseのみMin / Max / Ratio | Camera Lookも含むMin / Max / Ratio |
|---|---|---|
| 30fps | 508.139 / 731.964 / 1.44048 | 508.139 / 923.783 / 1.81797 |
| 60fps | 508.139 / 731.964 / 1.44048 | 508.139 / 923.783 / 1.81797 |
| 120fps | 508.139 / 731.964 / 1.44048 | 508.139 / 921.397 / 1.81328 |

Cameraの保持時間は各キー1/3秒。120fpsではCameraの表示frameと60Hz固定Rod更新の位相差を含む。比較用旧Math試験の431.221〜731.964px/1.6974倍とは操作範囲が同一ではないため、増減率だけで品質改善を主張しない。今回も実長一定と画面上の長さ一定は別物である。

## Mouse入力とLock診断

- Raw Mouse 20に対してEnhanced deltaは1.4、Queue受理/固定消費も1.4。初回のMouse sampleは既存の復帰ジャンプ防止で破棄されるため、往復net合計はRaw/Enhanced=0、Queue/Consumed=-1.4となる。これは受理後のQueue脱落ではない。
- 現設定のMouseX/Y sensitivity=0.07、exponent=1、deadzone=0、invert=false。bEnableMouseSmoothing=true、bEnableFOVScaling=true、FOVScale=0.01111を診断へ表示。Vector入力の実測倍率は0.07で、設定フラグだけからFOV/smoothingが原因と断定しない。設定変更なし。
- 毎frameの累積Raw/Enhanced/Queue/Consumed値、Queue投入deltaと受付可否、固定消費delta/Tick/Sequence/CastId/ModeEpochをログで比較可能。物理デバイスからViewportへ届く以前の欠落は未観測。
- Lockの正確な原因は今回未特定。EnvironmentInvalid発生も0件のため、推測を再現結果にしない。通常Resultでの装備ロックは既存契約。Abort時に帰還を捏造せずOnboard=falseを維持する従来仕様も変更していない。
- 安全停止に到達した場合、SessionはFinishInternalによる掃除より前の状態をDiagnosticAbortContextに保存する。PendingJerk、Jerk回数/開始Tick、Reeling、State/CastId/ModeEpoch/登録世代、Queue、Egi位置/ライン、End reason、Lock/Onboard、受付可能Actionを追跡できる。Controller側のRetrieveHeld/停止待ち/blocked/pressedも記録する。
- EnvironmentInvalidの戻り値・条件は保持し、診断文字列だけ追加。例: `Egi.InvalidStepInput.InvalidOceanSample` / `StaleSampleTick` / `NonFiniteBoat` / `InvalidActionParameters`、`Spatial.InvalidRodVelocityOrTravel`、`InvalidOceanAtEgi`、`InvalidOceanAtLine`、`InvalidCandidateVelocity`、`InvalidLineLength`、`InvalidLineSurfaceIntersection`、`ConstraintNotConverged`、`InvalidOceanAtConstraintDestination`、`InvalidDepth`、`ExcessiveMotionOrNonFinite`。速度/移動量guardは実値と既存上限も保存する。発生ソース行、CastId、前Tick、位置、ライン長を付記する。

## Insert詳細HUD

通常表示には追加しない。Insert詳細の末尾「R4A-1 Runtime診断（読取専用）」へ、Mode/Side/State、Boat/Station local Root/Tip/Direction、Rod長、Camera回転/FOV/ViewRect、frame/tick、Snapshotとの差、Input Context、Raw/Enhanced/Queue/Consumed、最後のRod/Action受付結果、終了/安全停止理由、PendingJerk/Reeling/RetrieveHeld、Lock/Onboard、受付可能Actionを表示。既存の折返し・スクロールを使用する。手動PIEでの可読性は未評価。

## 検証証跡

- 新規4件: `TipRun.R4A1.KnownFailure.CameraOnlyIndependence` は実FAILを返す。AddExpectedError等で成功へ変換しない。他3件は試験実行成功だが、Lock/Mouse申告症状の解消を意味しない。
- 直接回帰73件成功、試験内警告0: R1=4、R2=14、R3=6、R4=11、M04=6、M09=7、D=5、E=7、F=6、G=6、C安全契約=1。旧R4 Mathの成功は受入代用にしない。
- UHTをForceHeaderGeneration付き通常ビルドで実行。新しいRuntimeDiagnostics反射コードの生成を確認。変更.cppの実コンパイル/Link/Development Editor Win64成功。UE5.8.2、MSVC14.51.36257、Windows SDK10.0.22621.0。
- 基本証跡: `Saved/Logs/R4A1BuildFinal.log`、`Saved/Logs/R4A1Final.log`、`Saved/Automation/R4A1Final/index.json`（76 Success、1 Expected FAIL、notRun0）。最後の無効観測初期値・診断理由の区別を反映した再検証は `R4A1BuildFinal2.log` / `R4A1Final2.log` / `Saved/Automation/R4A1Final2/index.json`（再検証5件: 4 Success、Camera 1 Expected FAIL、警告0）。git diff --check成功。
- 各frameの詳細は `Saved/Automation/R4A1Observations/*.txt`。これらのSaved生成物はソースへコミットしない。
- 初回のWorld再利用/終了処理の試験基盤クラッシュと、Engine共通Context更新を欠いた起動失敗は修正済み。これらをExpected FAILの証拠にしない。
- 残る既存通知: MSVC推奨版との差、IncludeOrder Unreal5_6、試験開始前のEditor/非Win64環境診断、Pythonの旧 `bScreenControl` / `ScreenControl` 名衝突。今回の試験内警告0。プロセス終了コードだけで合否を判定せずAutomation JSONを読む。

再実行フィルタ:

```text
Automation RunTests TipRun.R4A1.
```

全フィルタでは既知不具合のFAILが残る。正常回帰と分離したい場合は `TipRun.R4A1.Runtime` と `TipRun.R4A1.KnownFailure` を別ジョブで実行し、後者のFAILを既知問題として報告する。後者を省略してR4合格とはしない。

## 今回の変更ファイル

Source/TipRunFishingUE5配下:

- 新規: `Public/Data/TRRuntimeObservation.h`、`Private/Game/TRRuntimeObservation.cpp`、`Private/Tests/TRRuntimeRodIntegrationTests.cpp`。
- 診断Snapshot: `Public/Data/TRHUDSnapshot.h`。
- 観測: `Public/Game/TRPrototypeViewActor.h` / `Private/Game/TRPrototypeViewActor.cpp`、`Public/Game/TRPlayerCameraManager.h` / `Private/Game/TRPlayerCameraManager.cpp`。
- 入力・寿命・状態診断: `Public/Game/TRPlayerController.h` / `Private/Game/TRPlayerController.cpp`、`Public/Game/TRFishingSessionActor.h` / `Private/Game/TRFishingSessionActor.cpp`、`Public/Game/TRSimulationWorldSubsystem.h` / `Private/Game/TRSimulationWorldSubsystem.cpp`。
- 安全停止理由: `Public/Fishing/TREgiSimulationComponent.h`、`Private/Fishing/TREgiSimulationComponent.cpp`、`Private/Fishing/TREgiSpatialSimulation.cpp`。
- 詳細HUDの値接続: `Private/UI/TRPrototypePresentation.cpp`。
- 文書: 本書、`AGENTS.md`、`Docs/ROADMAP.md`、`Docs/UI_SPEC.md`。

以前から残っていたR4のSource/文書/2つの保存Asset差分は保持。今回のAsset/Config追加変更なし。

## 次段階への判断

第1段階の観測基盤とCamera結合のFailing Integration Testは成立した。R4A-2ではこのFAILを修正前後の基準に使用できるが、今回は未着手。Pure Mouseの申告症状とLockは追加の実PIE証拠が必要で、未再現項目をPASSへ移さない。R4自体は正式不合格を維持する。

手動で追加証拠を取る場合は、保存Map→Enter/AまたはD/Enter→Insert。MouseなしWASD、Camera固定のMouse X/Y、Deploy直後右クリック→左保持の順に確認する。Lock発生時はEnd/Failure/Before Abort cleanupとMode/Side/Camera/入力値を記録する。R4A-1は症状を直していないため、解消確認を求める段階ではない。
