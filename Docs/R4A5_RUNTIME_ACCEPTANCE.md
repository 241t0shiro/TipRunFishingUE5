# R4A-5 Runtime Integration / Rod Ergonomics / Final Acceptance

2026-10-04。対象はA5のみ。A1〜A4の既存差分を保持。R4はユーザー手動PIEで正式不合格であり、この自動成功だけで正式合格へ変更しない。R5以降/H/M11未着手。

## 正本と今回の実装

永続正本は固定Station-local RodRootLocal、BaseRodDirectionLocal、LengthM。保存PrototypeのLengthM=2mを保持し、最終Tip=Root＋正規化FinalDirection×Lengthで決定する。Mouseの非ゼロCommandだけが発行時の実Camera観測、現在Base Tipの投影＋delta、Ray/Sphereを経てLocal方向を変更する。Camera-onlyではRoot/BaseDirection/BaseTip/Lengthは完全不変。ScreenControlや前FrameのWorld回転は永続正本に戻さない。

既存Rod DataAssetにopt-inのFTRRodEnvelopeParametersを追加。保存 `/Game/TipRun/Prototype/M105/Data/DA_TR_M105Rod_Prototype` にUE SavePackageで以下を明示保存した。新規項目以外の値は保持。

| 項目 | Prototype候補値 |
|---|---|
| bEnabled | true（未移行の旧資産はfalse） |
| Station outward yaw | -40〜+40度 |
| Station pitch | -10〜+35度 |
| Physical Length | 既存2m、変更なし |
| Fishing FOV | 既存80度、変更なし |

有限数/順序/前方のyaw範囲/過大pitchをDataAsset検証で拒否。これはゲーム近似の技術調整値であり、製品値/実測人間工学値ではない。異なるEnvelope、Grip、Eye/FOVに調整した場合は同じ受入を再実行する。

候補Directionと最も近いStation球面境界を求める。Yawを範囲へ制限し、そのYaw上で候補との内積が最大になるPitchを求め、設定Pitchと既存海面clearanceを満たす範囲へ制限する。Yaw/Pitchは制約計算用の一時Scalarで、累積回転の正本ではない。Prototype範囲の最小舷外成分は概ね0.627、後方/船内へ大きく向かない。Shakuriの既存時間profileと係数は保持し、Temporary Finalにも同じEnvelopeを適用する。上端では振上げが制限されるがBaseを書き換えず終了後に正確に戻る。

Mouse Targetは現在Baseを再投影した点から作る。Envelope外は有効方向へ連続制限し、無制限Target/deltaを蓄積しない。実Viewport端ではそのCommandのtargetを制限するが、恒久Screen Rectangleを姿勢正本にしない。理由はEnvelopeYaw / EnvelopePitch / EnvelopeYawPitch / ViewportBoundary、profileの制限はAction.接頭辞。既存NoSphereIntersection/BehindCamera/SafetyLimit等の拒否診断も維持する。

**境界に沿う移動はあり得る。** 追加100個の外向き入力で方向差最大0.0734215335（約4.2度）が観測された。これは受け取った各入力の最寄り球面投影による接線方向の移動であり、停止後に進む予約Targetではない。停止後1秒はBase完全一致、最初の反対入力で実Camera上の内向きへ移動することを両舷×3fpsで検査する。境界では純X/Yの軸一致より安全制約を優先し、交差軸差を内側の値と混ぜない。

## Presentationと詳細診断

Visual Rodは同じSnapshotのFinal Root/Tipだけで生成、Line Startも同じFinal Tip。Cubeの長さ方向ScaleはRoot→Tipの固定2m、径は既存.045で、Camera補正/長さ偽装/独自補間なし。ReelはGripからRod方向へ.15m、鉛直-.09mの既存表示位置と固定Scaleを保持。Rod/ReelはObserver Rootの子でありCameraの子ではない。

