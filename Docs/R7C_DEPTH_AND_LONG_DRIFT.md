# R7C Depth Scaling Validation / Long Drift Presentation Robustness

2026-10-13。対象はR7Cのみ。R7Bは自動受入成功だが、ユーザー最終PIEの深度未確認・長距離青画面で正式完了保留。R8以降、Range scoring/BITE、H/M11は未着手。

## A：係数を固定した水深監査

保存Map `/Game/TipRun/Prototype/M105/L_TR_M105_Prototype`、GameMode、Session、LocalPlayer、Controller、Active Camera、Boat/Rod/Egi/Line/R6 Reel/Presentationへ通常World Tickを通す既存Runtime fixtureを使用。保存Session/OceanのTransient複製でFlatDepthMだけ10/20/30/40/50へ変更する。Boat/Fishing/Rod/Inputは保存参照を共有し、風(0,2)、表層潮(.360111111,0)、深度Profileの絶対キー0/15/30mと(.360111111/.255055556/.15,0)を維持。Profileを水深へ比例伸縮しない。30mより深い潮は既存端値.15の延長、浅場は当地水深に評価深度を制限する。

全CastはEnter→自然着底→右1Action→最初のStayから1200固定Tick=20秒。Bottomの角度は竿先–エギのH/Vから `atan2(abs(V),H)` をdegreeへ変換、0〜90°。Boat中心HとRodTip基準Hを区別する。海面角と既存鉛直からの角度は別値。TF→Stayは同Tickで、独立TF積分行は捏造しない。

### 35g / シンカーなし / Port / Heading0

| 水深 m | Bottom s | Bottom Boat H m | Bottom Rod H m | Bottom V m | Bottom ° | Stay開始 ° | Stay10 ° | Stay20 ° | Depth差20 m | Boat速 m/s | Egi水平速 m/s | 相対速 m/s |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 10.000000 | 14.800000 | 11.719014 | 8.680010 | 11.508521 | 52.975503 | 52.172572 | 48.875327 | 47.651564 | -0.189297 | 0.637856 | 0.619045 | 0.019029 |
| 20.000000 | 29.100000 | 20.706847 | 17.652986 | 21.508521 | 50.622773 | 50.261624 | 49.091253 | 48.501533 | -0.078041 | 0.637856 | 0.607434 | 0.040070 |
| 30.000000 | 43.400000 | 29.515736 | 26.458506 | 31.508521 | 49.978966 | 49.747883 | 49.108657 | 48.760482 | -0.001949 | 0.637856 | 0.601679 | 0.067730 |
| 40.000000 | 57.700000 | 38.291722 | 35.238688 | 41.508521 | 49.670384 | 49.497653 | 49.023567 | 48.757119 | -0.007500 | 0.637856 | 0.603899 | 0.062094 |
| 50.000000 | 72.016667 | 47.125320 | 44.077803 | 51.508521 | 49.445136 | 49.307533 | 48.933542 | 48.718919 | -0.009438 | 0.637856 | 0.606390 | 0.054138 |

**浅場ほど小角・深場ほど大角というRuntimeの期待順序は成立しなかった。** 同じHならV増加で角度は大きくなるが、この実験では水深と落下時間が増えるためHも増える。竿先H/Vは10mで.7542、30mで.8397、50mで.8557へ増加し、海面角は52.98→49.98→49.45°へ小さくなる。

原因はBoat応答の立上り/竿先の約1.509m海面上offsetが浅場で相対的に大きいこと、深場ほど落下時間が長く相対移動が育つこと、絶対深度Profileにより受ける潮が変わること。10mの底潮は約.290074m/s、20mは.220037、30m以深は.15。背景の問題とは分離し、水深→角度補正・速度逆算・特殊端点移動・TargetAngle制御を追加しない。

35gは今回の同一環境・20秒窓では全5水深でDepth差絶対値.2m以内だった。全条件/長時間で最適と保証せず、水深別に別係数を作って結果を悪くすることもしない。異方向/不足/過多潮のR7B非Good条件は維持。重量による落下・レンジ差が残るため製品最適重量の確定には使わない。

### 10/30/50mの重量代表比較

