# 船・ドリフト技術設計

2026-09-17 M10.5-R設計改訂: A〜F基盤は保持。最新手動PIEでGはゲームプレイ品質不合格。Rは設計/実装分割のみ完了し、実装未着手。最新契約は本書末尾のM10.5-R節を優先。以前のG合否保留・固定Pulse・観測カメラ等は履歴。R自動検証とユーザー手動合格後もHへ自動進行しない。H/M11以降は保留。

2026-09-14 M10.5-B完了: 風/表層潮を別々に評価する船体方向別応答、抗力/慣性、解析的な固定更新とBoat Snapshot拡張を実装。UHT生成・実C++・Development Editor Win64成功、B 5件＋A/M03/M04/M05/M08回帰26件成功、各試験エラー/警告0。旧保存設定はModelRevision=1で互換維持、新モデルは明示revision 2。保存Prototypeの移行/PIE再評価は未実施。C〜HおよびM11以降は未着手、M10.5全体の品質ゲートは未合格。以下のA完了/設計のみの記録は履歴。

2026-09-13 M10.5設計改訂（実装未着手）: M00〜M10の実装・自動試験成功は履歴として保持するが、ユーザーのM10後PIE評価は再現性・操作性の品質不合格。M11への進行はM10.5品質ゲート合格まで保留する。本書のM10.5改訂契約を旧記述より優先し、M03〜M10完了記録は旧実装の証跡として読む。今回はMarkdownのみ更新し、改訂機能の実装・ビルド・試験は行っていない。

関連: [全体正本](GAME_DESIGN.md)、[海](OCEAN_SYSTEM.md)、[エギ](FISHING_SYSTEM.md)。M10.5-Rで簡易操船を設計。BoatSnapshotを釣り側へ供給する責務は維持する。

更新: v0.2 / 2026-09-12。D15改訂によりM10.5で風＋表層潮に船体応答を導入する。第2節の旧式とM05記録は移行前の履歴。波物理は対象外。D14は保留・非ブロック。D16の簡易操船はRで承認済み（第9節）。

## 1. 責務とクラス

| クラス / 継承元 | 責務・主要Component | 主要変数 | 主要関数 |
|---|---|---|---|
| `ATRBoatPawn : APawn` | 船の位置・向きの窓口と竿先基準。SceneRoot、StaticMesh、RodAnchor:USceneComponent、SpringArm、Camera、DriftComponent | BoatMode、Tuning参照、CurrentSnapshot | `InitializeBoat()`、`GetBoatSnapshot()`、`GetRodAnchorWorldM()`、`SetBoatMode()`、`ApplySimTransform()` |
| `UTRBoatDriftComponent : UActorComponent` | 位置/速度の正本、水平潮への応答 | PositionM:FVector、VelocityMps:FVector2D、HeadingRad:float、TargetDriftMps:FVector2D | `InitializeMotion()`、`StepDrift(dt,OceanSample)`、`BuildSnapshot()`、`ResetVelocity()` |
| `UTRBoatTuningDataAsset : UDataAsset` | ドリフト調整、竿先配置、将来の船別性能参照 | CurrentResponse01、VelocityResponsePerS、MaxDriftSpeedMps、HullHeightOffsetM、RodAnchorOffsetM | `IsDataValid()` |
| `UTRBoatMovementComponent : UPawnMovementComponent`（Alphaで作成） | 推進・旋回入力の計算 | Throttle、Steer、PropulsionVelocityMps、TurnRateRadPerS | `SetHelmInput()`、`ComputePropulsion()`、`StopPropulsion()` |

MVPの主要ComponentにBoatMovementは含めない。Physics Buoyancy、Chaos船体、波ごとの浮力、衝突ダメージ、航路探索は不要。DriftComponentのシミュレーション位置をActorへ適用し、メッシュ物理から正本へ書き戻さない。

