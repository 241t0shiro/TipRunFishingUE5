# 釣りシステム技術設計

2026-09-17 M10.5-R設計改訂: A〜F基盤は保持。最新手動PIEでGはゲームプレイ品質不合格。Rは設計/実装分割のみ完了し、実装未着手。最新契約は本書末尾のM10.5-R節を優先。以前のG合否保留・固定Pulse・観測カメラ等は履歴。R自動検証とユーザー手動合格後もHへ自動進行しない。H/M11以降は保留。

2026-09-15 M10.5-E完了: 左保持の通常回収／解放後Stayと、固定TickのQuickRetrievingを分離。今回の明示依頼を優先し、通常完了はResult（ロック維持）→NextCastでReady/解除、Quick完了だけ直接Ready/解除。海面近傍のライン拘束・巻取りを修正。UHT生成・実C++・Development Editor Win64成功、E 7件＋回帰54件成功、各試験エラー/警告0。保存資産移行・PIEは未実施。F〜H・M11以降は未着手、全体品質ゲート未合格。下のA〜D記録は履歴。

2026-09-14 M10.5-D完了: Mouse Axis2D→固定Input Queue→RodControl、右クリック/Spaceの同一Jerk、基準姿勢＋時間プロファイル、RodTip→Cライン接続を実装。Rod有効時は旧Lift/Reelを重ねない。UHT生成・実C++・Development Editor Win64成功、D 5件＋必要回帰49件成功、各試験エラー/警告0。Rod/Input資産は明示設定、既存保存Prototype移行/実マウスPIEは未実施。E〜H・M11以降は未着手、M10.5全体品質ゲートは未合格。以下のA〜C/設計のみの記録は履歴。

2026-09-14 M10.5-C完了: Egi WorldPositionを位置正本へ移行し、revision 2の深度潮/水中ライン抗力/需要繰出し/空間拘束とSnapshotを実装。UHT生成・実C++・Development Editor Win64成功、C 6件（243落下条件含む）＋回帰49件成功、各試験エラー/警告0。標準30mの最大ライン39.299694m、最長50.550秒で着底。保存Prototypeは旧係数revision 1のまま、資産移行/新モデルPIEは未実施。D〜H・M11以降は未着手、M10.5全体の品質ゲートは未合格。以下のA/B完了・設計のみの記録は履歴。

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
`Inactive, Ready, Deploying, FreeFall, BottomContact, Jerking, TensionFall, Stay, Retrieving, Fighting, Landing, Result, QuickRetrieving`

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
| Retrieving | 左解放/取消・安全停止 | Stay（底ならBottomContact） | 位置・速度・ラインは保持し、巻取りだけ停止 |
| FreeFall / BottomContact / Jerking / TensionFall / Stay / Retrieving | QuickRetrieve | QuickRetrieving | 予約/通常巻取りを解除、水中積分停止、固定Tick期限開始 |
| QuickRetrieving | 固定期限完了 | Ready | Retrieved・Cast終了・船上帰還・装備解除を一度だけ確定。Resultを挟まない |
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

## 9. M10.5の空間エギ・ライン・操作（C実装、操作改訂はD以降）

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

通常回収の完了条件は竿先直下の海面近傍（深度/水平距離が設定許容内）を維持し、海中モデルの外へ無理に引き上げない。2026-09-15のE依頼を優先し、通常回収はRetrieved→船上帰還→Resultとする。Result中は装備ロックを維持し、NextCast（N）でReadyへ進むと解除する。Quickだけが直接Readyへ進む。両回収を直接Readyとした旧案を置き換え、前投の結果・装備は保持する。

### Quick Retrieve

Q押下で新状態QuickRetrieving。活動中のFreeFall/Bottom/Jerking/TF/Stay/通常Retrievingから受理し、シャクリ予約/通常巻取り/入力保持を解除する。開始時のCastId/登録世代/開始Tick/所要Tickを凍結し、1〜2秒程度のQuickRetrieveDurationSをDataAssetの試験値として共通ceil換算する。ライン長に比例させず、80m/100mの試験でも同じ所要Tick。

