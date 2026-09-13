# 釣りシステム技術設計

2026-09-13 M10.5設計改訂（実装未着手）: M00〜M10の実装・自動試験成功は履歴として保持するが、ユーザーのM10後PIE評価は再現性・操作性の品質不合格。M11への進行はM10.5品質ゲート合格まで保留する。本書のM10.5改訂契約を旧記述より優先し、M03〜M10完了記録は旧実装の証跡として読む。今回はMarkdownのみ更新し、改訂機能の実装・ビルド・試験は行っていない。


2026-09-13 M10実装反映: Shakuri/TensionFall/Stay/Re-Fall/Retrieve、一投単位の装備ロックと船上帰還後の次投変更、深度速度・境界接触Snapshotを実装済み。旧AutoStay項目は型/検証/Prototype設定から撤去済み。M06〜M09記録は当時の履歴として保持し、現在の契約・検証範囲はROADMAPのM10完了記録を参照。M11以降は未着手。

関連: [全体正本・要決定事項](GAME_DESIGN.md)、[BITE・AI](SQUID_AI.md)。本書の数式と未指定操作は技術提案/暫定案。実測の釣りモデルと称さない。

更新: v0.2 / 2026-09-12。D01〜D09/D13/D15のMVP決定、D10〜D12のMVP暫定仕様を反映。数値式の詳細は技術設計であり、未指定の調整係数はDataAssetで校正する。

## 1. 責務とクラス

| クラス / 継承元 | 責務・主要Component | 主要変数 | 主要関数 |
|---|---|---|---|
| `UTRFishingComponent : UActorComponent` | 操作状態の唯一の所有者。Sessionに配置 | `State: ETRFishingState`、CastId、StateEnteredTick、LastActionTick、JerkCount（投内合計）、SeriesJerkCount、StayPenaltyJerkCount、bSeriesClosed、PendingJerkCount、bHadMiss、bHadEscape、ActiveCommand | `HandleCommand(const FTRFishingCommand&) -> ETRCommandResult`、`Step(float)`、`BeginCast()`、`TransitionTo()`、`AbortCast()`、`GetSnapshot()` |
| `UTREgiSimulationComponent : UActorComponent` | 空間位置・ライン近似の唯一の所有者。Sessionに配置 | WorldPositionM:FVector（M10.5予定）、DepthM/PositionXYMは導出読取、VelocityMps:FVector、LineLengthM:float、LineAngleRad:float、Tension01:float、TotalMassG:float | `InitializeCast()`、`StepEgi(dt,Ocean,Boat,Action)`、`ApplyJerk()`、`SetLineMode()`、`GetSnapshot()`、`Reset()` |
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

装備変更の許可条件は「エギが船上」「現在のCastが終了済み」「初投/次投の準備状態」をすべて満たすこと。初投前は活動中Castがないことを確認する。`StartFishing()`だけでは装備をロックせず、`Deploy`受理時に選択済みエギ・シンカーと総重量・係数のSnapshotを凍結し、一投中は変更を禁止する。Readyという状態名だけで許可しない。

Retrieve完了でCastを終了し、エギの船上帰還を論理状態として確定する。その後の次投準備では、同じ釣りセッションのままエギ・シンカーを変更できる。回収中、MISS後、BITE/Fight中は変更不可。Caught/Abort/EndFishing等でCastだけ終了しても、船上帰還が未確認なら変更不可。船上判定はGame/Sessionの正本で持ち、メッシュの表示/非表示や座標だけから推測しない。

装備変更時にM02の検証と必要なアセット解決を固定更新の外で行い、次投用候補を準備する。Deploy時は候補の有効性と変更許可条件を再検査して、受理とロック/凍結を一体で確定する。同Tickに後着した装備変更は拒否し、進行中Castと過去の結果Snapshotは書き換えない。次投では更新後の総重量・係数・メッシュを使用する。未準備/不正な装備では投入を受理しない。

### M02のデータ実装（2026-09-12）

- `TREquipmentData.h/.cpp`に装備行、`FTREquipmentSnapshot`、`TREquipment::ValidateTables/TryBuildSnapshot`を実装。初期エギIDは`InitialEgiId()`、0g専用IDは`NoSinkerId()`で取得。初期シンカーを製品設定として固定する処理や装備ロック処理は追加せず、ロックはM06で実装する。
- `TRFishingTuningDataAsset.h/.cpp`に`UTRFishingTuningDataAsset`、`FTRFishingParameters`、`FTREgiSimulationProfile`を実装。Fishingの数値設定を保持し、AIのBITE減衰曲線、海・船・入力のDataAsset、表示補間設定はそれぞれの後続タスクで追加する。当時の旧Stay遅延設定とHook 0.10/0.55秒以外の未指定係数は未設定値として初期化し、必要値を欠く設定は検証を通さない。
- M02の重量曲線は、30〜90gを覆う2点以上のキー、有限数、厳密に昇順の横軸、正の縦軸、線形補間を要求する技術実装。未検証のスプラインの負値・オーバーシュートを避ける。Snapshotには総重量で評価済みの沈下速度・水平応答係数と釣り設定を値コピーし、元の曲線・DataAssetへの参照を残さない。
- 行とTuningの`IsDataValid`に加え、`ValidateTables`でテーブル型、論理ID重複、行名とIDの一致、プロファイル参照、全装備の曲線定義域を検証する。エギおよび非0gシンカーのメッシュ参照を必須とし、0gのVisualMeshだけは空を許す。検証は参照メッシュを同期ロードする場合があるため、設定確認時に実行しTickから呼ばない。`TryBuildSnapshot`は固定刻みでの受付窓縮退・時間変換不正も拒否し、失敗時に出力を変更しない。
- `Content/TipRun/Prototype/Data`に`DT_TR_Egi_Prototype`、`DT_TR_Sinker_Prototype`、`DA_TR_FishingTuning_Prototype`をUEのSavePackageで作成済み。装備重量は本書の確定値、メッシュはEngineの仮Cube。係数は`TREquipmentDataTests.cpp`内で明示した試験用のゲーム近似であり、実測値・製品バランスではない。設定は同一形状用TestProfileの曲線を使用する。
- 保存アセットの再読込を含む`TipRun.M02`の6試験が成功。アセットを失った場合のみ、Editorを閉じた状態でUnrealEditor-Cmdに`-TRWriteM02PrototypeAssets`と`-ExecCmds="Automation RunTests TipRun.M02.PrototypeAssets"`を指定して再作成可能。通常の試験は読込のみで、生成モードも既存アセットを上書きしない。