依存はOceanの値型とDataだけ。釣り・イカ・HUDを参照しない。Coordinatorが更新後SnapshotをFishingへ渡す。カメラは表示のみで竿先の判定位置を変更しない。

## 2. M05までのドリフト近似（M10.5で置換）

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

## 5. M05までのテスト（風非寄与B03はM10.5で置換）

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

## 7. M10.5の船体応答（B実装済み、具体契約は第8節）

船を「前へ流す」のではなく、世界座標の風/表層潮に応じて横流しする。Prototypeでは釣り人が概ね風上を向き、船体に横風を受ける初期方位を試験設定する。船首方位、釣り人/竿の基準方位、風の移動方向、船速度は別値。船首を毎Tick速度方向へ自動回転させず、自由操船や自動風上追従は追加しない。

既存UTRBoatDriftComponentがBoat Position/Velocityを一度だけ更新する。風wと表層潮u、現在船速度vに対する基本式は線形抗力のゲーム近似（船体軸ごとの拡張は第8節）:

`I * dv/dt = Kw*(w-v) + Kc*(u-v) - Kd*v`

Kw=BoatWindResponse、Kc=BoatCurrentResponse、Kd=BoatDragは非負の有効係数[kg/s]、I=BoatInertiaは正の有効慣性[kg]。実船の質量/実測抗力と同一視しない。K=Kw+Kc+Kd>0、vTarget=(Kw*w+Kc*u)/K、tau=I/Kとし、一定サンプルの1ステップを`vNext=vTarget+(v-vTarget)*exp(-dt/tau)`で解く。位置は同じ解の積分`deltaX=vTarget*dt+(v-vTarget)*tau*(1-exp(-dt/tau))`を使う。小さいdtの減算誤差を避ける。速度防御上限を使う場合は位置/公開速度の整合を試験し、上限で過剰風速設定を隠さない。

全係数をUTRBoatTuningDataAssetへ分離、初期化時に凍結。旧CurrentResponse01/VelocityResponsePerSから単位の異なる係数へ暗黙換算せず、Prototype設定リビジョンを移行し再校正する。初期船速0でも第1Tickに風/潮速度へ瞬間一致しない。無風/無潮では初速0を保持し、初速ありなら減衰。同方向/逆方向/直交の合成、慣性増加による応答遅延を解析解と比較する。

船のZは固定海面＋船体offset、方位は試験配置値。既存RodAnchorは取付基準のTransformとしてBoatSnapshotへ公開し、可動Rod TipはFishing/RodControlがこの基準から作る。Boatはマウス入力を直接読まない。Bでは既存RodTipMを固定取付点として維持し、既存Egiへ同TickのBoatSnapshotを渡す。可動RodSnapshotへの切替は後続タスクであり今回未実装。

風の描画から数値へ逆流させないが、Oceanの風データは実際にドリフトへ寄与する。旧B03の「風非寄与」を廃止し、波の表示非寄与だけ維持する。R02で解析応答、方向合成、速度上限/大dt、無効環境、Pause、固定更新再現性を確認する。

## 8. M10.5-B実装契約・検証（2026-09-14）

