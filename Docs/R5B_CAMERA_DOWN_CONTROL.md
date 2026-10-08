# R5B Camera-down Rod Control / Shakuri Plane Repair

2026-10-05。R5Aの自動成功後、ユーザー手動PIEでCamera Down時のMouse操作が不合格。本記録はR5Bのみ。R5は正式不合格を維持し、修正後の手動再確認待ち。R6以降/R7/H/M11には着手しない。

## 修正前に再現した原因

保存 `/Game/TipRun/Prototype/M105/L_TR_M105_Prototype` のGame Worldをロードし、保存GameMode/Session/Station/Rod/Input、LocalPlayer、PlayerController、PlayerCameraManager、通常World Tick、Session Queue、固定更新、PrototypeViewActor、実Rod Meshで調査した。1920×1080、実水平FOV80。GUI自動PIEは使用していない。

原因B（Mouse projection）を再現。原因A（Shakuri自体の左右yaw）は今回の条件では再現しなかった。修正前Pitch Sweepの両舷20条件でShakuri yaw差0、最大Tip変位0.597752529894m、Recover後Final==Base。Camera Down時の画面上の横向きの見え方だけでAction yaw変更と断定しない。

修正前の `TRRodControlComponent.cpp::ApplyAim` は、現在Base Tipを投影しDeltaを足した後、Target NDCを±1へClampしていた。Envelope有効ならViewportBoundaryとし、無効ならScreenBoundaryで拒否。下向き視線でBaseが上端外へ出ると、Pure XでもYを上端へ戻す強制変位が混入する。その後Station-local Envelopeを適用するため、さらに下向きではPitch下限へ押し付け、Y入力をYaw移動へ化かした。後方投影ではBehindCameraで拒否。Controllerも実ProjectionData取得失敗をQueue前に破棄していた。

修正前の失敗証跡は `Saved/Automation/R5BBefore/index.json` / `Saved/Logs/R5BBefore.log`。PitchSweepAndMouseRecoveryはFAIL（26 assertion errors、warning0）。期待値を緩めず保持した。

| Camera実Pitch | 修正前の症状 | 修正後 |
| --- | --- | --- |
| −20° | Base Tip Y396px、中央で正常 | 従来と同じ投影Delta |
| −35.1666675° | Base Tip Y69.833px | 従来と同じ投影Delta |
| −39.8333344° | Base Y−43.119px。Pure XでY0へ強制移動、方向差0.041377。次の上入力の変位約6.78e−32 | 画面外座標を保持しPure XはXのみ3.84px移動、四方向Valid |
| −64.916669° | Base Y−929.136px。Pure XでPitch−10°へ押し付け、方向差0.128803。Pure YでXも移動 | Pure X後Y−929.136pxを保持、方向差0.00274728。Pure YはYのみ3.84px移動 |

修正前5°間隔で最初に再現したPitchは−39.8333344°。修正後に同じ物理Baseの境界を細かく観測すると、−38.0833343°ではY0.241px、−39.250001°ではY−28.521px。画面外になる境界はこの間。入力Targetが端を越える場合はBase自体がまだ画面内でも旧Clampが発生し得る。Boat/Camera/Baseごとの投影境界であり、普遍的なバグ発生角ではない。

## 現行Mouse契約

永続正本は固定Station-local Grip、BaseRodDirectionLocal、LengthM=2。CameraだけのTickでBaseを再生成しない。非ゼロMouse Commandだけが発行Frameの実POV、ProjectionScale/Offset/ViewRectとBoat基準を凍結してQueueへ渡す。

- 前方で有効な投影: 最新**Base** Tipのunbounded NDCへDeltaを加え、捕捉したProjectionの逆写像からRayを作り固定Root中心の2m球と交差させる。Current/TargetをViewRectへClampしない。ViewRect外はOffScreen診断であり拒否理由ではない。
- 後方/投影尺度無効: 発行時Camera Right/Upを現在Base Directionの接平面へ投影し、正規化した接線とsin/cosの短い大円ステップで新Directionを得る。結果だけをStation-local Baseへ保存。画面上の厳密な軸対応は投影可能時の契約であり、後方に有効な画面位置を捏造しない。
- 接線が軸の極で退化した場合は同じCamera Forwardの子午線を使って回復可能にする。非有限Camera/Pose自体を有効な観測として扱わない。
- 既存Mouse感度、反転、1Command Delta制限、同Tick速度予算を保持。Base Envelope yaw±40°/pitch−10〜35°、海面clearance、Station外向き半球を保持。実Envelopeだけで外向きを制限し、逆向きは最初の入力から内側へ戻れる。
- 保存PrototypeのEyeはGrip中心2m球の内側にあり、正規化Rayには前方交点があることをRuntimeで検証。幾何的に交点のない別Camera配置を今回新設していない。無効/非有限交点は明示拒否し、異常値で位置を作らない。
- Action Final TipをMouseの現在点へ保存しない。Shakuri中でも次のMouseはBase Tipから解釈する。

