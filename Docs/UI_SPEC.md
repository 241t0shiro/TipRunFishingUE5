# UI・入力接続技術設計


2026-09-13 M10実装反映: Shakuri/TensionFall/Stay/Re-Fall/Retrieve、一投単位の装備ロックと船上帰還後の次投変更、深度速度・境界接触Snapshotを実装済み。旧AutoStay項目は型/検証/Prototype設定から撤去済み。M06〜M09記録は当時の履歴として保持し、現在の契約・検証範囲はROADMAPのM10完了記録を参照。M11以降は未着手。

関連: [全体正本](GAME_DESIGN.md)、[操作](FISHING_SYSTEM.md)、[BITE](SQUID_AI.md)。MVPのデバッグ表示と製品のプレイヤー向け情報を区別する。

更新: v0.2 / 2026-09-12。D10〜D12は保留（MVP暫定仕様あり）。MVPでは固定重量・仮Cue1種・内部情報HUDを使い、製品仕様はAlpha前に再決定。

## 1. 画面遷移

完成形（将来）:

`START → ポイント選択 → ボート移動 → 釣り → 結果`

`START / ポイント選択 ↔ SHOP`（往復先と購入動線はD17で確定）。SHOPにはロッド、リール、エギ、シンカー、船を扱う予定。MVPに購入処理・仮通貨・ショップ画面を作らない。

MVP暫定仕様（使用承認済み）:

`起動・設定検証 → 釣り開始パネル → 釣りHUD → 結果パネル → 次投の準備`

| UI状態 | トリガー | 次 | 入力 |
|---|---|---|---|
| Initializing | GameのReady通知 | Ready | 釣り開始/投入への案内 |
| Initializing | 必須設定不足 | Error | 原因表示、ゲーム操作禁止 |
| Ready | 投入成功 | FishingHUD | Fishing Mapping Context |
| FishingHUD | OnCastCompleted | Result | 結果操作のみ |
| Result | 次投 | Ready | 古い結果の表示解除 |
| 任意のゲーム中画面 | Pause | Pauseを重ねる | 釣り入力解除、sim停止 |
| Pause | Resume | 元画面 | キー解放を確認してからゲーム入力 |

UI状態はGameのSessionPhaseとFishingStateから導出する。別のゲーム進行ステートマシンをWidgetに持たせない。PauseとErrorの表示状態だけUIが保持する。D13によりMISSでResultへ移らずFishingHUDを維持する。回収完了/釣獲/明示中断でResultへ移る技術設計。バラシは仮通知を出して釣りHUDへ戻り、その後の回収結果に理由を残す。

D08の装備変更UIは、エギが船上かつ現在Cast終了済み（初投前は活動中Castなし）の準備状態だけ有効。Retrieve完了後、次投Readyでエギ・シンカーを変更でき、EndFishingを要求しない。投中/回収中/船上未確認は無効。UIと変更APIが同じ許可条件を使用する。直前の投で観測した上昇/下降・潮流・船ドリフトを基に次投総重量を調整する。既存の深度/潮流/船速表示を観測に用い、自動重量選択は追加しない。初期エギ3.5号35g、シンカー選択肢「無し（0g）」を表示。ショップ購入処理は含めない。

## 2. クラスと責務

| クラス / 継承元 | 責務・主要Component | 主要変数 | 主要関数 |
|---|---|---|---|
| `ATRHUD : AHUD` | root Widget生成と接続。Componentなし | RootWidget:TObjectPtr、Controller/Session弱参照 | `BeginPlay()`、`BindSession()`、`UnbindSession()`、`EndPlay()` |
| `UTRRootWidget : UUserWidget` | Ready/HUD/Result/Errorの切替。WidgetSwitcher | CurrentView、子Widget参照 | `ApplySessionPhase()`、`ShowError()`、`ShowPause()` |
| `UTRFishingHUDWidget : UUserWidget` | スナップショットとCue表示。Text/ProgressBar等 | `DisplaySnapshot: FTRHUDSnapshot`、DebugVisible | `ApplySnapshot()`、`HandleBiteCue()`、`ClearTransientCues()` |
| `UTRResultWidget : UUserWidget` | 釣果・終了理由、次投ボタン | Result:FTRCatchResult | `ApplyResult()`、`OnNextCastClicked()` |
| `UTRInputConfigDataAsset : UDataAsset` | MappingContextとInputAction参照 | FishingContext、UIContext、各Action:TObjectPtr<UInputAction> | `IsDataValid()` |
| `ATRPlayerController : APlayerController` | 入力意味の変換、コマンド送信。Game文書と同一クラス | InputConfig、InputSequence、Session弱参照 | `SetupInputComponent()`、`SubmitFishingCommand()`、`SetInputContext()`、`SetPauseRequested()` |