- `FTRBoatParameters.ModelRevision=2`が新モデル。`WindResponseKgPerS`、`CurrentResponseKgPerS`、`DragKgPerS`、`InertiaKg`、`BowWindScale`/`SternWindScale`/`SideWindScale`、既存`MaxDriftSpeedMps`/船体・竿先offsetをDataAssetで指定し、初期化時にコピーする。応答/方向倍率は非負、慣性と各軸の合計応答は正、すべて有限で計算可能な範囲を要求する。未設定・非有限・混在revisionはRuntime/IsDataValidで拒否する。
- 既存の保存資産は既定の`ModelRevision=1`でM05の旧式・単位・上限処理を維持する。旧2係数から新係数への暗黙換算はしない。revision 1では新係数0、revision 2では旧`CurrentResponse01`/`VelocityResponsePerS`が0であることを要求する。M05の旧B03は互換経路だけの回帰であり、新モデルの風非寄与を意味しない。新モデルはBの一時的Test DataAssetで検証した。保存済みPrototype資産/Levelの移行・再校正はGであり、今回Content変更はない。
- AのEnvironmentが船位置・深度0・固定Tickで返す`WindMps`と`SurfaceCurrentMps`を独立に受け取る。`CurrentMps`（深度別潮）を船の駆動値として使わない。Ocean側で合成せず、Boatで船首Forwardと水平横軸へそれぞれ射影する。風速・潮速は各ベクトルの大きさであり、別の変更可能な速度正本は持たない。
- 各軸で第7節の式を解く。風係数Kwは基礎WindResponse×方向倍率。横軸はSide、前後軸はステップ開始時の相対風`w-v`が負ならBow、その他はStern。選んだ係数と環境をそのステップ内で一定として指数解と同じ解の位置積分を使う。小刻みでは級数展開で減算誤差を抑える。前後の受風が途中で切り替わる連続非線形系全体の厳密解とは主張しない。CurrentResponseとDragは両軸共通、風応答とは独立する。船首方位は初期配置値を保持し、Velocityから回転させない。
- 風・潮の開始/停止・反転でも既存Velocityを初期条件にして連続応答する。速度上限は新モデルでは目標速度のclampに使わず、ステップ終端速度または平均移動速度が上限を超えた候補を棄却する防御。非有限/無効環境と同様に最後の有効Snapshotを保持しDisabled、既存異常通知を一度だけ送る。候補先callback中にStop/破棄された場合も結果を確定しない。
- `FTRBoatSnapshot`は既存PositionM/VelocityMps/HeadingRad/RodTipM/TickにModelRevision、SpeedMps、ForwardVector、WindMps、SurfaceCurrentMps、WindDeltaVelocityMps、CurrentDeltaVelocityMps、DragDeltaVelocityMpsを追加。寄与は当該ステップで積分した速度変化[m/s]で、3寄与の和が速度差。入力場はステップ開始位置のサンプルで、到着位置での再評価値ではない。既存読取APIは変更せずBlueprintReadOnlyで取得できる。RodTipMは船位置＋Headingで回転した凍結offset、Actor/Sceneへの適用だけcm変換。可動竿/HUD表示追加は今回なし。
- BoatはWorld所有。Session破棄でSession登録を除去し、船は継続する。Boat登録解除後は固定更新・公開取得を停止。World破棄後にGCしてBoatが回収されることを確認する。Cのエギ/ライン位置正本・抗力・繰出しには変更しない。
- `TipRun.M105B`全5試験成功。解析解の独立比較は位置/速度1e-10許容（R02の1e-4より厳しい）。無風無潮、風のみ、潮のみ、同/逆/直交、船首/横/船尾、慣性増、開始/停止/反転、0.4/0.7/1.0 knot、1e-10〜1000秒刻みの有限性、防御上限、NaN/Inf、設定凍結/シリアライズ、Pause、30/60/120fps一致、Rod/Actor/M08同Tick接続、Session/Boat/World寿命を確認した。
- 代表試験値: 風2m/s、WindResponse=0.1、CurrentResponse=1、Drag=1 kg/s、Inertia=10kg、Bow/Stern/Side=1/0.5/2、上限1m/s。3潮速×3方向で10 sim秒間の船速が0.5m/s未満かつ防御停止なしを確認。解析専用条件はWindResponse=0.2、CurrentResponse=0.8、Drag=1、Inertia=4、方向倍率すべて1、風2m/s・潮0.5m/sで、目標0.4m/s・時定数2秒。いずれもゲーム近似のTest受入値であり実船の実測値・製品既定値ではない。
- UHT 9生成ファイル、実C++/Development Editor Win64成功。詳細な回帰件数・警告・証跡はROADMAPのB完了記録を参照。PIE操作感/実測校正/パッケージは未実施。Bは合格、Cへ接続可能だがC以降には着手していない。

