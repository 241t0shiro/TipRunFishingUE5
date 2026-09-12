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