## 3. 操作状態

### M06のSession・最小操作状態（2026-09-13、旧D08の実装履歴）

以下のセッション全体ロック・EndFishing限定解除と旧F16合格は当時の記録。現行D08は第2節を正本とし、M10で一投単位のロックへ移行する。M06で新仕様が実装・検証済みという意味ではない。

- `ATRFishingSessionActor`と`UTRFishingComponent`を追加。SessionはComponentをCreateDefaultSubobjectで所有し、装備・設定参照をUPROPERTY、Coordinatorを弱参照、船をCoordinatorの登録IDで保持する。Fishingが操作状態を所有し、SessionがSessionPhase・CastId採番・装備ロックを所有する。両者の独立Tickはない。EgiSimulation/Hook/FightのComponentと描画EgiActorは、それぞれの後続タスクで追加する。
- `Initialize`へ同一Worldの設定済Coordinator、登録済BoatId、エギ/シンカーDataTable、FishingTuning、明示的な初期シンカーIDを渡す。初期エギはM02の`InitialEgiId()`（3.5号35g）。初期シンカー未指定を0gへ補完しない。設定・選択・竿先位置の海を検証し、成功時はSessionPhase=Ready、FishingState=Inactive。失敗時は理由を返し、有効な設定で再試行できる。
- `TrySetEquipment`は開始前とEndFishing後だけ許可する。`StartFishing`でM02の検証・数値Snapshot作成を再実行し、登録成功後に装備をロックしてFishingState=Readyへ進む。元DataAssetの編集中変更は釣りセッション中と次投にも影響しない。装備解決は同期ロードを伴い得るため、Initialize/TrySetEquipment/StartFishingを固定ステップ内から呼ぶ要求は拒否する。Deploy時はロック済Snapshotをコピーするだけで、アセットをロードしない。
- `SubmitCommand(Type, ExpectedCastId, TargetTick)`でM04へ入力を送る。FTRFishingCommandへExpectedCastIdを追加し、Sessionは自身の現在IDとの一致とTargetTick→Sequence順を検査する。最初の投入前のIDは無効値0、それ以降は直近のCastIdを添える。未来Tickを先に予約しても、受付順だけで誤って拒否しない。拒否結果はGetLastCommandResultで読める。M06の入力実装はDeploy/NextCast/EndFishingだけで、未実装のJerk/Hook等を成功扱いしない。
- ReadyでDeployを受理するとCastIdを増やしDeployingへ移る。同TickのBoat更新後、Fishingフェーズで最新竿先のXYと海面を読み、深度0・初速度0のFTREgiSnapshotを作ってFreeFallへ進む。初期ライン長はMinLineMと竿先の海面上高さの大きい方を使用し、MaxLineLengthMに収まらない配置や海面より低い竿先は拒否/安全中断する。GetActionはFreeFallでPayoutと凍結したSinkScaleを返す。実際の深度・位置・ライン繰出し積分、着底、描画Actorは未実装でありM07以降に残す。M06のEgiSnapshot.Tickは投入確定時刻で、その後の物理更新済み時刻とは称さない。
- F16の次投Readyを検証する最小経路として、明示的な`AbortCast(ExpectedCastId)`→Result→`ResetCast`/NextCast→Readyを実装した。中断結果はAborted・重量0・対象SimIdなしで一度だけ保存し、重複/古いCastIdの中断は拒否する。Result/次投Readyでも装備ロックは維持。EndFishingだけがロックを解除してInactiveへ戻す。CastIdカウンタは投間・釣り開始/終了で戻さない。回収完了・Caught・MISS・Fight結果、UI動線、結果イベント配信は今回の実装範囲外。
- Abort/次投リセットではCoordinator登録を更新して旧登録IDの入力を無効化し、EndFishing/EndPlayで登録とキューを解除する。BeginPlay前の破棄ではEndPlayが呼ばれないためDestroyedでも同じ冪等な解放を行う。登録delegateは弱いUObjectバインドで、終了時は設定参照も解放する。船やOcean自体はSession終了で停止/破棄しない。活動中に船/Oceanの有効性を失った場合はAbortedへ進め、装備ロックは明示終了まで維持する。
- `TipRun.M06`の5試験でF16全境界、初期35g/明示0g、CastId増加・不正入力、Boat更新後の投入、設定凍結、旧入力破棄、未来入力順、ポーズ、環境喪失、BeginPlay前後の破棄を確認。試験の次投は明示中断経由であり、M10以降の回収やM15の結果UI完成を意味しない。Content/Config・保存アセットは変更していない。

STAYは「竿をあおるシャクリ動作をしていない通常の釣り状態」。FreeFall・着底・回収・Fight等まで無入力だけでStayに分類しない。原則の遷移は `Shakuri -> TensionFall -> Stay`。本書のShakuriは既存enumのJerkingに対応し、今回enumの改名は行わない。

`ETRFishingState`:
`Inactive, Ready, Deploying, FreeFall, BottomContact, Jerking, TensionFall, Stay, Retrieving, Fighting, Landing, Result`

**Attack/Bite/HIT/MISSをFishingStateに追加しない。** Attack/BiteはAI状態、HIT/MISSは `ETRHookOutcome`。Bite中のエギはStayのままである。

