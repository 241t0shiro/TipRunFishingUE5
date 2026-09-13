# アオリイカAI・BITE技術設計

2026-09-13 M10.5設計改訂（実装未着手）: M00〜M10の実装・自動試験成功は履歴として保持するが、ユーザーのM10後PIE評価は再現性・操作性の品質不合格。M11への進行はM10.5品質ゲート合格まで保留する。本書のM10.5改訂契約を旧記述より優先し、M03〜M10完了記録は旧実装の証跡として読む。今回はMarkdownのみ更新し、改訂機能の実装・ビルド・試験は行っていない。

2026-09-13 M10実装反映: Shakuri/TensionFall/Stay/Re-Fall/Retrieve、一投単位の装備ロックと船上帰還後の次投変更、深度速度・境界接触Snapshotを実装済み。旧AutoStay項目は型/検証/Prototype設定から撤去済み。M06〜M09記録は当時の履歴として保持し、現在の契約・検証範囲はROADMAPのM10完了記録を参照。M11以降は未着手。

関連: [全体正本](GAME_DESIGN.md)、[釣り](FISHING_SYSTEM.md)。生態の厳密な再現モデルではなく、校正可能な行動近似を設計する。活性・季節・重量分布を科学的な既知値として固定しない。

更新: v0.2 / 2026-09-12。D07の活性3段階/距離/レンジ差/STAY時間、D06の受付初期値、D03補足の10回超BITE減衰を適用。D10/D11はMVP暫定仕様、製品仕様はAlpha前に再決定。

## 1. 責務・クラス・依存

| クラス / 継承元 | 責務・主要Component | 主要変数 | 主要関数 |
|---|---|---|---|
| `ATRSquidActor : AActor` | 個体の寿命、識別、表示。SceneRoot、仮StaticMesh、BehaviorComponent | SimId、WeightKg:float、Behavior参照 | `InitializeSquid()`、`ApplyVisualSnapshot()`、`TryClaimForFight()`、`ReleaseFromFight()`、`EndPlay()` |
| `UTRSquidBehaviorComponent : UActorComponent` | AI状態、位置、追跡、反応スコア。SquidActorに配置 | State、StateEnteredTick、PositionXYM、DepthM、ActivityLevel:ETRSquidActivityLevel、PreferredDepthM、TargetCastId、RangeExposure01、StayElapsedS、JerkBiteMultiplier、CautionUntilTick、CooldownUntilTick、Disposition、FRandomStream | `StepAI(dt,Target,Ocean)`、`EvaluateInterest()`、`CanAttack()`、`ComputeJerkBiteMultiplier()`、`BuildBiteRequest()`、`OnBiteAccepted()`、`OnBiteRejected()`、`OnHookResolved()`、`EnterCaution()` |
| `UTRSquidTuningDataAsset : UDataAsset` | 行動時間・距離・確率・レンジ設定 | 下記データ項目 | `IsDataValid()` |
| `UTRHookComponent : UActorComponent` | Fishing側の単一受付・合わせ判定 | Token、OpenTick、CloseTick、Resolved | `TryOpenBite()`、`EvaluateHook()`、`ExpireBite()`、`CancelBite()` |

`ATRSquidCharacter`は使用しない。水中の簡易移動にCharacterMovementや陸上NavMeshが不要なためActorを選ぶ。Behavior Tree、StateTree、Mass、AIControllerはMVPで導入しない。将来移動方式を交換してもBehaviorの値契約を維持する。

`UTRSquidBiteComponent`もMVPでは追加しない。BITE受付の所有者をHookと重複させないため。AIはBITEを要求し、Hookが承認する。Coordinatorが両者を接続する。

`ETRSquidDisposition`: Available / Hooked / Caught / Removed。これは釣獲上のライフサイクルで、AI状態とは別。Available以外ではStepAIを実行しない。Hooked中はFight表示に追従、Caughtは結果へ重量をコピーした後に個体を除去する。MVPはテスト個体1体。釣獲して個体がなくなった次投の試験準備では同じ固定設定で1体を再生成する技術設計。MISS/バラシで残っている個体を再生成してCooldownを回避しない。バラシ通知ではAvailable/Cautionへ戻し、Fight前の有効位置から再開する。

