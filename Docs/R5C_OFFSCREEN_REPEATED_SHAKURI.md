# R5C Off-screen Repeated Shakuri Freeze / Abort Repair

2026-10-05。R5B自動成功後、ユーザー手動PIEで「画面外の竿を繰り返しシャクリ、その後下方/左右操作で停止、Nで復帰」を確認。R5正式不合格を維持。今回R5Cの幾何整合・Runtime受入・診断のみ。R6以降/R7/H/M11は未着手。修正後手動PIE待ち。

## 再現と正確な分類

保存 `/Game/TipRun/Prototype/M105/L_TR_M105_Prototype` を実Game Worldとしてロードし、保存GameMode/Session/Station/Rod/Input、実LocalPlayer/Controller/CameraManager/Viewport/PrototypeViewActor/実Meshを使う。通常World Tick→Input Queue→固定Simulation→Camera/Presentation。右クリック/キーはViewportのInputKey、Mouse再現は実ControllerのSubmitMouseDelta（発行時実Camera捕捉）を使用。R4A1のraw Mouse/Enhanced経路は別の直接回帰で確認する。Solver直接呼出し/手作業表示適用は使用しない。NullRHIでありGPU描画/物理OS Mouseは未検証、自動GUI PIEは起動していない。

初回108条件（両舷×投入後4/60/480frame×3視線×1/2/3/5/10/20クリック）と低/中/高Base・間隔4/26/60frameの36条件×50クリックだけでは停止しなかった。進捗で初回216と記したが実数は108であり、集計を訂正した。

ユーザー補足「約5mから約10回短い間隔で連打、水面付近で視線を下げ左右に振る」を受け、深度5.00500011444mまで実沈下→10回連打→S/D/A視線→Mouse左右を追加。両舷×Mouse量0/1/20/200の8条件中、Mouseなし2条件はPASS、Mouseあり6条件は修正前FAIL。保存証跡: `Saved/Automation/R5CSurfaceBefore/index.json`、`Saved/Logs/R5CSurfaceBefore.log`。実OS入力そのものの再現とは区別する。

停止直前はFishing/Stay、CastId1/ModeEpoch3/登録2、Sequence10/Completed10/Queued0、Temporary0、PendingRetrieve/ReFallなし、要求8m、実績約7.14728〜7.19999m。技術停止後はFishing/Result、LastResult=Aborted、Onboard=false、EquipmentLocked=true、登録3、RecoveryAvailable=true、受付可能Action=NextCastのみ。詳細理由は旧 `Spatial.InvalidLineSurfaceIntersection site=150`。Line約1.52332473〜1.56198668m、エギは海面、竿先移動後にLineが海面上高さより短くなり海面とライン球の交差がなくなった。

**分類はTechnical Abort→Recovery Available。** Resultが停止の結果であり、Sequence/Pendingのdeadlock、Queue starvation、Hold/rearm待ちではない。Nは既存A4の明示ResetInternalを通し、Abortedを保持したままCastIdを無効化、新登録、Ready/Onboard/Unlockへ復旧する。今回NやAbortの意味は変更しない。両舷6条件で同じN復旧を確認。

## 根本原因と修正

`TREgiSpatialSimulation.cpp::StepSpatial` のA4海面必要長処理がReelIn内部にしかなかった。連打の巻取りでエギが水面へ戻り、最終Recover→Stay/Holdとなると、Mouseで上がった竿先に対して必要なspanを更新せず、保持Lineと海面拘束が幾何的に矛盾した。Camera/Viewportは物理判定に使用していない。画面外でMouseを左右へ動かしたときの正当なLocal姿勢変更が、不整合を露呈した。

海面接触/海面へ達する拘束候補に対する既存必要長 `sqrt(SurfaceHeight² + max(0,HorizontalRadius−AllowedReelM)²)` を全LineMode共通の位置へ移す。Hold/Payoutにも必要な幾何span増加を認め、ReelInだけがAllowedReelMを要求する。ActualReelは最終spanを評価した後の正の短縮量のみ。巻けない要求を強制せず、当Tick/Action/累積Actual<=Requestedの既存検査と保守的float丸めは保持。

