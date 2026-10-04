# R4A-3 Mouse Delta / Runtime Presentation Integration

2026-10-04。R4A-2から継続。今回は入力Delta変換、診断、保存Prototypeを使うRuntime受入だけを実装した。R4正式不合格は維持。R4A-4のLock修正、R5以降/H/M11は未着手。Config/Content、Grip、Camera anchor/FOV、感度値、Shakuri/Reel係数、Egi物理/安全閾値/Abortの意味、Environmentは変更していない。

## 入力・位置正本

永続正本は固定Station-local RootLocal、正規化BaseDirectionLocal、凍結DataAsset LengthM。BaseTip = Root + Direction×Length。Station原点はPlayer anchor、基底はBoat Heading＋選択舷Facing。Mouseなしの固定Tickでは永続値を変更しない。Boat DriftはWorld変換だけへ作用する。

実ControllerのSubmitMouseDeltaは入力時のCameraManager cache POVとLocalPlayer ProjectionをFTRRodAimObservationへ凍結する。

| データ | 所有/契約 |
|---|---|
| CameraWorldM / CameraRotation / FOVDeg | 入力発行時の実POV、m/deg |
| ViewRect | 実Constrained ViewRect、pixel |
| ProjectionScale / ProjectionOffset | 実Perspective Projection Matrixの横/縦倍率・offset、無次元。bHasProjectionを伴う |
| BoatWorldM / BoatHeadingRad | Camera観測と同FrameのBoat基準、m/rad |
| Side / CameraFrame | 発行時の確定釣り舷と観測frame |
| TargetTick / Sequence / ExpectedCastId / ExpectedModeEpoch | FTRFishingCommandの既存順序/寿命識別 |
| Registration generation | 配送先FTRActorSimId、既存Queueの登録世代 |

非Viewportの旧Unit fixtureだけはFOVと明示Rectまたは16:9論理射影を使う。Runtime受入は必ず保存Mapの実Viewport/Projection係数を使い、これを代用にしない。

固定消費の順序:

1. Command観測のBoat/Station基底を使い、現在Base Tipと観測Cameraを同一Station-local座標へ変換する。後から動いた最新Cameraを取得しない。
2. Camera-local Forward/Right/Upへ分解してPerspective係数で現在のNDCを求める。Current pixelも保存する。
3. Mouse deltaを既存Screen.SensitivityとMaxMouseDelta、軸別固定Tick予算で変換する。感度は横半画面単位。Yは縦Projection倍率比を用い、pixelでは上向きが負。X/Yを独立に扱い、実解像度からpixelへ変換する。
4. Targetから同じ観測のRayを逆生成し、Root中心・Length半径の球面の前方/遠い交点を求める。Station外向き半球と従来の海面clearanceを再検証する。
5. 有効方向だけを正規化してBaseDirectionLocalへ保存する。無効なら直前Baseを保持し、InvalidProjection/BehindCamera/NoSphereIntersection/ScreenBoundary/SafetyLimitを記録する。許容外を近傍点へ飛ばす処理はMouse経路に使用しない。
6. 複数CommandはSequence順。各々の異なるCameraを保持し、直前処理のBaseを次の入力の開始点にする。

旧X±0.28矩形はMouse域の正本から撤去。初期互換設定と既存Action用設定は保持する。入力域は実画面内、球面交差、Station外向き、海面安全条件。画面端の数値丸め1e-12 NDCだけを丸め、X端でYまで拒否される問題を防いだ。これは最大でも約1e-9pxで、Egi安全閾値を緩めた変更ではない。

## Action・Presentation

MouseはRoot/Length/Action時刻/回収量を変更しない。既存Shakuri Up/Returnと凍結入力のStation-local投影平面による一時Finalを保持する。派生Screen読取値を新しい入力矩形へ戻さず、現在Baseの実投影から一時Liftを適用する。振幅、Duration、Reel Pulseは旧値のまま。終了時にFinal方向/TipはBaseへ完全一致する。

