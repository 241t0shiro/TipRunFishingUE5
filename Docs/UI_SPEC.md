# UI・入力接続技術設計

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

D08の装備変更UIは釣り開始前だけ有効。開始後のReadyや次投Readyでは表示だけとし、EndFishingで準備へ戻ってから変更する。初期エギ3.5号35g、シンカー選択肢に「無し（0g）」を表示。ショップ購入処理は含めない。

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
| AutoStay残り秒、10回超BITE減衰倍率 | 開発専用、DataAsset調整結果を確認 | 製品表示範囲はAlpha前に再決定 |
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
| IA_TR_Stay | Bool / Started | Stay |
| IA_TR_Hook | Bool / Started | Hook |
| IA_TR_Retrieve | Bool / Started、Completed、Canceled | RetrieveStarted/Stopped |
| IA_TR_EndFishing | Bool / Started | EndFishing |
| IA_TR_Pause | Bool / Started | Gameのポーズ要求 |

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
| U11 | 開始前/次投Readyで装備UI操作 | 釣り開始前のみ変更可能、0g選択可能、UI以外のAPIも同じ制限 |
| U12 | 10/11/20回→AutoStay | 回数上限なし、適用回数と減衰倍率一致、0.8秒の残り表示と実遷移一致 |

MVPではFunctional Testと目視で十分なレイアウト確認を行い、Widgetの内部構造をそのまま写した大量の単体テストは不要。