Quick中はQ再入力・左解放・F・シャクリ・Hook等を拒否し、途中停止しない。Attack/Bite適格性=falseをSnapshotの状態契約として後続へ渡す（AI本体を先行実装しない）。ロッド操作もQuick用演出を優先し、入力を完了後へ持ち越さない。Pauseは時計を停止、フォーカス喪失は保持を解除するが回収を取り消さない。Worldが動作中ならフォーカス喪失だけでsim時間を止める仕様は追加しない。

Quickは通常水中物理を凍結するテンポ改善の回収経路。開始位置/ラインを保持した演出Snapshotから進捗を表示し、水中運動/Range観測の有効性をfalseとして区別する。描画だけの補間位置をWorldPosition正本やAI対象へ戻さない。演出終了コールバックでなく`Tick >= StartTick+DurationTicks`で帰還を確定する。完了時は一度だけRetrieved（必要なら終了方法Normal/Quickの値を付記、Caughtではない）→Cast終了→Egi Onboard→Ready→装備解除。古い終了通知の二重処理を拒否する。

途中停止不可はプレイヤー釣り操作の条件。明示Session終了/World終了/対象破棄は安全中断を優先し、後からQuick完了や船上帰還を発行しない。終端CastIdを保持し、次Deployでだけ新IDを採番する。

## 10. M10.5-C 空間モデルの実装契約（2026-09-14）