Blueprintで上記UUserWidgetの派生Widgetを作り、配置・色・文字・演出を設定する。C++側でdelegate接続と表示データ作成を行う。OnNextCastClickedはControllerへ要求し、FishingComponentの受理後に画面を更新する。

依存: UI→公開Game/Fishing APIとData。SquidのComponentへ直接Bindしない。開発HUD用のAISnapshotはCoordinatorが提供する。Widget構築時の重複Bindを防ぎ、Destruct/EndPlayで解除する。

## 3. HUDデータ

`FTRHUDSnapshot`はC++で作る表示専用USTRUCT。状態の変更権限を持たない。

| 項目 | MVPデバッグ | 製品版 |
|---|---|---|
| 釣り状態、操作ガイド | 表示 | ガイド形式はD12 |
| エギ深度m、海底深度m、底からの距離 | 数値表示 | 精度・魚探との関係は未決 |
| エギ号数、総重量g、シンカーg | 表示 | 装備HUDに統合予定 |
| シャクリ回数 | 投内合計/一連/Stayに適用する回数、予約数を別表示 | 常設するか未決 |
| ライン長m、角度degree、張力代理値 | 試験用表示 | 実在の計器として扱わない |
| 船速、潮流 | 試験用ベクトル/数値。風・波の物理表示なし | 表示方法未決 |
| イカAI状態・深度・3段階活性・Exposure・STAY時間 | 開発専用 | 製品表示範囲はAlpha前に再決定 |
| RangeErrorM、RangeStability01、RangeHoldScore、維持倍率、10回超BITE減衰倍率 | 開発専用、DataAsset調整結果を確認 | 製品表示範囲はAlpha前に再決定 |
| Attack / Bite、Open/CloseTick、残り秒 | 開発専用 | 受付窓を表示するかは未決 |
| アタリCue | Prototypeの仮表示1種類 | 3種類をロッド演出へ接続、Alpha前に再決定 |
| HIT / MISSと理由 | 試験用表示 | 言葉・音・時間は未決 |
| FightProgress / FightTension01 / 連続超過残り時間 | 別ゲージ/数値、過大テンションとバラシ理由を表示 | 詳細ファイト時に見直す |

ステータスは固定更新の最後のSnapshotから更新する。WidgetのプロパティBindで毎フレームActorを検索しない。表示値の更新頻度は初期技術案10Hz、State/Cue/Resultイベントは次の描画フレームで反映する。10Hzの数値更新をBITE演出に流用しない。

結果: Caught時は重量kgと装備を表示。他Outcomeは「釣果なし」と終了理由を表示し、0kgの釣れたイカとして扱わない。結果の丸めはUIだけで行い、正本WeightKgを書き換えない。小数桁はD10。試験個体1.25kgなどはテストデータと明記する。

## 4. 入力設計