これはA4の簡易幾何整合の拡張（ゲーム近似）。R6のSlack-aware Reel/Drag/slip/弾性・リール物理は実装していない。Egiへの速度注入/直接瞬間移動、Safety thresholdの変更、Abort成功化、Camera/FOV/Envelope/感度/Length/Environment/名目0.8mの変更はなし。

必要な幾何長の増加は累積Actualへ加算しない。累積Actualは各Tickの正の短縮量の合計であり、開始Line−終了Lineではない。繰り返す竿Action中には必要span増加もあるため、40m要求/約31m累積Actualを「水深5mから31mネット回収」と解釈しない。

## 診断

Egi Snapshotへ読取専用のLineConstraintCorrectionM、GeometrySpanAccommodationMを追加。Insertの既存Sequence/Requested/Actual/Pending、Root/Base/Final、Rod/Egi速度、Queue/Cast/ModeEpoch/登録、Lock/Onboard、Abort前Contextと併読する。必要長、slack、deficit、最後の補正、必要span増加を追加。

旧複合InvalidLineSurfaceIntersectionを診断上、InvalidOceanAtLineConstraint / RodBelowSurface / LineBelowSurfaceSpanへ分離。後者は当TickのRod/Egi/Line/海面上高さ/不足を保存する。guardの条件・閾値は保持。Snapshot値をSimulationへ逆流させない。通常HUD/キーは変更なし。

## 最終Runtime結果

新規4試験、計218保存World条件。

- OffscreenRepeated: 両舷×投入後4/60/480frame×4視線/対応Base×1/2/3/5/10/20/50クリック =168条件。実Mesh Tipが実ViewRect外または後方であることをassert。Camera Down（上端外）、Camera Up、左右視線で確認。許可Camera範囲を変えず、要求Up20度は保存Clamp上限で評価する。後方では有効pixelを捏造しない。
- LowBaseCadenceProbe: 両舷×Base pitch−9/0/34度×投入後4/480frame×4/26/60frame間隔 =36条件、各50回。短ライン/水面での繰返しも正常。
- SurfaceLookAndMouse: 上記手動再現8条件。修正前6FAIL→修正後8PASS。10回のSequence完了、操作可能Stay、QでReady/Unlockまで確認。Nは不要。
- FiftyOffscreenFixedTickReplay: 両舷×30/60/120fps=6条件、各50入力。同一Tick/Sequence/状態イベント列、エギ位置1e−8m以内、累積Actual完全一致。

全てTechnical Abort0/永久Lock0、実行数/完了数正確、最終Queue0/Temporary0/Final==最新Base。固定Baseの条件ではRoot/Base/Length2m不変、実Mesh誤差<1e−7m、観測した位置/速度/Line有限。Hold RepeatなしとPending/Quick/UI/Pause/Focusは既存R5等で回帰。

OffscreenRepeatedの24条件/回数の最終値（m）:

| 回数 | Requested合計 | Actual合計Min〜Max | 最終Depth Min〜Max | 最終Line Min〜Max |
| --- | ---: | ---: | ---: | ---: |
| 1 | 0.8 | 0.447886〜0.799999 | 0〜3.624601 | 1.294684〜7.548548 |
| 2 | 1.6 | 0.893767〜1.599998 | 0〜3.174069 | 1.294684〜6.748549 |
| 3 | 2.4 | 1.338405〜2.399998 | 0〜2.946118 | 1.294684〜5.948549 |
| 5 | 4 | 2.226696〜3.999996 | 0〜0.646613 | 1.294684〜4.348551 |
| 10 | 8 | 4.446230〜7.307500 | 0 | 1.294684〜2.744319 |
| 20 | 16 | 8.883478〜13.203073 | 0 | 1.294684〜2.744124 |
| 50 | 40 | 22.190556〜30.908621 | 0 | 1.296443〜2.748387 |

50回時Required span=1.29644308〜2.74838667m。既存float Line境界/1e−5m幾何許容内で整合。固定Tick50回リプレイのPortはRequested40/Actual31.1360685825、Line1.51647567749/Required1.51647564103、Starboardは40/31.139349699、Line1.52445578575/Required1.52445577572。3fpsで同じ値、最終Depth0。