- `UTREgiSimulationComponent`が唯一の位置正本`WorldPositionM`を保持する。旧係数の互換経路も内部位置はWorldPositionへ移行した。`PositionXYM`/`DepthM`は読取コピー。WorldPosition指定の初期化では旧XY/Depth入力を使わず、旧呼出側は初期化境界で一度だけXY/Depthから位置を変換する。`bWorldPositionValid`はこの境界を識別する。Session投入は最初からWorldPositionを渡し、描画Actorもこれをm→cm変換して読む。
- `FTRFishingParameters.EgiModelRevision=2`で本節の新しい3D応答/分布ライン抗力/需要繰出しを有効にする。revision 1は保存済みM10係数と従来運動・繰出しの互換経路。新係数を0のまま無言で新モデルへ換算しない。新設定混在のrevision 1、未知revision、不正/非有限係数を拒否する。今回保存資産/Prototype Levelは変更しておらず、既存PIEを新モデルで検証済みとは扱わない。保存設定の明示移行と旧経路撤去判断はGに残す。
- 新しい係数は`VerticalResponsePerS`（正）、`LineDragKgPerMS`（非負）、`TautLineTransfer01`/`SlackLineTransfer01`（0〜1、Slack<=Taut）、`LineSlackAllowanceM`（非負）、`MaxStepTravelM`（正）。既存M02総重量によるSinkSpeed/HorizontalResponse曲線、状態別SinkScale、PayoutMps（最大繰出し速度）、MinLine/MaxLineLength、MaxEgiSpeed、TensionReferenceを併用し、一投開始時にコピーする。新係数の既定0は未設定、製品値ではない。
- 新計算は同じComponentの`TREgiSpatialSimulation.cpp`へ分離。Coordinatorへ計算式を移さない。EgiのXYと当地海面を問い合わせ、WorldPositionから導出した深度でAのOceanを再問い合わせする。分布抗力・拘束補正・移動先にもXY/Depth/Tickを渡す。BoatのWind/SurfaceCurrent計算はBが所有し、Egiは深度潮を使う。Boat VelocityそのものをEgiへ足さない。
- 自由目標速度は`CurrentAtDepth - Up*SinkSpeed(totalMass)*StateSinkScale`。M10の操作互換用リフトはZ目標の別作用として保持する（マウス竿移動はD未実装）。XYはM02応答率、ZはVerticalResponseで前の運動速度から指数応答する。一定サブステップの速度解とその積分から試行世界位置を得る。重力/浮力を別に二重加算せず、M02の重量別終端沈下曲線で沈下傾向を近似する。
- 水中の直線区間を3等分した中点で潮を読む。空中部分は除外、点速度は竿先とエギの端点速度の補間。`LineDrag * WetLength * Transfer / MassKg`を応答率へ換算し、エギ速度に依存する減衰項を指数解の率へ含めて大きな抗力でも陽的加算による発散を避ける。張り/弛みの伝達率は設定値。エギ端点の潮応答とライン抗力は独立で、片方だけを変更する比較試験を持つ。柔軟ライン形状の実測再現ではない。
- FreeFall/Payoutは`min(max(0, Dtrial+SlackAllowance-L), PayoutMps*dt)`のみ追加する。余長十分なら追加0。需要が速度上限を超える場合は実際のLで拘束する。新モデルのTensionFallとStayはL保持、Bottom無操作は繰出さない。MaxLineLength超過候補を拒否し、80mへclampしない。旧Reel操作の速度/完了条件はCでは変更しない。
- 固定刻みを最大1/60秒の内部サブステップへ分割し、最大240回まで。環境のTickは外側の固定Tickのまま、竿先移動を内部で線形補間する。1サブステップで海面/海底/ラインを最大8反復、許容1e-5mで同時解決。海面接線付近はライン球と海面の交差円を使う。海底問い合わせは水平補正後にもやり直す。収束不能、無効Sample、非有限、最大速度/全移動距離超過は候補を確定せず既存EnvironmentInvalid→Session中断へ渡す。
- ラインは外向き超過だけを補正し、運動速度の竿先に対する外向き成分も除去する。海底/海面の侵入速度も除去する。SnapshotのVelocityMpsは補正後の実移動量/外側dt、内部の運動速度は次ステップの応答初期値として別に保持する（位置を二重積分するものではない）。新モデルでは公開速度だけをclampしない。DepthVelocityは当地海面からの補正後Depth差/dt。Bottom開始/離脱イベントは確定状態の変化時だけ通知する。
- Snapshot追加: WorldPositionM、bWorldPositionValid、EgiModelRevision、HorizontalOffsetFromBoatM/FromRodTipM、各HorizontalDistance、BoatToEgiDistanceM、RodToEgiDistanceM、LineDirection（竿先→エギ）、SlackM、CurrentAtEgiDepthMps（確定位置）、TotalMassG。既存Depth/Velocity/LineLength/LineAngle/Tension01、Bottom/SurfaceContact、CastId/Tick、観測秒数を維持する。Tension01は拘束補正距離/TensionReferenceの0〜1代理値で、力[N]やFight評価ではない。RangeError/RangeStability/BITE評価は追加しない。
- R03試験値はBと同じ風2m/s/船係数、横風を受けるHeading、平底30m、竿先海面上1.5m、M02の保存済み27装備。CのVerticalResponse=2/s、LineDrag=0.0001 kg/(m*s)、Taut/Slack転送=0.05/0、余長0.1m、最大繰出し4m/s、安全値MaxLine=200m/MaxEgiSpeed=10m/s/MaxStepTravel=20m。これらは一時的Test設定で保存製品値ではない。
- R04は独立した制御試験。全重量で同じ初期位置（竿先から水平後方10m・下方10m）、L=sqrt(200)m、潮0、竿先水平速度0.7m/s、XYZ応答40/s、ライン抗力0、同じM02重量曲線を使う。10秒間を初期化直後から全観測し、30/35/40gだけを変える。定常竿先軌道を与える純粋試験で、Bの風2m/sから自動的に0.7m/sになるという意味ではない。GのPIEシナリオでは対応する船/環境設定・導線の校正が必要。製品の最適重量・安定閾値を確定しない。
- 試験結果・ビルド・残存警告はROADMAPのC記録を参照。D/Eのマウス/可動竿/回収解放/Quick/Ready仕様、FのHUD/UI、Gの保存Level、M11以降には着手しない。

## 11. M10.5-D Mouse Rod / Shakuri実装契約（2026-09-14）