| 現在 | トリガーと条件 | 次 | 副作用 |
|---|---|---|---|
| Inactive | 釣り開始・環境有効 | Ready | 設定検証、入力接続 |
| Ready | Deploy | Deploying | 船上/活動中Castなし/準備済み装備を検査し、受理と同時にCastId増加、エギ生成、装備固定 |
| Deploying | 配置成功 | FreeFall | キャストなしで竿先付近の海面へ投入、自動繰出し。アニメ終了を待たない |
| FreeFall | 海底に到達 | BottomContact | 深度を海底へ制限、着底通知を1回 |
| FreeFall | Jerk / TensionFall要求 | Jerking / TensionFall | 途中操作を許す暫定案。ライン制御へ戻した後は過渡処理終了でStay |
| BottomContact | Jerk | Jerking | 浮上動作を1回開始 |
| BottomContact | TensionFall | TensionFall | 海底制約は継続、海底接触を優先 |
| Jerking | 動作時間満了 | TensionFall | 動作分の巻取り完了。連続入力の処理はD03の下記契約 |
| Jerking | Jerk入力 | Jerking | PendingJerkCountへ予約、並列実行しない |
| Jerking | 明示Fall | FreeFall | 現動作を中止して予約取消。開始済動作の回数は保持 |
| TensionFall | シャクリ直後の過渡処理終了、予約Jerkなし | Stay | 同じ固定Tickで移行、ライン長を維持して固定。レンジ安定の成否は問わない |
| TensionFall | 海底に到達 | BottomContact | 着底通知 |
| Stay / TensionFall | Jerk | Jerking | 1受理入力につき1動作、回数上限なし。後続Stay用の回数を更新 |
| Stay / TensionFall | Fall | FreeFall | 再フォール。BITE予約/受付を取り消す |
| FreeFall / BottomContact / Jerking / TensionFall / Stay | Retrieve開始 | Retrieving | BITE取消、回収用巻取り |
| Stay | 有効なHook入力でHit | Fighting | ラインモデルの所有を簡易ファイトへ切替 |
| Stay等 | Hook入力がEarly/Late/NoBite | 元の状態 | MISS通知と機会消費。詳細SQUID_AI |
| Retrieving | ライン回収閾値到達 | Result | Cast終了と船上帰還を確定。Caughtにはしない。Escaped / Missed / Retrievedを履歴で選ぶ |
| Fighting | ファイト成功 | Landing | Caught候補を固定 |
| Fighting | 過大テンションが規定時間継続 | Stay | バラシ通知、bHadEscape=true、対象解放。投は回収まで継続する技術案 |
| Landing | 論理的取り込み完了 | Result | Caughtを一度だけ確定。演出完了依存にしない |
| Result | 次投 | Ready | 結果Snapshotを確定保持、Cast固有状態リセット。船上かつCast終了済みなら装備変更可 |
| 任意の活動状態 | 終了・環境無効・対象喪失 | ResultまたはInactive | Aborted、全受付無効。画面動線はD13 |

表にない入力は `RejectedInvalidState`。不正遷移は状態を変えず、開発ビルドで理由を記録する。BottomContactは観測状態でありプレイヤーの「着底ボタン」は設けない。BottomContact中に船の移動でエギが浮いた場合、開いたラインならFreeFall、固定ラインならTensionFallへ戻す。

`FTRFishingCommand`: Type、TargetTick、Sequence、AxisValue。TypeはDeploy / Fall / Jerk / TensionFall / Hook / RetrieveStarted / RetrieveStopped / EndFishing / NextCast。キー割当はD12。

釣りセッション開始はGame側の `StartFishing()` 要求でInactive→Readyへ入り、投入とは分ける。`ETRCommandResult` は Accepted / RejectedInvalidState / RejectedBusy / RejectedInvalidEnvironment / RejectedMissingData。戻り値だけでUIが先行遷移せず、状態変更通知を待つ。

`FTREgiAction`はFishingからEgiSimulationへ渡す値型で、FishingState、`ETRLineMode`（Payout / ControlledPayout / Locked / ReelIn）、SinkScale、LiftMps、ReelMpsを含む。EgiSimulationが入力やAI状態を再解釈しない。`StepEgi`はSnapshotと `ETREgiStepEvent`（None / ReachedBottom / LeftBottom / Retrieved / EnvironmentInvalid）を返し、Fishingだけが操作状態を遷移させる。

**D03（同日補足を反映）**: シャクリ回数は上限なし。1入力1動作で連続入力可能、各終了後TensionFallへ移る。Jerking中の入力はPendingJerkCountへ数え、TensionFallで次の1件を取り出してJerkingへ進む。動作を並列実行/合成しない。明示Fall/回収/中断では未実行の予約を取消す。実行前の予約をシャクリ回数に算入しない。カウンタはint64、ゲーム上の回数制限としてオーバーフローを利用しない。

**一連の回数と後続STAYの契約（技術設計）**:

- JerkCountは投内の実行合計、SeriesJerkCountは現在の一連の実行回数。1動作開始時に両者を1増加する。
- Stay初回進入でStayPenaltyJerkCount=SeriesJerkCountを固定し、bSeriesClosed=true。Stay中のBITE計算へこの整数値だけを渡す。10回以下は減衰なし、11回から回数依存の強い減衰（SQUID_AI参照）。
- 次の実際のJerk開始時、bSeriesClosedならSeriesJerkCountを0にして新しい一連を始める。これにより「再び誘ってStayする」ことで新しい回数が評価される。
- Fall/Stay切替、待機、MISS、BITE期限切れだけではStayPenaltyJerkCountを消さない。Stay再進入時、新しいJerkがなければ前の値を維持する。Stay前にFallを挟んでもSeriesJerkCountを維持する。
- 投終了/新Cast開始で全回数をリセット。イカのCaution/Cooldownは別所有でありリセットしない。新しい一連が始まっても、Stayに入るまではBITE不可。
- AI用FTREgiSnapshotにStayPenaltyJerkCountを追加。Fishingが回数を所有し、AIが減衰倍率を計算する。回数により強制Stayや入力拒否を起こさない。

**D04の状態判定契約**: TensionFallはシャクリ直後の残留リフト・姿勢/ライン制御の過渡処理を担う内部状態。その処理が終了した固定TickでStayへ移行し、無入力待ち時間を追加しない。完了はシミュレーション側の処理完了をFishingへ通知する設計とし、アニメーション終了や壁時計で判定しない。過渡処理の具体的な収束判定・係数はM10で調整可能にするが、鉛直速度ゼロ、RangeError低下、RangeStability達成は完了条件にしない。軽すぎて上昇・重すぎて下降していてもStayへ進む。同Tickの有効なFall/回収/終了/Jerkと予約Jerkを状態確定前に処理し、一瞬Stayを挟んでBITEを許可しない。AutoStayDelayS、期限Tick、残り秒、再計時は設計から撤去する。Stayは専用入力で実行する操作ではなく、旧Stayコマンドを前提にした経路・テストはM10で置換する。

Stayの継続評価だけではLastActionTickやStay進入時刻を更新しない。Retrieving中のRetrieveStoppedはStayへ戻す技術案。Fighting中のRetrieveStarted/Stoppedは状態遷移でなく巻きフラグに変換する。

## 4. エギ・ラインの軽量シミュレーション案

### M07の鉛直落下・着底実装（2026-09-13）