Normal42は全42Retrieved、Technical Abort0/Active0。Intentional NaN→Aborted→N→Readyと旧Cast/登録世代拒否も成功。R5 8件、R5A4件、R5B4件、R4A1〜A5、R3/R4/D/E/F/G/M04/M09、追加のC/M07/M08/M10直接物理回帰を実施。

## Build・回帰採用元・警告

UE5.8.2 / MSVC14.51.36257 / SDK10.0.22621.0。R5CRepairBuildはUHT7生成ファイル、変更Runtime/試験の実C++/Link成功。R5CFinalBuildは最終Runtime試験、R5CFixtureFinalBuildは下記共通fixture依存試験を実コンパイル/Link、Development Editor Win64成功。up-to-dateのみではない。最後のR5CVerifiedBuildでもSpatial実C++/Link成功。最後の変更は幾何整合を説明するコメントのみで、動作変更がないため150件を重複再実行していない。

R5CFinalは122件中121成功/1FAIL。追加したM07.LargeFixedStepの既存fixtureが、時計0.25秒なのにMode確定準備で1/60秒だけ進めていた。EnterFishingModeの準備だけを設定StepSeconds1回へ修正。プロダクトMode/Simulationは変更せず、当該fixture使用範囲（R1/R2/B/M06等含む）103件をR5CFixtureFinalで再実行し全成功/警告0。

最終採用 **150件成功、errors0/FAIL0/notRun0**（新規4＋関連146）。R5CFinalの47件とR5CFixtureFinalの103件を試験別に採用。単一150件実行ではない。採用元は`Saved/Automation/R5CAcceptedResults.json`。途中の修正前FAIL/fixture FAILの証跡は削除していない。

採用試験内警告6件はEngineの外部HTTP generate_204の3秒timeout（A5 Replay/Sweep、R5B Pitch、R5C Cadence/Repeated/Surface）。R5C由来コンパイル警告なし。既存MSVC推奨版差/include順通知、起動時非Win64 SDK/Python通知は保持。git diff --check成功。Config/Content全20ファイルの開始時SHA256と同一、追加/削除/変更0。中断前R5/R5A/R5Bと保存Rodの差分を保持。

## 今回変更したファイル

Source/TipRunFishingUE5:

- Private/Fishing/TREgiSpatialSimulation.cpp
- Public/Data/TRSnapshots.h
- Private/Game/TRFishingSessionActor.cpp
- Private/Tests/TRRuntimeRodIntegrationTests.cpp
- Private/Tests/TRSessionTestFixture.h（大ステップ回帰の準備のみ）

Markdown: 本記録、AGENTS.md、FISHING_SYSTEM.md、UI_SPEC.md、GAME_DESIGN.md、ROADMAP.md。ShakuriSequence/RodControl/Controller/各Tuning/Config/Contentの今回追加変更なし。

## 最小手動PIE再確認（3項目）

1. Editor再起動→保存L_TR_M105_Prototype→PIE→Enter/AまたはD/Enter→投入。Depth約5mから右約10クリックを短い間隔で実行し水面へ引く。Sで下を見る、A/Dで左右を見る（Mouseなし）、その後Mouseで竿を左右へ振る。両舷で操作が止まらず、N不要でShakuri/左回収/Qを続行できること。
2. 竿を上端/左右の画面外へ出し、10/20/50回クリック、右保持は1回だけ。InsertでCompleted、Queue0、Temporary0、Requested/Actual、失敗理由を確認。各Sequence後に最新Baseへ戻り、固定Grip/2mが不変であること。
3. Action途中左保持/Release/F/Q、Pause/Focus/Tab、Q後再投入。Pending/Ready/Unlockと保持解除が正常で、永久Lockがないこと。異常が出たらInsertのBefore Abort contextとCamera/Base/Line/Failureを記録する。

R5C修正と自動受入は成功。実OS入力/GPU表示/主観操作感の最終合否はユーザー手動待ち。**R5は正式不合格のまま。R6以降へ進まない。**