Enhanced Inputを使用し、キーそのものと意味上のコマンドを分離する。[Epic Enhanced Input](https://dev.epicgames.com/documentation/unreal-engine/enhanced-input-in-unreal-engine)

| InputAction案 | 値 / イベント | 送信先 |
|---|---|---|
| IA_TR_Deploy | Bool / Started | Deploy |
| IA_TR_Fall | Bool / Started | Fall |
| IA_TR_Jerk | Bool / Started | Jerk |
| IA_TR_TensionFall | Bool / Started | TensionFall |
| IA_TR_Hook | Bool / Started | Hook |
| IA_TR_Retrieve | Bool / Started、Completed、Canceled | RetrieveStarted/Stopped |
| IA_TR_EndFishing | Bool / Started | EndFishing |
| IA_TR_Pause | Bool / Started | Gameのポーズ要求 |

STAY専用ボタンは設けず、シャクリ直後の過渡処理終了後の通常状態として表示する。残り待機秒は表示しない。TensionFall入力はライン制御への要求でありSTAY実行ボタンではない。

Hook/JerkをTriggeredの毎フレームイベントから送らない。巻き入力のCompletedだけでなくCanceledでも停止する。Result移行・フォーカス喪失・Pause時は保持入力を0にし、キュー済みゲーム入力を破棄する。

UIContextとFishingContextは同じ入力を二重消費しないよう切替。キー配置、マウスホイール、長押し、ゲームパッドはD12。キーボードの仮割当を作る場合も `Prototype` と表示し、製品確定として文書化しない。InputContextを外した時点で押されていたキーは、再接続後に一旦解放するまでHook/Jerkを発行しない。

## 5. Blueprint公開・アセット

- C++の読取スナップショットはBlueprintReadOnly。レイアウトは `BindWidget` 等で明示接続。
- `HandleBiteCue`から表示イベントへ渡す。Widgetが `TryOpenBite` を呼ばない。
- InputConfig、Widgetクラス参照、フォント・色・表示書式はDataAsset/Widget設定へ分離。MVPにUI専用DataTableを必須化しない。
- UI文字列はFText、IDはFName。将来の翻訳が可能な形にするがMVPで翻訳機能を作らない。
- 開発HUDの表示切替は開発ビルド限定。製品HUDからデバッグ情報を削除できるよう独立パネル化する。

## 6. テスト

| ID | 操作 | 期待 |
|---|---|---|
| U01 | 開始→投入→HIT→結果→次投 | 画面と実状態一致、結果1回 |
| U02 | Hook長押し | Startedで1コマンド、毎フレーム連打にならない |
| U03 | 巻き中Pause/フォーカス喪失/Resultへ移行 | 巻きフラグ解除、再開で勝手に巻かない |
| U04 | Widget再作成10回、投を再実行 | イベント重複なし、破棄Widgetへの通知なし |
| U05 | 数値10Hz更新中にBite | Cueは数値更新周期を待たない |
| U06 | Caught/Aborted/Retrievedの各結果 | 重量の有効性が正しい |
| U07 | 1280×720、1920×1080、拡大率変更 | 操作ガイドと結果が重ならず読める |
| U08 | デバッグパネル非表示 | AI/受付窓が漏れず、釣りロジックは同じ |
| U09 | MISS→Stay継続→Fall→回収 | MISSではHUD維持、回収完了で結果1回 |
| U10 | Fightで巻く/離す/過大テンション継続 | 進捗とテンションを区別、バラシをHIT/MISSと混同しない |
| U11 | 初投準備/投中/回収中/Retrieve後の次投Ready/終了済みだが船上未確認で装備UIとAPIを操作 | 船上かつCast終了済みの準備状態だけ変更可、0g選択可。Retrieve後にEndFishing不要、次投の重量/メッシュ更新、前投の結果装備は不変 |
| U12 | 10/11/20回→TensionFall→過渡処理終了でStay、維持/上昇/下降 | 回数上限なし、適用回数と減衰倍率一致。STAY専用入力/待機秒なし、レンジ維持指標がSnapshotと一致 |

MVPではFunctional Testと目視で十分なレイアウト確認を行い、Widgetの内部構造をそのまま写した大量の単体テストは不要。

## 7. M09実装・検証記録（2026-09-13）

M09は完了。Enhanced Inputと最小の内部確認用HUDを追加した。M10以降の操作ロジック、Root/Resultパネル、製品HUD、BITE/Cue/イカ/レンジ評価は未実装。上記の完成形クラス表・試験表は将来範囲を含み、本節がM09の実装範囲を示す。

- `ATRPlayerController`が入力を意味コマンドに変換し、Sessionの公開API→M04 Input Queueへ送る。TargetTickと世界内Sequenceで処理順を決め、Game/Fishingの既存受理判定を維持する。Jerk/HookなどはStartedのみ、RetrieveはStartedとCompleted/Canceledで開始・停止を送る。Triggeredによる毎フレーム連打はしない。
- `ETRPlayerAction`、`FTRInputBinding`、`UTRInputConfigDataAsset`で機器と意味を分離。9種類の重複しないBoolean Actionと有効なMapping Context/キーを検証する。接続し直すときは自身のBinding/Contextだけを解除し、押下済みキーは解放まで無視する。UIContextの本格切替は結果パネル実装時に残す。
- 古いCastId、終了・破棄済みSession、異なるWorldへの接続を拒否する。Session登録世代/CastId変更、Pause、アプリの非アクティブ化、ViewportのFlushPressedKeysで保持入力とゲーム入力キューを解除する。Retrieve停止だけは同じ有効Castへの安全な停止コマンドとして再開後に送る。保持の自動再開はなく、安全側に再押下が必要になる場合がある。ControllerのEndPlay/DestroyedでBindingとSlate delegateを解除する。全キュー解除はMVPの1ローカルSession前提。
- `FTRHUDSnapshot`はSessionがPublishフェーズで保存するEgi/Boat/Oceanの表示コピーと、現在のPhase/CastId/装備/有効性から成る。読取APIは時計、キュー、海問い合わせ、数値シミュレーションを進めない。未投入・終了後のエギはN/A、無効な環境情報もN/Aと表示する。
- `ATRHUD`と`UTRFishingHUDWidget`がCastId、Fishing State、エギ深度、水深、鉛直速度、ライン長、角度、張力代理値、船速度、潮流、エギ・シンカー・総重量を表示する。鉛直速度は既存SnapshotのワールドZ速度（上向き正）であり、DepthMの差分速度ではない。数値表示は技術案10Hz、状態/投/有効性の変更は次描画で反映。Shippingでは当HUDを生成しない。現段階のC++仮レイアウトは製品レイアウトではない。
- `ATRGameModeBase`は設定検証、Ocean/Boat/Session生成とController接続を担当する。式や釣り操作は持たない。`UTRSessionConfigDataAsset`に参照と初期船座標を追加。時計だけの設定はM04互換とし、起動用参照を設定したDataAssetはIsDataValidで起動依存も検証する。起動途中の失敗では生成物を破棄し、海を解除し固定時計を止める。理由はStartupErrorへ保持し、設定修正後はWorldを再起動する。

### PrototypeでのEditor確認手順

今回UEのSavePackage/CreateBlueprintで以下の2アセットを新規生成し、別プロセスで再読込・Session開始・投入・HUD Snapshotを確認した。既存アセットを上書きする生成処理はない。

- `/Game/TipRun/Prototype/Data/DA_TR_M09Session_Prototype`: 60Hz/最大catch-up 8、平底30m、水平潮(1,0,0)m/s、船応答0.5/応答率2/上限3m/s、初期位置(10,20)mのテスト設定。Ocean/Boat/InputConfig/Mapping Context/9 Actionはこの設定が内部所有する。M02の既存装備Table・FishingTuningを参照し、35g＋シンカー0gで開始する。すべて試験用値で製品バランスではない。
- `/Game/TipRun/Prototype/BP_TR_M09GameMode_Prototype`: 上記設定を参照するGameMode派生。既存マップ・Project Default GameModeは変更していない。

1. Editorで検証用Levelを開き、World SettingsのGameMode Overrideへ`BP_TR_M09GameMode_Prototype`を指定する。
2. Selected ViewportのPlayで起動し、Viewportにフォーカスを置く。既存DefaultInput.iniのEnhancedPlayerInput/EnhancedInputComponent設定を使う。カメラは既存Boatの表示カメラを使用する。海面/船メッシュの表示資産はM09では追加していない。
3. 以下の仮割当で投入・Pause・中断とHUDを確認する。ゲームパッド配列は将来対応を妨げないための仮設定で、実機操作の検証は未実施。

| 意味 | Prototypeキーボード | Prototypeゲームパッド |
|---|---|---|
| Deploy | Enter | 下側フェイス |
| Shakuri（Jerk） | Space | 右ショルダー |
| Fall / Re-Fall | F | 左側フェイス |
| TensionFall要求 | T | 左ショルダー |
| Hook | H | 上側フェイス |
| Retrieve / Reel | Rの押下/解放 | 右トリガー |
| Cancel / EndFishing | Backspace | 右側フェイス |
| NextCast | N | Special Left |
| Pause / Resume | P | Special Right |

Deploy/NextCast/EndFishingは既存M06の状態条件に従う。CancelはSession終了であり、その後はPlayを再起動する。Shakuri/Fall/TensionFall/Hook/Retrieveはキューに届くが、M09では既存の未対応状態として拒否され、実動作しない。M10以降で対応する範囲を実装する。STAY専用キー、無入力タイマー、AutoStay表示は追加していない。D08の装備変更UI/一投単位ロックへの移行もM10に残す。

最終AutomationはM09 7件＋回帰13件成功、各試験のエラー・警告0件。U02はUEnhancedPlayerInputへ押下/解放を注入し、長押しと再Bindingでも1押下1コマンドを確認した。他にTick/Sequence順、30/60/120fps、古いCast/終了/破棄、Pause/フォーカス、読取非破壊性、不正起動、保存済み設定を検証した。NullRHI試験であり、PIEの目視、解像度/DPI、実キーボード/ゲームパッド、アプリ切替の実機確認は未実施。M09合格はC++/データ/自動試験の範囲を指す。

## 8. M10操作接続（2026-09-13）

第7節の「操作は未対応」という記録はM09当時の履歴。M10ではSpaceのShakuri、FのRe-Fall、TのTensionFall要求、R押下/解放の回収を実装した。Hookは引き続き未実装。キーは既存Prototype割当で、製品配置を確定していない。

確認ループは`Enter投入 → Spaceで任意回数シャクリ → TensionFallの過渡処理完了 → Stay → Fで再フォール`。回収はRを押して進め、離すと巻取りだけ停止する。Pause/フォーカス喪失では保持解除し、再開後に自動で巻かない。回収完了でResultと船上帰還を確定し、N（NextCast）でReadyへ進むと装備ロックを解除する。次のEnterで再ロックする。Result/Readyの切替をWidget独自の状態として管理しない。

M10の内部確認用装備変更はControllerのExecコマンド`TRSetEquipment <EgiId> <SinkerId>`で提供する。例: 次投Readyでコンソールから`TRSetEquipment Egi_4 Sinker_50`（40g＋50g）、または`TRSetEquipment Egi_3_5 Sinker_None`（35g＋0g）。同じSessionのTrySetEquipment/CanChangeEquipment条件を使い、一投中・回収中・船上未確認・Pauseでは拒否する。出力ログにAccepted/拒否理由enumを示す。製品の装備選択パネルや自動重量選択は作らない。

HUDへJerkCount/SeriesJerkCount/StayPenaltyJerkCount/PendingJerkCount、DepthVelocityMps（下向き正）、海底/海面接触、装備ロック、船上帰還、変更可能性を追加した。既存のworld Z速度（上向き正）と区別する。仮パネル高さを広げ、操作未実装の案内も更新した。RangeError/RangeStability/安定達成時間/BITE値は先行表示しない。RangeObservationSecondsは観測時間のSnapshot契約であり、安定スコアではない。

M10の入力・保持解除・装備変更試験とM09回帰は成功。UIのPIE目視、解像度/DPI、実キーボード/ゲームパッド/コンソール操作は未検証。Onboard/Ready条件やメッシュ変更はAutomationで検証している。製品UI・Resultパネルの完成はM15に残す。