Controller→Session Queue→Tick/Sequence/CastId/ModeEpoch/登録世代→RodControlの経路を維持。Projection取得失敗でも有限な実POVは無効尺度としてQueueへ渡す。投影不能でHeld入力を新たに発火させる処理はない。

## Shakuriの縦平面

R5Aの物理計算を再確認し、変更する必要がないことをRuntimeで確認した。Station-local Base Directionの水平成分Hを正規化し、UpをUとする。Base yawを固定した `Final = H*cos(BasePitch+Temporary) + U*sin(BasePitch+Temporary)` の縦平面だけで上げ戻しする。Action安全上限75°はこの平面内のPitchだけへ適用。Camera/Screen/FOVをAction量・面の決定に使用しない。

MouseはBase、SequenceはTemporary、Finalはその合成。固定Root/2mは全Action Frameで不変。終了時は最新Baseへ完全復帰。1Started=1要求、Hold Repeatなし、1/2/3/5予約Up→Recover、予約間Stayなし、最後のみTensionFall→Stay、最大1turn/名目0.8mを維持。R6のDrag/slip/弛み回収は未実装。

表示は既存の同一Snapshot Final Root/Tipから生成し、Line Startも同じTip。PresentationへCamera依存補正を追加していない。テストはComponent Transformと実Mesh boundsからVisual端点を取得してActive Cameraへ再投影する。Gripが後方でもTipの投影を独立に試し、無効投影を有効なpixelとして扱わない。

## 新規Runtime試験と結果

`TipRun.R5B.Runtime` の4件:

1. PitchSweepAndMouseRecovery: 両舷×14Pitch（−20から許可下限−65まで、境界付近を細分化）。Deploy/沈下→Camera Down→1右クリック→Recover→Mouse右/左/上/下。Root/Base/Final/yaw/pitch、実Mesh投影、前/Peak/Recover/Mouse後、可視性、投影/交差/制限理由を記録。四方向Valid、最初から変位、投影の他軸誤差<0.05px、反対入力で可逆。
2. ExtremeAimCameraPlaneAndRecovery: 両舷×Base中央/左−39°/右39°/上34°/下−9°×Camera正面/上/下/左/右=50Action。全60Frameの縦平面軌跡を比較し、下向き10条件ではRecover後の四方向も検証。
3. BehindProjectionFallbackAndEnvelope: 実Camera yaw55/pitch−65でBaseが後方になる条件、実POVを保持して投影尺度だけ0にしたfault、接線軸の極、左右画面外、真のBase各境界と最初の逆入力。Side/Cast/世代の検証は維持。投影faultは外部不調を制御注入した試験であり、UE APIが実際に失敗したことの実測ではない。
4. OffscreenFixedTickReplay: 両舷×30/60/120fps、実観測の同一Tick/Sequence列400Command。100組の左右/上下往復、同一結果、累積なし、物理/実表示端点一致。

最大Shakuri azimuth差2.22044604925e−16rad（基準<1e−12rad）。同じBaseの5Camera間で全Station-local軌跡差0m（基準<1e−10m）。両舷で最大Tip変位0.597752529894m、Recover差0。新規試験の実Mesh/Snapshot端点最大差1.42177919159e−15m。400Command後Base方向差最大7.30138034004e−16。真のEnvelope外向き制限と最初の内向きValidを確認。

関連A5広域Sweepの1080p正規化投影長は435.991〜1922.436px（比4.40935）。旧Viewport制限で到達できなかった姿勢も実Envelope内で到達可能になり、過去A5の範囲より最大投影長が大きい。固定物理長/実Mesh長は2mのまま。画面内への自動収容は行わず、Perspectiveの見え方/操作感は手動再確認の対象とする。