## 9. M10.5-R 操船・釣り座・ドリフト再評価（設計のみ）

Rは簡易Navigationを承認された対象とする。旧「自由操船は全てAlpha」「環境速度と推進速度を合成」は本節で改訂。Bの風/表層潮を別評価するモデルを保持する。

### 単一の船体更新

新設予定`UTRBoatNavigationComponent`は固定入力から推進力/操舵要求を作る。独立TickでPawnを動かさない。`UTRBoatDriftComponent`を船位置・速度・Headingの唯一の積分担当として拡張する。従来Bの軸別風/潮抗力へHeading方向の推進力を加える。概念式は `I dv/dt = Fwind + FsurfaceCurrent - Drag*v + Fthrust`。別計算の推進速度を後で加算しない。舵は調整可能な角速度/応答時間を持つ簡易モデルとし、Chaos/6自由度船舶物理にはしない。

Navigationでも環境作用は継続。Fishing移行で推進/Boost/舵要求と動的速度を0とし、位置とHeadingだけを保持して環境Driftを再構築する。速度方向へ船首を自動整列しない。釣り中の風による自動Yawは今回追加しない。操船用速度の安全範囲は旧MaxDriftSpeedと区別し、通常航行の速度はClampしない。Fishing確定時だけ意図的に動的速度を解除する。質量相当/推力/操舵速度/応答/速度安全限界はPrototype DataAssetへ分離する。

### Port / Starboard

新設予定`UTRFishingStationDataAsset`に左右舷のLocalPlayer/Eye/Mount位置、基準Facing、Rod参照を持つ。Boatローカル+Xが船首、+Yが右舷、-Yが左舷、+Zが上。釣り座は安定ID付きの配列として持ち、初期要素をPort/Starboardとする。船種ごとの座数変更を可能にするが、今回は左右舷の2要素だけを使用する。船首中央の共通Mountへ戻さない。

`WorldMountM = BoatPositionM + HeadingRotation * LocalMountM`。Eyeも同様。RodTipはこのMountからDのBasePose/一時Offset/長さで計算しCへ渡す。座標はSimulation m、表示境界だけcm。BoatSnapshotの旧固定RodAnchorと、実際に動くRodSnapshotのRodTipを区別し、ラインは後者を使用する。舷選択はFishing開始前だけ、Cast中に変更しない。

### ドリフト速度の測定・再調整

Bを廃棄せず、風と表層潮の応答係数、方向別受風係数、抗力、慣性を個別に測る。0.4/0.7/1.0 knot、無風/風のみ/潮のみ/同方向/逆方向/直交、船首違いで、10/30/60 Simulation秒の位置差(m)、平均/終端速度(m/s・knot)、Heading/移動方位を記録する。停止開始とNavigation/BoostからのFishing移行を別試験とし、後者にも航行慣性が残らないことを確認する。

Prototype調整は既存B値との比較表と固定世界基準物を使った手動評価で行う。現実の船舶実測値はないため「ゲーム近似」と明記。見た目の速さをカメラ追従だけで隠さず、単純な速度Clampだけで調整しない。実装前に試験条件と受入速度帯を記録し、失敗後に期待値を都合よく変えない。製品速度の確定はしない。

NavigationとCameraの自然さ、選んだ船首を保つ横流し、左右舷の竿位置を手動ゲートにする。無効環境時は推進を止め最後の有効状態を保持して異常を通知する。架空の水深/船上帰還は作らない。詳細タスクはROADMAP R2/R3/R7。

### R2実装: Navigation作用（2026-09-18）