- `UTREgiSimulationComponent`をSessionの所有Componentとして追加。数値正本は`FTREgiSnapshot`、装備はM02の`FTREquipmentSnapshot`のコピーを一投中保持する。M06の投入Tickは深度0・速度0のまま維持し、次の固定Tickから落下する。下記の全体積分案のうち、今回は鉛直FreeFallと海底制約だけを実装した。
- 落下速度は`min(SinkSpeedMps * FreeFallSinkScale, MaxEgiSpeedMps)`。SinkSpeedMpsはM02の総重量（エギ＋シンカー）による曲線評価済み値で、シンカー0gも正常値。係数は既存FishingTuningを使用し、新しい製品バランス値は追加しない。Prototype曲線を用いたゲーム近似であり、実測モデルではない。
- 試行深度は`DepthM + SpeedMps * StepSeconds`、結果を海面0〜M03のBottomDepthMに制限する。深度は下向き正、SnapshotのZ速度はUE座標の上向き正とし、海底補正後の実移動量を固定ステップで割って求める。着底後は深度を保持し、次Tickの速度は0になる。
- SessionがM04のFishingフェーズで最新M05 BoatSnapshotとエギXY位置のOceanSampleを渡す。計算器はBoatの値契約を受け取るが、水平移動・船追従にはまだ使用しない。Ocean無効・非有限値・不正ステップは数値へ反映せず、Sessionは既存のAborted経路で安全中断する。
- `ReachedBottom`を受けたFishingだけがFreeFall→BottomContactへ遷移する。確定したSnapshotを保持した後、同TickのPublishフェーズで`OnFishingStateChanged`を一度だけ配送する。M07で追加する通知はこの着底遷移に限定する。
- CastId不一致、重複/過去Tick、Reset後・破棄中のSession所有Componentへの更新を拒否する。中断・終了・Session破棄で数値をResetし、所有EgiActorを破棄する。BeginPlay前の破棄もM06のDestroyed経路で解放する。
- `ATREgiActor`は衝突/物理/独立Tickを使わず、同CastIdのSnapshotと海面高さからcm座標を適用する。M02装備行のMeshを釣り開始時に解決・保持し、固定Tickで同期ロードしない。Actor位置を変更しても計算へ戻さない。
- M08の水平潮応答、ライン長の更新・球面制約、船追従には未着手。XY・ライン長・ライン角は投入時の値を保持するため、M07だけではラインの幾何的整合を保証しない。F02/F12は鉛直部分、F03は異なる平底水深で検証し、ライン関連はM08、斜面シナリオはM16で扱う。
- M07 Automation 6件成功。30/35/40g＋0g、0.1/3/30m着底、0.25秒固定ステップ、遷移一度、ポーズ、30/60/120fps一致、旧CastId・Session破棄、不正値と環境喪失を確認。NullRHIで描画Actorの座標・物理無効も確認したが、PIEでの見た目確認は未実施。

### M08の潮応答・船追従・ライン制約（2026-09-13）

- M07の「鉛直のみ・ライン項目は仮値」という記録を拡張し、既存EgiSimulationへ第4節の水平指数応答、繰出し、球面投影を実装。新しい調整データ型や製品係数は追加せず、M02の評価済みHorizontalResponsePerS/SinkSpeedMps、PayoutMps/TensionPayoutMps、MinLineM/MaxLineLengthM、状態別SinkScale、TensionReferenceM、MaxEgiSpeedMpsを一投中コピーして使う。
- 潮への応答は`alpha=1-exp(-k*dt)`。総重量による沈下と合成した試行速度を上限で制限して積分する。M05 Boatの更新済み位置・速度・竿先を検証し、移動後の竿先をライン球の中心にする。船速度を潮流や追加リフトへ足さない。牽引は余長がなくなった時だけ生じる。
- Payout/ControlledPayout/Lockedの数値計算を扱い、最大長で繰出しを止める。ReelIn、Lift/Reel操作はM10に残し、未対応要求は成功扱いしない。制御されたStay/Locked入力は計算器の試験に使用するが、SessionにStay入力やShakuri→Stay遷移を追加していない。
- Gameが渡す一時的な`SampleDestination` callbackでM03の移動先海を問い合わせる。計算器はSubsystem/Actorを検索・保持しない。ライン投影→移動先海面/海底補正→ライン再検査を最大4回とし、距離超過許容は数値誤差用1e-5m。これらは収束制御であり製品バランス値ではない。解なし、海域外、非有限値、時刻不一致ならSnapshotを確定せずEnvironmentInvalidを返し、Sessionが既存中断経路で解放する。
- 補正後位置からライン角（鉛直下向き基準）・Tension01（ライン補正距離/TensionReferenceM）・速度を確定する。前回海面Zを内部保持し、世界速度は前回世界位置との差から算出して速度上限を適用する。DepthMはfloatの正本として維持し、描画には最終移動先の海面Zを渡す。
- 接触開始だけReachedBottom、接触終了だけLeftBottomを返す。M07からのBottomContactはLockedであり、牽引により離底するとFishingがTensionFallへ移し、ControlledPayout/凍結したTensionFallSinkScaleで数値更新を続ける。この物理的離底の接続に限定し、過渡完了→Stayの操作処理はM10に残す。着底/離底の通知は同TickのPublishで配送する。
- RangeErrorの元になるDepthM、固定Tick、世界VelocityMps、ライン長/角度/張力を既存Snapshotで保持する。深度変化は連続SnapshotのDepthM差と固定刻みで得られる。速度上限が働く場合や海面変化時、`-VelocityMps.Z`だけを深度速度とみなさない。DepthVelocityMps/境界接触の公開項目追加は予定どおりM10、RangeError/RangeStability/評価DataAssetはM11、BITE接続はM12。今回それらの指標を仮の固定値で追加していない。
- M08全7試験成功。静水、35/80gの潮応答/沈下差、同じライン/潮/船速で30g上昇・35g釣合い・80g下降、最短/最大長、2秒ステップ、解なし、移動先海域外、離底、M04/M05固定更新・ポーズ・30/60/120fps一致、旧CastId・破棄Sessionを検証。重量釣合い比較は同一初期状態から1固定ステップの局所応答であり、長時間の製品レンジ安定性や実測校正の完了を意味しない。M07鉛直回帰は無潮に明示固定し、旧ゼロライン仮値の受理を設定最小長未満の拒否へ更新した。
- D04/D07/D08改訂を正本として認識し、AutoStayをM08で使用していない。旧設定の撤去、Retrieve/船上帰還と一投単位の装備変更へのコード移行はROADMAPどおりM10。現在の旧M06ロック試験合格を改訂D08の合格と称さない。入力/HUDはM09、斜面シナリオはM16、PIEの目視・パッケージ検証は未実施。