広域試験で通常のSetWorldLocationAndRotation経路の近似回転更新に最大約3.35e-7mの端点差を検出した。表示専用ComponentへSetWorldLocationAndRotationNoPhysicsを使用し、近似による旧回転保持を避けた。Simulationへ逆流させず、厳しい既存端点許容値を緩めていない。m→cmは表示境界で1回だけ。

Insert詳細の既存Local Root/Base/Final/Length、Active Camera/Projection/ViewRect、Projected Tip/Length、LastMouse各段階、PendingJerk/Retrieve/Action、Result/Failure、Lock/OnboardにEnvelopeの有効/直近制限/理由を追加。診断行を「Rod / Input / Action 最終診断（読取専用）」へ整理。通常HUD/キー/レイアウトは保持。最後のCommandのtargetと現在Frameの投影値は区別する。

## 保存Runtimeでの受入構成

保存Level `/Game/TipRun/Prototype/M105/L_TR_M105_Prototype` と実BP GameMode、Session/Input/Station/Rod設定をロード。Game World、LocalPlayer/SceneViewport、実Controller、Session Queue、固定Simulation、PlayerCameraManager、PrototypeViewActor、実Rod/Reel/Line Meshを通常World Tickで通す。実Mesh BoundingBoxの両端をComponent TransformでWorldへ変換し、そのFrameのLocalPlayer Active Projection/実FOV/ViewRectへ投影する。Synthetic Solverや手動ApplyObservationをA5受入の代用にしない。

Sweepは両舷×4Heading×5視線×2解像度×9姿勢=720測定。姿勢はcenter/左右/上下/4斜め、各方向の境界まで入力。Boat Headingは実Steeringで0/90/180/270度を狙い、停止誤差は最大約0.482度。正確な数学的0/90/180/270度は既存Geometry回帰でも確認。Cameraも実WASD経路のため停止Frameを含む実値をCSVへ記録する。

実Camera視線と実投影到達域（両舷/4Headingの範囲。pixel原点は左上）:

| 実Yaw/Pitch度 | 1080p X | 1080p Y | 1440p X | 1440p Y |
|---|---|---|---|---|
| -0.750 / -20.583（中央代表） | 234.866〜1729.838 | 約0〜569.257 | 313.155〜2306.451 | 約0〜759.010 |
| -15 / -20（左代表） | 608.315〜1928.556 | 約0〜640.581 | 811.087〜2571.408 | 約0〜854.107 |
| +15 / -20.583（右代表） | -8.781〜1312.790 | 約0〜629.129 | -11.708〜1750.386 | 約0〜838.839 |
| 0 / -10.083（上代表） | 256.050〜1663.950 | 約0〜779.144 | 341.400〜2218.600 | 約0〜1038.858 |
| -0.750 / -29.917（下代表） | 224.553〜1740.350 | -1.263〜381.888 | 299.404〜2320.467 | -1.684〜509.183 |

各視線での左右域は約68.76〜78.95%、上下域は約35.48〜72.14%。Port/Starboardは同じ比率、1440pはpixelが4/3で感度/相対域は同じ。割合を満たすための物理改変はしていない。Station境界とViewport端の最寄り投影により一部Tipが約12px外へ出るケースは明記する。Camera独立性により大きく見回すとRodが画面外になることも許容する。全Camera clamp域で常時可視とは報告しない。

| 投影長の操作列 | A3/A4 | A5（1080p換算） |
|---|---|---|
| A1の同じ連続入力＋Camera Look | 10.9937〜1106.74px、比100.67 | 516.600〜993.199px、最大比1.92257 |
| A3広域入力列 | 115.979〜1343.592px、比11.5848 | 446.990〜1195.899px、比2.67545 |
| A5の720姿勢/代表視線Sweep | 未実施 | 435.991〜1382.100px、比3.17002 |

新Sweepの実1440p長は581.321〜1842.800px。720姿勢のPhysical Lengthは2m。Eye→Grip rayとRod方向の最小分離角29.8056度、旧約11pxの消える姿勢を除外。受入試験の「代表視線の投影長が画面高10%以上」「分離角25度超」は今回のPrototype技術候補であり製品品質値ではない。長さ一定の3D透視変化をなくす試みではなく、主観的な自然さは手動6項目へ残す。