## 2. 状態遷移

`ETRSquidState`: Idle / Interested / Approach / Attack / Bite / Caution / Cooldown。

| 現在 | 条件 | 次 | 処理 |
|---|---|---|---|
| Idle | 有効ターゲット、感知距離内、Interest判定成功 | Interested | 対象CastIdを保存 |
| Interested | 注視時間経過、対象有効 | Approach | 追跡開始 |
| Interested / Approach | 対象消失、感知範囲外 | Idle | 対象解除・露出値リセット |
| Approach | 距離<=AttackDistanceM、対象Stay、Attack判定成功 | Attack | 攻撃開始Tickを固定 |
| Attack | 対象Stay、距離<=BiteDistanceM、AttackDuration経過、Bite判定成功 | Bite要求 | まだStateをBiteにしない |
| Attack | Hookが要求承認 | Bite | Tokenと開始時刻を受領。Cue発行はHook側1回 |
| Attack | 拒否、Attack最大時間超過、Stay解除、距離超過 | Caution | 即時要求再送しない |
| Bite | 合わせHit | AI停止 | Disposition=Hooked。Fightへ引き渡す |
| Bite | Early / Late / 期限切れ / 操作で解除 | Caution | Token解除、警戒開始 |
| Caution | CautionDuration経過 | Cooldown | 再反応禁止期間開始 |
| Cooldown | CooldownDuration経過 | Idle | 対象解除、再反応可能 |
| Availableの任意状態 | 個体削除・海域終了 | AI停止 | Disposition=Removed、予約解除 |

MVPでは**FreeFallとTensionFallを含め、Stay以外でBiteを承認しない**。FALL中BITEを将来追加する場合は別途仕様変更する。Interested/Approachは非Stayでも可とする技術設計で、誘いに寄ってからSTAYで食わせる流れを表現する。Attack/Biteの判定要素はD07でMVP決定済み。

同Tickで複数状態を連鎖してIdle→Biteに飛ばさない。1回のStepAIにつきAI状態遷移は最大1回。要求承認とDisposition変更はその後の明示通知として扱う。

## 3. レンジと行動評価

### レンジ評価

イカの好適深度 `PreferredDepthM`、レンジ半幅 `RangeHalfWidthM>0`、エギ深度 `d` に対し:

`RangeScore = clamp(1 - abs(d-PreferredDepthM)/RangeHalfWidthM, 0, 1)`

TensionFall/Stay中の好適レンジ滞在履歴を評価する指数移動平均:

`Exposure += (RangeScore-Exposure) * (1-exp(-dt/ExposureTimeS))`

TensionFall/Stay以外では `Exposure *= exp(-dt/ExposureDecayS)`。時定数は正。ターゲットCastIdが変わったらExposure=0。短い通過とレンジ維持を区別する。Exposureはエギが好適レンジにいる履歴であり、イカが現在エギと同じ深度にいることとは別。

距離 `R` は水平と深度を合わせた3D距離m。`DistanceScore = clamp(1-R/DetectRadiusM,0,1)`。活性は `ETRSquidActivityLevel { Low, Medium, High }` の3段階で、SquidTuningの段階別Interest/Attack/BiteRateScaleを読む。任意のActivity01を製品入力として併用しない。係数は0以上、Low<=Medium<=Highで検証する。MVPでは季節IDから補正を引かず、季節設定がなくても起動・反応する。

`StayElapsedS`は対象の連続Stay時間。Stay進入で0、Stay離脱で0、対象CastId変更で0。`StayTimeScore=clamp(StayElapsedS/StayBuildUpS,0,1)`、StayBuildUpS>0。レンジ履歴Exposureと区別する。過渡処理終了によるStay進入から測定し、状態を継続評価するだけではリセットしない。StayBuildUpSは反応の評価時定数であり、Stay移行待ち時間ではない。

