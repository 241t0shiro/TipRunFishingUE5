# TipRun Fishing — 全体技術設計の正本


2026-09-13 M10実装反映: Shakuri/TensionFall/Stay/Re-Fall/Retrieve、一投単位の装備ロックと船上帰還後の次投変更、深度速度・境界接触Snapshotを実装済み。旧AutoStay項目は型/検証/Prototype設定から撤去済み。M06〜M09記録は当時の履歴として保持し、現在の契約・検証範囲はROADMAPのM10完了記録を参照。M11以降は未着手。

設計版: 0.2 / 作成・更新日: 2026-09-12 / 対象リポジトリ: `TipRunFishingUE5`
対象: Unreal Engine 5.8.2、C++、Windows / Steam。**M00〜M10は完了。M10操作・一投単位の装備ロック・観測Snapshot・旧AutoStay撤去を実装し、UHT生成/C++/Development Editor Win64ビルド成功。M10 8件・必要回帰32件成功、各試験のエラー・警告0件。M11以降は未着手。PIE目視・実機入力は未検証。**

## 1. 文書の効力と読み方

- **確定要件**: ユーザーが指定したゲーム要件。変更には仕様更新が必要。
- **技術設計**: 本書で提案する実装契約。クラス分割、単位、更新順、参照管理など。実装時の基準とする。
- **暫定案**: ゲーム体験に影響する未承認仕様。実装を必要とする場合は後述の決定IDに紐付け、確認する。
- **MVP決定**: 今回ユーザーが承認したMVP仕様。再承認を求めず実装時の基準とする。未指定の調整値まで確定した意味ではない。
- **保留（MVP暫定仕様あり）**: D10〜D12。MVPでは記載の暫定仕様を使用してよい。製品仕様はAlpha前に再決定する。
- **保留（MVP非ブロック）**: D14、D16〜D18。MVP対象外であり、決定を待ってMVP全体を止めない。
- **テスト用値**: 自動試験の再現性を得るための人工的な値。製品のバランス値でも釣りの実測値でもない。

本書を全体の正本とし、釣りは [FISHING_SYSTEM.md](FISHING_SYSTEM.md)、AIは [SQUID_AI.md](SQUID_AI.md)、船は [BOAT_SYSTEM.md](BOAT_SYSTEM.md)、海は [OCEAN_SYSTEM.md](OCEAN_SYSTEM.md)、UIは [UI_SPEC.md](UI_SPEC.md)、実装順は [ROADMAP.md](ROADMAP.md) を参照する。矛盾を発見した場合は本書の確定要件を優先し、詳細文書を修正してから実装する。Codexの作業規約は [../AGENTS.md](../AGENTS.md)。

## 2. コンセプトと達成範囲

アオリイカのティップランエギングを、エギが通るレンジ、船の流れ、誘い、STAY、微細なアタリ、合わせの判断として体験する。リアリティは映像品質だけで評価せず、操作とレンジ・反応の因果関係が説明できることで評価する。沈下係数やイカの行動モデルは、釣り経験者のレビュー・実測による校正が必要なゲーム近似である。

完成形のループ:

`エリア選択 → 自由操船 → ポイント探索 → 釣り開始 → 投入 → フォール → 着底 → プレイヤー操作のシャクリ → テンションフォール → STAY → ATTACK → BITE → 合わせ → ファイト → 取り込み → 重量計測`

MVPの成立条件:

