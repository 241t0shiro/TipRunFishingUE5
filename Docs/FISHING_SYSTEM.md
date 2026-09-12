# 釣りシステム技術設計

関連: [全体正本・要決定事項](GAME_DESIGN.md)、[BITE・AI](SQUID_AI.md)。本書の数式と未指定操作は技術提案/暫定案。実測の釣りモデルと称さない。

更新: v0.2 / 2026-09-12。D01〜D09/D13/D15のMVP決定、D10〜D12のMVP暫定仕様を反映。数値式の詳細は技術設計であり、未指定の調整係数はDataAssetで校正する。

## 1. 責務とクラス

| クラス / 継承元 | 責務・主要Component | 主要変数 | 主要関数 |
|---|---|---|---|
| `UTRFishingComponent : UActorComponent` | 操作状態の唯一の所有者。Sessionに配置 | `State: ETRFishingState`、CastId、StateEnteredTick、LastActionTick、JerkCount（投内合計）、SeriesJerkCount、StayPenaltyJerkCount、bSeriesClosed、PendingJerkCount、AutoStayDeadlineTick、bHadMiss、bHadEscape、ActiveCommand | `HandleCommand(const FTRFishingCommand&) -> ETRCommandResult`、`Step(float)`、`BeginCast()`、`TransitionTo()`、`AbortCast()`、`GetSnapshot()` |
| `UTREgiSimulationComponent : UActorComponent` | 深度・水平位置・ライン近似の唯一の所有者。Sessionに配置 | DepthM:float、PositionXYM:FVector2D、VelocityMps:FVector、LineLengthM:float、LineAngleRad:float、Tension01:float、TotalMassG:float | `InitializeCast()`、`StepEgi(dt,Ocean,Boat,Action)`、`ApplyJerk()`、`SetLineMode()`、`GetSnapshot()`、`Reset()` |
| `ATREgiActor : AActor` | 結果の描画専用。SceneRoot、StaticMesh。任意で表示用ライン | Previous/CurrentSnapshot、Mesh参照 | `ApplySimulationSnapshot()`、`UpdateVisualInterpolation()` |
| `UTRHookComponent : UActorComponent` | BITEトークン・受付・合わせの唯一の判定者。Sessionに配置 | ActiveToken、TargetSimId、BiteStartTick、OpenTick、CloseTick、Resolved | `TryOpenBite()`、`EvaluateHook()`、`ExpireBite()`、`CancelBite()`、`Reset()` |
| `UTRFightComponent : UActorComponent` | HIT後の簡易テンション/進捗。Sessionに配置 | FightState、Progress01、FightTension01、OverTensionTicks、ReelHeld、HookedSquidSnapshot、FightStartTick | `BeginFight()`、`SetReeling()`、`StepFight()`、`CompleteFight()`、`ResolveEscape()`、`AbortFight()` |
| `UTRFishingTuningDataAsset : UDataAsset` | 釣り・フック・簡易ファイトの設定 | 下記パラメータ群 | `IsDataValid()` |

依存: EgiSimulationはOcean/Boatの値を受けるだけ。HookはAI Componentを直接呼ばず結果イベントを発行。FightはHIT時のイカ情報コピーを使う。SessionがComponentを所有し、Coordinatorが順序とシステム間通知を管理する。

## 2. 装備データ

`FTREgiSpecRow : FTableRowBase`: EgiId:FName、DisplayName:FText、SizeGo:float、BaseMassG:float、SimulationProfileId:FName、Mesh:TSoftObjectPtr<UStaticMesh>。D05に従い、沈下/潮応答曲線などのシミュレーション係数は `UTRFishingTuningDataAsset` のProfileId別プロファイルに置く。DataTableは重量・識別・プロファイル参照を保持し、係数を二重定義しない。

| EgiId案 | 号数 | 基本重量（確定） |
|---|---:|---:|
| Egi_3 | 3 | 30g |
| Egi_3_5 | 3.5 | 35g |
| Egi_4 | 4 | 40g |