Rod meshは同一SnapshotのFinal World Root/Tipから生成。実Mesh BoundingBoxのX長軸両端をComponent Transformで取得し検証する。Camera attachment/補正/表示側姿勢の再計算はない。Line Mesh始点も同じFinal Tip。設定Stationの無効Snapshotでは表示を隠し、Camera依存Fallbackは作らない。m→cm変換はPresentation境界で一度。

Quick前/中/ReadyはBase/Root/Lengthを維持する。Right holdはRepeatしない。Left hold/Release、既存固定QueueとMode/Cast寿命契約を保持。R5のSequence/0.8m Reelへ変更していない。

## Runtime受入構成と結果

保存`/Game/TipRun/Prototype/M105/L_TR_M105_Prototype`と保存GameMode/Session/各DataAssetをロードする独立Game World。実GameInstance/LocalPlayer/Viewport/Controller/CameraManager/Session/Station/Rod/PrototypeViewActor/Meshを使用。通常World Tick→固定Queue/Simulation→Camera→PostUpdateWork Mesh更新→Active Camera投影を通す。手動ApplyObservation、Solver直呼び、合成Cameraだけの試験は受入にしない。

代表Headingは実Navigation Steeringで到達し、確定したStationへ移行する。実値は0、89.763313、180.481705、270.244904度（2度以内の到達基準）。正確な0/90/180/270度は既存座標Unitも実施する。両舷×4Heading×1080p/1440pのRuntime検証を行った。

| 項目 | 最終結果 |
|---|---|
| Mouse Raw経路 | Viewport InputAxis→Enhanced Input→Controller→Queue→固定消費。従来の復帰直後の最初の非ゼロsample破棄を確認してから軸試験。設定変更なし |
| Pure X/Y・diagonal | 実Mesh投影最大交差軸差0.00006103515625px、許容0.05px。符号も両舷/方位で一致 |
| Camera-only | 任意Mouse pose設定後のWASDでRoot/BaseDirection/BaseTipは完全不変。その後のMouse右/上が新しい視界の右/上へ移動、最初の入力で旧目標へ戻るJumpなし |
| 解像度 | 実1920×1080/2560×1440。相対Raw Mouseの画面幅比が1e-7以内で一致 |
| 2000往復 | 両舷×4Heading×2解像度、各軸2000往復（各軸4000Command）。交差軸/往復投影差0px、Local方向差最大5.66635278185e-15、NaN/Inf/物理長driftなし |
| 水平Reach | 約1920px/2560px、最小幅比0.999999961。旧537.6px（28%）を超えた。viewport float丸めで端が約0.0003px外へ見える量は許容投影誤差内 |
| 垂直Reach | 両舷/方位でX端でもYが動く。1080pの端点Y約12..1068px、1440p約16..1424px（各端の余りは離散入力幅）。全域Cartesian矩形を保証する仕様ではなく、各Rayの安全判定による |
| 同Tick順序 | Sequence4→5、独立Camera観測（Yaw -90→-75度）の2Commandを同Tickで一度ずつ消費。30/60/120fpsで最終Local方向が1e-9以内で一致 |
| Camera観測凍結 | 既存A2 future Mouse後にCameraを変える試験PASS。消費時のCameraへ置換しない |
| 実長/表示 | 凍結2m。World/Visualとも1e-8m許容内。Snapshot vs Visual最大誤差約2.22e-15m。実Line Mesh始点もFinal Tipと1e-7m以内 |
| Action/UI | Mouse変更済みBaseで実右holdは1 Shakuri、Base固定とFinal復帰、左hold/Release、Q Ready/Unlock成功。Pause/Focus/UIのController guardと再入力を検証 |

NullRHIのRuntime試験であり、OSの物理Mouse capture、Slate/GPU描画、主観操作感は未検証。自動GUI PIEは実施していない。A3新規4試験は実装契約の受入成功でありR4全体の正式合格ではない。

