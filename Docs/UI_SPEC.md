# UI・入力接続技術設計

2026-09-18 R1実装・自動検証完了: 初期Navigationと明示Fishing/Navigation固定Command、Session所有Mode、ModeEpoch/拒否条件/Snapshot/入力Context接続を実装。UHT16生成ファイル・実C++・Development Editor Win64成功。R1 4件＋関連回帰32件成功、試験内エラー/警告0。今回PIEは未実施。Sideは未選択を許容する型のみ、操船/Camera/Sequenceは未実装。R2以降/H/M11未着手。APIはGAME_DESIGN末尾、証跡はROADMAP末尾、開発確認方法はUI_SPEC末尾を参照。以下のR設計のみ/G記録は履歴。

2026-09-17 M10.5-R設計改訂: A〜F基盤は保持。最新手動PIEでGはゲームプレイ品質不合格。Rは設計/実装分割のみ完了し、実装未着手。最新契約は本書末尾のM10.5-R節を優先。以前のG合否保留・固定Pulse・観測カメラ等は履歴。R自動検証とユーザー手動合格後もHへ自動進行しない。H/M11以降は保留。

2026-09-15 M10.5-E完了: 左保持の通常回収／解放後Stayと、固定TickのQuickRetrievingを分離。今回の明示依頼を優先し、通常完了はResult（ロック維持）→NextCastでReady/解除、Quick完了だけ直接Ready/解除。海面近傍のライン拘束・巻取りを修正。UHT生成・実C++・Development Editor Win64成功、E 7件＋回帰54件成功、各試験エラー/警告0。保存資産移行・PIEは未実施。F〜H・M11以降は未着手、全体品質ゲート未合格。下のA〜D記録は履歴。

2026-09-14 M10.5-D完了: Mouse Axis2D→固定Input Queue→RodControl、右クリック/Spaceの同一Jerk、基準姿勢＋時間プロファイル、RodTip→Cライン接続を実装。Rod有効時は旧Lift/Reelを重ねない。UHT生成・実C++・Development Editor Win64成功、D 5件＋必要回帰49件成功、各試験エラー/警告0。Rod/Input資産は明示設定、既存保存Prototype移行/実マウスPIEは未実施。E〜H・M11以降は未着手、M10.5全体品質ゲートは未合格。以下のA〜C/設計のみの記録は履歴。

2026-09-13 M10.5設計改訂（実装未着手）: M00〜M10の実装・自動試験成功は履歴として保持するが、ユーザーのM10後PIE評価は再現性・操作性の品質不合格。M11への進行はM10.5品質ゲート合格まで保留する。本書のM10.5改訂契約を旧記述より優先し、M03〜M10完了記録は旧実装の証跡として読む。今回はMarkdownのみ更新し、改訂機能の実装・ビルド・試験は行っていない。


2026-09-13 M10実装反映: Shakuri/TensionFall/Stay/Re-Fall/Retrieve、一投単位の装備ロックと船上帰還後の次投変更、深度速度・境界接触Snapshotを実装済み。旧AutoStay項目は型/検証/Prototype設定から撤去済み。M06〜M09記録は当時の履歴として保持し、現在の契約・検証範囲はROADMAPのM10完了記録を参照。M11以降は未着手。

関連: [全体正本](GAME_DESIGN.md)、[操作](FISHING_SYSTEM.md)、[BITE](SQUID_AI.md)。MVPのデバッグ表示と製品のプレイヤー向け情報を区別する。

更新: v0.2 / 2026-09-12。D10/D11とD12の未指定部分は保留（MVP暫定仕様あり）。D12の基本操作はM10.5で決定済み。MVPでは固定重量・仮Cue1種・内部情報HUDを使い、製品仕様はAlpha前に再決定。

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

## 4. M09/M10までの入力設計（M10.5で更新）

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

## 8. M10操作接続の履歴（2026-09-13、M10.5で更新予定）

第7節の「操作は未対応」という記録はM09当時の履歴。M10ではSpaceのShakuri、FのRe-Fall、TのTensionFall要求、R押下/解放の回収を実装した。Hookは引き続き未実装。キーは既存Prototype割当で、製品配置を確定していない。

確認ループは`Enter投入 → Spaceで任意回数シャクリ → TensionFallの過渡処理完了 → Stay → Fで再フォール`。回収はRを押して進め、離すと巻取りだけ停止する。Pause/フォーカス喪失では保持解除し、再開後に自動で巻かない。回収完了でResultと船上帰還を確定し、N（NextCast）でReadyへ進むと装備ロックを解除する。次のEnterで再ロックする。Result/Readyの切替をWidget独自の状態として管理しない。