### M08までの正本と座標（M10.5で世界位置正本へ置換）

海面Zを `z_s`、エギ水平位置を `x`、深度を `d` とする。世界位置mは `e=(x.x,x.y,z_s-d)`。`d` はfloatの正本。描画Actorに物理シミュレーションを有効化しない。毎ステップ移動先のOceanを再取得し、海底と海面を検証する。

船の竿先 `r`、ライン長 `L`、竿先とエギの距離 `D=|e-r|`。角度は鉛直下向きから `atan2(水平距離, max(r.z-e.z,ε))`、表示はdegreeへ変換する。L>=0、`0<=d<=BottomDepthM`、有限数を不変条件とする。

### M08までの積分順と単位（M10.5で繰出し・空間応答を置換）

1. 総重量M[g]からエギの `SinkSpeedByTotalMass(M)` [m/s]、`HorizontalResponseByTotalMass(M)` [1/s]を読む。沈下は下向き正。沈下曲線は重量増で非減少を推奨するが、実測校正前のゲーム近似である。
2. 当地の潮 `u` [m/s] に対し、水平速度を `v_xy += (u_xy-v_xy)*(1-exp(-k(M)*dt))` で応答させる。重量が増えるほど応答が鈍る曲線を仮定。船速度を潮へ加算しない。
3. 深度速度を `dDot = SinkSpeed(M)*StateSinkScale - u.z - JerkLiftMps` とし、`xTrial=x+v_xy*dt`、`dTrial=d+dDot*dt` を得る。StateSinkScaleは状態別設定。
4. ラインモードにより長さ更新。FreeFallは `L += PayoutMps*dt`、TensionFallは `L += TensionPayoutMps*dt`、Stayは固定、Jerking/Retrievingは `L=max(MinLineM,L-ReelMps*dt)`。Payout上限とReel速度は設定化する。
5. 竿先から試行位置への距離がLを超えたら、`eProjected = r + normalize(eTrial-r)*L` として球面へ戻す。余長がある時は補正しない。ラインは引張のみで押さない。
6. 移動先の海を問い合わせ直す。海面/海底を越えたら深度を制限し、ライン制約を再検査する。小さな固定反復回数で双方を満たすか確認し、収束しない/竿先と最小ライン長が矛盾する場合は環境エラーで安全中断する。無限ループやNaN継続は禁止。
7. 補正後位置を正本へ保存。速度は補正後の位置差から再算出して上限を適用。海面高さが変化する場合は前ステップの世界位置も保持し、深度差と世界Z速度を混同しない。

`StateSinkScale`はFreeFall、TensionFall、Stayの各設定。STAYでも沈下項を0に固定しない。ライン制約、潮への応答、竿先の移動によって最終レンジが決まる。Stay進入時は既存のLを維持して固定する技術設計。D05で数値シミュレーション方式を決定済みであり、式・係数はDataAssetで調整する。MVPではu.z=0、風と波の力を加えない。

`Tension01 = clamp(CorrectionDistanceM / TensionReferenceM,0,1)` は表示・挙動調整用の代理値でありニュートンではない。ケーブルの張力、糸伸び、抗力分布、弛んだライン形状は解かない。`TensionReferenceM>0`、補正量は固定刻み基準で調整する。

投入時は海面上の竿先からエギ初期位置までの距離以上にLを初期化する。海中モデルで竿先そのものまで回収しようとすると海面制約と矛盾するため、回収終点は「竿先直下の海面近傍、水平距離と深度がRetrievalToleranceM以内」とする技術案。到達時にRetrievedを返し、海中モデルを終了する。SessionはCast終了と論理的な船上帰還を確定してエギを非表示/装備表示へ切り替える。表示切替だけでは装備変更を許可しない。回収中の最小長は竿先の海面上高さ以上とし、上空まで海中エギを引き上げない。演出と許容距離はD02/D13で調整する。

### レンジ維持と重量判断（D04/D07）

TensionFall/Stay中は、船ドリフトで竿先が移動し、張ったラインがエギを上へ引く作用と、エギ＋シンカー総重量による沈下作用との釣合いをゲーム近似で表す。上記ライン投影が上向き作用を担い、船速由来の追加リフトを二重加算しない。深度を目標へ固定・自動補正して安定を作らない。

理想は狙いレンジ付近を安定維持する状態。ドリフト過大/軽すぎによる上昇、重すぎによる下降の双方でBITE評価を低下させる。同じ深度を一瞬通過しただけの状態より、釣合いによって維持できた状態を高く評価する。Stayの状態判定とレンジ維持の良否は別であり、TensionFall中にも維持評価を蓄積するがBITE承認はStayのみ。

エギ重量・シンカー重量を選び、狙いレンジを安定維持することを主要な判断要素とする。プレイヤーは直前の投で観測したレンジ上昇・下降、潮流、船ドリフトを基に、Retrieve完了→Cast終了・船上帰還→次投準備で総重量を調整→再投入を繰り返す。一投ごとの調整がレンジ安定化と釣果向上につながるよう設計し、EndFishingを毎投要求しない。

EgiSimulationは補正後のDepthMと深度変化速度（下向き正、DepthVelocityMps）をSnapshotへ提供する設計。AIが対象レンジに対するRangeErrorM、RangeStability01、RangeHoldScoreを計算する（正本はSQUID_AI第3節）。指標は実行時の値、許容幅・評価時定数・BITE倍率曲線はSquidTuning DataAssetの調整値として区別する。境界に押し付けられた静止を釣合いと誤認しないよう海底/海面接触も渡す。具体的な係数はPrototype/Testで検証し、製品値は固定しない。

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

DataAsset項目: ProfileId別SinkSpeedByTotalMass/HorizontalResponseByTotalMass曲線、最大ライン長、PayoutMps、TensionPayoutMps、MinLineM、ReelMps、状態別SinkScale、JerkDurationS/LiftMps/ReelMps、TensionReferenceM、MaxEgiSpeedMps、`HookOpenDelayS=0.10`、`HookCloseDelayS=0.55`、上記Fight調整値、回収閾値、VisualInterpolation設定。D03のBITE減衰曲線はSquidTuningにのみ置く。固定ステップはSession側。初期値指定のない項目に製品確定値を捏造しない。