1. 仮の海・船・エギ・イカ・HUDで1投を開始できる。
2. シャクリは1入力1動作で連続入力可能、回数上限なし。10回を超える一連のシャクリは、後続STAYのBITE確率を回数に応じて極端に低下させる。STAYは竿をあおるシャクリ動作をしていない通常の釣り状態。Shakuri（既存enum名Jerking）→TensionFall→Stayとし、シャクリ直後の過渡処理終了で即時移行する。無入力タイマーは使用しない。STAY後にも自分で再フォールできる。
3. TensionFall/Stay中、船ドリフトによるラインの上向き作用とエギ＋シンカー総重量による沈下作用が釣り合い、狙いレンジを安定維持できる状態を最もBITEにつながりやすい理想状態とする。ドリフト過大/軽すぎによる上昇、重すぎによる下降はともに評価を下げる。重量選択が最重要判断であり、直前の投のレンジ上昇・下降、潮流、船ドリフトを観測し、D08に沿いRetrieve完了後の次投準備で総重量を調整する。一投ごとの重量調整をレンジ安定化と釣果向上につなげる。レンジ安定はStay進入条件ではなくBITE確率の評価要素。BITE承認は原則Stayのみ。RangeError/RangeStabilityの許容幅・時定数・倍率曲線はDataAsset化し、製品値は未確定とする。詳細はFISHING_SYSTEM第4節とSQUID_AI第3節。
4. Approach、Attack、Biteを区別し、早い・適正・遅い合わせを判定できる。
5. HIT後は簡易テンションと巻上げ進捗を扱い、過大テンションの継続でバラシになる。取り込み成功時は固定テスト重量を含む結果を一度だけ生成する。
6. MISSでは投を終了せず、STAY継続または再フォールが可能。通常の未釣獲の投はプレイヤーの回収完了で終える。バラシはFightを終了してStayへ戻す技術設計で、回収時の結果理由に残す。釣獲・明示中断は別の投終了経路とし、古いBITEが次の投に持ち越されない。

MVP対象外: SHOP、購入、通貨、船の自由操船、複数エリアの選択、季節進行、天候変化、本格的なライン物理・ロッド物理、詳細ファイト、取り込み操作、永続セーブ、Steam SDK連携、実績、大会。拡張境界は設けるが、未使用クラスを大量に作らない。

## 3. 構成と依存方向

MVPではRuntimeモジュール `TipRunFishingUE5` を1つとし、内部を責務別ディレクトリに分ける。最初から7個のUEモジュールに分割する必要はない。

| ディレクトリ | 責務 | 参照できる領域 |
|---|---|---|
| `Data` | 共通enum、値型、設定DataAsset、DataTable行 | UE基礎型。ゲームActorを参照しない |
| `Ocean` | 海面・海底・潮流・環境の問い合わせ | Data |
| `Boat` | 船の姿勢、速度、竿先の位置 | Data、Oceanの値契約 |
| `Fishing` | 入力操作、エギ、ライン、合わせ、ファイト | Data、海と船のスナップショット |
| `Squid` | イカの状態・接近・反応・BITE要求 | Data、ターゲットの値契約 |
| `Game` | 生成、ライフサイクル、固定更新、接続と仲裁 | 上記全体 |
| `UI` | 入力接続・表示 | Data、公開されたGame/Fishing読取API |

FishingとSquidは互いのActor/Componentを直接操作しない。`Game` が値型とイベントを渡す。UIからAI状態を変更しない。Oceanから船やエギを検索しない。

将来の配置: `Source/TipRunFishingUE5/{Public,Private}/{Game,Data,Fishing,Squid,Boat,Ocean,UI}`。テストは `Private/Tests`、仮アセットは `Content/TipRun/Prototype`、設定は `Content/TipRun/Data`。主要enum・structも `ETR...` / `FTR...` とする。

## 4. 共通の数値・時間・データ契約

| 型 / 項目 | 定義・所有者 |
|---|---|
| 座標 | UE境界はcm、+Z上。シミュレーションはm、s、g、kgを明記。変換は入出力境界で一度だけ |
| `DepthM: float` | エギ深度の正本。当地の海面から下向き正。描画Zを深度へ逆流させない |
| `FTRSimTime` | `int64 TickIndex` と固定刻み `StepSeconds: double`。世界ごとに所有し、投ごとにはリセットしない |
| `FTRCastId` | セッション内単調増加 `int64`。無効値0。古い投の要求を拒否 |
| `FTRBiteToken` | CastId + 単調増加Sequence。二重解決・期限後解決を防止 |
| `FTRActorSimId` | Coordinatorが登録順に付与する安定ID。Actorアドレスを並び順に使わない |
| `FTROceanSample` | 有効性、海面Z、海底深度、潮流m/s、風m/s、環境ID。詳細はOCEAN |
| `FTRBoatSnapshot` | Tick、位置m、竿先m、速度m/s、向き。読取専用コピー |
| `FTREgiSnapshot` | CastId、Tick、XYm、DepthM、速度m/s、DepthVelocityMps（下向き正）/海底・海面接触（M10で追加済み）、ライン長m、角度rad、張力代理値0..1、釣り状態、StayPenaltyJerkCount:int64 |
| `FTRSquidSnapshot` | SimId、位置m、深度m、AI状態、ActivityLevel（Low/Medium/High）、重量kg |
| `FTRBiteRequest` | CastId、SimId、要求Tick。承認前の要求にすぎない |
| `FTRCatchResult` | CastId、SimId、Outcome、重量kg、装備ID、所要sim秒。釣果確定時にコピーして保持 |
| `ETRCastOutcome` | Caught / Missed / Retrieved / Aborted / Escaped。重さはCaughtのみ有効 |