M10の内部確認用装備変更はControllerのExecコマンド`TRSetEquipment <EgiId> <SinkerId>`で提供する。例: 次投Readyでコンソールから`TRSetEquipment Egi_4 Sinker_50`（40g＋50g）、または`TRSetEquipment Egi_3_5 Sinker_None`（35g＋0g）。同じSessionのTrySetEquipment/CanChangeEquipment条件を使い、一投中・回収中・船上未確認・Pauseでは拒否する。出力ログにAccepted/拒否理由enumを示す。製品の装備選択パネルや自動重量選択は作らない。

HUDへJerkCount/SeriesJerkCount/StayPenaltyJerkCount/PendingJerkCount、DepthVelocityMps（下向き正）、海底/海面接触、装備ロック、船上帰還、変更可能性を追加した。既存のworld Z速度（上向き正）と区別する。仮パネル高さを広げ、操作未実装の案内も更新した。RangeError/RangeStability/安定達成時間/BITE値は先行表示しない。RangeObservationSecondsは観測時間のSnapshot契約であり、安定スコアではない。

M10の入力・保持解除・装備変更試験とM09回帰は成功。UIのPIE目視、解像度/DPI、実キーボード/ゲームパッド/コンソール操作は未検証。Onboard/Ready条件やメッシュ変更はAutomationで検証している。製品UI・Resultパネルの完成はM15に残す。

## 9. M10.5マウス操作・日本語Prototype UI（D/E/F実装・自動検証完了）

D12改訂: マウス中心を製品の基本方針として採用。数値感度/可動域/演出/最終レイアウトは調整事項。既存キーボードはバックアップとして残せるが、UIの主案内を競合させない。

| 主操作 | 意味 | Enhanced Input/固定更新契約 | バックアップ |
|---|---|---|---|
| マウス移動 | 竿Pitch/Yaw | Axis2Dの移動量→RodAimコマンド→RodControl | パッドは将来レート入力を同じ意味へ変換 |
| 右クリック押下 | Shakuri1回 | Startedのみ。保持Triggeredで連打しない | Space |
| 左クリック保持/解放 | 通常巻取り/停止 | StartedとCompleted/Canceled。停止でStay相当へ復帰 | R |
| F | Re-Fall | Started、同Cast | 既存F |
| Q | Quick Retrieve | Started、一度受理後は途中停止不可 | 製品以外の別割当は任意追加しない |
| Enter | Deploy | Ready/船上でのみ受理 | 既存Enter |
| P | Pause/Resume | 時計停止、保持解除 | 既存P |
| Readyの装備パネル | エギ/シンカー選択・適用 | 公開APIの許可条件を再検査 | Consoleは任意診断のみ |

H/Hookは未実装と示し主ガイドから外す。Tは既存のPrototype診断用TensionFall要求として残す場合だけ補助欄へ表示する。2026-09-15のE依頼に従い、通常回収のResult後はN（NextCast）でReadyへ進み装備解除する。Quick完了は直接Ready/解除でN不要。双方でN不要とする旧案は撤回。BackspaceのSession終了は補助欄で「中断・再開にはPIE再起動」と明示し、Q回収と混同させない。

### Rod Mouse Controlと入力寿命

予定UTRRodControlComponentをSession所有のFishing責務として追加し、Boatの取付Transformから竿先位置/回転を作る。DataAsset案はUTRRodTuningDataAsset（SessionConfig参照）。RodYaw/Pitchのmin/max、マウス感度[rad/入力単位]、竿長m、取付offset、あおり振幅/時間/復帰応答を分離する。Yaw/Pitchは数値正本、合成姿勢は可動範囲へclamp。可動範囲min<max、正の竿長、有限値、通常姿勢とあおり後の安全な竿先高さを検証する。製品角度は未確定。

PlayerControllerはマウスdeltaへ描画dtを二重に掛けない。受信順に次の未処理Tick/Sequence/CastId/登録世代を付け、同Tickでクリックより前後どちらの竿姿勢を使うかをこの順序で確定する。単純な1フレーム合計でクリック前後を混ぜない。パッドの角速度入力は固定dtを掛ける別アダプタとし、マウスdeltaと同じ単位だと扱わない。固定Tick入力列の再生が決定性試験の対象であり、人間の異なるfps操作が自動的に同一Tickになるとは保証しない。

Readyでも現在の登録世代/最終CastId（初投前0）でRod入力を照合する。Deploy境界で旧世代/旧Castの未処理竿入力を破棄し、受理済み基準姿勢だけを次投初期姿勢へ引き継ぐ。Cast終了・対象破棄・Pause・フォーカス喪失・UIContext切替でdelta/保持/予約入力を解除する。復帰時に溜まったマウス移動を一括適用しない。マウス/バックアップキーは同じ意味の保持状態へ集約し、二重巻取り速度にならない。

### 装備変更導線

Ready時はカーソルを表示し、日本語の簡易装備パネルを表示する。技術案はエギ選択（3号30g/3.5号35g/4号40g）、シンカー選択（無し0g/5/10/15/20/25/30/40/50g、既存Tableの9選択を正本として列挙）、総重量、適用ボタン。実際の選択肢はM02 Tableから生成し、UI側に重複した商品定義を持たない。存在しないID/メッシュ参照は選択成功にしない。