`FTRSinkerSpecRow : FTableRowBase`: SinkerId:FName、MassG:float、DisplayName:FText、VisualMesh:soft参照。重量は **0（無し）/ 5 / 10 / 15 / 20 / 25 / 30 / 40 / 50g**。0gは `Sinker_None` という有効な行IDとし、欠損IDと混同しない。0gのVisualMeshは空でよい。負の重量を拒否し、0g以外は元の8種類を維持する。

`FTREquipmentSnapshot`: EgiId、SinkerId、BaseMassG、SinkerMassG、TotalMassG、凍結した関連係数。`TotalMassG = BaseMassG + SinkerMassG`。組合せは3×9=27通り、総重量30〜90g。初期エギは `Egi_3_5`（35g）、初期シンカーは未指定のため試験構成では `Sinker_None` を使い35gとする。同総重量でも形状プロファイルが異なってよい。係数比較試験では形状曲線を揃える。

`StartFishing()`で装備をロックし、MVPの装備変更はその前だけ許す。Deploy前Ready、MISS後、次投Readyでも同じ釣りセッション中は変更不可。EndFishingでロック解除。一投ごとの凍結とは別にセッションロックを持ち、Ready判定だけで装備変更を許さない。

## 3. 操作状態

`ETRFishingState`:
`Inactive, Ready, Deploying, FreeFall, BottomContact, Jerking, TensionFall, Stay, Retrieving, Fighting, Landing, Result`

**Attack/Bite/HIT/MISSをFishingStateに追加しない。** Attack/BiteはAI状態、HIT/MISSは `ETRHookOutcome`。Bite中のエギはStayのままである。

| 現在 | トリガーと条件 | 次 | 副作用 |
|---|---|---|---|
| Inactive | 釣り開始・環境有効 | Ready | 設定検証、入力接続 |
| Ready | Deploy | Deploying | CastId増加、エギ生成、装備固定 |
| Deploying | 配置成功 | FreeFall | キャストなしで竿先付近の海面へ投入、自動繰出し。アニメ終了を待たない |
| FreeFall | 海底に到達 | BottomContact | 深度を海底へ制限、着底通知を1回 |
| FreeFall | Jerk / TensionFall / Stay要求 | 対応状態 | D02/D03の途中操作を許す暫定案 |
| BottomContact | Jerk | Jerking | 浮上動作を1回開始 |
| BottomContact | Stay / TensionFall | 同名状態 | 海底制約は継続 |
| Jerking | 動作時間満了 | TensionFall | 動作分の巻取り完了。連続入力の処理はD03の下記契約 |
| Jerking | Jerk入力 | Jerking | PendingJerkCountへ予約、並列実行しない |
| Jerking | 明示Fall / Stay | 同名状態 | 現動作を中止して予約取消。開始済動作の回数は保持 |
| TensionFall | Stay | Stay | ライン固定モードへ |
| TensionFall | 海底に到達 | BottomContact | 着底通知 |
| Stay / TensionFall | Jerk | Jerking | 1受理入力につき1動作、回数上限なし。後続Stay用の回数を更新 |
| Stay / TensionFall | Fall | FreeFall | 再フォール。BITE予約/受付を取り消す |
| TensionFall | 無入力時間がAutoStayDelaySに到達 | Stay | 初期値0.8秒、DataAssetで調整。MVPで自動移行を採用 |
| FreeFall / BottomContact / Jerking / TensionFall / Stay | Retrieve開始 | Retrieving | BITE取消、回収用巻取り |
| Stay | 有効なHook入力でHit | Fighting | ラインモデルの所有を簡易ファイトへ切替 |
| Stay等 | Hook入力がEarly/Late/NoBite | 元の状態 | MISS通知と機会消費。詳細SQUID_AI |
| Retrieving | ライン回収閾値到達 | Result | Caughtにはしない。Escaped / Missed / Retrievedを履歴で選ぶ |
| Fighting | ファイト成功 | Landing | Caught候補を固定 |
| Fighting | 過大テンションが規定時間継続 | Stay | バラシ通知、bHadEscape=true、対象解放。投は回収まで継続する技術案 |
| Landing | 論理的取り込み完了 | Result | Caughtを一度だけ確定。演出完了依存にしない |
| Result | 次投 | Ready | 結果解除、Cast固有状態リセット |
| 任意の活動状態 | 終了・環境無効・対象喪失 | ResultまたはInactive | Aborted、全受付無効。画面動線はD13 |