## Automation・Action安全性

最終採用は **93 Success / 0 Fail / notRun0**（A5新規5＋関連88）。採用元を`Saved/Automation/R4A5AcceptedResults.json`に試験ごとに保存。92件の実行は`R4A5Acceptance/index.json`、追加の境界固定Tickリプレイは`R4A5EnvelopeReplay/index.json`、反対入力の実画面方向まで強化した境界試験は`R4A5BoundaryFinal/index.json`。単一実行を93件と偽っていない。試験内errors0、warnings1（Sweep中の外部Google generate_204 HTTPタイムアウト）。この警告はゲーム処理/今回変更由来ではなく、無関係な設定で隠していない。

| 項目 | 採用結果 |
|---|---|
| A5新規5件 | SavedEnvelope / ErgonomicSweep / BoundaryNoAccumulation / ActionAndQuickAtEnvelope / EnvelopeFixedTickReplay成功 |
| Camera independence | Camera-onlyでRoot/BaseDirection/BaseTip/Length完全不変 |
| 実Visual Pure X/Y | 内側の交差軸差最大0.0000610352px（既存許容0.05px） |
| Stress | 各軸2000往復、累積投影差0px、Local差最大約5.67e-15、Length固定、NaN/Infなし |
| Visual/Snapshot | Sweep端点最大約5.03e-15m、Geometry単位回帰成功、Lineと同一Tip |
| Envelope固定Tick | 実Controllerから120Commandを同じ固定Tick列へ予約。両舷×30/60/120fpsの最終Direction/受付結果/Tick順列一致 |
| 同一正常42条件 | 42通常Retrieved / Stay継続0 / Technical Abort0 |
| 舷変更後の既知条件 | 両舷変更/3fpsの6条件成功、Technical Abort0 |
| Intentional Abort/Recovery | 意図的NaN→既存安全停止→Aborted保持→実NでReady/Onboard/Unlock。次Cast、旧Cast/ModeEpoch/登録世代拒否成功 |
| Pending操作 | Retrieve保留/早期Release、Fall保留、Base復帰Tickで開始、Pause/Focus解除、決定的Action順成功 |
| Quick | 通常/進行中profile/Envelope境界でRoot/Base/Length/Cameraを保持。Ready/Unlock、一度の完了、FinalはBaseへ自然復帰 |
| UI/Side/Input | R3取消/再選択、R1/R2モード、D右1入力1動作/左保持、M09、Fパネル遮断、Pause/Focus回帰成功 |

A4の14Stayは不合格へ変更していない。今回はEnvelopeで下向きの極端姿勢を除外した結果、同じ42入力列が全Retrievedとなった。回収量/船上条件/安全閾値を調整して結果を強制したものではない。既存回復試験は変わらずAbortedを保持し成功へ偽装しない。

回帰フィルタ: `TipRun.R4A1.+TipRun.R4A2.+TipRun.R4A3.+TipRun.R4A4.+TipRun.M105R3.+TipRun.M105R2.+TipRun.M105R1.+TipRun.M105R4.+TipRun.M105D.+TipRun.M105E.+TipRun.M105F.+TipRun.M105G.+TipRun.M09.+TipRun.M04.`。無関係な全Egi/H統合を開始していない。旧Screen単体fixtureはEnvelope=falseを明示して旧SolverのUnit回帰として維持し、A5受入は保存資産trueのRuntimeだけで行う。境界の交差軸を内側の軸精度失敗へ混ぜず、実到達域を測定する方針へ期待を更新した。

途中失敗を保持: 初期Buildは私有Reelの直接参照/TPair初期化を修正。初回試験は表示端点精度と「外向き入力で境界の接線移動すら禁止する」という過強な試験期待を検出。後者は入力停止後不変/反対入力即内向きという本来の契約へ分けた。中間版Quaternion変更では端点差を解消できず、NoPhysics表示更新で解消した。端点許容/安全閾値は緩めていない。`R4A5First`、`R4A5Final`など中間ログは削除していない。