- Session所有Navigation Componentは入力Throttle/Steering（各-1〜1）を保持し、指数応答 `x += (target-x)*(1-exp(-response*dt))` で推力N/旋回角速度rad/sを更新する。後退だけReverseScaleを適用。Navigation以外/Focus解除等では要求を0にする。通常の運動は独立Tickや速度の直接代入で進めない。Fishing確定時の動的速度解除は明示的な例外とする。
- Drift ComponentだけがHeadingとWorld位置/速度を積分。各固定ステップでHeadingを角速度から更新し、そのForwardへEngineForceを加える。Bの軸別解析解の目標へ長手方向だけ `EngineForce/K` を追加する（Kは風/表層潮/抗力の係数和）。船首はVelocity方向へ揃えない。無風無潮でも相対流体抗力と慣性は作用するゲーム近似。
- B SnapshotのWind/Current/Drag寄与は引き続き各環境作用の速度変化。Engine有効時は3寄与だけで全速度差にはならず、推力の寄与 `Forward*EngineForce*dt/Inertia` を含む。新Navigation Snapshotから推力を参照できる。船座標/速度/固定RodTipは既存公開口を維持する。
- 操船要求はBoatステップ後に消費し、次ステップは生存Sessionから再送する。Session破棄・登録解除後に推力/舵が残留しない。Fishing移行時は推力/Boost/舵/補助と動的速度を即解除し位置/Headingを保持。Bの環境Driftを停止状態から再構築する。
- `MaxSpeedMps`は通常目標をClampする値ではなく、非有限/範囲外の候補を棄却する安全限界。明示設定した船では旧Drift上限との大きい方を使用し、Fishing移行後も保持する。上限は値検証用であり、Fishing開始時の速度解除とは独立する。範囲違反/無効海の既存Disabled・最終有効Snapshot保持を維持。
- Prototype専用Navigation DataAsset: EngineForce=8N、ReverseScale=.5、EngineResponse=2/s、SteeringRate=.45rad/s、SteeringResponse=3/s、安全限界8m/s。全てゲーム近似の仮値で、実船実測/製品値ではない。Bの保存Boat調整値は変更していない。Drift最終調整はR7へ残す。

### R2操船Prototype値の再調整（2026-09-18）

手動PIEで前後進/操舵/三人称/風潮中の航行は正常だったが、移動と旋回が遅いとの結果を受け、上記の旧Navigation値だけを改訂した。積分式、Boatの風/潮応答・抗力・慣性、Fishing中の自然Drift係数は変更しない。

|Navigation項目|旧値|改訂値|
|---|---:|---:|
|EngineForceN|8|20|
|ReverseScale|0.5|0.6|
|EngineResponsePerS|2|4|
|SteeringRateRadPerS|0.45|1.0|
|SteeringResponsePerS|3|6|
|MaxSpeedMps（候補の安全限界）|8|14|

保存`DA_TR_M105Navigation_Prototype`へ明示適用。Camera設定は保持。これらは移動を快適にするためのPrototypeゲーム近似であり、製品値/実船値ではない。最大速度を強制設定する変更ではなく、既存の力・抗力・慣性に応答する。Fishing移行時は航行慣性を解除するため、環境Driftは0速度から再形成する。

無風無潮、Bの試験船係数（慣性10kg、CurrentResponse/Drag各1kg/s、WindResponse .1）の独立比較: 5秒後前進2.322641→6.012489m/s、後退1.172509→3.643671m/s、1秒操舵のHeading変化.311002→.841921rad。新旧とも安全限界未満。体感の最終合否は保存Prototypeの手動再PIE待ち。

### R2最終修正: 推進中の旋回補助（2026-09-18）

ユーザー再PIEで前後進/操舵は合格だが、旋回中の横滑り慣性が強いとの指摘。従来は船首を回しても横速度はBの弱い抗力だけで減衰していた。推力/SteeringRateを再増加せず、Navigation専用の横減衰を追加する。

`NavigationLateralResponsePerS`は有限0〜20/s、0で無効、保存Prototypeは1.5/s。有効値はNavigationかつ推力有効時の`設定値 * abs(Throttle)`。Throttle解放・Pause/Focus解除・Session終了・Fishingへの遷移で0となる。推力応答に残量があってもThrottle=0なら補助を止める。公開Navigation Snapshotの`LateralResponsePerS`で確認可能。