表にない入力は `RejectedInvalidState`。不正遷移は状態を変えず、開発ビルドで理由を記録する。BottomContactは観測状態でありプレイヤーの「着底ボタン」は設けない。BottomContact中に船の移動でエギが浮いた場合、開いたラインならFreeFall、固定ラインならTensionFallへ戻す。

`FTRFishingCommand`: Type、TargetTick、Sequence、AxisValue。TypeはDeploy / Fall / Jerk / TensionFall / Stay / Hook / RetrieveStarted / RetrieveStopped / EndFishing / NextCast。キー割当はD12。

釣りセッション開始はGame側の `StartFishing()` 要求でInactive→Readyへ入り、投入とは分ける。`ETRCommandResult` は Accepted / RejectedInvalidState / RejectedBusy / RejectedInvalidEnvironment / RejectedMissingData。戻り値だけでUIが先行遷移せず、状態変更通知を待つ。

`FTREgiAction`はFishingからEgiSimulationへ渡す値型で、FishingState、`ETRLineMode`（Payout / ControlledPayout / Locked / ReelIn）、SinkScale、LiftMps、ReelMpsを含む。EgiSimulationが入力やAI状態を再解釈しない。`StepEgi`はSnapshotと `ETREgiStepEvent`（None / ReachedBottom / LeftBottom / Retrieved / EnvironmentInvalid）を返し、Fishingだけが操作状態を遷移させる。

**D03（同日補足を反映）**: シャクリ回数は上限なし。1入力1動作で連続入力可能、各終了後TensionFallへ移る。Jerking中の入力はPendingJerkCountへ数え、TensionFallで次の1件を取り出してJerkingへ進む。動作を並列実行/合成しない。明示Fall/Stay/回収/中断では未実行の予約を取消す。実行前の予約をシャクリ回数に算入しない。カウンタはint64、ゲーム上の回数制限としてオーバーフローを利用しない。

**一連の回数と後続STAYの契約（技術設計）**:

- JerkCountは投内の実行合計、SeriesJerkCountは現在の一連の実行回数。1動作開始時に両者を1増加する。
- Stay初回進入でStayPenaltyJerkCount=SeriesJerkCountを固定し、bSeriesClosed=true。Stay中のBITE計算へこの整数値だけを渡す。10回以下は減衰なし、11回から回数依存の強い減衰（SQUID_AI参照）。
- 次の実際のJerk開始時、bSeriesClosedならSeriesJerkCountを0にして新しい一連を始める。これにより「再び誘ってStayする」ことで新しい回数が評価される。
- Fall/Stay切替、待機、MISS、BITE期限切れだけではStayPenaltyJerkCountを消さない。Stay再進入時、新しいJerkがなければ前の値を維持する。Stay前にFallを挟んでもSeriesJerkCountを維持する。
- 投終了/新Cast開始で全回数をリセット。イカのCaution/Cooldownは別所有でありリセットしない。新しい一連が始まっても、Stayに入るまではBITE不可。
- AI用FTREgiSnapshotにStayPenaltyJerkCountを追加。Fishingが回数を所有し、AIが減衰倍率を計算する。回数により強制Stayや入力拒否を起こさない。

**D04の時間契約**: TensionFallへ入ったTickから `ceil(AutoStayDelayS/StepSeconds)` Tick後を期限にする（共通換算の整数近傍補正を適用）。その状態で受理した新たな操作で期限を再設定する。既にTensionFallであるだけの重複コマンド、拒否入力、HUD操作は期限を延長しない。Jerkが受理されればJerkingへ移り、次のTensionFall進入時に0.8秒を計り直す。未処理のシャクリ入力があればAutoStayより先に処理する。自動移行はFreeFall/Jerking/Fighting/BottomContactでは発火しない。60Hz・初期値なら48Tick、同Tickの有効入力を期限処理より優先する。

Stay中のStayは冪等でLastActionTickも更新しない。Retrieving中のRetrieveStoppedはStayへ戻す技術案。Fighting中のRetrieveStarted/Stoppedは状態遷移でなく巻きフラグに変換する。