| 水深 m | エギ | Bottom s | Bottom ° | Stay20 ° | Depth差20 m |
| --- | --- | --- | --- | --- | --- |
| 10 | 3 | 17.183333 | 48.390039 | 42.265505 | -0.562329 |
| 10 | 3_5 | 14.800000 | 52.975503 | 47.651564 | -0.189297 |
| 10 | 4 | 13.016667 | 56.817707 | 52.683364 | 0.144487 |
| 30 | 3 | 50.550000 | 45.493258 | 43.849515 | -0.396011 |
| 30 | 3_5 | 43.400000 | 49.978966 | 48.760482 | -0.001949 |
| 30 | 4 | 38.033333 | 53.785960 | 53.189738 | 0.388763 |
| 50 | 3 | 83.966667 | 44.973184 | 43.967982 | -0.422951 |
| 50 | 3_5 | 72.016667 | 49.445136 | 48.718919 | -0.009438 |
| 50 | 4 | 63.066667 | 53.238652 | 52.929559 | 0.410030 |

3水深すべてで30g上昇 < 35g相対安定 < 40g沈下の順序、重いほど早い着底を維持。海底貫通/非有限/異常Warp/Technical Abortなし。保存Asset/係数は再調整していない。生データは `Saved/Automation/R7CDepthWeights.csv`、全Tick CSV/eventsは `R7BObservations/R7C_Depth_*`。

## B：修正前の長距離遮蔽再現

自然Fishing Driftを210秒、両舷で進めた。位置setter/速度注入/World recenterなし。実Gunwale MeshのTransformから長手21×高さ3の63点を作り、同FrameのActive Camera/FOV/ViewRectへ投影。近距離の長いガンネルはBox cornerが全て画面外でも面が画面内へ入るので、角点だけに限定しない。画面内点へのCamera rayが実Background CubeのローカルBoxへ交差するかを検査する。

修正前は `TipRun.R7C.Runtime.LongDriftReproduction` が意図したFAIL。Port155 Frame / Starboard154 Frameで全画面内Gunwale点へのrayが背景Boxと交差した（GPU pixelを判定したFrame数ではない）。DepthAndWeightAuditはPASS。

| 舷 | Readyから秒 | 初発移動距離 m | Boat位置 m | Active Camera m | 選択Gunwale中心 m |
| --- | --- | --- | --- | --- | --- |
| Port | 162.483333 | 102.642468 | (23.611,99.905,.5) | (24.211,99.505,2.1) | (23.611,98.905,1.55) |
| Starboard | 160.233333 | 101.207292 | (23.281,98.508,.5) | (23.881,98.908,2.1) | (23.281,99.508,1.55) |

Heading0、Boat V=(.146984,.620690,0)、Camera Pitch−20/YawPort−90・Starboard90。Station Cameraと実Cameraは一致、Boat MeshとSimulation Boatも一致。RootはPort(24.211,98.905,1.6)/Starboard(23.881,99.508,1.6)へ正しく追従。

BackgroundはWorld(0,100,0)mに固定、cube scale(400,1,400)、World bounds X/Z±200m・Y99.5〜100.5m。船/目線がこの厚さ1mの壁へ入る/壁を挟むことで、HUD/Rodの一部を残してガンネル/船形状が遮蔽される。Observer Actorは原点固定だがBoat/Gunwale Componentは各World Transformを更新しており、固定された親からの離脱が原因ではない。

海底は中心(0,0,−30.2)m、XY±100mの有限表示。海面グリッドは±50m、枠は±12×8mで原点固定。長距離ではそれらも表示範囲外となる独立の問題。Simulation Ocean boundsはXY±1000mで有効なまま。無効環境/Camera fallback、Boat追従欠落、Gunwale固定、精度問題、Near/Far clippingによる消失はこの再現の根本原因として支持されない。実BoundsはTransformへ一致しておりculling設定を緩める修正はしない。

修正前Transform/Bounds/Frame/Tickの全文は `Saved/Automation/R7C/Before/R7C_Repro_Port.txt` / `R7C_Repro_Starboard.txt`。これらは修正後に上書きせず保持。

## 最小の表示修正