### 釣合いによるレンジ維持評価（技術設計、製品係数未確定）

MVPの評価基準となる狙いレンジは対象イカのPreferredDepthMとする技術案。Stay進入時のエギ深度で毎回目標を上書きしない。プレイヤーの目標レンジ入力UIは追加しない。実行時指標と調整データを分離する。

- `RangeErrorM = d - PreferredDepthM`（下向き正）。上昇/下降の方向はEgiSnapshotの`DepthVelocityMps`で読む。小さい誤差が好ましく、従来のRangeScoreが現在の近さを担う。
- `RangeStability01`はTensionFall/Stay中の深度変化速度の絶対値を平滑化して評価する。技術式は `SpeedEMA += (abs(DepthVelocityMps)-SpeedEMA)*(1-exp(-dt/RangeStabilityTimeS))`、`RangeStability01=clamp(1-SpeedEMA/RangeStableSpeedMps,0,1)`。上昇と下降で同じ尺度を使い、上下振動が相殺されて高評価にならないよう符号付き平均は使わない。
- `RangeHoldScore = RangeScore * RangeStability01`。好適レンジ外の静止や好適レンジを速く通過する状態より、好適レンジを維持した状態を高く評価する。海底/海面の制約で静止しているTickは釣合い維持とみなさずRangeHoldScore=0とする。
- `RangeHoldBiteMultiplier = RangeHoldBiteScaleByScore(RangeHoldScore)`。曲線は0..1の有限値・単調非減少、最高スコアで最大倍率。上昇/下降を表す比較用スコアでは理想維持より厳密に低い倍率とし、定数曲線は受け入れない。倍率の製品値は未指定。

RangeStabilityTimeS>0、RangeStableSpeedMps>0、RangeHalfWidthM>0、RangeHoldBiteScaleByScoreはSquidTuning DataAssetへ置き、IsDataValidで有限数/正の許容幅・時定数/曲線域と単調性を検証する。設定はセッション開始時に凍結する。指標そのものを固定値として保存しない。TensionFall→Stayでは履歴を保持し、両状態からの離脱・対象CastId変更・対象喪失でSpeedEMAの履歴を破棄、次の有効サンプルの絶対速度で初期化する。未計測時はRangeStability01=0。Pause中は更新しない。Exposureの減衰は上記の別契約を維持する。

レンジ評価は釣り状態を変更しない。TensionFall中に高得点でもBITEは承認しない。活性・距離・Exposure・Stay時間・シャクリ回数を揃えた比較で、釣合い維持が上昇/下降より高いBITE確率になることを保証する。理想維持でも必中ではなく、既存の距離/Stay/対象/受付条件と10回超減衰を維持する。

### フレームレート非依存の確率

段階別ハザードλ[1/s]に対し、1固定ステップの成功確率を `p = 1-exp(-lambda*dt)` とする。`lambda>=0`、有効時のみ乱数 `U in [0,1)` を1回引き `U<p` で成功。Δt当たり固定%を使わない。

- `lambdaInterest = BaseInterestRate * ActivityProfile.InterestScale * DistanceScore * BehaviorInterestScore`
- `lambdaAttack = BaseAttackRate * ActivityProfile.AttackScale * DistanceScore * RangeScore * StayTimeScore`
- `lambdaBite = BaseBiteRate * ActivityProfile.BiteScale * DistanceScore * RangeScore * Exposure * StayTimeScore`

上記は**MVP決定D07を実現する技術式**。BehaviorInterestScoreは状態別テーブル0..1。Attack/BiteのStay条件・距離条件・Disposition条件は確率計算前のハードゲート。範囲外ならλ=0。Biteの待機はAttackDuration以降、AttackMaxDurationまでとする。季節補正は式・必須データから除外する。

### シャクリ10回超のBITE減衰（D03補足）