値型は必要に応じて `USTRUCT(BlueprintType)`、enumは `UENUM(BlueprintType)`。不変スナップショットを渡し、UObjectの所有権を値型に混ぜない。シミュレーション時間はポーズ中に進めない。BITE受付に描画フレーム時間、実時間、アニメーション通知を使わない。

M01の共通型は `Public/Data` に実装。IDは別々のstructで保持し、0を無効値として負値も拒否する。初期化前のFTRSimTimeはStepSeconds=0（未設定）、OceanSampleはbValid=false。スナップショットの数値初期値は未設定時の値であり、製品バランスではない。フィールドはBlueprintReadOnlyで、ID採番・状態遷移・ゲーム判定は後続タスクの所有者が実装する。

イベント契約はFishingCommand、EgiAction、BiteRequest、BiteCue、CatchResultとnative delegateのシグネチャのみ。合わせ結果の理由をETRHookReasonで表し、対象喪失/Stay解除/投終了はTargetLost/StayReleased/CastEndedに対応させる。ETRSampleErrorのNoneはエラーなしを表す。M01で先送りした装備行・FishingTuning・装備係数SnapshotはM02で追加済み。HUD集約型、BITE仲裁処理は各後続タスクで導入する。

秒設定はdoubleで保持し、共通の秒→Tick換算で `ceil(DurationS/StepSeconds)` を用いる。ただし比が整数から1e-6 Tick以内なら先にその整数へ丸め、表現誤差で1Tick延びることを防ぐ。Hook、Cooldown、Fight継続時間で同じ換算を使う。60HzのHook初期値0.10/0.55秒は6/33Tick。Stay進入には秒→Tickの待機設定を使わない。

固定刻み60Hz、最大catch-up 8ステップ/描画フレームを**初期技術案**とする。超過した実時間は積み残さず、診断カウンタを増加させる。低負荷時と同じ実時間進行を保証せず、sim時間と入力判定の一貫性を優先する。高負荷時の体感は要検証。乱数は `FRandomStream` を用途別に分離し、SessionSeed + SimId + 用途IDから安定した整数演算でseedを生成する。同じビルド・同じ入力Tick列で再現できることを目標とし、CPU/バージョンを跨ぐbit一致は保証しない。

## 5. Gameクラス

| クラス / 継承元 | 責務・主要Component | 主要変数 | 主要関数 |
|---|---|---|---|
| `ATRGameModeBase : AGameModeBase` | マップ初期化、依存生成と検証。Componentなし | `SessionConfig: TObjectPtr<UTRSessionConfigDataAsset>`、Coordinator参照 | `StartPlay()`、`InitializeSession()`、`ValidateRequiredActors()` |
| `ATRPlayerController : APlayerController` | Enhanced Inputをコマンド化。表示の作成 | Fishing弱参照、InputMappingContext、入力Sequence | `SetupInputComponent()`、`SubmitFishingCommand()`、`SetInputContext()` |
| `ATRFishingSessionActor : AActor` | 1人分の釣りセッションの所有。Fishing / EgiSimulation / Hook / Fightの4Component | ActiveEgi強参照、Boat弱参照、bEquipmentLocked、LockedEquipment、`LastResult: FTRCatchResult` | `Initialize()`、`StartFishing()`、`TrySetEquipment()`、`EndFishing()`、`ResetCast()`、`FinalizeCast()`、`EndPlay()` |
| `UTRSimulationWorldSubsystem : UTickableWorldSubsystem` | 固定ステップ、登録、更新順、イベント配送、BITE候補仲裁。計算式は持たない | TickIndex、Accumulator、SessionSeed、登録弱参照、CommandQueue、BiteRequests | `Tick()`、`RegisterSession()`、`RegisterSquid()`、`EnqueueCommand()`、`AdvanceFixedStep()`、`Unregister...()` |
| `UTRSessionConfigDataAsset : UDataAsset` | セッション構成。Componentなし | Fishing/AI/Boat/Ocean設定、装備行ID、初期seed、開始エリアID | `IsDataValid()` |