## 4. エギ・ラインの軽量シミュレーション案

### 正本と座標

海面Zを `z_s`、エギ水平位置を `x`、深度を `d` とする。世界位置mは `e=(x.x,x.y,z_s-d)`。`d` はfloatの正本。描画Actorに物理シミュレーションを有効化しない。毎ステップ移動先のOceanを再取得し、海底と海面を検証する。

船の竿先 `r`、ライン長 `L`、竿先とエギの距離 `D=|e-r|`。角度は鉛直下向きから `atan2(水平距離, max(r.z-e.z,ε))`、表示はdegreeへ変換する。L>=0、`0<=d<=BottomDepthM`、有限数を不変条件とする。

### 積分順と単位

1. 総重量M[g]からエギの `SinkSpeedByTotalMass(M)` [m/s]、`HorizontalResponseByTotalMass(M)` [1/s]を読む。沈下は下向き正。沈下曲線は重量増で非減少を推奨するが、実測校正前のゲーム近似である。
2. 当地の潮 `u` [m/s] に対し、水平速度を `v_xy += (u_xy-v_xy)*(1-exp(-k(M)*dt))` で応答させる。重量が増えるほど応答が鈍る曲線を仮定。船速度を潮へ加算しない。
3. 深度速度を `dDot = SinkSpeed(M)*StateSinkScale - u.z - JerkLiftMps` とし、`xTrial=x+v_xy*dt`、`dTrial=d+dDot*dt` を得る。StateSinkScaleは状態別設定。
4. ラインモードにより長さ更新。FreeFallは `L += PayoutMps*dt`、TensionFallは `L += TensionPayoutMps*dt`、Stayは固定、Jerking/Retrievingは `L=max(MinLineM,L-ReelMps*dt)`。Payout上限とReel速度は設定化する。
5. 竿先から試行位置への距離がLを超えたら、`eProjected = r + normalize(eTrial-r)*L` として球面へ戻す。余長がある時は補正しない。ラインは引張のみで押さない。
6. 移動先の海を問い合わせ直す。海面/海底を越えたら深度を制限し、ライン制約を再検査する。小さな固定反復回数で双方を満たすか確認し、収束しない/竿先と最小ライン長が矛盾する場合は環境エラーで安全中断する。無限ループやNaN継続は禁止。
7. 補正後位置を正本へ保存。速度は補正後の位置差から再算出して上限を適用。海面高さが変化する場合は前ステップの世界位置も保持し、深度差と世界Z速度を混同しない。

`StateSinkScale`はFreeFall、TensionFall、Stayの各設定。STAYでも沈下項を0に固定しない。ライン制約、潮への応答、竿先の移動によって最終レンジが決まる。Stay進入時は既存のLを維持して固定する技術設計。D05で数値シミュレーション方式を決定済みであり、式・係数はDataAssetで調整する。MVPではu.z=0、風と波の力を加えない。

`Tension01 = clamp(CorrectionDistanceM / TensionReferenceM,0,1)` は表示・挙動調整用の代理値でありニュートンではない。ケーブルの張力、糸伸び、抗力分布、弛んだライン形状は解かない。`TensionReferenceM>0`、補正量は固定刻み基準で調整する。

投入時は海面上の竿先からエギ初期位置までの距離以上にLを初期化する。海中モデルで竿先そのものまで回収しようとすると海面制約と矛盾するため、回収終点は「竿先直下の海面近傍、水平距離と深度がRetrievalToleranceM以内」とする技術案。到達時にRetrievedを返し、海中モデルを終了してエギを非表示/装備表示へ切り替える。回収中の最小長は竿先の海面上高さ以上とし、上空まで海中エギを引き上げない。演出と許容距離はD02/D13で調整する。

### 総重量が与える4つの影響

| 要件 | 本モデルの因果関係 |
|---|---|
| 沈下速度 | M→沈下曲線→dDot |
| 潮への影響 | M→応答係数k→水平速度 |
| 船ドリフトへの追従 | 船の竿先移動→ライン制約。Mによる沈下/水平位置の差→補正量の差 |
| STAYレンジ変化 | M・潮・船・固定L→ライン角度と補正→DepthM |