## 投影長の測定・残る視覚問題

物理長/FOV/Gripを変えて見かけを揃えていない。域拡張により透視短縮が強いposeも許容され、改善完了とは報告しない。

| 操作列 | 1080p相当Min / Max / Ratio(px) |
|---|---|
| A1旧域Mouse | 508.139 / 731.964 / 1.44048 |
| A1旧域Camera込み | 508.139 / 923.783 / 1.81797（30/60fps） |
| A2旧域Camera込み | 507.015〜507.056 / 731.964 / 約1.4436 |
| A3小Deltaのcenter/左右/上下/diagonal＋Camera後 | 556.021061 / 591.246114 / 1.06335201 |
| A3拡張域のcenter/左右/上下/端diagonal/Camera後の端点標本 | 115.978943 / 1343.59215 / 11.5847939 |
| A1と同じ軸sweep入力をA3へ適用（連続標本） | 10.9937〜10.9938 / 1106.74 / 約100.67（両舷×30/60/120fps） |

1440pは同じ相対poseでpixel長が4/3倍。小Delta表・拡張域端点表は両解像度の値を1080pへ正規化して集計。域/標本点が異なる表を直接改善率へ換算しない。連続sweepの約11pxは竿が視線方向へ近づく透視短縮で、SnapshotとMeshの物理長は2mのまま。この見かけの極端な変化は未解消の観測事項。今回指示では投影長の最終閾値を設定せず、係数/FOV/Grip/Lengthの変更で隠していない。後続Presentation評価と手動PIEに残す。

## Known Lock：修正せず追跡

同じ42条件を再実行した。A2は42回収/Abort0だったが、A3は34回収/8Abort/Active0。旧「上下端へ24回Mouse」を新入力変換へ適用したため到達poseは変化している。入力列を差し替えて失敗を隠していない。新入力域で下向きposeのAction不具合が自動検出できたのであり、安全停止の根本修正はA4へ残す。

追加8条件はPose=-1 × Deploy1/4frame × Jerk後Delay9/15/24/32frame。全てSpatial.ExcessiveMotionOrNonFinite（site164）。

| Deploy条件 | 移動量m | 実Step速度 / 既存上限m/s | Rod速度m/s |
|---|---:|---:|---:|
| 1frame | 0.365487115 | 21.9292269 / 10 | 4.88991194 |
| 4frame | 0.361199876 | 21.6719926 / 10 | 4.88940853 |

舷変更後の従来3条件も同じFAILのまま。30/60/120fpsのStep速度12.9406839 / 12.1521742 / 11.3149647m/s（上限10）、A2証跡と同値。

合計11条件でEnd=Aborted/Result、Onboard=false、EquipmentLocked=true。既存cleanup後のPendingJerk=0/Reeling=0/Queue=0を記録。帰還を捏造せず、Abortを成功へ変換せず、当該2テストはAddErrorの実FAILを維持。AddExpectedError/誤差緩和/強度低下/安全閾値変更は行わない。A4はこの増えた再現列を含めて調査する必要がある。

## Insert診断

BaseDirectionLocal/BaseTipLocal/FinalTipLocal/Lengthと既存Runtime観測に加え、最後のMouse delta、Command開始Current pixel、Target pixel、観測Camera frame、Sequence、交点結果/拒否理由を表示。現在Frameの実Mesh投影はScreenTipを読む。入力時Current pixelと現在の投影を混同しない。Snapshot/Visualの誤差、Context/Mode/Cast/終了診断は従来のまま。通常Product HUDへ追加しない。

## ビルド・試験証跡