Subsystemの生成対象はGame/PIE worldのみ。Editor preview等ではシミュレーションしない。GameInstanceには投やActorを保持しない。MVPはローカル1人・1セッション・1エギ・テスト用イカ1体（D01、MVP決定）。複数イカは仲裁契約だけ先に用意し、複数個体の配置は試験に限定する。

ゲーム進行 `ETRSessionPhase`: `Initializing → Ready → Fishing → Result → Ready`、設定不正時は `Initializing → Error`。一投の詳細状態はFishingが所有する。開始に成功してからFishingへ、終端イベントの一度だけの受理でResultへ遷移。中断の結果表示方法はD13。

SessionPhaseの正本はSessionActorが所有する。`StartFishing()`で釣りComponentをReadyにし、Deploy受理でPhaseをFishingにする。GameModeは初期化を担当し、Phaseを独自に複製しない。`FinalizeCast(Result)`は現在CastIdと終了済フラグを確認し、最初の終端だけLastResultへ保存してOnCastCompletedを発行する。

### 固定ステップの処理順

時刻Tの状態に対して次を実施し、終了後にTickを1つ進める。

1. 登録解除・中断要求を処理。時刻TまでのコマンドをTick / Sequence順に処理する。既存BITEの期限判定より合わせ入力を先に扱う。新規BITEはこの段階では存在しない。
2. 未解決BITEの期限切れ、その他既存状態のタイマー終了を処理する。
3. Ocean時刻更新 → Boat移動 → Fishing操作/エギ積分を実行し、スナップショットを確定する。
4. SimId順にSquid AIを進める。BITE要求は収集し、まだ確定しない。
5. Hook未占有、CastId有効、エギStay、対象が有効な要求のみ、距離昇順→SimId順で1件承認。他はCautionへ通知。承認時刻はT。
6. Fight更新、終端イベントの確定、スナップショットと表示イベントを配信する。表示側からの新しいコマンドは次Tick以降へ送る。

入力はイベント受信時に「次の未処理Tick」と連番を付け、過去Tickへ挿入しない。同じTickで早い合わせをした場合は、その投のBITE要求を抑止しCaution通知する。BITE期限の境界はSQUID_AIに統一する。

CoordinatorがAIスコア、沈下、フッキング成否、ファイト進捗を計算するのは禁止。ComponentのAPIを決められた順に呼ぶだけにする。

### M04の実装契約（2026-09-13）