BlueprintReadOnly: State、DepthM、JerkCount、SeriesJerkCount、StayPenaltyJerkCount、PendingJerkCount、装備ロック/ID/総重量、ライン角度、エギ張力代理値、FightTension01、OverTensionTicks、FightProgress、CatchResult。
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
| F07 | Stay→Fall→着底→Jerk→TensionFall→Stay | 再フォール成立、旧BITE無効 |
| F08 | Shakuri→TensionFallの過渡処理未完了/完了、予約Jerk、同TickのJerk/Fall/回収/終了 | 未完了はTF、完了TickでStay。固定待ちなし、入力優先で一瞬のStay/BITEなし。上昇中/下降中でも完了でStay |
| F09 | 同じ完了イベント2回、Result中に古いHook | 結果1件、釣果重複なし |
| F10 | Fightで安全な巻き/休止を反復 | 巻きでテンション/進捗増加、休止でテンション低下/進捗維持、成功1回、重量固定 |
| F11 | 中断・エギ破棄・次投100回 | CastId増加、古い参照・タイマー・delegate残存なし |
| F12 | ゼロ距離、最短L、最大L、無効海、NaN設定 | ゼロ除算なし、不正設定開始拒否、環境不正は中断 |
| F13 | 過大テンションが閾値以下/継続必要Tickの1つ前/到達 | 閾値以下で連続時間0、必要Tickでバラシ1回。短い超過の合算ではバラシにならない |
| F14 | 同Tickでバラシ条件と進捗完了 | バラシ優先、Caughtなし、回収までは同CastId維持 |
| F15 | MISS→Stay→Fall→再誘い→回収 | MISSでは結果なし、CastId維持、回収完了で初めて結果1件 |
| F16 | 初投準備/Deploy直前・直後/投中/MISS/回収中/Retrieve完了後の次投Ready、Cast終了だが船上未確認で装備変更 | 船上かつ活動中Castなしの準備状態だけエギ・シンカー変更可。Retrieve後はEndFishing不要。投中・回収中・船上未確認は拒否。次投に新重量/係数/メッシュ、前投の結果に旧装備を保持。同TickのDeploy受理後は変更拒否 |
| F17 | 無入力のFreeFall/BottomContact/Jerking/Fighting、Pause/再開、30/60/120fps | 無入力だけではStay化しない。Pause中は過渡処理・評価履歴停止、同じ固定Tick入力で遷移一致 |
| F18 | 11回→Stay→Fall→Stay、MISS、次に1回Jerk→Stay | 切替/MISSだけでは減衰回数11を保持、新しい一連のJerk後は1へ更新。新Castで0 |
| F19 | 同じ狙いレンジ付近で釣合い維持/上昇/下降を比較（M10） | 補正後深度速度と接触状態を正しく出力、全ケースが過渡処理完了後Stayへ進む。維持指標/BITE率の比較はS19/S20で検証 |

数値許容誤差は試験側で明示（例: 静水直線積分0.001m）。ラインと海底の両制約に解がない試験も含める。確率試験と操作試験の乱数を混在させない。

## 8. M10の操作・装備・観測契約（2026-09-13）

M10の旧実装記録。第2〜4節の当時のD03/D04/D08に移行した。M10.5で通常回収停止/Ready接続・空間モデルを改訂する。M06〜M09の記録中にある全セッションロック、操作未対応、旧設定撤去予定は過去の履歴であり、本節が現在の実装を示す。

- FishingComponentが操作と回数を所有し、Sessionの固定コマンド受理からHandleCommandへ渡す。入力→操作時間満了確認→既存Boat/Oceanを使ったEgi積分→状態確定→Publishの順。Shakuriのenum名はJerkingを維持する。
- 1入力で1動作を開始。動作中のJerkはint64のPendingJerkCountへ予約し、動作終了後のTensionFallで次の1件を取り出す。予約の途中にStayを挟まない。JerkDurationSは既存の秒→整数Tick換算を使用し、開始から所定Tick数だけJerkLiftMps/JerkReelMpsを適用する。並列合成・固定回数の自動動作・瞬間リフトの二重加算はない。
- JerkCount/SeriesJerkCountは実際の開始時に増加する。Stayで一連の回数をStayPenaltyJerkCountへ固定し、次の実Jerkで新しい一連を開始する。Fallだけでは前の適用回数を消さない。Fall/Retrieve/中断は未実行予約を取り消す。ゲーム上の回数上限はなく、int64演算の飽和/表現限界時の防御はゲームバランスの上限とは区別する。
- TensionFallではEgiSimulationが残留リフト速度を`ResidualLift *= exp(-TensionLiftDecayPerS * dt)`で減衰し、そのステップの沈下に上向き速度として加える。残留速度が`TensionLiftCompletionMps`以下になったら0へ確定し、Fishingへ完了を返す。完了Tickで即Stay（予約Jerk・有効入力・海底接触を優先）。残留リフトがない明示TensionFall要求は、ライン制御を1固定ステップ処理した時点で完了できる。
- 新しい減衰率・完了速度はFishingTuningの正値/有限数を要求し、完了速度はJerkLiftMps未満とする。Prototypeは12/sと0.05m/sのゲーム近似。製品値ではない。深度速度ゼロ、無入力時間、RangeError、安定達成は完了条件にしない。シャクリ時間以外のStay待機タイマーはない。
- Stayは既存ライン長をLockedで維持し、M08の潮応答・重量沈下・移動した竿先による牽引を継続する。Re-FallでFreeFall/Payoutへ戻る。FreeFall/Bottomは無入力だけではStay化しない。底に接触したままTensionFallを要求してもBottomContactを優先する。
- RetrieveStartedでRetrieving/ReelIn、RetrieveStoppedで巻取り速度だけ0へ戻す。解放後もRetrievingに留まり、潮・重量・ライン制約は継続する。再押下で巻取り再開。最低ライン長は設定MinLineMと竿先の海面上高さの大きい方とし、海面補正時はライン球と水平面の交差円へ水平位置を制限する。球が海面に接する極限でも反復収束待ちや海上への貫通を避ける。
- 回収完了は巻取り中、深度と竿先からの水平距離がそれぞれRetrievalToleranceM以内になった時に1回だけRetrievedを返す。SessionがCast終了・論理的船上帰還・前投の結果と装備コピーを確定し、数値エギ/描画Actorを解放する。Resultから既存NextCastでReadyへ進むと装備変更を許可する。EndFishingは不要。操作手順はUI_SPECのM10追記を参照。
- StartFishingは装備をロックしない。TrySetEquipmentは船上・活動中Castなし・Ready・非Pause・固定更新外を共通条件とし、M02検証とメッシュ解決で次投候補を準備する。Deploy受理と装備コピー/ロックを同じ処理で確定。候補は外部から書換不能な値コピーで、保持したメッシュの有効性も再確認する。Tick内の同期ロードはない。
- Abort/EndFishing/環境異常で投を終えただけでは船上帰還を作らない。Abort後にReadyへ戻しても装備変更・再投入は拒否する。この場合のPrototype再開はWorld/Playの再起動とし、架空の帰還操作を追加しない。CastIdは同じSession内で再利用しない。
- `GetLastResultEquipment()`で前投の凍結した総重量・係数を取得できる。NextCast/次投の装備変更/Deployで前投の結果を上書きしない（次の終端結果で更新）。OnCastCompletedは通常Publishで1回通知し、EndFishingで登録を解除する場合は残った終端通知を終了処理で1回だけ配送する。