Fishingの `StayPenaltyJerkCount=N` を使用し、`Excess=max(0,N-10)`。N<=10なら倍率1。N>=11では `BiteProbabilityScaleByExcessJerks` 曲線（横軸=超過回数、縦軸=0..1）から、回数に応じて極端に小さくなる倍率を取得する。1回超過で大きく低下し、以降は単調非増加。定義域外は末尾の小さい値に固定し、外挿で回復させない。これは回数制限ではなく確率の減衰である。

倍率はハザード自体ではなく基礎成功確率へ適用する: `pBiteBase=1-exp(-lambdaBite*dt)`、`pBite=pBiteBase*RangeHoldBiteMultiplier*JerkBiteMultiplier`。これにより高λでも「極端に低下」が消えない。判定は1回だけ乱数を引く。基礎ハザード方式と固定Tickは維持するが、刻みを変更する場合は減衰後の時間当たり確率を再校正する。

係数の製品初期値は未指定。**試験用曲線例**は超過0→1、1→0.1、2→0.01、3→0.001、10→0.000001、以降末尾固定、区間は線形補間とする。10回/11回/12回/13回/20回で倍率1/0.1/0.01/0.001/0.000001を確認する。実際のMVP調整では「11回以上が明確に極端な低下」であることを比率ログとプレイ試験で確認し、任意の緩い曲線へ置き換えない。

適用対象は**後続StayのBITEのみ**。ATTACK/接近確率をこのペナルティで直接減らさない。回数の区切り・Fall/Stayでの保持・次の一連での更新はFISHINGが所有する。AIのCooldownやTarget取得処理で回数を初期化しない。

イカ位置は `SpeedMps*dt` を上限とする対象方向への移動で更新し、通り越しを防ぐ。海底/海面を制限する。Idleは固定位置、Approach/Attackは接近、Biteはエギ付近に留まる簡易方式を暫定案とする。遊泳・群れ・障害物経路探索は将来。

## 4. BITE承認契約

`TryOpenBite(Request,TargetSnapshot,T) -> FTRBiteDecision`:

1. CastId一致、エギがStay、対象Available、距離条件成立を最新スナップショットで再検査。
2. 未解決トークンがある場合は拒否。今Tickに早合わせ/取り消しがあった投も拒否。
3. Coordinatorによる候補順位（距離→SimId）で1件だけ処理。MVPが1イカでもAPIは複数要求に対応。
4. Token、BiteStartTick、OpenTick、CloseTickを保存。Cueを1回発行しAIへ承認通知。

拒否理由: StaleCast / NotStay / Busy / InvalidTarget / OutOfReach / SuppressedThisTick。拒否したAIはCautionへ進み、毎Tickの再送を禁止する。

## 5. 合わせ時間の厳密な境界

設定 `HookOpenDelayS >= 0`、`HookCloseDelayS > HookOpenDelayS`。GAME_DESIGNの共通秒→Tick換算（整数近傍補正後ceil）を使い、`CloseOffsetTicks > OpenOffsetTicks` を再検証する。

`OpenTick = BiteStartTick + OpenOffsetTicks`
`CloseTick = BiteStartTick + CloseOffsetTicks`

受付は **OpenTick <= InputTick < CloseTick** の半開区間。ちょうどOpenTickはHIT、CloseTickはMISS。体感は固定ステップ分の量子化を伴う。

| 入力時の条件 | Outcome / Reason | 消費・通知 |
|---|---|---|
| 有効Token、InputTick<OpenTick | Miss / Early | Token消費、Cautionへ |
| 有効Token、受付区間内、対象有効 | Hit / InWindow | Token消費、対象をFight用に確保 |
| 有効Token、InputTick>=CloseTick | Miss / Late | Token消費、Cautionへ |
| Token無し | Miss / NoBite | そのTickのBiteを抑止。追跡中対象へCaution通知する技術契約 |
| 対象消失 / Stay解除 / Cast終了 | Cancelled / 対応理由 | HIT/MISSの音を鳴らさず解除 |
| 解決済Tokenの再解決 | Ignored / AlreadyResolved | 新しい結果イベントなし |