- `UTRSimulationWorldSubsystem`をGame/PIEだけに生成。`UTRSessionConfigDataAsset`はM04で必要な`StepSeconds / MaxCatchUpSteps / SessionSeed`だけを実装した。刻み・上限の初期値0は未設定として拒否し、有限性・正値・フレーム予算のオーバーフローをRuntime検証とIsDataValidで確認する。60Hz/8ステップ、seed 12345は一時的なTest設定で、製品設定アセットは作成していない。将来の装備・AI・船等の設定参照は該当タスクで追加する。
- `Configure`はWorldごとに一度だけ設定を値コピーする。固定時刻、登録ID、入力連番を投ごとにリセットするAPIは設けない。`GetSimulationTime`は次の未処理Tickを返す。Oceanには更新中の`T * StepSeconds`をBoatフェーズ前に渡す。未初期化のOceanは従来どおり時刻設定を拒否し、Coordinatorが代替の海を生成しない。
- `RegisterSession / RegisterSquid`は同一WorldのActorを弱参照で登録し、全登録共通の増加SimIdを付与する。同一Actorの重複登録を拒否し、IDを再利用しない。EndPlayから`Unregister`を呼ぶ契約とし、重複解除を許容する。破棄済みActorは各呼出し前に検査し、次の固定更新冒頭で登録・入力を除去する。更新中の追加登録は次Tickから参加し、解除は同Tickの残りの通知にも反映する。World終了時に全登録・入力を解除する。
- 後続Componentの接続口はnativeの`FTRSimulationStep / FTRSimulationCommand` delegate。`ETRSimulationPhase`により、入力→全登録のTimers→Ocean→SessionのBoat/Fishing→SquidのAI用フェーズ→SessionのBiteResolution/Fight/Publishの順で配送する。各フェーズ内はSimId順。M04は配送順だけを実装し、BITE要求の収集・仲裁、状態遷移、船・エギ・AIの計算は後続タスクに残す。delegate内でActorを所有参照せず、登録所有者に結び付けた弱いバインドを使用する。
- `EnqueueCommand`は行先SimIdとコマンド種別を受け、CoordinatorがWorld共通Sequenceを採番する。通常入力は次の未処理Tick、固定更新中の入力はT+1以降へ割当てる。決定論的再生では明示した未来Tickも受け付ける。過去Tick、不正な種類・非有限Axis、無効な行先、Squid宛て入力を拒否する。配送はTargetTick→Sequence順。更新中の`ClearCommands`は取り出し済みの未配送入力も破棄する。CastId/Tokenによるゲーム状態の検証はM06以降の所有者が追加する。
- `SetSimulationPaused`またはEngineのWorld pauseで時計を停止し、入力と端数時間を破棄する。Engine pause中も入力破棄のためSubsystemのTickだけを受ける。コールバック中のポーズ要求は進行中の固定ステップを完了し、後続ステップを止める。UIの保持キー解除・再接続はM09で実装済み（UI_SPEC第7節）。catch-upは端数を含む累積時間を1フレーム予算へ制限し、超過分を持ち越さず`CatchUpDropCount`を増やす。非有限/負の経過時間を無視し、整数時計のオーバーフローでは停止する。再入による二重更新を拒否する。
- `CreateRandomStream`はSessionSeed、64bit SimIdの両半分、uint32用途IDを固定の符号なし整数演算で混合する。用途IDは呼出し側で固定し、生成したFRandomStreamを各所有者が継続保持する。同じAPIを再度呼ぶと初期状態へ再生成するため、毎Tickの再生成は行わない。Actorアドレス・グローバル乱数・FNameの内部番号に依存しない。同一ビルドの再現性を検証し、異機種間のbit一致や確率バランスの評価は行っていない。

## 6. 所有・イベント・Blueprint

以下は旧D08のM06実装記録であり、一投単位ロックへの移行はM10で行う。M06で`ATRFishingSessionActor`と最小の`UTRFishingComponent`を実装済み。Sessionは釣り開始からEndFishingまで装備をロックし、単調増加CastIdとSessionPhaseを保持する。M04入力にExpectedCastIdを追加して古い投を拒否し、M05 Boat更新後に竿先直下の海面へ投入Snapshotを作る。M06ではFreeFallのPayout指示までを実装した。明示中断・次投Ready・終了は装備ロックと寿命を検証する最小経路で、結果UI・釣果イベント配送は先行実装していない。詳細はFISHING_SYSTEM第3節のM06契約を参照。

M07で`UTREgiSimulationComponent`の鉛直落下・海底制約と、数値Snapshotを表示する`ATREgiActor`を追加。M02の凍結した重量由来落下速度と調整係数を使用し、M03のエギ位置の海面/水深を問い合わせ、M04のFishingフェーズでBoat更新後に一度だけ進める。着底によるBottomContact遷移はFishingが所有し、Publishフェーズで一度だけ通知する。描画・Chaosから数値へ逆流させない。水平潮応答・船追従・ライン繰出し/球面制約はM08に残し、M07のライン項目は投入時の仮値を維持する。詳細・検証限界はFISHING_SYSTEM第4節のM07契約とROADMAP完了記録を参照。

M08でEgiSimulationの水平潮応答・ライン長更新・竿先を中心とした球面制約を追加。Gameから移動先Ocean問い合わせを渡し、両制約が成立した数値だけを確定する。牽引で離底した際はFishingがTensionFallへ移す。上記M07のライン仮値という記録は実装履歴として残す。計算・試験・レンジ評価へ渡す情報と後続範囲はFISHING_SYSTEM第4節のM08記録を参照。改訂D08の一投単位ロックはM10で移行し、M08では旧M06の寿命契約を維持する。

M05で`UTRSimulationWorldSubsystem::RegisterBoat`と船Snapshotの読取APIを追加済み。GameがM03 Oceanから値を取得し、M04のBoatフェーズで`ATRBoatPawn`→`UTRBoatDriftComponent`を一度だけ進める。船は共通SimId・弱参照で登録し、EndPlay/Unregister/World終了で登録とバインドを解除する。船の計算、調整値の凍結、無効環境への技術防御、試験と未検証項目は[BOAT_SYSTEM](BOAT_SYSTEM.md)第6節を参照。M06のSessionActor、自由操船、風・波の物理は追加していない。