### 後続レンジ評価へ渡す値

FTREgiSnapshotへDepthVelocityMps（補正後DepthM差/固定dt、下向き正）、bBottomContact、bSurfaceContact、JerkCount、SeriesJerkCount、PendingJerkCount、StateEnteredTick、RangeObservationSecondsを追加した。既存CastId/Tick/世界VelocityMps/ライン長・角度/張力/StayPenaltyJerkCountも保持する。世界速度には既存速度上限があるため、深度速度を単に`-VelocityMps.Z`で代用しない。

RangeObservationSecondsは連続したTensionFall/Stayの観測時間であり、安定していた秒数・RangeStabilityではない。TF→Stayでは保持、両状態から離れたら0、新Castでも0、Pause中は停止する。安定の閾値・速度履歴・イカ好適深度に対する誤差・BITE倍率はM11/M12が評価する。今回未定義の安定秒数やスコアを仮値で埋めない。読取Snapshotは時計・数値・入力を変更しない。

### 移行・試験

AutoStayDelaySを宣言・時刻換算・データ検証から削除し、旧前提の試験を置換した。汎用秒→Tick試験は維持。既存DA_TR_FishingTuning_PrototypeをUEで再保存し、未知の旧プロパティを除去、新係数だけを追加した。既存重量曲線・釣り設定は保持し、M02全6件と別プロセスの再読込で確認。Content内に旧項目名が残らないことも確認した。

M10はF06/F07/F08/F16/F17/F18/F19と設定移行を含む8試験成功。1/10/11/20入力、予約取消、一連の保持/更新、無入力Bottom、同Tick入力優先、回収/解放/再開、0g/次投90g、同Tick装備変更拒否、次投メッシュ差替え、旧Cast、Pause/フォーカス、30/60/120fpsの状態列・結果時刻一致、深度速度・境界接触・読取非破壊を確認した。詳細な結果と回帰内訳はROADMAPを参照。M11以降のAI/確率/Hook/Fightは未実装、実測校正・PIE目視・実機入力・パッケージ起動も未実施。

## 9. M10.5の空間エギ・ライン・操作（未実装）

### 世界位置と相対幾何

EgiSimulationのWorldPositionMを唯一の位置正本とし、同じ確定位置からPositionXYM、DepthM、Boatに対するHorizontalOffsetM/HorizontalDistanceM、竿先とのDistanceM、LineAngleを導出する。DepthMはfloatの読取値を維持するが積分しない。DepthVelocityMpsは補正後の深度差/固定dt。Boat Position/Velocity、Rod Tip Position/Rotation、CurrentAtEgiDepth、重量Snapshotは同Tickの値を用いる。世界座標mとUE描画cmの変換は描画境界だけ。

ライン角は竿先→エギの直線を鉛直下向きから測る。HUDの「船との水平距離」とライン角に用いる「竿先との水平距離」は区別する。ラインが弛んでいる時の角度は端点間の幾何角であり、実際の曲線の竿先接線角ではない。Rod RotationはRod長/取付位置からTip位置を求めるために使う。ライン方向を竿の向きへ強制一致させない。

`D=|EgiPosition-RodTip| <= L + epsilon`、`0<=DepthM<=WaterDepthM`、全値有限を不変条件とする。SlackM=max(0,L-D)。エギを毎Tick船の真下へ置かない。海面/海底補正後にライン条件を再検査し、固定の最大反復/内部サブステップ数を超えた失敗は最後の有効値を保持して既存の技術中断へ進める。80m clampによる見かけの合格は禁止。

### 空間速度とライン応答の技術案

1. 凍結した総重量MからM02の沈下曲線/応答係数を評価する。沈下終端速度を持つ3D速度応答とし、前Tickの鉛直速度を毎回定数で上書きしない。無潮・無風・十分な自由ラインでは重量曲線の終端沈下へ収束する。
2. Oceanからエギ位置/深度の潮uを取得し、自由速度の目標を`u - Up*SinkSpeed(M)*StateSinkScale`として、正の応答率による指数応答で更新する技術案。Shakuriの入力作用と水中ライン抗力による速度変化を加える。重量を単なる表示値にしない。沈下速度や抗力はゲーム近似で製品値未確定。
3. 海中ラインの潮流作用を最小の分布サンプルで近似する。水中にある竿先〜エギ区間の固定比率点で深度潮を読む（初期技術案3点）。空中区間へ水中抗力を掛けない。各点速度は端点速度の補間、相対流速はu(point)-v(point)。`Fline ~ LineDragPerM * WetLength * weightedRelativeFlow`とする線形抗力案。LineDragPerMは[kg/(m*s)]、エギへの伝達率は無次元、加速度への換算に有効質量[kg]を用いる。端点の潮応答とライン抗力を別係数にして二重計上を検査する。弛み時の風下牽引を捏造せず、張り/弛みに応じた伝達を設定化する。
4. ライン長を下記の需要方式で更新し、試行世界位置を積分。動く竿先と有限ラインの引張制約を解く。ラインは押さない。張った時の相対速度の外向き成分も制約と整合させ、補正前の速度へ戻して次Tickに同じ貫通を反復しない。
5. 海面/海底とラインを整合させて確定する。接触開始/離脱は一度だけ通知し、補正後位置・実移動速度・深度速度を公開する。速度上限のため公開速度だけを切って位置差と不整合にする方式は見直す。必要な防御は候補積分段階で適用する。