- SessionがCreateDefaultSubobjectでUTRRodControlComponentを所有する。SessionConfig.Rod（UTRRodTuningDataAsset）は明示opt-in。未設定なら従来固定RodTipを維持する。設定する場合はCのEgiModelRevision=2を必須とし、起動設定はRodAimの割当ても検証する。係数はSession初期化時にコピーし、Editor編集を実行中へ反映しない。
- RodControlのBasePitchRad/BaseYawRadがマウス基準姿勢の正本。RodAimのAxis2Dは移動量であり描画dtを掛けない。X→Yaw、Y→Pitch、各感度[rad/入力単位]と反転を適用。各イベントのMaxMouseDeltaと、同一固定Tickの軸別総角度変化量MaxAimRateRadPerS*dtで極端な跳びを制限する。超過分は蓄積せず捨て、Pause後へ持ち越さない。最後に基準可動域へclampする。
- ApplyAimは固定キュー配送からだけ呼ぶ。RodAimとJerkを同TickのSequence順で処理し、実Jerk開始時点の基準姿勢をShakuriStartBaseへ記録する。その後のマウス入力は現在Baseを更新する。戻り先は最新Baseであり、開始時のBaseへ書き戻してプレイヤー入力を消さない。実行中予約は既存FishingのPendingJerkCountを使い、次の実開始でその時点のBaseを記録する。
- 更新順はInput→既存操作時間処理→Ocean/Boat→Fishing内のRodControl→Egi→状態確定/Publish。Rod更新前に既存Fishing::PrepareStepを呼ぶ。Rodは独立Tickを持たず、Egi位置を直接変更しない。Readyでも登録世代/最終CastIdで照合したRod入力とBoat追従を固定更新する。
- MountWorld=Boat.PositionM+RotateZ(Boat.HeadingRad,MountOffsetM)。Direction=(cos(Pitch)*cos(Heading+Yaw),cos(Pitch)*sin(Heading+Yaw),sin(Pitch))、TipWorld=MountWorld+LengthM*Direction。姿勢は世界Pitch/Yaw・Roll0のQuaternionも提供する。すべてm/rad、描画境界だけcm/degrees。船側のRodTipは従来取付基準、可動竿先はRodSnapshotとして分離し、Egiへ渡すBoat値コピーのRodTipだけ置き換える。Boatの数値正本は書き換えない。
- ShakuriはUp/Returnそれぞれ秒→整数Tickへ変換し、その和を既存FishingのJerk期間に設定。上昇割合sを0→1→0にし、振幅*`s*s*(3-2*s)`のPitch一時オフセットを作る。ピークは両相の境界、固定待機時間は追加しない。FinalPitch=clamp(BasePitch+Offset)、FinalYaw=BaseYaw。終了時はOffset=0。最短でも各相1Tick、無限速度テレポートではない。
- Rod有効時のJerkは旧Action.LiftMps=0、JerkReel=0、LineMode=LockedとしてCへ渡す。追加インパルスは使わない。RodTip移動→ライン拘束→エギの作用が正本。既存Jerking→TensionFall→Stay、BottomからのJerk、予約回数・10回超の履歴契約を維持する。TFは同固定Tick内で完了し得るため、観測用の待機Timerを入れない。
- Pause/フォーカス喪失はControllerの保持・キューを解除し、押下中のJerkを解放確認まで再受付しない。Dの未実行Jerk予約の取消要求は次の固定処理で適用し、Pause中に物理状態を進めない。実行中のShakuriはPauseで時刻を止め、復帰で残りを続ける。Cast終端・Session終了ではRodSnapshotを無効化し、古い活動中表示を残さない。World/Actor破棄では参照と設定コピーを解除する。
- FTRRodSnapshot: bValid、Tick/CastId、Base/Final Pitch/Yaw、開始時Base、TipWorldPositionM/TipWorldRotation/TipDirection、bShakuriActive、ShakuriPhase（0基準/1上昇/2復帰）。HUD集約Snapshotへ追加し、読取だけで取得できる。日本語表示・Rodメッシュ・新UIは追加していない。
- DataAsset項目: Pitch/Yaw最小最大・初期値、感度XY、反転XY、MaxMouseDelta、MaxAimRateRadPerS、LengthM、MountOffsetM、ShakuriAmplitudeRad、UpSeconds、ReturnSeconds。有限/範囲/正値をRuntime/IsDataValidで検証し、Session初期化では最小Pitchの竿先が海面下へ入らないことを検査する。
- Test値: Pitch[0,1.2]rad、Yaw[-0.6,0.6]rad、初期Pitch0.1rad/Yaw0、感度X0.01/Y0.02、delta上限50、最大操作角速度1.2rad/s、竿2m、船相対取付(0,0,1)m、振幅0.3rad、Up0.15秒/Return0.25秒（60Hzで9+15=24Tick）。製品値/実測値ではない。Cの空間調整値は専用Test設定を利用する。
- Eの左マウス回収・解放後Stay・Quick・帰還Ready変更は今回なし。Dの試験/回帰結果はROADMAPを参照。保存PrototypeへRod/Inputを割当てる資産移行、実マウスPIEと操作感の確認はG/Hへ残す。

