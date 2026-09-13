# TipRunFishingUE5 — Codex実装規約


2026-09-13 M10実装反映: Shakuri/TensionFall/Stay/Re-Fall/Retrieve、一投単位の装備ロックと船上帰還後の次投変更、深度速度・境界接触Snapshotを実装済み。旧AutoStay項目は型/検証/Prototype設定から撤去済み。M06〜M09記録は当時の履歴として保持し、現在の契約・検証範囲はROADMAPのM10完了記録を参照。M11以降は未着手。

このファイルは対象リポジトリのルートへ配置する。対象はUnreal Engine 5.8.2 / C++ / Windows・Steam向け「TipRun Fishing」。実装状況はDocs/ROADMAP.mdを参照し、ユーザーから依頼されたタスク範囲だけ実装する。

設計版0.2 / 更新2026-09-12。ユーザーのMVP決定とD03の同日補足を反映。

## 1. 最初に読む

1. [Docs/GAME_DESIGN.md](Docs/GAME_DESIGN.md): 全体正本、確定要件、要決定事項D01〜D18。
2. 作業に対応する [釣り](Docs/FISHING_SYSTEM.md)、[AI](Docs/SQUID_AI.md)、[船](Docs/BOAT_SYSTEM.md)、[海](Docs/OCEAN_SYSTEM.md)、[UI](Docs/UI_SPEC.md)。
3. [Docs/ROADMAP.md](Docs/ROADMAP.md): 小タスクと受入基準。

上位のシステム/開発者/ユーザー指示を優先する。このファイルは不要な承認要求や作業停止を増やすための規約ではない。既に許可された範囲の通常の修正・読取・テストは自律的に進める。

## 2. 仕様の扱い

- ユーザーの確定要件を優先し、勝手にショップ・オンライン・未承認の自動操作を追加しない。FreeFall自動繰出しは承認済み。AutoStayDelayと無入力時間によるSTAY移行は廃止。
- D01〜D09/D13/D15はMVP決定。D10〜D12は使用承認済みのMVP暫定仕様で、製品仕様のみAlpha前に再決定。D14/D16〜D18はMVP対象外・非ブロック。これらを理由なく再承認待ちへ戻さない。
- 未決ゲーム仕様を独断で確定しない。D番号を提示し、必要な箇所だけ確認する。依存しない型・計算器・試験は継続できる。
- 文書内の「暫定案」「テスト用値」を製品の既定値へ無断昇格させない。テスト資産名にはPrototype/Testを付ける。
- 技術実装の局所選択はこの設計内で判断し、細部ごとに確認を求めない。公的仕様・設計と食い違う変更は文書も更新する。
- 1つのRoadmapタスク、または同等に小さく独立検証できる範囲で実装する。未使用の将来機能を足場だけ先に大量追加しない。

## 3. アーキテクチャ

- Runtimeモジュールはまず1つ、内部はGame / Data / Fishing / Squid / Boat / Ocean / UIで分離。
- 接頭辞は `ATR...`、`UTR...`、`FTR...`、`ETR...`。ファイル例 `TRFishingComponent.h`。UEマクロ/API形式は実際の5.8.2ヘッダで確認する。
- Fishing操作、EgiSimulation、Hook判定、Fight、SquidBehaviorを別の責務として保つ。Coordinatorに式やAIを移さない。
- Ocean→Data、Boat→Oceanの値契約、Fishing/Squid→スナップショット、Game→接続、UI→読取/コマンドの依存方向を守る。
- 主要ロジックをLevel Blueprint、Widget、巨大なGameMode、Actor Tickへ集約しない。
- Blueprintはレイアウト、アセット、表示演出、入力接続補助。成否・確率・時刻・釣果はC++の正本が決める。

## 4. C++・データ・寿命

- UCLASS/UPROPERTY等はUE規約通り。所有参照はUPROPERTY/TObjectPtr、非所有ActorはTWeakObjectPtr。生ポインタを長期保存しない。
- ComponentはコンストラクタでCreateDefaultSubobject。初期化中に未生成のworld/Actorを参照しない。
- delegate登録と解除を対にする。EndPlayで予約・入力・登録・参照を解除。破棄通知も重複して安全に扱う。
- DataAsset/DataTableに調整値を分離し、IsDataValidで参照、範囲、有限数、重複IDを検証する。
- 一投中の設定はスナップショット。装備変更は次投用候補を更新し、次のDeploy受理時に凍結する。過去の投・釣果の装備Snapshotを変更しない。不意のEditor編集でバランスを変えない。
- RuntimeにEditor専用依存を混ぜない。Core/CoreUObject/Engine、EnhancedInput、UMG等は必要なコードを導入するタスクでBuild.csに追加し、公開/非公開依存を確認する。
- アセットパスをロジックに散在させず設定参照を使う。Tickで同期ロード・Actor全件探索をしない。
- `.uasset`をテキストとして生成/編集しない。利用できるUE Editor/検証済みアセット作成手段を使い、作成できなければ必要なEditor手順と未実施項目を明記する。