ラインの完全なカテナリー、多数の物理リンク、Chaos/CFDは実装しない。上記の分布抗力は近似であり、実際のライン形状の再現を名乗らない。必要なら純粋な計算器へ分離するが、Coordinator/Widgetへ式を置かない。Tension01は引張制約/抗力の正規化代理値0〜1で、ニュートンやFightテンションと混同しない。旧補正距離/TensionReferenceMの依存試験は移行対象。

### FreeFallとTensionFallの繰出し

FreeFallは自動繰出しを維持するが、`L += 一定速度*dt`を無条件に行わない。候補距離Dtrialと試験余長SlackAllowanceMから`Lneeded=Dtrial+SlackAllowanceM`を求め、`deltaL=min(max(0,Lneeded-L), MaxPayoutMps*dt)`だけを繰り出す技術案。余長が十分なら追加しない。繰出し上限より需要が大きい時は実際のLで牽引を解き、黙ってラインを伸ばさない。FreeFallは巻取りをせず、着底後の無操作で繰出しを継続しない。

TensionFallは原則既存Lを保持して過渡作用を解く。制御繰出しが必要な技術ケースは同じ需要方式と独立した上限/余長を用い、無条件のTensionPayoutによる余長蓄積は禁止する。StayはL固定。竿先移動や船ドリフトによる水平/鉛直作用とM02重量沈下を同時に計算する。

30m・代表潮0.4/0.7/1.0 knot・27装備の検証をROADMAPのR03で行う。L/D/余長/ライン角、FreeFall時間、船とエギの移動距離/相対距離、風/表層潮/エギ潮、総重量を記録する。80m以上を不合格とするのは定義したPrototype標準条件であり、全環境での物理的上限を80mにする仕様ではない。

### Shakuri / TensionFall / Stay

1右クリック押下=1シャクリ。保持では増加しない。複数押下は既存Tick/Sequence順に予約し、固定回数の自動シャクリを作らない。受理時にRodControlへ一時あおりを要求し、完了後はその時点のマウス基準姿勢へ戻す。マウス基準姿勢を一時オフセットで上書きしない。動作は固定更新の有限時間で表現し、無限加速度の瞬間ワープはしない。

技術案では数値竿先の一時移動をライン経由でエギへ伝え、旧JerkLiftを同じ量で重ねない。追加のゲーム用リフトが必要なら別寄与として明記/設定化し、1入力の作用量と単独/合成の試験を持つ。製品シャクリ量/速度/アニメーションは固定しない。

基本遷移はJerking（表示シャクリ）→TensionFall→Stay。過渡作用の処理完了で即Stay、海底接触・予約/新入力の優先は維持。AutoStayDelay/無入力0.8秒/安定達成待ちは存在させない。軽量で上昇、重量過多で下降していてもStayへ進む。再右クリックでShakuri、FでFreeFallへ戻る。

Stayでは船・竿先・エギ深度潮・ライン抗力/角度/長さと重量の釣合いでレンジを決める。全重量で沈むことも全重量で上がることも、条件を問わず強制してはならない。R04の同じ環境/初期幾何で軽量/適正/重量過多を比較する。境界静止は安定と数えず、DepthVelocity/接触/Tick/位置を後続へ渡す。RangeError/RangeStabilityは実行時評価値であり、調整するのは後続SquidTuningの閾値/時定数/曲線。M10.5ではBITE評価や安定スコアを追加しない。

### 通常Retrieveと解放

左押下でRetrieving/ReelIn、保持中だけLを巻き取る。左解放/取消は巻取り速度を0にし、同CastのStayへ戻す技術契約（海底接触中はBottomContact優先）。その入力境界でWorldPosition/Velocity/L/Angleを初期化せず、次の固定積分から潮・船・重量による運動を継続する。旧Retrievingのままの沈下係数は使わない。解放でFreeFall自動繰出しを開始せず、船直下スナップもしない。

通常回収の完了条件は竿先直下の海面近傍（深度/水平距離が設定許容内）を維持し、海中モデルの外へ無理に引き上げない。Retrieved→船上帰還→ReadyをSessionが一度だけ確定し、装備UIを有効化する。旧Result→N必須を廃止する改訂技術契約。前投の結果・装備は保持する。

### Quick Retrieve

Q押下で新状態QuickRetrieving。活動中のFreeFall/Bottom/Jerking/TF/Stay/通常Retrievingから受理し、シャクリ予約/通常巻取り/入力保持を解除する。開始時のCastId/登録世代/開始Tick/所要Tickを凍結し、1〜2秒程度のQuickRetrieveDurationSをDataAssetの試験値として共通ceil換算する。ライン長に比例させず、80m/100mの試験でも同じ所要Tick。

Quick中はQ再入力・左解放・F・シャクリ・Hook等を拒否し、途中停止しない。Attack/Bite適格性=falseをSnapshotの状態契約として後続へ渡す（AI本体を先行実装しない）。ロッド操作もQuick用演出を優先し、入力を完了後へ持ち越さない。Pauseは時計を停止、フォーカス喪失は保持を解除するが回収を取り消さない。Worldが動作中ならフォーカス喪失だけでsim時間を止める仕様は追加しない。

Quickは通常水中物理を凍結するテンポ改善の回収経路。開始位置/ラインを保持した演出Snapshotから進捗を表示し、水中運動/Range観測の有効性をfalseとして区別する。描画だけの補間位置をWorldPosition正本やAI対象へ戻さない。演出終了コールバックでなく`Tick >= StartTick+DurationTicks`で帰還を確定する。完了時は一度だけRetrieved（必要なら終了方法Normal/Quickの値を付記、Caughtではない）→Cast終了→Egi Onboard→Ready→装備解除。古い終了通知の二重処理を拒否する。

途中停止不可はプレイヤー釣り操作の条件。明示Session終了/World終了/対象破棄は安全中断を優先し、後からQuick完了や船上帰還を発行しない。終端CastIdを保持し、次Deployでだけ新IDを採番する。