- 所有Component/Actor/設定は `UPROPERTY` と `TObjectPtr`。非所有の船・イカ・対象には `TWeakObjectPtr` を使い、各ステップで有効性を確認する。
- Session終了時はエギ破棄、BITE無効化、Coordinator登録解除、delegate解除を実施。イカ破棄時も予約解除。破棄済みActorへイベントを送らない。
- Gameplay更新を各Actorの独立Tick、TimerManager、Blueprint Tickへ分散させない。メッシュ補間だけ描画Tickを許可。
- 内部通知はnative delegateまたは明示的関数。UI向け `OnFishingStateChanged`、`OnBiteCue`、`OnHookResolved`、`OnCastCompleted` は必要なものだけdynamic multicastで公開。
- Runtime変数は `BlueprintReadOnly`。入力入口だけ `BlueprintCallable`。表示演出用 `BlueprintImplementableEvent` の戻り値に成否を依存させない。
- GameModeのSessionConfigは `EditDefaultsOnly`。Actor配置の初期座標は `EditInstanceOnly`。不正な必須参照は開始を止め、Errorと不足アセット名を表示する。

## 7. データ設計

設定DataAssetは `UTRSessionConfigDataAsset`、`UTRFishingTuningDataAsset`、`UTRSquidTuningDataAsset`、`UTRBoatTuningDataAsset`、`UTROceanAreaDataAsset`、`UTRInputConfigDataAsset`。MVPは `UDataAsset` で十分。エリアの非同期ロードが必要になった段階で `UPrimaryDataAsset` / Asset Manager導入を検討する。[Epic: Data Assets](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-assets-in-unreal-engine)

反復する商品行は `FTREgiSpecRow : FTableRowBase`、`FTRSinkerSpecRow : FTableRowBase`。文字列の表示名で識別せずFName IDを使う。初期エギは3.5号35g、シンカー無し0gを正式に許可（D08）。装備変更はエギが船上にあり、現在のCastが終了済みの準備状態でのみ許可する（初投前は活動中Castなし）。Deploy受理時に装備をロック/スナップショット化し、一投中のエギ・シンカー変更は禁止。Retrieve完了によるCast終了と船上帰還の確定後、次投準備で再選択できる。EndFishingは必須ではなく、Cast終了だけ/Readyだけで船上条件を代用しない。前投の釣果Snapshotは維持する。初期シンカー選択値は未指定で、試験構成では0gを使用する。

必須バリデーション: エギ/総重量>0、シンカー重量>=0（0gは無しの専用ID）、深度>=0、時間>=0、有限数、曲線の定義域、OpenDelay<CloseDelay、速度上限>0、ID一意、設定参照の存在。値が欠落した場合に黙って製品用既定値を補わない。

M02で装備行とFishingTuning、装備係数Snapshotを実装済み。PrototypeのDataTable 2件とFishingTuning 1件をUEで保存し、別プロセスの読込で27組合せを検証した。設定確認は行/TuningのIsDataValidとテーブル間のValidateTablesで行う。Snapshotは評価済み係数と設定のコピーを保持する。技術上の検証制約・試験係数の扱いはFISHING_SYSTEM第2節、ビルド・試験結果はROADMAPのM02完了記録を参照。その他のDataAsset、装備ロック、海の問い合わせやシミュレーション処理は後続タスクの範囲。

M03でOceanAreaDataAsset、平底SeabedProvider、OceanWorldSubsystemを追加し、上記M02時点で未実装だった海の問い合わせを実装済み。Game/PIE限定で、初期化時の設定コピーから値を返す。設定検証・Provider寿命・境界と不正値を扱う技術防御を実装し、D14の境界ゲームルールは追加していない。詳細はOCEAN_SYSTEMのM03記録を参照。M04で固定時計・Coordinatorの基盤とOceanへの時刻配送を追加済み。斜面はM16に残す。

## 8. 要決定事項の正本

更新日・根拠: 2026-09-12、ユーザーによるD01〜D18の明示指定。D01〜D09/D13/D15はMVP決定、D10〜D12はMVP暫定仕様を承認、D14/D16〜D18はMVP非ブロックとして保留。v0.1の相反する暫定案を本表で置き換える。