最終採用 **91件Success / FAIL0 / notRun0 / errors0**（新規4＋関連87）。関連はR5=8、R5A=4、A1=4、A2=5、A3=4、A4=3、A5=5、R4=11、R3=6、D=5、E=7、F=6、G=6、M09=7、M04=6。Normal42条件はnormalResults42 / active0 / Technical Abort0。Pending Retrieve/Re-Fall、Quick、Intentional Abort/N復旧、旧Cast/世代拒否、UI/Pause/Focus、Camera-only、実Mouse device軸も直接回帰で成功。

採用元: `Saved/Automation/R5BAcceptedResults.json`。R5BFinalの関連87件と最終R5BVisualFinalの新規4件を採用。単一実行の最終91件ではない。R5BFinal自体も91成功。最後は実Tip投影をGrip可視性と独立に観測するtest-only変更であり、その4件だけ再実行した。

途中R5BFirstでは逆入力後の変位を操作前でなく往復開始点と比較するfixtureと、故障注入観測のSide欠落が失敗。正しいSideを保持しCommand直前との差を検証するよう修正した。プロダクト契約/許容誤差をこの失敗のために緩めていない。観測用PeakPixelのC4701は初期化して最終ビルドで解消。修正前/途中失敗レポートは保持。

## Build・警告・保全

UE5.8.2 / MSVC14.51.36257 / SDK10.0.22621.0。R5BFirstBuildでUHT6生成ファイルと変更Runtime C++/Linkが成功。R5BFinalBuildでRodControl/Runtime試験を実コンパイル、R5BVisualFinalBuildで最終試験を実コンパイル・Link、Development Editor Win64成功。up-to-dateだけではない。git diff --check成功。

採用試験内警告3件は既存外部HTTP generate_204タイムアウト（A5 Replay/Sweep、R5B Pitch）。今回コード由来の残存コンパイル警告なし。既存MSVC推奨版/include順通知、Editor起動の非対象SDK/Python通知は残る。

開始時のConfig/Content全20ファイルをSHA256照合、追加/削除/変更0。開始前のR5/R5A、保存Rod移行Asset差分を保持。Camera範囲/Anchor/FOV、Mouse感度、Rod長、Base Envelope、0.8m、Egi安全閾値、Environment、Shakuri/Reel係数の追加変更なし。

## 今回変更したファイル

Source4: Private/Fishing/TRRodControlComponent.cpp、Private/Game/TRPlayerController.cpp、Public/Data/TRSnapshots.h、Private/Tests/TRRuntimeRodIntegrationTests.cpp。

Markdown7: 本記録、AGENTS.md、FISHING_SYSTEM.md、UI_SPEC.md、ROADMAP.md、GAME_DESIGN.md、R5A_CAMERA_INDEPENDENCE.md。追加診断はInsert詳細のみ。AimMapping（UnboundedProjection/CameraTangent）、ProjectionStatus（OnScreen/OffScreen/BehindCamera/ProjectionInvalid）、valid/outside、交差結果/判別式、Envelope理由を分離。通常HUD/キーは保持。

## 最小手動PIE再確認（4項目）

1. Editor再起動→保存Map→PIE→Enter/AまたはD/Enter→投入・沈下。Mouseを止め、Sで十分下へ向け右1クリック。竿が画面外になっても動作消失/横方向の物理的逃げがなく、Recoverで元Baseへ戻ること。
2. そのままMouse右/左/上/下。最初の入力から反応し、往復で元へ戻ること。上端/左右画面外でも操作が張り付かないこと。Insertのmapping/projection/clampを必要時に記録。
3. Cで両舷を替え、Baseを左右/上下境界付近に動かし1/2/3/5クリック・Hold。実Envelope外向きだけ制限、逆入力で即復帰、Sequence数と最新Base復帰を確認。
4. Action中左Hold/Release/F/Q、Tab/Pause/Focus。Pending/Quick Readyと装備解除、永久Lockなし。異常時はCamera Pitch/Yaw・Base/Final・診断理由を記録する。

headless Runtimeは実保存資産/入力Queue/実Mesh/実Camera投影を使うが、NullRHIのためGPU描画・実OSのMouse capture・主観的操作感の合格を代用しない。R5B自動検証完了、**R5正式不合格・修正後手動PIE待ち**。R6以降へ進行しない。