船体横軸の係数和へ`InertiaKg * 有効Response`を足し、既存の軸別解析解で位置と速度を一度だけ積分する。横方向の追加力は`-InertiaKg * Response * 横速度`。速度の方向を直接書き換える/スナップする処理ではない。船首はSteeringだけで決まり、風・表層潮の独立した外力、通常抗力、慣性を継続。環境の寄与を0へ置き換えない。Bの3寄与Snapshotは既存の環境作用で、補助分の速度変化`-横軸 * Response * 横移動量`は別項として扱う。

固定要求は各Boatステップで消費し、Fishingでは0を配送する。補助0の経路は従来Bの計算と同一。係数0/1.5の2つの同条件WorldをFishingで600ステップ進め、位置・速度の完全一致を確認。環境ありの旋回比較で2秒操舵後の船首/速度方位差は補助なし82.766°、補助あり33.480°。最初の1ステップの速度変化は.25m/s未満で瞬間整列なし。30/60/120fps・寿命/PauseとB回帰も成功。係数はゲーム近似のPrototype値、今回の手動操作感再評価は保留。

### R2追加修正：Navigation Boost（2026-09-19）

- Shift保持＋ThrottleでNavigation推力だけを増幅。Shift単独では加速しない。Steering/横減衰補助、Wind/SurfaceCurrent、Drag/Inertiaの既存単一積分は維持する。
- 固定更新でBoostBlendを目標0/1へ `1-exp(-BoostResponsePerS*dt)` で近づける。目標推力は通常推力×`1+(BoostThrustMultiplier-1)*BoostBlend`、その後既存EngineResponseを通す。Velocityの瞬間倍率変更は行わない。解放後は滑らかに通常推力へ戻る。
- Navigation DataAssetのPrototype値は倍率2、応答2/s、BoostMaxSpeedMps=24。通常のEngineForceN=20、MaxSpeedMps=14は保持。24m/sは速度目標やClampではなく、通常/Boost航行の既存異常速度検出の上限。Fishingへ移行後も上限設定は保持するが推力・Boost・横減衰補助・動的速度を0とし、自然Driftを再形成する。
- Mode/Focus/Pause境界ではBoost要求とBlendを解除。FishingでBoost Commandを直接投入してもSessionが拒否。入力はModeEpoch/CastId/Sequence/登録寿命を通す。製品値/実測船舶モデルではない。

### R2 Mode Transition速度契約の改訂（2026-09-19、旧慣性継承を廃止）

- ユーザーPIEでBoost航行の残留慣性が釣り開始後も高速移動を続けると確認されたため、旧「Navigation→FishingでVelocity/inertiaを保持」は廃止。
- 現行モデルは環境と推力を単一Velocityへ積分しており、由来別の履歴分離は行わない。今回許可された簡潔な方式として、受理されたFishing開始Command内でCoordinator→Boat Driftへ`ResetDynamicVelocityForFishing`を依頼し、Velocity/Speedと前Tickの速度寄与を0へ戻す。Navigation ComponentのEngine/Throttle/BoostBlend/Steering/Assistも解除する。
- Position/Heading/Forward/RodTip、環境設定、登録、固定時計は保持。Command確定時点で位置/Headingは完全に同じ。後続Boatフェーズから既存BのWind/SurfaceCurrent/船体応答/Drag/Inertiaだけで自然Driftを再形成する。無風無潮では静止、環境ありでは環境による変位だけが生じる。拒否されたMode変更では速度をリセットしない。
- Boost係数/通常Navigation速度/表示の手動合格箇所は変更しない。速度の減算推定やClampで高速残量を隠さず、Fishing開始という明示境界で動的状態を解除する。Navigationへの復帰時にはFishingの自然Driftをリセットしない。