- Backgroundだけを実Active Cameraの正面100m、視線と垂直な面へ移す。Cubeのサイズ/Material、Camera位置/Look/FOV、Rod poseは変更しない。船の近傍を固定World壁が通過しない。
- 水面グリッド/枠と海底の表示窓を船周辺へ配置。グリッド中心はWorldの5m整数セルへsnapし、線は世界軸/5m座標を保持する。船と完全同速度で滑るGridにはしない。
- World原点ブイは固定のまま。船/Rod/Line/Egiの端点表示は既存Snapshotからのみ生成する。Ocean Simulation、Boat位置/Heading、Station/Camera契約へ表示から逆流しない。World Origin RebaseやCameraの再Attachは追加しない。

実装箇所: `Private/Game/TRPrototypeViewActor.cpp::ApplyObservation` の旧原点固定Background/Seabed/Grid配置。Camera/Rod/船体本体/Gunwaleの式は維持。

## Runtime Stress / 受入

両舷×Heading0/90/180/270要求、50/100/250/500mの32地点。Headingは実操舵で要求から2°以内へ到達する。保存Ocean/Boat/Fishing/Rod/Inputで投入→自然Bottom→右1回→Stayの活動Castを長距離ドリフトさせる。World frame .1秒で既存1/60秒の6固定Stepを通す（catchup8以内）。位置/速度setter、時間スキップ、Safety緩和は使わない。

各地点でGunwaleの画面内・非遮蔽、Boat geometry表示、Rodの実Mesh端点とSnapshot一致、固定RootLocal/2m、Station/Camera誤差、Boat visual誤差、背景距離100m/Camera内部侵入なし、海面/海底の船周辺coverage、実Mouse/右入力を検査。Quick→Ready/Unlock→Navigation→反対舷Fishing→Deploy/Quick→Navigation、遠地点で再Steering→Fishingを確認しComponent数の不変性も検査する。

最終32地点すべて成功。Camera–Station最大誤差5.8592855e-14m、Boat visual最大誤差5.68434189e-14m。各遠地点の右入力32件すべてがCompletedCountを正確に1だけ増加。両舷・4Headingで活動CastのTechnical Abort0、Quick/Mode往復/遠地点再Steering/Input正常、Component重複0。サマリーは `Saved/Automation/R7CStressSummary.csv`、全Transform/Bounds/入力カウンタは `R7CStress.txt`。NullRHIのため『投影可能で背景に遮られない描画候補』を判定しており、GPU pixelや主観構図を確認済みとしない。

## Insert診断

既存R7AのWaterDepth/H/V/海面角/相対Driftを維持。読取専用FTRWorldPresentationObservationを追加し、Frame/Boat Tick、Boat World位置、Camera–Station誤差（Fishing時）、Visual Boat誤差、Water中心/Bounds、海底coverage/Bounds、Backdrop距離/Camera内部侵入を既存Insert RuntimeDiagnosticsへ表示。通常HUD/操作キー/物理Snapshotは変更しない。

## 手動PIE（最大3項目）

1. Editor再起動→保存Prototype→Heading0で両舷Fishing。35g投入→Bottom→右1回→Stay20秒、約50°/深度安定を確認。Mouse/WASD、右連打、左Release/F/Qも確認する。
2. Navigationでポイントを100m/250m/500m程度移動し、各地点でFishingへ入る。両舷とHeading違いでBoat/Gunwale/Rod/海底/海面の構図、Insertの位置/誤差/Boundsを確認。自然DriftでWorld Y≈100mを越える旧再現も、青い壁に覆われないことを確認する。
3. 遠地点でQuick→Navigation→別舷Fishingを往復し、Tab/Pause/Focusから復帰。表示重複/操作停止がなく、30/35/40gを変更して次投へ反映できること。

10〜50mの深度比較は上記Runtime監査表を使用し、手動確認のために保存Product Assetの深度/係数を書き換えることは要求しない。

R7Cの自動受入成功後も、修正後GPU表示のユーザー確認まではR7B正式完了を保留する。深度の逆転結果は未確認として隠さず明示し、製品の深度別角度/最適重量を独断確定しない。R8/H/M11へ自動進行しない。

## 最終Build / 回帰 / 保全

