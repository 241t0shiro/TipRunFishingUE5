# 船・ドリフト技術設計

関連: [全体正本](GAME_DESIGN.md)、[海](OCEAN_SYSTEM.md)、[エギ](FISHING_SYSTEM.md)。MVPはプレイヤー自由操船を実装せず、将来も同じBoatSnapshotを釣り側へ供給する。

更新: v0.2 / 2026-09-12。D15のMVP決定により一定水平潮だけがドリフトに寄与する。風・波の物理影響はMVP対象外。D14/D16は保留、MVP非ブロック。

## 1. 責務とクラス

| クラス / 継承元 | 責務・主要Component | 主要変数 | 主要関数 |
|---|---|---|---|
| `ATRBoatPawn : APawn` | 船の位置・向きの窓口と竿先基準。SceneRoot、StaticMesh、RodAnchor:USceneComponent、SpringArm、Camera、DriftComponent | BoatMode、Tuning参照、CurrentSnapshot | `InitializeBoat()`、`GetBoatSnapshot()`、`GetRodAnchorWorldM()`、`SetBoatMode()`、`ApplySimTransform()` |
| `UTRBoatDriftComponent : UActorComponent` | 位置/速度の正本、水平潮への応答 | PositionM:FVector、VelocityMps:FVector2D、HeadingRad:float、TargetDriftMps:FVector2D | `InitializeMotion()`、`StepDrift(dt,OceanSample)`、`BuildSnapshot()`、`ResetVelocity()` |
| `UTRBoatTuningDataAsset : UDataAsset` | ドリフト調整、竿先配置、将来の船別性能参照 | CurrentResponse01、VelocityResponsePerS、MaxDriftSpeedMps、HullHeightOffsetM、RodAnchorOffsetM | `IsDataValid()` |
| `UTRBoatMovementComponent : UPawnMovementComponent`（Alphaで作成） | 推進・旋回入力の計算 | Throttle、Steer、PropulsionVelocityMps、TurnRateRadPerS | `SetHelmInput()`、`ComputePropulsion()`、`StopPropulsion()` |

MVPの主要ComponentにBoatMovementは含めない。Physics Buoyancy、Chaos船体、波ごとの浮力、衝突ダメージ、航路探索は不要。DriftComponentのシミュレーション位置をActorへ適用し、メッシュ物理から正本へ書き戻さない。

依存はOceanの値型とDataだけ。釣り・イカ・HUDを参照しない。Coordinatorが更新後SnapshotをFishingへ渡す。カメラは表示のみで竿先の判定位置を変更しない。

## 2. ドリフト近似

MVPは海面で取得した一定水平潮 `u_xy` だけを使う。風の入力・応答係数はMVPの計算と必須DataAsset項目から外す。

`TargetDrift = CurrentResponse01*u_xy`

`Velocity += (TargetDrift-Velocity)*(1-exp(-VelocityResponsePerS*dt))`

`Position.xy += clampMagnitude(Velocity,MaxDriftSpeedMps)*dt`

`Position.z = Ocean.SurfaceZ_M + HullHeightOffsetM`

係数は無次元、応答率は1/s、位置m、速度m/s、dt秒。CurrentResponse等はDataAssetの調整値で、実船の抗力係数と同一視しない。船首方位はMVP固定とする技術設計。RodAnchorの相対位置を船の向きで変換して竿先ワールド座標を得る。

船の更新後にエギを更新する。エギの水平速度へ船速度を直接足さず、移動した竿先からライン制約を解く。風/波の表示を変えても船・エギのシミュレーションを変えない。潮と同速度で船・エギが流れる場合、相対移動は小さくなり得る。

海域外/無効サンプルでは直前の有効位置を保持し、EnvironmentInvalidを通知する。活動中の投を中断するのはSession側。これは無効データへの技術的防御であり、D14の境界ゲーム仕様をMVPへ追加するものではない。通常試験は有効範囲内で実施し、岸/根掛かり/境界演出は作らない。

## 3. 状態遷移

`ETRBoatMode`: Uninitialized / DriftOnly / Disabled。将来Helmを追加。

| 現在 | 条件 | 次 | 副作用 |
|---|---|---|---|
| Uninitialized | 設定・海が有効 | DriftOnly | 初期位置/速度設定 |
| DriftOnly | 海が無効・終了 | Disabled | 更新停止、異常通知 |
| Disabled | 明示的再初期化 | DriftOnly | 有効位置へ初期化してから再開 |
| DriftOnly（将来） | 操船開始が許可 | Helm | 推進入力を有効化 |
| Helm（将来） | 釣り開始/操船終了 | DriftOnly | 推進入力停止、慣性の扱いはD16 |

ゲームのポーズはBoatModeを変更せずsim時間を止める。釣り開始/終了だけで海の流れをリセットしない。

AlphaではDriftが環境速度、Movementが推進速度を出力し、位置の積分は一箇所だけで実施する。両Componentが同じActorを別々に移動させない。舵と速度の合成規則は自由操船実装前に設計を拡張する。

## 4. データ・Blueprint

BoatTuningを船ごとに差し替えられるようにする。MVPに価格、燃料、積載、耐久など未使用商品データを追加しない。将来装備が提供する船ID→BoatTuning解決をGame側へ追加する。

BlueprintReadOnly: BoatMode、速度、竿先位置、Snapshot。EditDefaultsOnly: Tuning、メッシュ、カメラ、RodAnchor相対設定。BlueprintCallableはSnapshot取得のみ。将来の操船入力はController経由に限定。BlueprintでDrift式を上書きしない。