UI操作中は釣りマウス入力を消費し、ボタン左クリックで巻取り、右クリックでシャクリを発行しない。ReadyはUI操作を優先、投入後はゲーム側へマウスを捕捉、帰還Readyでカーソルとパネルを戻す。Enterはパネルの編集中/フォーカス状態と競合させず、適用済み装備を表示してから投入要求を送る。Widgetは状態を確定せずSessionの応答で選択表示を更新する。

Cast中/回収中/船上未確認/Pauseでは変更欄を無効化し、日本語で理由を表示する。Readyになった古い画面からの要求もAPIで再検査。Apply/Deployの競合は同一のSession境界で拒否し、投中装備を変更しない。前投の終端装備と観測値は次投候補と区別し、次Castのメッシュ/係数まで反映する。Console Commandは任意診断に残し、無言失敗を改善する計画だが、UI受入にコマンド操作を要求しない。SHOP/購入/保存は対象外。

### 日本語HUDと見える因果関係

常時ガイド: 「マウス移動：竿操作」「右クリック：シャクリ」「左長押し：巻き上げ」「F：再フォール」「Q：クイック回収」「Enter：投入」「装備変更：回収後の準備パネル」。状態に応じて使用不可の操作は淡色＋理由を示す。Quick中は残り進捗と「途中停止不可」、Pauseは「一時停止」を表示する。

| 日本語ラベル | 表示値/注意 |
|---|---|
| 投ID（CastId）/釣り状態 | Ready=次投準備、FreeFall=フリーフォール、BottomContact=着底、Jerking=シャクリ、TensionFall=テンションフォール、Stay=ステイ、Retrieving=通常回収、QuickRetrieving=クイック回収 |
| エギ深度/水深 | m、深度は下向き正、未投入/回収後は「—（船上）」 |
| 深度変化速度 | m/s、下向き正、上昇/下降の文字を添える。安定スコアとは呼ばない |
| 船との水平距離/水平方向 | mと相対方向。竿先との距離は詳細欄で区別 |
| ライン長/ライン角度/余長 | m/度/m。角は鉛直下向き基準、弛み時は端点間の幾何角 |
| 張力（代理値） | 0〜1。実張力N/Fightと混同させない |
| 船ドリフト | 世界方向＋速度m/s、knot併記可 |
| 風向・風速 | 「流れる向き」基準、m/s。無風は方向なし |
| 表層潮流/エギ深度の潮流 | それぞれ方向とm/s＋knot。単にCurrent一行にまとめない |
| エギ重量/シンカー重量/総重量 | g、シンカー無し0g、ロック/変更可否 |

ラベルと値を別Widget/表示要素にし、異なる色と配置で区別する。色だけに依存せず単位・符号・状態名を表示。日本語対応フォント、背景とのコントラスト、1280×720/1920×1080と拡大表示で欠け・重なりを確認する。数値は既存10Hz案、状態/ロック/エラーは即時反映。Quick中の水中値は「回収演出中」として凍結値と区別し、RangeError/RangeStability/BITEの仮値を表示しない。

保存済み検証Level `/Game/TipRun/Prototype/M105/L_TR_M105_Prototype` はGで作成。専用GameMode/SessionConfigを接続し、EditorStartupMapへ設定した。PIEの入力・表示確認状況は下のG記録とROADMAPを参照。コンソールURLやWorld Settingsの手修正を通常の起動手順にしない。

### Dで実装した入力・読取口（2026-09-14）

- ETRPlayerAction/ETRFishingCommandTypeへRodAim、FTRFishingCommandへAxis2Dを追加。InputConfig検証は固定Boolean9件という個数判定から、必須意味コマンドの存在・重複なし・Boolean/Axis2Dの型対応へ変更した。RodAim未設定の旧入力資産は互換読込する。
- CreateMouseRodPrototypeは既存Prototypeのキーボード/パッド割当てを残し、JerkへRightMouseButton、RodAimへMouse2D（Axis2D）を追加する明示opt-inの一時設定生成口。左マウス/Quickは追加しない。JerkはStartedのみ、Completed/Canceledで再受付可能にする。RodAimはTriggeredのdeltaを送る。Started/保持の重複はControllerの既存Pressed/BlockedUntilReleaseで抑止する。
- Controller::SubmitMouseDelta→Session::SubmitRodAim→Coordinator::EnqueueCommandを使用。登録IDはキュー行先とSession受理時に検査、CastIdは送信時と固定配送時の両方で照合する。描画dtの二重乗算やControllerからの角度直接書換えはない。Sequence順は保持し、Deployより後の旧Castのdeltaは破棄する。
- ControllerのPause/Focus/Flush/Unbindで保持と入力キューを解放。復帰時の保持Jerkは解放確認まで再受付せず、Dの未実行Shakuri予約も固定更新境界で取消す。既に開始した動作はPause中に進めず復帰後に継続する。実マウスのOS/PIE操作感は未検証。
- FTRHUDSnapshot.Rodで基準/最終角、開始時基準、竿先位置/回転/方向、活動フラグ・相・Tick/CastIdを読む。Widgetの日本語化・値配置・新UIは変更なし。Boatの取付基準RodTipと可動Rod.TipWorldPositionMを混同しない。
- 利用時はRodTuning DataAssetとMouse Rod用InputConfigを明示作成し、SessionConfig.Rod/Inputへ割り当て、FishingはC revision 2にする。GameModeはRod参照をSessionへ渡す。今回Contentを保存変更しておらず、既存Levelを開くだけで新操作へ移行した状態ではない。保存資産移行と新LevelはG、操作感/品質再評価はHに残す。数値とプロファイル契約はFISHING_SYSTEM第11節、結果はROADMAPのD記録を参照する。