入力が無い場合、時刻T>=CloseTickで `Miss / Expired` を1回発行する。その後のHook入力はNoBiteであり、前のTokenのLateを再発行しない。CloseTickちょうどの入力をLateと判定できるよう、固定更新では入力処理を期限切れ処理より前に行う。

Attack中の合わせはNoBite/Early相当の表示。D06のMVP初期値は `HookOpenDelayS=0.10`、`HookCloseDelayS=0.55` で、BITE開始基準の受付は `[0.10秒,0.55秒)`。両方ともFishingTuningで調整可能。早合わせ/時間切れはMISSでTokenを消費し、後の連打で同TokenをHITへ変更しない。追加の連打ペナルティは導入しない。D13に従い、MISSで投を終了させずStay継続または再Fallを許す。

### MVP初期値の境界試験

60Hz、BiteStartTick=100、OpenOffset=6、CloseOffset=33として、105=Early、106=Hit、132=Hit、133=Late。入力なしの133=Expired。BiteStartTickだけが試験値で、0.10/0.55秒は承認済みMVP初期値。秒→Tick変換は倍精度で行い、整数境界の浮動誤差で0.55秒が34Tickにならないよう、整数に十分近い比を丸めてからceilする技術契約。許容差は1e-6 Tickとし、境界試験を必須にする。

## 6. Caution / Cooldownとアタリ

Caution中は攻撃をしない。Cooldown中はInterestも評価しない。両期間はTickで管理し、再投入でリセットしない。同一イカの警戒をCastId変更だけで回避できないようにする。テスト個体の再生成は明示した試験リセットのみ。

`FTRBiteCue`: Token、StartTick、`ETRBiteCueType`、Intensity01、DurationS。enumは `Prototype / TipLoad / TipUnload / Subtle`。D11のMVP暫定仕様はPrototypeの仮Cue1種類のみ。将来の3種はティップが入る/戻る/モゾモゾを表し、型は予約するが演出分岐はMVPに実装しない。大型との相関、重量→Intensity曲線等の製品仕様はAlpha前に再決定する。

Cueは表示要求でありBITE判定の入力ではない。描画が遅延しても受付時間を延長しない。ポーズではAI・受付・表示の進行も止める。再開時に保留された合わせ入力を送らない。

## 7. 設定とBlueprint

DataAsset: DetectRadiusM、AttackDistanceM、BiteDistanceM（Bite<=Attack<=Detect）、ApproachSpeedMps、AttackSpeedMps、InterestedDurationS、AttackDurationS、AttackMaxDurationS、BaseInterest/Attack/BiteRatePerS、3段階ActivityProfiles、BehaviorInterestScoreByFishingState、RangeHalfWidthM、RangeStabilityTimeS、RangeStableSpeedMps、RangeHoldBiteScaleByScore、ExposureTimeS/DecayS、StayBuildUpS、CautionDurationS、CooldownDurationS、BiteProbabilityScaleByExcessJerks。季節補正データはMVP必須項目に含めない。無減衰の境界10回は確定ルール、曲線の倍率が調整対象。

個体の初期深度・固定重量・3段階活性はテストシナリオ/配置データ。テスト用イカは1体、重量は個体生成から釣果まで固定。量産用重量分布はAlpha前のD10再決定まで保留。Hook受付値はFishingTuningにのみ置く。

BlueprintReadOnly: AIState、WeightKg、Position、ActivityLevel、Cue。RangeErrorM/RangeStability01/RangeHoldScore/RangeHoldBiteMultiplier/Exposure/StayElapsedS/λ/StayPenaltyJerkCount/JerkBiteMultiplier/Token期限は開発HUD専用。BlueprintCallable: 読取のみ。攻撃強制・重さ書換え・HIT強制は製品公開しない。テストの強制BITEは開発用C++試験APIで、Stay/対象/Token制約を迂回しない。

## 8. テスト