## 12. M10.5-E Normal / Quick Retrieve実装契約（2026-09-15）

- 通常回収は既存RetrieveStarted/Stoppedの意味を維持し、Left Mouse/R/パッドを同一Boolean Actionへ接続。Startedで1回だけ開始、保持中はFishingのbReelingからReelInを継続、Completed/Canceledで停止。Controllerは数値を変更せず、既存Session→固定Tick/Sequence/CastId/登録世代の経路を使用する。
- StopNormalRetrieveは同じCastのStayへ戻す。既に海底接触中ならBottomContactを優先する。入力境界でWorldPosition/Velocity/LineLength/LineDirection/Angle/Offset/Currentを変更しない。次の固定積分からStay係数でCの潮・重量・ライン抗力・Boat/Rod移動を継続する。自動繰出し/船直下スナップはない。再Shakuri/Re-Fall/Retrieveが可能。
- 既存FishingTuningのReelMps（要求速度m/s）、MinLineM、RetrievalToleranceMを再利用し、一投開始時のEquipment Snapshotで凍結する。加速/減速の製品値は追加しない。解放時の巻取りは即0。通常回収はライン長を短縮してCの球面拘束を解き、エギ位置の直接Lerpはしない。
- Cの海面交差円で、補正済み座標の丸め誤差を半径の厳密超過と判定し続ける問題を修正。既存1e-5m許容を交差円でも使用する。通常回収の海面近傍では一定dL/dtが水平速度の急増を生むため、h=竿先海面上高さ、r=更新前位置と当該竿先の水平距離、v=要求巻取り速度、dt=内部刻みから、rNext=max(0,r-v*dt)、Lsurface=sqrt(h*h+rNext*rNext)を算出。海面に接する場合は要求ライン長をLsurface以上（ただし元のL以下）に制限する。巻取りを遅くする幾何条件であり、エギ位置の直接移動や速度上限緩和ではない。速度拘束にも実際に短縮したL/dtを使用する。海面から離れた通常回収は従来のv*dt巻取り。
- 通常完了は従来の深度/竿先水平距離<=RetrievalToleranceMを満たす巻取り中に一度だけRetrieved。SessionはOnboard/Cast終了/Resultを確定し、装備はロックしたまま。NextCastでReady/解除する。今回のユーザー明示指定に従うため、M10.5設計当初の「両回収が直接Ready」は置換した。
- QuickRetrieveコマンドとQuickRetrieving状態は既存enumの末尾へ追加し、保存済みenum値を変えない。FreeFall/Bottom/Jerking/TF/Stay/通常Retrievingで受理し、未実行Jerkと通常巻取りを解除する。Ready/Result/船上ではRejectedInvalidState、Quick中の釣りコマンド（再Q、左停止、F、Jerk、Hook、RodAim、Deploy、NextCast等）はRejectedBusy。同TickでもSequence順に受理/拒否を通知し、後続コマンドを無言で一括破棄しない。明示EndFishing/Session終了/Actor破棄は安全中断を優先する。
- FishingTuning.QuickRetrieveDurationSを投開始時に凍結し、共通ceil換算の整数QuickRetrieveTicksで管理。Tick-開始Tick>=所要Tickで完了、ライン長非依存、TimerManager/実時間/演出通知不使用。保存済み旧設定の既定0は「未設定・Quick利用不可」として互換読込し、要求にはRejectedMissingDataを返す。有限非負、正値のTick表現可能性、開始Tickとの加算範囲を検証する。起動InputにQuickを割り当てるなら正の明示時間を要求する。
- Quick中は最後の物理Egi SnapshotとRod姿勢を保持し、Egi/ライン積分を停止。Boat/Oceanは通常固定更新を継続する。FTRRetrievalSnapshot.bUnderwaterSimulationActive=falseとQuick状態により後続Attack/Bite/Hookの対象不適格を表す。演出用補間位置は今回追加せず、凍結位置を最新の水中物理として扱わない。Egi Snapshot.Tickは最終積分時刻、Retrieval.Tickは操作進捗時刻として区別する。環境・対象寿命が無効なら帰還を捏造せずAborted。
- Quick完了は一度だけRetrieved、bQuickRetrieved=true、過去装備コピー、Egi数値/Actor解放、Onboard、Cast終了、Ready、装備解除を同固定Tickで確定。CastIdを保持し次Deployでのみ採番。登録世代を更新して古い入力を失効させる。終端通知は既存Publish方式で新登録の次のPublish時に一度だけ配送する。物理期限と通知配送を区別する。N追加操作は不要。
- Pause/Focus LostはControllerの保持・キューを解除する。Sessionには通常回収の安全停止要求も残し、Pauseでキューが破棄されても再開の最初の固定境界で適用する（Pause中の数値更新はなし）。Quick受理時はControllerの保持も同期解除し、保持キーを解放確認まで再受付しない。Quick自体はFocus Lostで中断せず、Pauseでは期限を進めない。Controllerの結果観測delegateは接続/解除を対にし弱いUObjectバインドを使用。
- FTRRetrievalSnapshotはCastId/Tick、bIsRetrieving/bIsQuickRetrieving、bUnderwaterSimulationActive、RetrieveSpeedMps（直前の実ライン減少/固定dt）、RequestedRetrieveSpeedMps、RemainingLineLengthM、QuickRetrieveProgress01を読取コピーとして提供する。実速度はfloatライン値由来の丸めを含む。非巻取り中は両速度0。HUD集約型にRetrievalを追加し、装備ロック/変更許可は既存フィールドを維持する。FTRCatchResult.bQuickRetrievedはQuick成功のみtrue、Abortはfalse。Range評価・AI・日本語表示は追加しない。
- Test設定はA/B/C/Dの明示revision 2、平底30m、潮(0.2,0,0)m/s、風(0,2)m/s、Dと同じ竿、要求巻取り1m/s、Quick1.5秒=60Hzで90Tick。製品値/実測値ではない。長いライン2/30/80/100mは投入時余長の隔離条件で、標準FreeFallの過大繰出しを許す試験ではない。
- E 7件と必要回帰54件が成功。通常回収停止境界、Enhanced Action押下/保持/解放、幾何、Quick全開始状態/期限/拒否、Pause/Focus、装備変更/次投、旧Cast/登録世代/破棄、30/60/120fps、読取非破壊、設定検証/シリアライズを確認。実マウス・PIE・保存資産移行は未実施。ログ/ファイル一覧はROADMAPのE完了記録を参照。