## Build・変更範囲

UE5.8.2 / MSVC14.51.36257 / Windows SDK10.0.22621.0。UHTのEnvelope構造体/UPROPERTY/診断Snapshot追加生成を生成.cppで確認（10/04 18:01生成）。変更Runtime実Compile/LinkはBuild3/5、最終試験追加はBuild6/7の実Compile/Link、Development Editor Win64成功。Live Coding/up-to-dateだけではない。git diff --check成功。

残存環境通知は既存MSVC推奨版14.50.35717との差、IncludeOrder Unreal5_6。起動時の既存Python screen_control名重複/Editor layout警告もあり、A5由来ではない。試験内のHTTP警告1件と区別する。

今回追加変更10 Source:
- Public/Data/TRRodTuningDataAsset.h、Private/Data/TRRodTuningDataAsset.cpp（Envelope/検証）。
- Public/Data/TRSnapshots.h（有効/直近制限理由）。
- Public/Fishing/TRRodControlComponent.h、Private/Fishing/TRRodControlComponent.cpp（Station方向制約）。
- Private/Game/TRPrototypeViewActor.cpp（表示端点の更新精度）。
- Private/Game/TRPlayerController.cpp、Private/UI/TRPrototypePresentation.cpp（Insert診断のみ）。
- Private/Tests/TRRuntimeRodIntegrationTests.cpp、TRRodScreenTests.cpp（受入/旧Unit互換）。

ContentはDA_TR_M105Rod_Prototype.uassetのEnvelopeだけ明示追加。保存Input/StationのSHA256は作業開始時と一致。Config/Map/Environment/Boat/Egi/FOV/CameraAnchor/Grip/Sensitivity/Shakuri/Reel量/安全閾値に今回の変更なし。中断前のA2〜A4差分を今回作成と混同しない。文書は本書＋AGENTS/GAME_DESIGN/FISHING_SYSTEM/UI_SPEC/ROADMAP。ログ/生成物/CSV/JSONはSaved/Intermediate内の証跡として扱いソースへ追加しない。

## 手動PIE最大6項目（正式受入の残作業）

Editorを再起動し保存 `/Game/TipRun/Prototype/M105/L_TR_M105_Prototype` を開く。NavigationのEnter→A左舷またはD右舷→EnterでFishing。Insertは詳細。GUI PIEは自動起動しない。

1. 両舷でMouse左右/上下/斜め。入力方向と見えるTipの方向が一致し、内側で不要な円運動がないこと。境界の理由表示と、反対Mouseで即座に内側へ戻る操作感を確認する。
2. Mouseを止めWASDで視線変更。Local Root/BaseDirection/BaseTip/Lengthが不変でRodは船に対して動かないこと。その後Mouseが現在画面基準で動くこと。
3. 1080p/1440pで両舷の到達域、端の見かけ長さ、Grip/Reel/Gunwale/Sea構図を評価。物理2mは一定、極端に消える短縮/伸縮感がなく、範囲が窮屈でないこと。最寄り境界でのわずかな画面端外と接線移動も評価する。
4. Enter投入→即右1クリック→左保持/Release、F再Fall。Baseへ自然復帰後に保留操作が進み、Technical Abort/永久Lockがないこと。異常が出た場合はInsertを記録しNで安全復旧（Abortを成功へ変えない）。
5. 中央以外のBaseでQ。前/中/ReadyでGrip/Length/Base/Cameraが保持され、profile実行中ならFinalだけ自然復帰すること。Ready/Unlockから装備変更/再Deployできること。
6. Tab開閉、C舷変更/Backspace取消、Pause/Focus往復。入力遮断と復帰、保持入力の残留/Mouse jump/表示重複がなく、Navigation/Fishingの入力が混在しないこと。

上記6項目で重大な操作違和感がないというユーザー手動合格が得られた場合のみR4を正式合格にできる。現在はA5自動受入成功・手動未実施で、R4正式不合格の状態を維持する。R5/H/M11への進行は別途明示依頼が必要。