## 5. 数値・固定更新

- DepthMはfloat、下向き正。シミュレーションm/s/g/kg、UE境界cm。単位を変数名に書く。
- 更新順はGAME_DESIGNのCoordinator契約を厳守。各Componentの独立Tickで同じ数値を進めない。
- BITEと合わせは整数Tick、受付は `[OpenTick, CloseTick)`。TimerManager、壁時計、演出終了で成否を判定しない。
- 同Tick入力順はSequence。CastIdとBiteTokenで古い要求・二重処理を拒否。
- 基礎確率は `1-exp(-lambda*dt)`。D03の後続StayのBITE確率にはSquidTuningの10回超減衰倍率を掛ける（SQUID_AI参照）。用途別FRandomStreamを使い、グローバル乱数やActorアドレス由来seedを使わない。
- 海底・海面・ライン制約の有限数と範囲を保つ。無効海を深度0として使用しない。
- 海の描画、エギメッシュ、ロッド演出から正本の位置や判定へ逆流させない。

## 6. ゲーム不変条件

- シャクリは1入力1動作、連続入力可能、回数上限なし。10回超の一連回数に応じて後続StayのBITE確率を極端に下げる。回数による強制Stay/入力拒否をしない。Fall/Stay切替やMISSだけで適用回数を消さない。
- STAYは竿をあおるシャクリ動作をしていない通常の釣り状態。Shakuri（既存enum名Jerking）→TensionFall→Stayとし、シャクリ直後の過渡処理終了で即時移行する。無入力タイマーやレンジ安定達成を移行条件にしない。再シャクリ・再フォールはプレイヤーコマンドで可能。
- キャストなし、竿先付近投入、FreeFall自動繰出し。初期エギ3.5号35g、シンカー0gを正式許可。装備変更はエギが船上かつ現在Cast終了済みの準備状態でのみ許可（初投前は活動中Castなし）。Deploy受理から投終了までロックし、Retrieve完了後の次投Readyでエギ・シンカーを変更できる。EndFishingは変更の必須操作ではなく、終了通知だけで船上帰還を代用しない。
- STAYのエギ深度を完全固定しない。船ドリフトによるラインの上向き作用とエギ＋シンカー総重量の沈下作用が釣り合い、狙いレンジを安定維持するほどBITE評価を高める。上昇・下降の両方で評価を下げ、重量調整を最重要判断とする。RangeError/RangeStabilityの係数・曲線はDataAssetに分離し、製品値を固定しない。
- MVPのBITE承認はStayだけ。AttackとBiteを一つに潰さない。
- 活性はLow/Medium/Highの3段階、距離・レンジ差・Stay時間を判定に反映。季節補正はMVPに入れない。
- Hook初期値はBITE開始後 `[0.10秒,0.55秒)`、DataAsset調整可能。早合わせ/時間切れはMISS。
- Fightはテンション＋巻上げ進捗。過大テンションの連続超過でバラシ、休止でテンションを下げる。数値係数はDataAsset。
- MISSで投を終了せずStay/再Fall可能、通常の未釣獲の投は回収完了で終了する。Caught/明示中断等とは区別する。
- 環境は一定水平潮のみ。風・波の物理影響を入れない。無効環境への技術防御と保留D14のゲーム仕様を混同しない。
- MVPはテストイカ1体・固定重量・Prototype Cue1種・内部情報HUD。Cue型は将来3種へ拡張可能とする。
- 早合わせ/期限切れ後に同TokenでHitへ変更しない。Caution/Cooldownを投の再開で消さない。
- 1投の終端結果は一度のみ。Caught以外に重量のある釣果を作らない。
- 製品バランスに実測根拠がない場合は「ゲーム近似」と書く。

## 7. テストと報告

- 変更に関係するRoadmap/詳細設計の試験を実行する。数値・状態遷移・受付境界・寿命の試験を優先する。
- 初回はEditorターゲットの通常ビルド、UHTを確認する。反射宣言の変更をLive Coding成功だけで確認済みにしない。
- 自然AI統合と制御された決定論的試験を両方持つ。強制HITだけで1投成立と報告しない。
- Windowsパッケージ起動はMVP完了前に実施。Steamへの公開・リポジトリpush等は実装/ローカル検証とは別の外部操作としてユーザーの依頼範囲を確認する。
- 文言だけの修正に実装をなぞるテストを追加しない。必要な試験が通ったら、理由なく全試験を繰り返さない。
- 報告は変更・検証・未検証・残る仕様を簡潔に記す。実施していないビルド、実機操作、実測を実施済みと書かない。

## 8. リポジトリ保全

- 作業前に既存の変更を確認し、他の作業を上書き/破棄しない。大規模リネームやリセットを独断でしない。
- バイナリ生成物、Intermediate、Saved、DerivedDataCache等をソースとしてコミットしない。
- 認証情報・Steam秘密情報を文書/コード/ログへ保存しない。
- この規約と設計を変更した場合は理由と影響する契約・試験を示す。