## 13. G追加修正・Shakuri interaction revision（2026-09-17）

この節はD当時の「Rod有効時は巻取り0」を更新する。M11以降の評価は追加しない。

- 原因: Upでライン拘束が上向き速度を与えた後、Returnで竿先が戻ってもライン長は固定だった。エギの上昇慣性と竿の戻しで弛みが蓄積し、次のあおりがラインを張れなくなる。旧D試験の「一連で一度でも上昇」では各回の非作用を検出できなかった。
- 1右押下=1Action、保持連打なし、既存PendingJerkCount/固定Tick/Sequenceを維持。Upは従来のRod Snap、Return開始から短いReel Pulseを適用する。上げ速度ピークへ巻取り速度を重ねず、戻し中の弛み回収を優先する。
- RodTuningへShakuriReelSpeedMpsとShakuriReelSecondsを追加。0/0は旧設定互換で無効。正値は有限・float速度表現可能・時間<=Return時間を要求し、初期化時に凍結する。時間は既存ceil整数Tick換算で、[UpTicks, UpTicks+ReelTicks)だけ要求速度を出す。予約Actionごとに同じプロファイルを再開する。
- 保存Gの仮設定は8m/s×0.25秒（60Hzで15Tick、海面制約のない条件で2m/Action）。実測/製品巻取り速度ではなくゲーム近似のPrototype調整値。通常回収のReelMps=1m/sとは別。係数が不足すると弛みを回収しきれないため、製品バランスはH以後の評価で扱う。
- SessionはRodの要求巻取りをFTREgiActionへ渡し、旧LiftMpsは常に0。EgiのWorldPosition/Velocityへの追加インパルスは導入しない。Cの既存LineLength短縮→位置/速度拘束だけで作用する。
- Shakuri中はReelIn経路を使い、Up中の要求速度は0。竿先が海面から離れるときの最小ライン長（竿先の海面上高さ）を維持する。海面では最小長が増えることがあり、無理な短縮はしない。Eの海面交差円による巻取り要求制限をShakuriにも適用し、浅場の水平ワープ/異常終了を防ぐ。Retrieved通知は通常Retrievingだけであり、Shakuriが勝手にCastを終了しない。
- 通常回収中のJerkは受理し、通常巻取りを停止して複合Actionへ切替、完了後はTF→Stay。通常巻取りを再開するには左を離して再押下する。Shakuri中の左ReleaseはPulseを取消さない。逆順でRetrieveStartedが後なら通常回収へ切替え未実行Jerkを取消す。同TickでもSequence順、両巻取り速度は加算しない。
- Pause中はPulse時間/ラインを進めない。Focus Lostは従来どおり保持/未実行予約を解除し、開始済みActionは再開後に完了する。旧Cast/破棄Sessionの入力契約は維持する。