## 5. テスト

| ID | 条件 | 期待 |
|---|---|---|
| B01 | 潮0・風0・初速度0 | 位置不変 |
| B02 | 一定潮・風0 | 設定した目標ドリフトへ収束、速度上限を超えない |
| B03 | 潮0・初速度0、表示用の風/波だけ変更（存在する場合） | 位置不変。風設定アセットなしでも起動。表示用環境から物理寄与なし |
| B04 | 同じdt列/seedで描画fpsだけ変更 | Snapshot一致 |
| B05 | 竿先offsetと船首方位を試験で変更 | 座標変換が正しく、釣り側へ同じ値を供給 |
| B06 | 海域境界通過 | 有効位置保持、異常1回、釣果成功にしない |
| B07 | Boat更新→Egi更新の統合 | Stayのライン角度・深度が船に反応 |
| B08 | 無効Tuning/負の上限/NaN | 開始拒否、ログに原因 |

MVPでは静水/一定潮の方向・速度別シナリオで船速とレンジ履歴を比較する。横風・波の物理評価はAlpha以降。ゲーム近似としての検証と実測校正を区別する。

## 6. M05実装・検証記録（2026-09-13）

- `ATRBoatPawn`、`UTRBoatDriftComponent`、`UTRBoatTuningDataAsset`、`FTRBoatParameters`、`ETRBoatMode`を追加。PawnはSceneRoot、BoatMesh、RodAnchor、SpringArm、Camera、DriftComponentをコンストラクタで所有生成する。Pawn/Driftの独立Tick、船体物理、自由操船、風・波の物理計算はない。SpringArm/Cameraは表示だけで、正本の位置・竿先へ逆流しない。
- Tuningの`Parameters`に第1節の5調整項目をまとめた。潮応答は有限な[0,1]、速度応答率と上限は有限な正値、船体・竿先offsetはcm境界でも有限であることをRuntime検証/IsDataValidで要求する。速度応答率・上限の初期値0は未設定として拒否する。調整値は初期化時にコピーし、実行中のDataAsset編集を反映しない。失敗時は理由を`TArray<FText>`へ返し、試験で予期した不正値を警告ログとして出し続けない。
- `InitializeMotion`は明示XY位置、固定方位、海面サンプルから初期状態を作る。初速度は0、Zは海面＋船体offset。`StepDrift`が指数応答を計算し、速度を上限へ制限してから位置を積分する。Snapshotに保存する速度も実際の積分に用いた制限後の値とする。係数が一定で上限に達しない場合、速度は`Target*(1-exp(-rate*t))`へ一致する。これは実測値で校正していないゲーム近似。
- M04へ`RegisterBoat`を追加。GameがM03の海をDepthM=0で問い合わせて初期化し、同一Worldの船を共通SimIdで弱参照登録する。船登録はBoatフェーズだけに配送され、Fishingコマンドの行先にはできない。更新順はOcean時刻→船→Fishing。Gameが海の値を取得し、計算はDriftComponentへ任せるためBoat→Game/OceanSubsystemの逆依存はない。
- 移動前と移動候補先の海が有効な場合だけSnapshotを確定する。候補先検証はGameから一時的なcallbackとして渡し、Componentに保存しない。無効環境・非有限値・不正な時刻では直前の有効Snapshotを保持してDisabledへ移り、`OnEnvironmentInvalid`を初期化1回につき1度通知する。通常のUnregister/終了では異常通知せず停止する。再開には明示的な有効設定での再初期化が必要。D14の境界ゲーム仕様や投の中断処理は追加していない。
- 竿先は凍結した`RodAnchorOffsetM`を固定方位で回転し、シミュレーション位置へ加算する。Pawnへの適用時だけm→cm、rad→degree変換を行う。`GetBoatSnapshot`/`GetRodAnchorWorldM`は正本の読取値で、SceneComponentから再計算しない。Gameの`GetBoatSnapshot(BoatId, Out)`は登録・動作状態が無効ならfalseを返し、Outを変更しない。Fishingフェーズの試験callbackで当該TickのSnapshotを読めることを確認。実エギへの接続・B07はM07/M08以降に残す。
- Gameが船のOnEndPlayを登録し、EndPlay・明示Unregister・Subsystem終了で対応するバインドと登録を解除する。破棄検査はM04の弱参照管理も併用する。船はEndPlayでDriftと自分の異常通知delegateを解除する。二重登録を拒否し、再登録でもSimIdとWorld時計をリセットしない。
- `TipRun.M05`の6試験がすべて成功、警告/エラー0、終了コード0。B01/B03静水・表示用風非寄与、B02方向・解析的速度・上限、B04の30/60/120fps一致と明示/Engine pause、B05の竿先変換・Snapshot配送順・表示編集の非逆流、B08の不正設定、Provider破棄と解除を確認した。通常試験の移動は有効範囲内。極小海域の試験は無効な移動候補を確定しない技術防御だけで、M16の境界シナリオ完成を意味しない。
- 試験値は一時的なTestデータ（例：潮応答0.5、応答率2/s、速度上限3m/s、船体offset0.5m、竿先offset(2,1,1)m）。製品既定値ではなく、Content/Configへの保存や製品メッシュ・マップの作成は行っていない。Editor表示用にはBoatPawn派生BlueprintでBoatMesh/Cameraを設定し、BoatTuningを割り当てる。起動接続側はOceanと固定時計を初期化後に`RegisterBoat`へ明示XY/方位を渡す。今回の検証はNullRHIのC++試験で、PIE目視・船カメラ調整・Windowsパッケージは未実施。