| ID | 状態 | MVPで適用する仕様 | 残る詳細・再決定時期 |
|---|---|---|---|
| D01 | MVP決定 | ローカル1人、テスト用イカ1体 | 配置は試験設定、製品の個体群はAlpha以降 |
| D02 | MVP決定 | キャストなし、竿先付近投入、FreeFall自動繰出し | 繰出し速度等は調整値。ライン近似はFISHINGの技術設計 |
| D03 | MVP決定 | 1入力1シャクリ、連続入力可能、**回数上限なし**、終了後TensionFall。**10回超で、その後のStayのBITE確率を回数に応じ極端に低下** | 同日のユーザー補足で「上限あり」を置換。低下曲線はDataAsset。回数の区切り・保持はFISHING、確率式はSQUID_AI |
| D04 | MVP決定（2026-09-13改訂） | STAYはシャクリ動作をしていない通常の釣り状態。Shakuri→TensionFall→Stay | 過渡処理終了で即移行。AutoStayDelay/無入力タイマー廃止。上昇/下降中もStay可、レンジ安定度と状態判定を分離 |
| D05 | MVP決定 | Chaos完全依存を避け、Depth/Velocity/LineAngle/Current/BoatDrift/Weightで数値シミュレーション | 係数はDataAsset、実測校正は別途 |
| D06 | MVP決定 | BITE開始から0.10秒で受付開始、0.55秒で終了。早合わせ/時間切れはMISS | 開閉値は調整可能。区間は `[0.10,0.55)` |
| D07 | MVP決定（2026-09-13補足） | 活性3段階、距離、レンジ差、STAY時間に加え、ドリフトと総重量の釣合いによるレンジ維持をBITE評価 | RangeError/RangeStabilityによる維持良好ほどBITE高確率、上昇/下降で低下。許容幅・時定数・曲線はDataAsset調整、製品値未確定。季節補正は後回し |
| D08 | MVP決定（2026-09-13改訂） | 初期エギ3.5号35g、シンカー無し0g正式許可。船上かつCast終了済みの準備状態のみ装備変更可。一投中は禁止、Retrieve完了後の次投準備で変更可 | 直前の投のレンジ上昇/下降・潮流・船ドリフトから次投総重量を調整。初投前は活動中Castなし。初期シンカー選択は未指定、試験では0g |
| D09 | MVP決定 | 簡易テンション＋巻上げ進捗、過大テンション継続でバラシ | 上昇/回復率・閾値・継続秒数はDataAsset調整値 |
| D10 | 保留 | MVP暫定: 固定重量テストイカ。重量分布は実装しない | 固定kg値は試験設定。製品仕様はAlpha前に再決定 |
| D11 | 保留 | MVP暫定: 仮BITE Cue 1種類、3種へ拡張可能な型 | 3種演出・大型との相関等、製品仕様はAlpha前に再決定 |
| D12 | 保留 | MVP暫定: 開発HUDに内部情報を表示 | キーは試験割当。製品HUD/入力仕様はAlpha前に再決定 |
| D13 | MVP決定 | MISSで投を終了せずStayまたは再Fall。プレイヤー回収で投終了 | 釣獲/バラシ/明示中断の終端処理はFISHING参照 |
| D14 | 保留 | MVP非対象・非ブロック。境界/岸/根掛かりゲーム仕様を追加しない | Alpha以降。無効データ防御は技術処理として維持 |
| D15 | MVP決定 | 一定水平潮流。風・波の物理影響なし | 風/波/鉛直潮/変動潮はAlpha以降 |
| D16 | 保留 | MVP非対象・非ブロック: 自由操船等 | Alpha以降 |
| D17 | 保留 | MVP非対象・非ブロック: SHOP/経済/セーブ | Alpha以降 |
| D18 | 保留 | MVP非対象・非ブロック: Steam連携/実績/大会等 | Alpha以降 |

IDは維持し、変更日・根拠と詳細設計/試験を一緒に更新する。MVP決定を再び未承認扱いにしない。D03の低下倍率など未指定の調整値、技術式、テスト用数値は製品バランスの確定とは区別する。

## 9. 検証と実装前確認