## 14. M10.5-R Shakuri Sequence / Slack-aware Reel（2026-09-17、設計のみ）

第13節はG実装・自動試験の履歴として保持する。最新手動PIEでは2回目以降の作用が不安定で、約2m/Actionは過剰回収。Rでは固定Reel Pulseを置換する。今回コード/調整値は変更していない。

### 原因の切り分け

現行Rodは時間だけでUp/Returnを進め固定Pulseを要求する。Fishingは各Action終端から次を開始できるが、Rod復帰とラインの伝達準備を一つの条件として扱わない。静水・一定姿勢の試験成功だけでは実マウス姿勢、ドリフト、連打間隔の品質を保証できない。R実装時は各TickのBase/FinalPose、Phase、要求/実行回数、L、RodTip-Egi距離D、Slack、Constraint補正、TensionProxy、Egi位置/速度を記録し、無Pulse/旧Pulse/新方式を比較する。姿勢上限でSnapが潰れる場合も区別する。

### 状態と責務

`Stay/Bottom → [必要なら初期Slack準備] → Jerk1(Up) → Recover(Return + ReelSlack) → Jerk2 → Recover → … → TensionFall → Stay`

- 外側FishingStateは一連中Jerking。内部SequencePhaseをPreparing/Up/Recoverとして管理し、各Jerk間にStay/TensionFallを挟まない。新設予定`UTRShakuriSequenceComponent`はPhaseと受付済みActionを担当し、物理式はCへ置く。Fishingは状態遷移、Rodは姿勢、Egi/Lineは位置・拘束を担当する。
- 1右Down=1要求。Held/Repeatで増やさない。固定Tick/Sequence順にキューへ積み、Up開始時に実行回数を1増やす。回数制限や予約なしの自動Upを追加しない。CastId/登録世代/ModeEpoch違い、破棄済みSessionは拒否。
- UpはDの有限時間プロファイル。RecoverはBasePoseへ戻す処理と必要Slack回収。プレイヤー基準姿勢と一時Offsetは別。戻し中の基準入力も固定順に適用し、当該Actionの基準更新規約を試験する。通常Base最大PitchにはSnap用の余裕を持たせ、上限に貼り付いて無動作となる設定をDataValidationで拒否する。
- 次のUpはRod復帰とSlack準備が成立してから、予約がある場合だけ開始する。準備できなければ安全な速度/時間予算内で継続し、予算超過は診断付きで操作を停止/予約解除する。成功したJerkや通常Stayを偽装しない。異常停止後はF/回収等の明示操作で復旧できるようにする。
- 予約なしでRecoverが完了したら即TensionFall、過渡収束後Stay。無入力秒数、AutoStayDelay、連打猶予タイマーは使わない。安全タイムアウトは異常防御でありStay開始タイマーではない。遅れて届いた押下は次のSequenceとして扱う。
- 既存の一連回数/後続Stay評価用履歴を各Recoverでリセットしない。10回超の将来BITE減衰を実装するのは後続タスク。RにBITE/Range評価を追加しない。