最終採用**124件すべてSuccess（新R7C3＋関連121）、FAIL/errors/notRun0**。単一124件一括成功ではなく、R7CFinalの非R7C121件とR7CVerifiedの最終R7C3件を試験IDで採用。Saved/Automation/R7CAcceptedResults.jsonに採用元を保存。

30mのR7B重量・両舷・悪条件・保存Profile・固定更新Replay4試験を回帰。35gの着底角約49.979°、20秒Depth差Port−.001949m/Starboard−.153868m、30g rise/40g sink/90g sinkを保持。R4A1〜A5、旧R3/R4/D/E/F/G、R5/5A/5B/5C、R6、R1/R2、M04/M09も回帰。正常42は全42正常Result / aborted0 / active0、Intentional NaN→Aborted→N→Readyも成功。無関係な全Egi/B全式/R9/H統合は今回実施していない。

UE5.8.2 / MSVC14.51.36257 / Windows SDK10.0.22621.0。Internal UHT（WarningsAsErrors）成功、反射契約追加なしのため生成書出し0。修正ProductionのTRPrototypeViewActor.cpp/TRRuntimeObservation.cppと関連module、最終Runtime試験の実C++/Link・Development Editor Win64成功。最終fixtureはR7CCastFixtureBuild、ProductionはR7CFixBuild2。Live Coding/up-to-dateだけではない。

途中履歴: ObservationBuildはEditorがDLLを保持してLink失敗、ユーザー終了後Build2成功。FixBuildはNoPhysicsの回転引数をQuatとしたC++エラー、UEヘッダ通りRotatorへ変換してBuild2成功。R7CBeforeはDepth PASS/LongDrift FAIL（意図した修正前検出）。After/Finalは表示・Mode・物理の検証に成功したがStressの最初のMouse判定が8WorldでFAIL。原因はModeだけでなくDeployのCastId変更でReleaseInput→bDiscardNextRodDeltaが再設定される既存契約。微小.01はEnhanced Input閾値を越えず、Deploy前のprimeも後続境界で破棄された。最終はDeploy後の実デバイス20deltaでcaptureをprimeし、private latchを変更せず、各遠地点のMouseによるBase変化/右1回の新Action完了を検査した。期待やInput安全契約を緩めて通したものではない。途中ログ/FAILを保持。

Config/Content全20ファイルはR7C開始時SHA256と一致。既存Boat/EgiSpatial/Sessionの追跡差分も開始時と同一でR7Aの変更を保持。R7BのWind/Profile/Kw/Kc/Drag/Inertia/Stay倍率、全Rod/Camera/Mouse/Reel/安全閾値、Input/Map/Materialは変更しない。git diff --check成功。開始時のR7A/R7B未コミット変更と、今回の差分を混同しない。

今回Source6ファイル:

- Public/Data/TRRuntimeObservation.h（表示専用診断）
- Private/Game/TRRuntimeObservation.cpp（Insert記述）
- Private/Game/TRPrototypeViewActor.cpp（表示窓/背景と診断採取）
- Private/Tests/TRRuntimeRodIntegrationTests.cpp（new inl接続）
- Private/Tests/TRDriftRangeTuningRuntimeTests.inl（監査用Configを渡す任意引数のみ。R7B受入期待値は維持）
- Private/Tests/TRDepthLongDriftRuntimeTests.inl（新規3試験）

文書9ファイル: AGENTS.md、GAME_DESIGN、OCEAN_SYSTEM、BOAT_SYSTEM、FISHING_SYSTEM、UI_SPEC、ROADMAP、R7B_DRIFT_RANGE_TUNING（新手動結果追記）、本記録。Saved/Intermediate/log/測定CSVはソースへ追加しない。

R7Cの実装・自動受入は合格。ただしこのheadless診断はGPUの青画面そのものを撮影したものではない。修正後の船べり構図/GPU表示をユーザーが確認するまでR7B正式完了は保留する。

採用試験内警告は13件。すべて既存Engine外部HTTP generate_204の3秒timeout。既存推奨MSVCとの差/IncludeOrder通知、起動時Python名衝突・EditorLayout・非対象SDK通知を保持。新C++警告なし。