### Eで実装した回収入力・HUD接続口（2026-09-15）

- `UTRInputConfigDataAsset::CreateMouseRetrievePrototype`はDの設定を継承し、既存Retrieve ActionへLeftMouseButton（R/パッドと共有）、別のQuickRetrieve ActionへQを割り当てる。通常はStarted/Completed/Canceled、QuickはStartedのみを処理する。保存済み入力資産は自動変更しない。
- 新設定の利用にはSessionConfig.Inputへの明示割当てと、FishingTuningの正のQuickRetrieveDurationSが必要。0は旧資産互換の「Quick未設定」であり、時間を勝手に補完しない。保存資産への適用はGに残す。
- 通常回収完了はResult中の装備ロックを維持し、NでReady/解除。Quick完了は直接Ready/解除でN不要。画面側は終端通知だけから常にResultへ遷移せず、SessionPhaseと結果のbQuickRetrievedを読むこと。パネル本体の実装はF以降。
- `FTRHUDSnapshot.Retrieval`から通常/Quickフラグ、要求/実巻取り速度、残ライン、進捗、物理更新有効性を取得できる。既存bEquipmentLocked/bCanChangeEquipmentも利用する。Quick中のEgiは凍結した最終物理Snapshotであり、最新の水中運動として表示しない。
- 状態・寿命・数値の契約は[FISHING_SYSTEM第12節](FISHING_SYSTEM.md#12-m105-e-normal--quick-retrieve実装契約2026-09-15)、検証結果は[ROADMAPのE完了記録](ROADMAP.md#m105-e完了記録2026-09-15)を正本とする。実マウスPIE、日本語HUD、装備パネル、新Levelは未実施。

### FのPrototype UI実装（2026-09-16、実装・自動検証合格）

上のE記録までの「日本語HUD・装備パネル未実装」は履歴。本節がFの現行接続を示す。Gの保存設定移行・新Level、Hの最終PIE評価は含めない。

- 既存UTRFishingHUDWidgetをC++ UMGの日本語観測パネルへ改訂。ラベルと値は別TextBlockで2列配置し、LabelColor/ValueColor/DisabledColorをWidgetのPrototype表示設定として分離。1120×640の基準枠をScaleBoxで画面へ収め、観測値と装備欄にScrollBoxを使用する。基本操作ガイドは装備欄のスクロール外へ常設。Engine標準複合フォントの日本語fallbackを利用する。これは製品レイアウト・最終フォントを確定するものではない。
- 表示はCastId/日本語状態、深度/水深/下向き正の深度速度、船/竿先からの水平距離、ライン長/鉛直基準角/弛み、張力Proxy、船速、風、表層潮、エギ深度潮、エギ/シンカー/総重量、ロック/変更可否、通常回収速度、Quick状態/進捗、シャクリ回数/予約数。船・風・表層潮はBoat Snapshotの船地点サンプル、エギ潮はEgi Snapshotを使用。方向は流れる向き、世界+X=0°/+Y=90°（地理的北とは呼ばない）、m/sとAの換算によるknotを表示する。
- 船上/無効なエギ値を深度0で代用しない。Quick中の水中値には「回収中・凍結値」を明示する。RangeError/RangeStability/BITE等の仮値を追加しない。通常数値更新は既存10Hz、状態/投/ロック/変更可否/Pauseは次描画で反映する。
- 操作ガイドはMouse/右押下/左保持/左解放/F/Q/Enter/N/Tab/P。可否はSessionのAvailableCommands、装備可否は既存CanChangeEquipmentと共有する理由判定から取得する。UIはFishing遷移条件を持たず、UI入力捕捉中だけ釣り操作の表示を無効にする。Controllerは実際のInputConfigで主キーが未割当なら灰色表示と説明を加える。保存済み旧入力設定を自動移行しない。
- Prototypeの追加キーはTab（パネル開閉）。初投Ready、Quick帰還Ready、通常Resultでは自動表示し、投入後は閉じる。投中もTabで開いて変更不可理由を確認できる。ResultはN/次投ボタンからReadyへ、Quick後はN不要。Readyで閉じた場合もTabで再表示できる。Pはパネル中も停止/再開できる。
- Sessionの既存Equipment Tableから行をコピーし、重量/ID順に選択肢を生成。UIに3種/9種の製品定義を持たない。シンカー0gは「なし」。現在使用中（投中/Resultはロック中）の装備、選択候補、次投の適用済み装備を区別する。適用ボタンはController→Session::TrySetEquipmentを使用し、重量/係数/メッシュの解決・ロック条件は既存APIが正本。無効ID/参照は成功表示しない。
- 未適用の候補がある状態でEnter/投入ボタンを押すと「選択を適用してから投入してください」と表示する。コンボを編集中のEnterは選択の確定だけに使い、同じ押下で投入しない。投入/NextCastは既存固定キューを使用し、UIを閉じて保持/キューを解除した後に要求を登録する。装備適用は既存の固定更新外の設定APIであり、Widgetから数値状態を書き換えない。
- ControllerはGameAndUIとGameOnlyを切替え、パネル中は釣り入力アダプタ自体も遮断する。未処理のマウス移動/保持を解除し、再接続は押下キーを解放まで無視する。Slateが先に消費した押下も考慮する。アプリフォーカスとUI捕捉を別管理し、Pause/Focus LostのE契約を維持する。
- WidgetはController/Sessionを弱参照し、表示時CastId/登録世代を要求時に再検査。終了・破棄後は候補と表示を無効化し、別Sessionへ古い画面の要求を送らない。失敗時は投中・Result・Pause・船外等の日本語理由を表示する。
- F全6件とD/E/M09/M10の回帰27件は成功。プレイヤーコンテキストのないWorldで初期化通知が省略されても、RebuildWidgetからレイアウトを生成する。実UMGの日本語ラベル/値/色、Enhanced入力遮断、Native Enter/Tab/P、装備27組合せ、旧Cast/Session寿命を検証した。保存Prototype移行、実マウスPIE、1280×720/1920×1080・100/150%の視認性/日本語欠字の目視確認は未実施。ビルド/試験証跡と範囲はROADMAPのF記録を参照する。

### G 保存Prototypeと目視確認手順（2026-09-16、手動確認待ち）

- Level: `/Game/TipRun/Prototype/M105/L_TR_M105_Prototype`
- World SettingsのGameMode Override: `/Game/TipRun/Prototype/M105/BP_TR_M105GameMode_Prototype`（親`ATRGameModeBase`）。Controller/HUDは既存`ATRPlayerController`/`ATRHUD`。
- Session: `/Game/TipRun/Prototype/M105/Data/DA_TR_M105Session_Prototype`。同フォルダのOcean/Boat/Fishing/Rod/Inputへ明示参照する。旧M09資産は維持。
- EditorStartupMapへ上記Levelを設定。プロジェクトを開き、Level名を確認してPlay（Alt+P）。別Levelを開いている場合だけContent Browserから上記Levelを開く。GameDefaultMapは従来設定を維持し、Shipping用マップの確定にはしない。
- 観測Actor`ATRPrototypeViewActor`は永続StaticMesh Componentで船（白）、竿（橙）、竿先（黄橙）、ライン（黄）、エギ（赤）を描く。海面は水色の開放枠、海底は海面下30mの簡易床、背景は青灰色。表示専用Unlit材質を保存参照し、照明なしでも識別可能にする。Collisionは無効、Simulation状態は変更しない。編集画面はPIEカメラ/材質初期化前であり、確認はPlay後に行う。
- Prototype HUDは右上の幅最大420・通常高560 Slate単位へ縮小（DPI適用、幅は画面30%以下）。主要11項目と基本操作7項目だけ常時表示。F1で詳細数値/詳細操作を開閉する。装備または詳細表示中は最大高900、画面高を超える場合はScroll。1920×1080/2560×1440の実際の視認性は手動再確認待ち。
- 装備パネルは初期Closed。Ready/Result/Quick完了でも自動で開かない。TabでOpen、再Tab/閉じるボタンでClosed。Tableから選択→適用→Enterまたは投入ボタン。閉じている時のEnterは既存固定Input Queueで投入する。投中は装備変更不可。TabはControllerのUIキーでありFishingのInputActionではない。
- Mouse移動→竿、右クリック→1シャクリ、左保持→通常回収、解放→Stay、F→再フォール、Q→固定時間のQuick回収。Quick後はReady/ロック解除→装備変更→再投入。Nは通常回収のResultから次投へ進むDebug操作。
- 手動スモークでは各入力到達、パネル中の釣りマウス遮断、閉じた後の竿操作復帰/保持残留なしを確認する。画面の釣り状態・竿値・ライン長・装備ロックを併せて見る。操作感や重量バランスの最終品質判定はHで扱う。
- 2026-09-16のユーザー手動結果: 釣り操作/装備/Quick/日本語HUDは正常、3D黒画面・HUD過大・パネル自動表示・revision不明でGの表示品質は不合格。下記修正後の目視再試験は未実施。自動PIEで強制終了するとの申告により、以後PIEはユーザーが手動実施する。

### G可視化/UI修正後の再試験

- カメラは船位置から(-8,-12,+7)m、注視点は船+(1,0,0)m、Perspective/FOV60°。保存Levelの観測Actorで調整可能なPrototype表示値。船/竿先/ライン上部を優先し、全30mを常時収めるカメラではない。GameModeの接続後も観測ViewTargetを確実に選ぶ。
- Rodは公開TipとMountの間、Lineは公開TipとEgi WorldPositionの間、Egi表示はWorldPosition×100cmへ追従。太さ/球サイズは識別用の拡大表示で物理形状ではない。初投前のRodは設定された初期姿勢を船上表示するだけで、Rod入力/物理を進めない。Quick中は既存の凍結Snapshot表示、終了後は水中エギ表示を消す。
- 小さなModel Rev欄は`Env 2 / Boat 2 / Egi 2`を表示。Environment/Boatは実Snapshot、Egiは投中の実Snapshot、船上は装備Snapshotの投入設定。未取得は未取得と表示し、2を捏造しない。Rev1等の不一致は赤い「警告：Rev2不一致！」。これはロード/接続確認でありH品質合格マークではない。

| 順序 | 手動操作 | 正常の判定 |
|---|---|---|
| 1 | プロジェクトを開き`L_TR_M105_Prototype`を確認。PlayメニューからNew Editor Window (PIE)を選び、Advanced Settingsで1920×1080を指定してPlay | 青灰色背景、白い船、橙の竿、水色海面枠。HUDは右上の小領域。装備パネルClosed。Rev表示は2/2/2（初投前Egiは投入設定） |
| 2 | Tab→Tab。再度Tab→装備選択/適用→閉じる | Open/Closeが切替。開いている間の左右マウスで釣り動作なし。閉じた後の巻取り保持残留なし |
| 3 | Enterで投入し、沈み切る前にマウスを左右/上下へゆっくり動かす | 橙の竿と黄橙の竿先が動く。黄色のラインが竿先から赤いエギへつながる。投入後もRev2/2/2 |
| 4 | 右クリックを1回。必要ならQ回収→再投入して浅い位置で観察 | 竿先の短い上げ/戻しが目視でき、ライン方向とエギ表示が追従。数値のJerk回数はF1詳細で確認。空間位置の一致はAutomationでも確認済み |
| 5 | 落下を観察。左保持/解放、F、Qも確認 | 赤いエギがライン端点と一致し、水平距離が変化。深いエギが画面外ならライン方向と深度/水平距離で追う。水深表示30m。Quick後Ready/解除だがパネルはClosed、Tabで変更できる |
| 6 | F1を2回、必要ならTabを開いて詳細をScroll | 詳細/詳細操作が開閉。通常表示は主要11項目・操作7項目のみ。主要ラベル/値の重なり・欠けがない |

3Dが依然黒い、竿が見えない、操作に追従しない、主要ガイドが画面外になる場合はG未合格。手動結果が揃うまでHへ進まない。2560×1440も可能なら同じ配置を確認する。全物理統合回帰・最終操作感判定はHに残す。

### G追加修正の現行操作・手動再試験（2026-09-17）

前回の手動試験で3D背景/海面/海底、各表示、Mouse Rod、Tab、F1、Rev2、FreeFall、Normal/Quick Retrieveは正常と確認された。今回はそれらを維持した追加修正。以下を旧G手順より優先する。PIE自動実行は行わない。

- 通常HUD: 右上320×285 Slate単位を上限に、状態、深度/水深、船からの水平距離、ライン長/角度、船ドリフト、エギ地点の潮、総重量、ロックの8行。ラベル/値は11ptの別色。F1に風・各潮・revision・詳細数値/有効操作を移す。Rev不一致警告だけは通常時も表示。常時ガイドは3行、詳細/装備時は最大420×900でScroll可能。
- Shift（左右どちらも）+Mouse: 船の周りを回るPrototype観測カメラ。Home: 初期視点へ戻す。Shift中のAxis2Dはカメラだけに渡し、RodAimキューへ送らない。Shiftを離すと竿操作へ復帰する。Tabパネル中/Focus Lost/Pause中はCamera Lookも遮断。製品キー配置ではない。
- 初期カメラ/船追従は維持。海面の5m間隔グリッド（±50m）、原点近くのピンクのブイ、海面枠、海底を世界座標へ固定。船の移動と一緒に基準物を動かさない。船体に青い船首と暗い船室、橙の竿、黄のTip/Line、赤い細長いエギを表示する。全てCollisionなしの表示専用で数値へ逆流しない。
- 保存Rod Assetは`/Game/TipRun/Prototype/M105/Data/DA_TR_M105Rod_Prototype`。Pitchは-0.35〜1.4rad（約-20〜80°）、Yawは±1.2rad（約±69°）、最大姿勢変化速度は2rad/s。感度は既存X=.01/Y=.02radを維持。Shakuri振幅.3rad、Up .15秒/Return .25秒、Return中PulseはFISHING_SYSTEM第13節。上限付近はあおり振幅がClampされる。全てPrototype用で調整可能。

|順序|操作|確認|
|---|---|---|
|1|`L_TR_M105_Prototype`を開き1920×1080の手動PIE|通常8行/短いガイド、Tab初期Closed。F1でRev2/2/2と詳細を確認し閉じる|
|2|Enter、Mouse上下左右、Shift+Mouse、Home|竿の可動域拡大。Shift中は視点だけ動き竿の基準値不変、解放後は竿操作に復帰|
|3|5mグリッド/ピンクのブイと船を比較|船が固定基準物に対して流れる。青い船首方向と移動方向を区別できる|
|4|十分に沈めて、右を2/3/5回押下（毎回解放）|各回で竿があおられ戻る。ラインが短くなり、各回でエギへ作用。F1の回数/予約・弛み/張力を併用。水面到達後は上昇不能なので深い位置で評価する|
|5|右を保持、左保持→右押下→左解放、再び左保持/解放|右保持で連打なし。右で複合Actionへ切替→Stay、通常回収の再開は左再押下。巻取りが二重加算されない|
|6|浅場で右、F、Q、Tab開閉も再確認|異常終了/ワープなし。Q後Ready/Unlock、パネルはTabで操作、閉じた後の保持残留なし|

今回の視認性・操作感の合否はこの手動再試験待ち。H最終統合・M11には進まない。

## M10.5-R モード別操作・カメラ・表示設計（2026-09-17、未実装）

最新手動PIEでGはゲームプレイ品質不合格。前節のShift観測/固定Pulse/F1正常は旧履歴。本節はRの提案設計で、現在の保存Mapへ適用済みではない。

### カメラと入力

新設予定`ATRPlayerCameraManager`がNavigation/Fishing/開発DebugのViewTargetを一元管理する。`ATRPrototypeViewActor`はSnapshot表示専用へ縮小し、毎TickのViewTarget奪取を廃止する。カメラの補間は表示だけで、Boat/Rod/Egiを動かさない。

|Context|Mouse|主操作（Prototype案）|視点|
|---|---|---|---|
|Navigation|三人称のYaw/Pitch Look|W/S推進・減速/後進、A/D操舵、E釣り開始パネル|船の後方〜斜め後方。Mouse方位へ船首を勝手に合わせない|
|Fishing|制限付きFishingAimを操作|右Down=1Jerk、左Hold=通常回収、Release=停止、F再Fall、Q Quick、Enter投入、Tab装備|選択舷の目線からRod/Reel/海面を見る一人称|
|UI|カーソル操作|左右舷/開始・装備の選択、明示Close|釣り/操船入力遮断|
|Debug|開発用の独立Context|F1詳細。観測カメラは明示Debug切替のみ|通常プレイにShift操作を要求しない|

EによるFishing退出はReady/船上のときだけNavigationへ戻す案とする。Cast中は「回収してから操船へ戻れます」、ResultはNextCast操作を案内する。左右舷パネルには船首/風/表層潮/ドリフトの矢印と選択位置を示す。風向は「流れていく向き」等の表記を統一し、気象の吹いて来る方位と混同しない。操船キー、感度、FOV等は製品配置として確定せず設定参照とする。

Fishing Mouseは共通の有界Aim入力として固定キューへ渡し、BaseRodPoseを更新する。カメラはその基準Aimを読み、目線用の小さい追従係数/制限でRodと海面を画面内に保つ方式を初期案とする。Camera LookとRodの独立Yaw積分を同じMouseへ重ねない。Shakuriの一時Offsetは竿だけに適用し、毎回カメラを急激に跳ね上げない。固定更新間の表示補間を許可するが正本へ書き戻さない。

FishingのYawは選択舷の外向き、Pitchは海面/竿の周辺に制限し真上/真下を避ける。Eye/MountはBOAT_SYSTEMの釣り座から計算。首振り幅、目線追従、BaseRod可動域とSnap余裕は別DataAsset項目。Navigationの周囲確認とFishingの手元操作を同じ無制限Orbitにしない。身体/手の本格アニメーションは要求しないが、竿元とリールが船上の目線に対して自然な位置に見えることを受入にする。

Mode/UI/Focus/Pause切替では保持、Mouse蓄積、操船意図、古い予約入力を解除する。モード変更は固定更新のModeEpochで検査。復帰時は新しいDownが必要で、左保持や右Repeatを再生成しない。UIを閉じたクリックが釣りへ漏れない。通常巻取りとSequenceの切替詳細はFISHING_SYSTEM第14節。

### HUDとF1不具合

通常HUDはモード、釣り状態、深度/水深、水平距離、ライン長/角度、船ドリフト、エギ潮、重量/ロックを小領域へ配置。Navigationでは不要な投中値を隠し、船首/速度/水深と釣り開始案内を優先する。詳細環境/revision/Sequence/Slack/入力診断はF1へ分離し、当該モードの短い操作ガイドだけ常設する。1920×1080で竿/海面を妨げず、2560×1440でも文字/パネルが欠けないこと。装備パネル初期Closed、Tabで開閉、既存TableとSession経由変更を維持する。

ユーザー報告の「F1で紫色のDebug/モデル表示、閉じられない」は未修正の正式不具合。ローカルUE5.8の`Engine/Config/BaseInput.ini`にはF1→`viewmode wireframe`のDebugExecBindingがあり、独自HUD F1との競合が有力候補。紫色の全原因と閉じられない原因は未確定で、入力/Focus経路も調べる。

R8aではプロジェクト側でこの継承F1割当を限定的に除外し、Engineファイルを編集しない。Controller/WidgetのF1を共通の表示要求へ統合し、1Down1Toggle、Repeat無視、二重配送防止を確認する。詳細にCloseボタンも設ける。F1はWidget可視性だけを変え、ViewMode/ShowFlags/Material/Cameraを変更しない。通常/装備UI/Pause各状態で20回開閉し、終了時に元へ戻ることを試験。自動入力試験と手動実キーの両方が必要で、直接Toggle関数を呼ぶ試験だけで合格にしない。

### 最小視認品質

|対象|Rの受入|
|---|---|
|Boat|塗りのあるシルエット、船首・船尾・左舷・右舷を識別。左右舷選択と実位置一致|
|Rod / Reel|目線に対して自然な竿元/リール位置。Up/Returnが肉眼で分かる|
|Line / Egi|RodTip→Egiの実Snapshotに追従。海面と異なる色/明暗、水平移動が分かる|
|Water / Underwater / Seabed|水面境界と水中の奥行きが判別可能。簡易透明/断面表示等で試験条件のエギを観測可能。30m海底と水面を混同しない|
|固定基準|世界に固定されたブイ/目盛/グリッド。船追従カメラでもドリフトを認識|

Engine標準形状/簡易材質でよいが全面Wireframeだけでは評価しない。深いエギを通常一人称で常時画面内へ強制せず、ライン方向/HUDで追えることと、開発観測で空間整合を確認できることを分ける。表示拡大や透明化は検査用と明記し、物理/海問い合わせへ逆流させない。市販Asset、本格海面/船体制作、製品Cameraは対象外。

### R実装後の手動PIEゲート（今回は未実施）

1. 保存R Prototypeを1920×1080で開き、Navigationの船体/水面/固定基準と小型HUDを確認。
2. Mouseで三人称周囲確認、推進/操舵で移動/船首決定。Mouseだけで船首が変わらないこと。
3. 開始パネルでPort/Starboardをそれぞれ選び、船上位置/竿元/リール/目線が自然な一人称へ変わること。
4. 推進停止後の慣性→横流しを観察。船首と移動方向が分離し、10/30/60秒の移動記録と体感を評価。
5. 投入/着底後、右を1/2/3/5回、間隔を空ける場合と連続予約する場合で試す。全回の作用、戻し/余長回収、過剰回収なし、TF→Stayを確認。
6. 左巻取り/解放、右との切替、F、Q、装備変更/再投入。投中の操船/舷変更を拒否し、帰還後に戻れること。
7. F1の実キーで詳細を繰り返し開閉。紫色/RenderMode変化なし。UI/Focus/Pause後に保持残留/勝手な操作なし。
8. 2560×1440でもHUD/パネルを確認。通常操作にShift観測は不要で、Boat/Rod/Reel/Line/Egi/水面/海底を評価できること。

PIEはユーザー手動で行う。自動PIEを再起動しない。失敗項目をRへ戻し、全項目合格の明示結果と次の依頼を得るまでHへ進めない。

### R1期間の接続方法（2026-09-18）

Session起動時はNavigation。`ATRPlayerController::RequestPlayerMode()`がSessionへ要求を送る。後続のモード選択UIは未実装で、R1の開発確認用にConsoleの`TRSetFishingMode true`（Fishing要求）/`TRSetFishingMode false`（Navigation要求）を用意した。これは完成Prototypeの必須操作として採用するものではなく、R2〜R4の操作接続までのAPI確認手段。Fishing移行確定後に既存Enter/Mouse/回収操作が有効になる。Cast中やResultからのNavigation要求は拒否される。

`GetDebugSnapshot().PlayerMode`でMode/Side未選択/可否/拒否理由を取得できる。既存HUDのレイアウト、Camera、保存Input資産、F1は今回変更していない。したがって従来の「起動直後Enterで投入」はR1ではFishingへの明示移行後に読み替える。手動PIEは今回実施していない。