- 最終`Saved/Automation/R4A3Final2/index.json`：85件、82 Success＋1 Success with warnings、2 Fail、notRun0。合計83成功。R4A-3新規4件成功（Stressだけ外部HTTP警告あり）。直接回帰72件成功。A1/A2正常7件成功。残る2Failは上記Lock試験（assert8＋3）。
- 新規：ProjectionSidesHeadingsResolutions / RepeatedDeltaReach / SequenceAndDeterminism / ActionPoseAndGuards。
- 直接回帰：R1=4、R2=14、R3=6、R4=11、M04=6、M09=7、D=5、E=7、F=6、G=6。旧MathはUnit扱いで、今回Camera変更前後の異なるScreen読取値を直接大小比較する旧期待と旧矩形受入だけを現行契約へ更新。実Mesh Runtimeが正式受入。
- UHTのTREvents/TRSnapshots生成コードにProjectionScale等/AimCurrentPixel等を確認。最初の反射変更の実生成は10/04 15:30:59。変更C++の実Compile/Link/Development Editor Win64成功。実変換コードの最終ビルド`Saved/Logs/R4A3BuildFinal4.log`。全試験後は旧Unit fixtureの説明コメントだけ整理し、`Saved/Logs/R4A3BuildFinal5.log`で実コンパイル/Link成功。動作変更なしのため全試験は重複再実行していない。UE5.8.2 / MSVC14.51.36257 / SDK10.0.22621.0。
- 最初の追加試験の文字列構文エラー、変数未初期化警告、Heading折返しのtest fixture、復帰Mouse初sampleの扱い、直接guardプローブのRelease欠落は試験側で修正した。実Runtime不具合を期待値変更で成功へ置き換えていない。
- Rod変換側では正確なscreen端の丸め誤差で別軸まで拒否されるA3不具合を修正し、画面端の垂直移動assertを追加して再検証した。
- 残存警告：試験内1件、Engineの外部HTTP到達確認`generate_204`の3秒timeout。Rod/Action警告ではないが削除/ExpectedErrorで隠していない。Buildには既存MSVC推奨版との差・IncludeOrder Unreal5_6通知。試験前の既存Editor/非Win64診断・Python旧bScreenControl/ScreenControl名衝突も保持。
- 詳細：`Saved/Logs/R4A3Final2.log`、`Saved/Automation/R4A1Observations/R4A3_*`（端点、Camera/frame、Mesh投影）、同フォルダLock_*/R4A2_Local_*。同名観測は再実行時更新されるため、A1/A2旧数値は過去JSON/文書で比較する。Saved証跡はソースへコミットしない。

## 今回変更したファイル

Source/TipRunFishingUE5配下:

- Public/Data/TREvents.h（凍結Projection係数）、TRSnapshots.h（Mouse結果診断）、TRRodTuningDataAsset.h（旧Min/Max用途コメント、値は不変）。
- Public/Fishing/TRRodControlComponent.h、Private/Fishing/TRRodControlComponent.cpp（現在Tip＋Delta→Local方向、入力域/一時ActionのBase投影接続）。
- Private/Game/TRPlayerController.cpp（実Projection採取/Insert診断）、TRFishingSessionActor.cpp（Command SequenceをRod診断へ渡す）。
- Private/Tests/TRRuntimeRodIntegrationTests.cpp（保存Worldの解像度/方位/実Mesh/各軸2000往復/順序/Action受入）、TRRodScreenTests.cpp、TRFishingStationTests.cpp（旧Unit契約を更新）。
- 文書：本書、AGENTS.md、FISHING_SYSTEM.md、UI_SPEC.md、ROADMAP.md。

A2までの未コミット差分は保持。2つの保存Assetは作業前後SHA256一致。今回のConfig/Content追加変更なし。git diff --checkは最終文書反映後も成功。

## 次段階

Mouse変換/Camera独立性/実Mesh接続をRuntimeで確認でき、R4A-4へ渡す失敗証拠は揃った。ただしR4正式合格、主観PIE操作感の合格、投影長品質、Lock解消は宣言しない。R4A-4を含む後続は新たな明示依頼でのみ開始する。手動再確認はUI_SPEC末尾の3項目。R5以降/H/M11は未着手。