同じ潮・船・Lで軽重を比較できるログを提供する。常に重いほど深いという断定はしない。ライン制約や海底に達した場合は差が消えることがある。

### シャクリ

1回の動作開始でJerkCount/SeriesJerkCountを各1増加し、`JerkDurationS` の間 `JerkLiftMps` と `JerkReelMps` を適用する。予約の受理時に回数や移動量を先取りしない。上昇量は速度×時間から決まり、瞬間移動量を二重加算しない。海面を越える上昇はclamp。動作時間・振幅・巻取りはDataAsset調整値。アニメーションは同じ動作開始イベントを受けるのみ。

## 5. フッキングと簡易ファイト

Hookの時刻契約、BITE成立条件、Cautionは [SQUID_AI](SQUID_AI.md) が正本。Hookは一つのトークンを一度だけHit/Miss/Cancelledとして解決する。HitならCoordinatorが対象の `TryClaimForFight(CastId)` を実行して捕獲中にし、成功後にFightingへ移行する。対象喪失時はHitを表示せずCancelled/TargetLostで終了する。

**MVP決定D09: 簡易テンション＋巻上げ進捗、過大テンション継続でバラシ。** 以下の更新式はそれを実現する技術設計。

- `ETRFightState`: Inactive → Active → Won / Escaped / Aborted。終端通知は1回。
- BeginFightでProgress01=0、FightTension01=InitialFightTension01、OverTensionTicks=0に初期化。
- 巻き中は `FightTension01=clamp(FightTension01+ReelTensionRisePerS*dt,0,1)`。離すと `FightTension01=max(0,FightTension01-TensionRecoveryPerS*dt)`。この値はFight専用の無次元代理値で、エギのTension01と混ぜない。
- 更新後 `FightTension01>OverTensionThreshold01` なら連続超過Tickを1増加、それ以下なら0へ戻す。`OverTensionTicks>=ceil(OverTensionDurationS/StepSeconds)` でEscaped。ポーズ中は進めない。合計超過時間ではなく**連続**時間。
- まだEscapedでなければ、巻き中 `Progress01=min(1,Progress01+ReelProgressPerSecond*dt)`、離した時は進捗維持。1でWon。同Tickで進捗完了と継続超過が競合した場合はEscapedを優先する技術契約。
- ファイト中はエギ積分を停止し、表示だけ進捗で補間。Wonは論理Landing→Caught結果へ。Escapedは対象をAvailable/Cautionへ戻し、最後の有効エギSnapshotからStayへ再開しbHadEscapeを保持する技術案。見た目のFight補間座標を海中の正本に逆流させない。
- 低テンションによるバラシ、ランダム逃走、詳細ドラグ、重量別引き、進捗減少はMVPに追加しない。
- D10のMVP暫定仕様に従い、WeightKgはテスト個体の固定設定。HIT/取り込みで再抽選せず結果にコピーする。重量分布はAlpha前の再決定まで未実装。
- EndFishing / レベル終了はAborted。Won以外でCaughtを作らない。

調整値: InitialFightTension01（0..閾値）、ReelTensionRisePerS>0、TensionRecoveryPerS>0、OverTensionThreshold01（0より大きく1未満）、OverTensionDurationS>0、ReelProgressPerSecond>0。全てFishingTuningへ置く。製品向け数値は未指定。試験例は初期0.2、上昇0.4/s、回復0.5/s、閾値0.8、継続0.5秒、進捗0.1/sとし、無休の巻きがバラシ、休止を挟む巻きが成功することを検証する。

**MVP決定D13**: MISSはHookイベントでありOnCastCompletedを発行しない。Stayを継続でき、Fall入力で再フォールできる。同じ投の再BITEはCooldown後。プレイヤーの回収が完了した時だけ通常の未釣獲の投を終了する。結果理由は技術契約としてbHadEscapeならEscaped、次にbHadMissならMissed、それ以外Retrievedとする。成功はCaught、明示中断/環境異常はAbortedで別に終了する。バラシをMISSに読み替えない。

## 6. 設定・Blueprint公開