- 純粋計算試験: 単位換算、深度制限、確率、受付境界、終了の冪等性。
- UE Automation: Component状態遷移、設定検証、破棄と再投入、同seed再現。
- Functional Test: `L_TR_MVP_Test`で入力から1投完結、自然反応と制御された試験反応を別に確認。
- PIEとWindows Developmentパッケージで起動、入力、結果、再投を確認。描画30/60/120fpsで同じTick入力列の結果を比較する。
- M00確認済み（2026-09-12）: UE 5.8.2（CL 56702186）、Runtimeモジュール `TipRunFishingUE5`、Game/Editorターゲット、基本ディレクトリとBuild.cs依存を確認。`TRBase`はC++化確認のための仮クラスとして残存する。
- 確認環境: Visual Studio Community 2026 18.10.0、MSVCツールセット14.51.36231（コンパイラ14.51.36257）、既存ビルド設定のWindows SDK 10.0.22621.0。MSVCはUEの推奨範囲より新しい旨の警告あり。これは確認環境の記録であり、必須バージョンの指定ではない。
- Development Editor Win64の通常ビルドは `Succeeded`（終了コード0）。`Target is up to date`、実行アクション0件であり、新規コンパイル・UHT反射コード生成の実処理は今回未確認。既存EditorログでプロジェクトDLL読み込みとEngine初期化成功を確認したが、新規の空マップ起動・PIE操作は未実施。M01の反射宣言導入時に通常ビルドとUHTの実処理を確認する。詳細はROADMAPのM00完了記録を参照。

公式資料確認日: 2026-09-12。5.8.2公開は [Epic Hotfix告知](https://forums.unrealengine.com/t/5-8-2-hotfix-released/2746335) で確認。入力接続は [Enhanced Input](https://dev.epicgames.com/documentation/unreal-engine/enhanced-input-in-unreal-engine)、世界単位サービスは [Programming Subsystems](https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-subsystems-in-unreal-engine)、検証の実行手段は [Automation System User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-user-guide-in-unreal-engine) を参照。これらはUE機構の参照であり、本作のゲーム仕様・釣りの科学的根拠ではない。

## M09実装補足（2026-09-13）

`ATRPlayerController`→Session→固定Input Queueの入力経路と、Publish時の数値コピーから作る`FTRHUDSnapshot`を追加。HUDは状態/深度/重量/ライン/船速/潮流を表示し、釣りの数値・状態を更新しない。`ATRGameModeBase`は既存Ocean/Boat/Sessionと入力/HUDの初期化・接続のみを担当する。調整値・仮キー割当はPrototype SessionConfigおよび所有するDataAsset/Input Actionに保持する。製品値を確定していない。

UHTは新規反射型を含む17ファイルを生成し、C++実コンパイルとDLLリンクに成功。保存済みPrototypeを別プロセスで再読込する試験を含め、M09 7件と回帰13件が成功。詳細な実装契約・操作準備・未検証範囲はUI_SPEC第7節、変更一覧・ログはROADMAPのM09完了記録を参照。既存M06の装備ロック寿命は保持し、D08移行はM10で実施する。Shakuri/TensionFall/Stay、回収の実動作、旧AutoStay設定撤去、レンジ評価、BITEには着手していない。

## M10実装補足（2026-09-13）

Fishingが入力受理、シャクリ予約/一連の回数、Shakuri→TensionFall→Stay/Re-Fall/Retrieveを所有する。EgiSimulationが固定更新で上向き作用・残留リフト減衰・巻取りをM08の潮流/重量/船/ライン制約へ合成する。過渡完了は残留リフトの処理完了であり、無入力待機やレンジ安定判定ではない。新係数はDataAssetの試験値で、式と受入条件はFISHING_SYSTEM第8節を参照。

Sessionは次投候補の準備とDeployでの凍結、RetrieveでのCast終了・船上帰還、NextCastでのReady/装備ロック解除を管理する。EndFishingなしで次投重量を変更でき、旧Cast・過去の結果/装備コピーは更新しない。Abortだけでは船上帰還を作らない。M06当時のセッション全体ロックの記録を現行仕様と読み替えない。

SnapshotのDepthVelocityMps/境界接触/状態開始Tick/連続TF・Stay観測時間を後続へ公開した。RangeObservationSecondsは安定時間ではなく、安定履歴の評価はM11、BITEへの倍率接続はM12に残す。旧AutoStayの宣言・検証・Prototype保存項目を撤去し、汎用時計の試験は維持。検証・変更一覧はROADMAPのM10記録、仮入力と次投装備変更の手順はUI_SPEC第8節を参照。