### Slack-aware Reelの計算案

固定長/Actionを廃止する。`Slack = max(0, L - |Egi-RodTip|)` を正本から導出。Preparing/Recoverだけで、目標余長`TargetSlackM`を超える部分を`SlackReelSpeedMps`以内で回収する。Upそのものに固定回収量を重ねない。

Cの同一サブステップで、現在距離Dと拘束前予測距離Dtrialを求め、概念的に次を満たす要求量を計算する。

```
RequiredLength = max(MinLineLength, RodHeightAboveSurface,
                     max(D, Dtrial) + TargetSlackM)
ReelDelta = min(SlackReelSpeedMps * dt, max(0, L - RequiredLength))
```

この式は実装前の数値方式案。古いHUD Snapshotから計算しない。Rod更新後の同じ座標/海面問い合わせを使い、ラインの唯一の更新担当であるCが短縮と拘束を確定する。RequiredLength>Lだからといって通常時に追加繰出しはしない。FreeFall需要繰出し/海面での物理的最小長の既存契約を保持する。

張っているラインをさらに既定長巻く最低ノルマは設けない。TensionProxyは補正量由来で0でも幾何的に張っていることがあるため、正の張力を準備完了の必須条件にしない。目標余長はSnapで解消可能な幾何範囲に制限する。

UpのRodTip移動→Cライン拘束→Egi作用を唯一の伝達経路とし、直接Egi速度加算・深度ジャンプを追加しない。Recoverの回収だけで毎回のダートを代用しない。なお弛みだけを回収しても、強すぎるUpや残留速度による実際の上昇は残るため、Rod振幅/時間と既存水抵抗も測定し、5回で過剰回収しないことを独立に評価する。

調整項目案: TargetSlackM、SlackReelSpeedMps、RecoverySafetyDuration、RodReturnTolerance、既存Up振幅/Up時間/Return時間。すべてPrototype DataAsset。固定2mを製品値/新方式の受入値として継承しない。

### Normal Retrieveとの優先関係

通常回収はLMB保持中の任意巻取りで、Slack回収とは目的・要求を分離する。Rでは既存Gの安全な明示切替を初期案として維持する: 通常回収中にJerkが受理されれば通常回収を停止してSequenceへ移る。Sequence中の新たなLMB Downは明示通常回収への切替として残り予約を解除する。同TickはSequence順で最後に受理された切替が有効。二つの回収速度を加算しない。

右操作後に通常回収を再開するには左を解放して再押下する。これはPrototype操作案として表示し、手動Rゲートで不自然なら保持意図を別管理する案を再検討する。無断の自動Retrieve再開は追加しない。左Releaseで位置/速度/ラインをリセットしない。Focus/Pause/UI捕捉で保持と予約入力を安全に解除し、復帰後は新Downを要求する。F/Quickによる明示切替も旧Castを更新せず、Quickは途中キャンセル不可。

### 受入と観測

1/2/3/5回それぞれ、間隔を空けた押下/動作中予約の両方を試験。全UpでRodTip変位とライン経由の作用が成立し、右Heldでは1回だけ。Snapを無効化した対照ケースとの比較で、単なる前回慣性を今回の作用として数えない。各回の位置差・ライン補正・回収量を個別報告する。

過剰回収の技術受入案: 水深30m、初期エギ深度20m、35g/シンカー0、G標準環境の制御条件で5回後も船外かつ深度15m以上、Slack回収合計2.5m以下。これは未検証のPrototype試験案であり製品バランス/現実計測ではない。実装着手時に条件一式を凍結し、満たせなければ失敗と原因を報告する。他重量、無潮/0.4/0.7/1.0 knot、左右舷、基準姿勢端、浅場も試験し、全条件へ同じ上昇量を強制しない。

有限数、非負L、海面/海底/ライン整合、30/60/120fps、Pause/Focus/Cast寿命を維持。R5は状態/キュー、R6はこの伝達・回収・過剰回収を受入境界とする。手動で各回のダートと自然なTensionFall→Stayを確認するまで品質合格としない。