DataAsset項目: ProfileId別SinkSpeedByTotalMass/HorizontalResponseByTotalMass曲線、最大ライン長、PayoutMps、TensionPayoutMps、MinLineM、ReelMps、状態別SinkScale、JerkDurationS/LiftMps/ReelMps、`AutoStayDelayS=0.8`、TensionReferenceM、MaxEgiSpeedMps、`HookOpenDelayS=0.10`、`HookCloseDelayS=0.55`、上記Fight調整値、回収閾値、VisualInterpolation設定。D03のBITE減衰曲線はSquidTuningにのみ置く。固定ステップはSession側。初期値指定のない項目に製品確定値を捏造しない。

BlueprintReadOnly: State、DepthM、JerkCount、SeriesJerkCount、StayPenaltyJerkCount、PendingJerkCount、AutoStay残り秒、装備ロック/ID/総重量、ライン角度、エギ張力代理値、FightTension01、OverTensionTicks、FightProgress、CatchResult。
BlueprintCallable: Controller経由のコマンド送信、読取スナップショット取得。`SetDepth` / `SetState` / `ForceHit` は製品BPに公開しない。
BlueprintAssignable: OnFishingStateChanged、OnBottomContact、OnHookResolved、OnCastCompleted。描画ActorはSnapshot適用APIのみ。

## 7. テスト

| ID | 条件・手順 | 期待 |
|---|---|---|
| F01 | 3×9全装備の総重量計算、0g/負値/欠損ID | 27組合せ、30〜90g、0g有効、負値/欠損拒否。初期エギ35g |
| F02 | 無潮・平底・十分なラインでFreeFall | テスト用沈下速度×時間と深度が誤差内一致 |
| F03 | 斜面へ移動、海底より深い試行位置 | 海底貫通なし、着底イベントは接触開始時のみ |
| F04 | 同じ形状曲線、35g/80g、潮と船速度一定 | 重量による沈下/水平応答/追従差が記録される |
| F05 | Stay・固定Lで船のみ移動 | 水平位置とレンジが変化、L不変 |
| F06 | 1/10/11/20回、動作中の連続入力、予約取消 | 回数上限なし、受理入力を逐次実行。取消済予約は回数に含めない、強制Stayなし |
| F07 | Stay→Fall→着底→Jerk→Stay | 再フォール成立、旧BITE無効 |
| F08 | TensionFall開始T、60Hz、初期0.8秒 | T+47はTF、T+48でStay。期限TickのJerk優先、再TFで再計時。調整値変更でも対応Tick一致 |
| F09 | 同じ完了イベント2回、Result中に古いHook | 結果1件、釣果重複なし |
| F10 | Fightで安全な巻き/休止を反復 | 巻きでテンション/進捗増加、休止でテンション低下/進捗維持、成功1回、重量固定 |
| F11 | 中断・エギ破棄・次投100回 | CastId増加、古い参照・タイマー・delegate残存なし |
| F12 | ゼロ距離、最短L、最大L、無効海、NaN設定 | ゼロ除算なし、不正設定開始拒否、環境不正は中断 |
| F13 | 過大テンションが閾値以下/継続必要Tickの1つ前/到達 | 閾値以下で連続時間0、必要Tickでバラシ1回。短い超過の合算ではバラシにならない |
| F14 | 同Tickでバラシ条件と進捗完了 | バラシ優先、Caughtなし、回収までは同CastId維持 |
| F15 | MISS→Stay→Fall→再誘い→回収 | MISSでは結果なし、CastId維持、回収完了で初めて結果1件 |
| F16 | 釣り開始前/開始後Ready/投中/次投Ready/EndFishing後で装備変更 | 前と終了後だけ許可。投間も装備ロック継続 |
| F17 | FreeFall/Jerking/Fightingで0.8秒待機、Pause/再開 | 無関係状態のAutoStayなし、Pause中に期限が進まない |
| F18 | 11回→Stay→Fall→Stay、MISS、次に1回Jerk→Stay | 切替/MISSだけでは減衰回数11を保持、新しい一連のJerk後は1へ更新。新Castで0 |

数値許容誤差は試験側で明示（例: 静水直線積分0.001m）。ラインと海底の両制約に解がない試験も含める。確率試験と操作試験の乱数を混在させない。