| ID | 検証 | 期待 |
|---|---|---|
| S01 | 全AI状態の遷移と禁止遷移 | 許可した遷移のみ、Idleから瞬間Biteなし |
| S02 | FreeFall / TensionFall / Jerkingで高λ | BITE数0。Stayだけ承認可能 |
| S03 | 範囲中心/範囲外で同じ入力とseed群 | 範囲外λBite=0、中心・維持で高い反応率 |
| S04 | λ=2/s、維持倍率とシャクリ倍率を1に固定、適格状態1秒、独立seed 10,000試行 | 理論 `1-exp(-2)` と事前設定許容差±0.02内 |
| S05 | 初期0.10/0.55秒、Tick105/106/132/133を独立試験 | Early/Hit/Hit/Late、133で無入力はExpired。開閉設定変更時も対応する境界で一致 |
| S06 | 早合わせ後に窓内で連打 | 同じTokenでHITに覆らない |
| S07 | 二個体が同Tickで要求 | 距離→IDで1件承認、他はCaution |
| S08 | MISS後に再投して近くへ投入 | Caution/Cooldown終了まで再Biteなし |
| S09 | Bite中にFall、回収、個体破棄、終了 | 受付解除、次Tickで古いToken使用不可 |
| S10 | 同seed・同Tick入力列を30/60/120描画fps | AIイベント順・結果一致、深度は規定誤差内 |
| S11 | Bite生成と同TickにHookコマンド | NoBite扱いで新規要求抑止、未来のBiteで成功しない |
| S12 | ゲームポーズで受付期間を跨ぐ実時間待機 | sim期限不変、再開直後の保留入力なし |
| S13 | Low/Medium/High、同距離/レンジ/STAY時間 | 設定段階に応じAttack/Bite率が非減少、季節データなしで動作 |
| S14 | 同じ条件で距離/レンジ差/STAY時間だけ変更 | 各要素が式に反映。Stay継続評価で時間リセットなし、Fall離脱で0 |
| S15 | テスト曲線、10/11/12/13/20回、同じ基礎λ・維持倍率 | pBite比が1/0.1/0.01/0.001/0.000001、Attack率は不変、20回超も入力可 |
| S16 | 11回Stay→Fall→Stay、MISS/再取得 | 減衰保持。新しい一連のJerk後のStayでのみ新しい回数へ更新 |
| S17 | 曲線範囲外/負倍率/増加曲線、λ極大 | 範囲外は末尾固定、不正曲線拒否、高λでも減衰倍率維持 |
| S18 | MVPの複数BITE通知、固定重量の捕獲/バラシ | CueはPrototypeのみ、重量再抽選なし、バラシでCautionへ |
| S19 | M11: 同レンジで維持/上昇/下降/上下振動、範囲外静止、境界静止、TF→Stay、Pause、対象変更 | 誤差と絶対速度履歴を反映、維持が最高。境界静止は維持加点なし。TF→Stayで履歴保持、未計測/リセット/凍結/不正設定を検証 |
| S20 | M12: 他条件を揃え、理想維持/上昇/下降の計算確率とDataAsset変更を比較 | 理想維持のpBiteが最大、上昇/下降で低下。非Stayは0、10回超減衰と両立。単発の乱数結果ではなく式と多数試行で検証 |

確率試験は分布用の隔離された計算器で実行し、接近移動やCooldownによる有効時間短縮を混ぜない。統合試験では自然反応と試験要求の両方を使い、強制成功だけでAI完成としない。

## M10.5との接続境界（設計のみ、M11以降未着手）

M10.5品質ゲート合格をM11開始条件とする。Egi World Positionから導出したDepthM/DepthVelocityMps、同Tickの水平位置、海底/海面接触、状態/観測有効性、CastIdを読む。旧DepthM正本への書戻しを要求しない。RangeError/RangeStability/RangeHoldScoreとBITE確率の本評価は既存M11/M12に残す。

QuickRetrieving中は水中評価対象無効、Attack/Bite/Hook不可。開始時に既存の対象/要求/Tokenが存在する後続実装では解除し、完了/Readyや新Castで復活させない。M10.5はこの入力/状態契約と不適格性の試験だけを用意し、AI/Token処理そのものを先行実装しない。海底/海面静止をレンジ安定の加点にしない従来条件は維持する。
