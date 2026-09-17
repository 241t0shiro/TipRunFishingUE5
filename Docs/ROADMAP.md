# ロードマップ・Codex向けMVP実装順序

2026-09-17 M10.5-R設計改訂: A〜F基盤は保持。最新手動PIEでGはゲームプレイ品質不合格。Rは設計/実装分割のみ完了し、実装未着手。最新契約は本書末尾のM10.5-R節を優先。以前のG合否保留・固定Pulse・観測カメラ等は履歴。R自動検証とユーザー手動合格後もHへ自動進行しない。H/M11以降は保留。

2026-09-15 M10.5-E完了: 左保持の通常回収／解放後Stayと、固定TickのQuickRetrievingを分離。今回の明示依頼を優先し、通常完了はResult（ロック維持）→NextCastでReady/解除、Quick完了だけ直接Ready/解除。海面近傍のライン拘束・巻取りを修正。UHT生成・実C++・Development Editor Win64成功、E 7件＋回帰54件成功、各試験エラー/警告0。保存資産移行・PIEは未実施。F〜H・M11以降は未着手、全体品質ゲート未合格。下のA〜D記録は履歴。

2026-09-14 M10.5-D完了: Mouse Axis2D→固定Input Queue→RodControl、右クリック/Spaceの同一Jerk、基準姿勢＋時間プロファイル、RodTip→Cライン接続を実装。Rod有効時は旧Lift/Reelを重ねない。UHT生成・実C++・Development Editor Win64成功、D 5件＋必要回帰49件成功、各試験エラー/警告0。Rod/Input資産は明示設定、既存保存Prototype移行/実マウスPIEは未実施。E〜H・M11以降は未着手、M10.5全体品質ゲートは未合格。以下のA〜C/設計のみの記録は履歴。

2026-09-14 M10.5-C完了: Egi WorldPositionを位置正本へ移行し、revision 2の深度潮/水中ライン抗力/需要繰出し/空間拘束とSnapshotを実装。UHT生成・実C++・Development Editor Win64成功、C 6件（243落下条件含む）＋回帰49件成功、各試験エラー/警告0。標準30mの最大ライン39.299694m、最長50.550秒で着底。保存Prototypeは旧係数revision 1のまま、資産移行/新モデルPIEは未実施。D〜H・M11以降は未着手、M10.5全体の品質ゲートは未合格。以下のA/B完了・設計のみの記録は履歴。

2026-09-14 M10.5-B完了: 風/表層潮を別々に評価する船体方向別応答、抗力/慣性、解析的な固定更新とBoat Snapshot拡張を実装。UHT生成・実C++・Development Editor Win64成功、B 5件＋A/M03/M04/M05/M08回帰26件成功、各試験エラー/警告0。旧保存設定はModelRevision=1で互換維持、新モデルは明示revision 2。保存Prototypeの移行/PIE再評価は未実施。C〜HおよびM11以降は未着手、M10.5全体の品質ゲートは未合格。以下のA完了/設計のみの記録は履歴。

2026-09-14 M10.5-A完了: 環境の風/表層潮/深度別潮、位置依存評価口、knot換算を実装。UHT生成・C++実コンパイル・Development Editor Win64成功、A 5件＋M03/M05/M08回帰20件成功、各試験エラー/警告0。B〜HおよびM11以降は未着手。M10.5全体のPIE品質ゲートは未合格。下の2026-09-13設計のみの記録は履歴。

2026-09-13 M10.5設計改訂（実装未着手）: M00〜M10の実装・自動試験成功は履歴として保持するが、ユーザーのM10後PIE評価は再現性・操作性の品質不合格。M11への進行はM10.5品質ゲート合格まで保留する。本書のM10.5改訂契約を旧記述より優先し、M03〜M10完了記録は旧実装の証跡として読む。今回はMarkdownのみ更新し、改訂機能の実装・ビルド・試験は行っていない。


2026-09-13 M10実装反映: Shakuri/TensionFall/Stay/Re-Fall/Retrieve、一投単位の装備ロックと船上帰還後の次投変更、深度速度・境界接触Snapshotを実装済み。旧AutoStay項目は型/検証/Prototype設定から撤去済み。M06〜M09記録は当時の履歴として保持し、現在の契約・検証範囲はROADMAPのM10完了記録を参照。M11以降は未着手。

関連: [全体正本と要決定事項](GAME_DESIGN.md)、[実装規約](../AGENTS.md)。**M00〜M10は実装・自動試験完了、M10後PIE品質不合格。M10.5はA〜F実装/自動検証完了、Gゲームプレイ不合格、R設計のみ完了・実装未着手、H保留。M11〜M18は未着手で進行保留。** 工数・発売日・担当者は未決。

更新: v0.2 / 2026-09-12。MVP決定済みD01〜D09/D13/D15を前提とする。D03は同日の補足「回数上限なし、10回超で後続StayのBITE確率を極端に低下」を優先。D10/D11とD12の未指定部分はMVP暫定仕様を使い、残る製品仕様をAlpha前に再決定（マウス基本操作はM10.5で決定済み）、D16の簡易NavigationはR対象。D14/D17/D18はMVP非ブロック。

## 1. フェーズ

| フェーズ | 実装範囲 | 完了条件 | 持ち込まないもの |
|---|---|---|---|
| MVP | 仮船/風＋表層潮＋深度別潮/空間ライン/1投、上限なしシャクリと10回超減衰、非シャクリ状態のStayとレンジ維持評価、3段階活性AI、0.10〜0.55秒受付、テンション/進捗ファイト、固定重量結果 | Windowsで自然な1投と境界・バラシ・回収試験が再現できる | SHOP、Rの簡易操船を超える本格操船、波の物理、CFD、季節補正、高度描画、Steam実績 |
| Alpha | 自由操船、ポイント探索、地形差、複数エリア、潮の差、ロッドCue改善、実測調整 | エリア選択から釣果まで反復して遊べる | 未決の経済・大会を先行実装しない |
| 将来版 | 季節/天候、SHOP/装備、詳細ファイト/取り込み、セーブ、Steam配布/実績、大会 | 各仕様決定後の個別受入基準 | オンラインや大会形式を推測で追加しない |

拡張点は契約を維持して差し替える。Ocean Provider→地形、Boatの推進成分→操船、装備Snapshot→SHOP、OnCastCompleted→保存/実績。MVPから空のショップ/大会/実績サービスを用意する必要はない。

## 2. 実装開始時の前提

1. `AGENTS.md` と7設計書を読む。最初の実装タスクを選び、関連D番号の状態を確認する。
2. 対象リポジトリが `TipRunFishingUE5` であることを確認する。UE 5.8.2 / C++プロジェクトは作成済みであり、既存構成を維持する。
3. ローカルUE 5.8.2、Windows用コンパイラ/SDK、プロジェクト生成・ビルド手段を確認。導入不足は不足内容を報告する。
4. MVP決定と使用承認済みのMVP暫定仕様は再承認を求めない。未指定の倍率/係数は調整項目として区別し、試験例は `Prototype/Test` アセットへ隔離する。D14/D17/D18の保留でMVPを止めない。D16はRの限定範囲を実装する。

## 3. MVPの小タスク

各タスクはレビュー可能な変更単位。1タスクでUI・AI・物理を同時に作り込まない。依存が通ってから次へ進む。

| 順 / ID | 作業と主な成果物 | 前提 | 受入・試験 |
|---|---|---|---|
| 01 M00（完了） | 既存プロジェクト・開発環境確認。`.uproject`、Runtimeモジュール、Game/Editorターゲット、基本ディレクトリ、Git・生成物除外を確認 | 既存UEプロジェクトへ適用 | Development Editor Win64通常ビルド成功（最新判定・0アクション）。検証範囲は下記完了記録参照 |
| 02 M01（完了） | Data共通enum/struct、m/cm換算、ID、Snapshotとイベント契約 | M00 | 新規コードのUHT生成・C++コンパイル成功、M01 Automation 4件成功。下記完了記録参照 |
| 03 M02（完了） | 装備行、3種エギ/無しを含む9選択、係数DataAsset、初期エギ35g | M01、D08決定済み | F01を含む6試験成功、Prototype保存・再読込、新規UHT/C++ビルド成功。下記記録参照 |
| 04 M03（完了） | Oceanの平底ProviderとSampleOcean | M01/02、テスト環境値 | 新規UHT/C++ビルド成功。2026-09-13の最終7試験すべて成功・警告/エラー0件。下記記録参照 |
| 05 M04（完了） | Coordinatorの固定時計、入力キュー、登録解除、用途別seed | M01 | 新規UHT/C++ビルド成功。Tick順・ポーズ・catch-up・再現性等の6試験成功、試験警告/エラー0。下記記録参照 |
| 06 M05（完了） | 仮BoatPawnと一定水平潮ドリフト、RodAnchor、Snapshot接続 | M03/04、D15決定済み | B01/B02/B03/B05を含む6試験成功、30/60/120fps・pause・無効値も確認。UHT/C++ビルド成功、下記記録参照 |
| 07 M06（完了） | SessionActor、装備ロック、キャストなし投入、最小遷移 | M02/04、D01/D02/D08 | CastId増加、不正入力拒否、F16全境界を含む5試験と必要な既存回帰6件成功。UHT/C++ビルド成功、下記記録参照 |
| 08 M07（完了） | EgiSimulationの鉛直落下と海底制約、描画用EgiActor | M03/05/06 | F02/F03/F12のM07範囲を含む6試験と依存回帰8件成功。仮エギの着底座標、UHT/C++ビルド成功。範囲・未検証は下記記録参照 |
| 09 M08（完了） | 水平潮応答、ライン長/球面制約、船追従 | M07、D02/D05 | F04/F05/B07を含む7試験と依存回帰14件成功、UHT/C++ビルド成功。制御入力による検証範囲は下記参照 |
| 10 M09（完了） | Enhanced Input接続と最小デバッグHUD（状態/深度/装備） | M06/07、D12 | U02を含む7試験＋回帰13件成功、各試験の警告・エラー0。UHT生成/C++/Development Editor Win64成功。目視・実機未検証は下記参照 |
| 11 M10（完了） | 連続シャクリと一連回数、Shakuri→TensionFall→Stay、再Fall、回収、一投単位の装備ロック/次投変更、レンジ評価用Snapshot、旧タイマー/設定/テスト撤去 | M08/09、D03/D04/D08/D13 | F06/F07/F08/F16/F17/F18/F19を含む8試験と回帰32件成功。UHT/C++/Development Editor Win64成功。詳細・未検証は下記記録 |
| 11.5 M10.5（A完了・B〜H未着手） | Prototype Realism Revision。風/潮/船体応答、空間エギ/ライン、マウス操作、Quick Retrieve、日本語HUD/装備UI | M00〜M10実装、ユーザーPIE不合格報告 | 下記M10.5-A〜HとR01〜R12、自動試験＋PIE品質再評価に合格 |
| 12 M11（未着手） | テストイカ1体、3段階活性、距離/RangeError/RangeStability/RangeHoldScore/Exposure/STAY時間、評価用DataAsset | M10.5品質ゲート合格、D01/D07 | S01/S13/S14/S19、維持指標検証、季節データ不要 |
| 13 M12 | Attack/Bite、10回超減衰、Caution/Cooldown、仲裁、Hook最小API | M11、D03/D07 | S02〜S04/S07/S08/S15〜S17/S20。非StayのBITEゼロ、維持良好ほど高確率、回数に応じ強い減衰 |
| 14 M13 | Hook受付初期0.10/0.55秒、早/適正/遅判定、仮Cue1種 | M12、D06、D11暫定 | S05/S06/S09/S11/S12、S18のCue部分。HIT/MISS重複なし |
| 15 M14 | Fightテンション/進捗、継続超過バラシ、対象解放、Landing、固定重量 | M13、D09、D10暫定 | F10/F13/F14/S18、安全な巻きとバラシ、二重結果なし |
| 16 M15 | 内部HUD、Result/次投、MISS継続、回収終了、入力解除/破棄 | M14、D13、D12暫定 | F09/F11/F15/U01/U03/U04/U06/U09〜U12 |
| 17 M16 | 斜面シナリオ、無効環境、開発用試験シナリオ/ログ | M15 | O02/O07/O08/B06/F12 |
| 18 M17 | 自然AIでの1投統合、制御入力による各結末のFunctional Test | M16 | 下記MVP受入シナリオをすべて実行 |
| 19 M18 | Windows Developmentパッケージ、起動/入力/結果/再投確認 | M17 | Editor非依存の起動、30/60/120fps比較、ログに致命的エラーなし |

M10は旧AutoStay用の設定・期限・HUD契約・タイマー前提テストの撤去/置換を含む。M02由来の型/検証/Prototype保存アセットに残る旧設定の移行と必要最小限の回帰確認もM10の範囲。D08改訂に従いM06のセッション全体ロックを一投単位へ移行し、Retrieve後の船上帰還/次投変更、Snapshot・メッシュの更新と旧F16期待値の置換を検証する。汎用時刻換算試験は維持する。レンジ指標の評価とDataAssetはM11、BITE倍率への接続はM12の将来タスクとし、今回の文書更新では実装しない。D14の境界ゲーム仕様はM16に含めず、無効サンプルへの技術防御だけ検証する。Steam SDK・配信アップロードはM18に含まない。既存プロジェクトへ適用する場合はM00を環境/構成確認に読み替え、既存コードを作り直さない。

### M00完了記録（2026-09-12）

- 既存プロジェクトへの適用として環境・構成確認を実施し、合格と判断。M01へ進める状態。ゲーム機能、独自GameMode/Controllerの追加は行っていない。M01〜M18は未着手。
- UE 5.8.2 / C++、Runtimeモジュール `TipRunFishingUE5`、Game/Editorターゲット（BuildSettingsVersion.V7）、`Source / Config / Content / Docs / AGENTS.md`を確認。`TRBase`は空のコンストラクタ・デストラクタのみのC++化確認用仮クラスとして残す。Contentにはフォルダのみ存在し、プロジェクト固有アセット・マップは未作成。
- Development Editor Win64を通常のBuild.batで確認し、`Succeeded`・終了コード0。最新判定で0アクションのため、新規コンパイル・UHT反射コード生成の実処理は未確認。既存EditorログでプロジェクトDLL読み込み・Engine初期化成功を確認。新規の空マップ起動・PIE操作は未実施。M01の反射宣言導入時に通常ビルドとUHTの実処理を確認する。
- 開発環境とMSVCの非推奨バージョン警告は [GAME_DESIGNの検証記録](GAME_DESIGN.md#9-検証と実装前確認) を参照。ビルドエラーはなく、実装コード・Config・Contentの修正は不要だった。
- M00調査前後のGitはクリーン。主要UE生成物の除外と、除外対象の追跡済みファイルがないことを確認。`.vsconfig`も除外されるためVS導入構成はGit共有されない。DefaultEngine.iniには同値のDefaultGraphicsRHI重複があるが、ビルドを妨げておらず変更していない。
- 今回の文書更新は「UEプロジェクト未作成」「全タスク未着手」という記述と実態の不一致を解消するもの。ゲーム仕様・システム間契約・M01以降の受入試験は変更しない。

### M01完了記録（2026-09-12）

- 追加: 共通enum 15種、値struct 14種、native delegate 4種、m/cm換算、共通秒→Tick換算。値型の詳細表現はGAME_DESIGN第4節を参照。DataAsset・装備行・Actor/Component・状態遷移は追加していない。既存TRBase、Build.cs、Target.cs、Config、Contentは変更なし。
- Development Editor Win64通常ビルド成功（終了コード0、約22.8秒、9アクション）。Internal UnrealHeaderToolが新規ヘッダを処理し11生成ファイルを書き出した。TRSimulationTypes.cpp、TRCommonTypesTests.cpp、Module.TipRunFishingUE5.gen.cpp等のコンパイルとDLLリンクを確認。最新判定だけの確認ではない。
- NullRHIのUnrealEditor-Cmdで `TipRun.M01` を実行。Units、Identifiers、TimeConversion、Reflectionの4件すべて成功、各試験の警告・エラー0件、プロセス終了コード0。100cm/1m、ID無効値・Token同一性、0.10/0.55/0.8秒→6/33/48Tick（当時の汎用換算試験記録であり、現行Stay遅延の要件ではない）、丸め境界・不正値・オーバーフロー、反射登録・Blueprint読取専用を確認。
- ビルド警告: MSVC 14.51.36257が推奨範囲より新しいこと、既存IncludeOrderVersionがUnreal5_6互換であること。試験開始前のEngine起動ログにはCondition failed等のエラー出力とレイアウト警告もあるが、M01試験結果には記録されていない。起動ログの原因調査は未実施。M01のコード修正を要するビルド・試験エラーはなかった。
- 検証記録: `Saved/Logs/M01Build.log`、`Saved/Logs/M01Tests.log`、`Saved/Automation/M01/index.json`（いずれもGit除外）。M02へ進める状態。M02〜M18は未着手。今回の文書更新は実装・検証範囲の記録であり、D番号のゲーム仕様と後続タスクの受入条件は変更しない。

### M02完了記録（2026-09-12）

- 装備行2種、FishingParameters、EgiSimulationProfile、EquipmentSnapshot、FishingTuningDataAssetを追加。試験用の装備DataTable 2件とFishingTuning 1件を`Content/TipRun/Prototype/Data`に保存。UEのSavePackageを使用し、uassetをテキスト生成していない。詳細・試験値の扱いはFISHING_SYSTEMのM02記録を参照。
- Development Editor Win64の通常ビルド成功（終了コード0、約13.2秒、7アクション）。Internal UnrealHeaderToolが6生成ファイルを書き出し、TREquipmentData.cpp、TRFishingTuningDataAsset.cpp、TREquipmentDataTests.cpp、生成コードのコンパイルとDLLリンクを確認。
- アセット生成試験1件成功後、別プロセスで`TipRun.M02`の6件（F01Combinations、FrozenSnapshot、InvalidRows、PrototypeAssets、ReferencesAndDuplicates、TuningValidation）すべて成功。各試験の警告・エラー0、プロセス終了コード0。27組合せ・30〜90g・初期エギ35g・0g受理、欠損/重複/不正値、曲線と時刻境界、凍結、保存アセットの再読込を確認。
- ビルド警告は既存のMSVC 14.51.36257推奨範囲外とUnreal5_6互換include順序。Engine起動時にはM01でも見られたCondition failed等の出力と対象外プラットフォームSDK不足があるが、Win64はVALIDでM02試験にはエラーなし。対象外の環境設定は変更していない。
- ログは`Saved/Logs/M02Build.log`、`M02Generate.log`、`M02Tests.log`、結果は`Saved/Automation/M02/index.json`（Git除外）。既存M01コード・TRBase・Build.cs・Target.cs・Configは変更なし。M03へ進める状態で、M03〜M18は未着手。ゲーム仕様D番号・後続の受入条件は変更せず、データ検証と試験係数の技術上の扱いを文書化した。

### M03完了記録（実装2026-09-12 / 最終確認2026-09-13）

- OceanAreaDataAsset、SeabedProviderActor、OceanWorldSubsystemとOcean設定・深度結果型を追加。DataのIsDataValid、平底、一定水平潮、境界、不正数値、Providerの破棄/再設定に対応。独立Tick・Actor探索・描画からの逆流なし。M03に対応する技術上の扱いはOCEAN_SYSTEM第7節を参照。
- Development Editor Win64通常ビルド成功（終了コード0、約18.0秒、9アクション）。Internal UnrealHeaderToolが12生成ファイルを書き出し、Oceanの新規cpp・Automation Test・生成コードのコンパイルとDLLリンクを確認。
- `TipRun.M03`の7件（O01FlatBottom、O03DepthQueries、O04Boundaries、O05ProviderLifetime、O06UnitsAndCurrent、SnapshotAndTimeIndependence、ValidationAndWorldScope）がすべて成功。初回O05のcontext警告に対し試験用Worldの生成・解放にEngine context登録・解除を追加し、修正コードの再コンパイルとDLLリンクも成功（約4.9秒）。2026-09-13に修正後の全7件を再実行し、各試験の警告・エラー0件、終了コード0を確認。最終確認時の追加コード修正は不要だった。
- ビルド警告は既存のMSVC 14.51.36257推奨範囲外とUnreal5_6互換include順序。Engine起動時の既知のCondition failed等・対象外SDK不足と、M03の試験結果は区別する。ビルド・試験エラーはなく、上記テストWorld警告への修正のみ行った。
- ログは`Saved/Logs/M03Build.log`、`M03BuildFinal.log`、最終試験は`Saved/Logs/M03TestsFinal.log`・`Saved/Automation/M03Final/index.json`（Git除外）。Content・Config・Build.cs・Target.cs・TRBaseは変更なし。固定時計/Coordinator・斜面・描画検証は未実施で、それぞれの後続タスクに残す。M03は完了、M04へ進める状態。M04〜M18は未着手。

### M04完了記録（2026-09-13）

- `UTRSimulationWorldSubsystem`、時計項目だけの`UTRSessionConfigDataAsset`、nativeフェーズenumと配送delegateを追加。M01の時刻・ID・コマンド型を使用し、M02の検証・設定コピー方針を適用。M03のOceanへBoatフェーズ前に固定時刻を渡す。実装契約はGAME_DESIGN第5節のM04追記を参照。D番号のゲーム仕様や後続受入条件は変更していない。
- Development Editor Win64通常ビルド成功（終了コード0、約12.8秒、7アクション）。Internal UnrealHeaderToolが5生成ファイルを書き出し、TRSessionConfigDataAsset.cpp、TRSimulationWorldSubsystem.cpp、TRSimulationTests.cpp、生成コードの実コンパイルとDLLリンクを確認。最新判定だけではない。
- 初回試験は寿命テストがSubsystemを直接Deinitializeした後にWorldを終了し、UEの二重Deinitializeアサーションで中断した。テストをWorld所有の通常終了経路へ修正し、再コンパイル・DLLリンク成功（終了コード0、約5.0秒、4アクション）。エラーを抑制せず原因を修正した。
- 修正後の`TipRun.M04`全6件（CommandQueue、ConfigurationAndWorldScope、FrameRateAndSeedReplay、PauseAndCatchUp、RegistrationLifetime、TickOrderAndOcean）が成功。各試験の警告・エラー0、プロセス終了コード0。30/60/120fpsの同一入力Tick列・乱数列、用途/個体別seed、入力順と再入拒否、明示/Engine pause、catch-up上限と超過破棄、設定不正/コピー、World種別、更新中の登録変更・Actor破棄・World終了、Ocean→Boatを含む配送順を確認した。船・AI等の実処理は試験用callbackで代替しており、自然な1投の統合試験とは区別する。
- 残存するビルド警告は既存のMSVC 14.51.36257推奨範囲外とUnreal5_6互換include順序。最終試験開始前のEngine起動ログには既存のCondition failed 19件・Editorレイアウト警告1件があり、対象外プラットフォームSDK不足の出力もある。これらはM04試験中の警告・エラーではなく、原因の解消は未実施。Win64 SDKはVALID。
- 初回ビルドは`Saved/Logs/M04Build.log`、修正後ビルドは`M04BuildFinal.log`、最終試験は`Saved/Logs/M04TestsFinal.log`・`Saved/Automation/M04Final/index.json`（Git除外）。コード5ファイル追加、GAME_DESIGN/ROADMAP更新。Content・Config・Build.cs・Target.cs・TRBase・既存M01〜M03コードは変更なし。M04は合格、M05へ進める状態。M05〜M18は未着手。

### M05完了記録（2026-09-13）

- BoatPawn、BoatDriftComponent、BoatTuningDataAsset、BoatParameters、BoatMode、異常通知delegateを追加。SimulationWorldSubsystemに船の初期化・弱参照登録・固定配送・Snapshot読取・EndPlay解除を追加した。既存M04の未コミット変更を保持して拡張。M03 Oceanコード、Content、Config、Build.cs、Target.cs、TRBaseは変更なし。D15の一定水平潮だけを使用し、M06のSessionActor・自由操船・風/波物理は実装していない。
- UHTが新規反射宣言を処理し9生成ファイルを書き出した。新規Boat/Data/TestとGameのC++コンパイル成功後、Unity結合された既存M02の2ファイルで匿名namespace内の同名`Require`関数が衝突し初回ビルドが失敗。`TRFishingTuningDataAsset.cpp`の内部関数と呼出しを`RequireFishingTuning`へ変更する最小修正を実施。データ項目・検証条件・釣り機能は変更していない。
- 修正後のDevelopment Editor Win64は成功（終了コード0、約7.6秒、4アクション）。`-DisableAdaptiveUnity`で全モジュールをUnity結合して再コンパイルし、新規生成コードを含むコンパイルとDLLリンクを確認。関数衝突を非Unityへの切替で回避せず、結合時にも修正が有効なことを確認した。Build/Targetの恒久設定は変更なし。
- `TipRun.M05`の6件（B01B03StillWater、B02CurrentAndSpeedLimit、B04FrameRatesAndPause、B05RodAnchorAndSnapshotOrder、B08InvalidConfigurationAndSamples、EnvironmentAndRegistrationLifetime）がすべて成功。変更に関係するM02/M04の12件も回帰確認し、全18件の警告/エラー0、終了コード0。静水、一定潮の方向と速度収束・上限、固定60Hzの30/60/120描画fps一致、明示/Engine pause、設定凍結、竿先・単位・更新順、無効値・Provider喪失・解除を確認。
- 船の計算/接続の詳細と一時Test係数はBOAT_SYSTEM第6節を参照。実際の釣りComponentへの接続、PIEでの船・カメラ目視、製品/Prototypeの保存アセット作成、Windowsパッケージは未実施。M05の合格は固定更新下の船・Snapshot基盤の検証であり、1投統合・実船校正の完了ではない。
- 残存するビルド警告は既存のMSVC 14.51.36257推奨範囲外とUnreal5_6互換include順序。Engine起動時の既存Condition failed 19件、Editorレイアウト警告1件、対象外SDK不足の出力は残るが、M05試験由来の警告は0件。Win64 SDK 10.0.22621.0はVALID。
- ログ: `Saved/Logs/M05Build.log`、修正後`M05BuildFinal.log`、`M05Tests.log`。結果: `Saved/Automation/M05/index.json`（全てGit除外）。M05としてコード7件追加、Game 2件と既存内部関数1件を変更、設計書3件を更新。M05は合格、M06へ進める状態。M06〜M18は未着手。

### M06完了記録（2026-09-13）

以下は旧D08（セッション全体ロック）の実装・検証履歴。新D08の一投ごとの装備変更は未実装で、M10/F16およびM15/U11で検証する。過去の合格を新仕様の合格に読み替えない。

- SessionActor/FishingComponentとM06試験を追加。M04の入力型/受付APIへExpectedCastIdを渡し、Session側で現在IDとTick/Sequence順を検証。開始前限定の装備変更、釣りセッション中の設定凍結、Boat更新後の海面投入、FreeFall/Payout指示、明示中断→次投Ready、終了/破棄時の解除を実装。詳細はFISHING_SYSTEM第3節を参照。M07の積分・EgiActorと後続のゲーム処理は追加していない。
- Development Editor Win64通常ビルドは成功（終了コード0、約23.5秒、10アクション）。Internal UnrealHeaderToolが7生成ファイルを書き出し、新規FishingComponent・SessionActor・SessionTests、更新したCoordinator、生成コードを含むモジュールの実コンパイルとDLLリンクを確認した。
- 初回M06試験は4/5成功。寿命試験のActor初期化不足によりEndPlay経路へ入らず、破棄直後のキュー件数が1件残る検証が失敗した。試験Worldで通常のActor初期化を行い、加えてBeginPlay前の破棄もDestroyedで即時解放する実装・試験を追加。未来Tickの予約が受付Sequenceだけで拒否されないようSessionの順序判定をTick→Sequenceへ修正した。修正後の再ビルド成功（終了コード0、約8.6秒、6アクション）。
- 最終`TipRun.M06`の5件（F16EquipmentLock、DeploymentAndCastIdentity、FrozenEquipmentAndQueueBoundaries、InvalidInputsAndDependencies、SessionLifetime）が全件成功、警告/エラー0、終了コード0。既存回帰は変更/依存契約に絞り、M01.Reflection、M02.F01Combinations/FrozenSnapshot、M04.CommandQueue/RegistrationLifetime、M05.B05RodAnchorAndSnapshotOrderの6件が初回実行で全件成功・警告/エラー0。後続の変更はSessionとその試験内に限定し、成功済みの既存試験は不要に繰り返していない。
- ログは`Saved/Logs/M06Build.log`、`M06BuildFinal.log`、`M06Tests.log`、`M06TestsFinal.log`。既存回帰を含む初回結果は`Saved/Automation/M06/index.json`（M06寿命試験の初回失敗も保持）、修正後M06結果は`Saved/Automation/M06Final/index.json`。いずれもGit除外。
- 残存するビルド警告は既存のMSVC 14.51.36257推奨範囲外とUnreal5_6互換include順序。Engine起動前処理の既存Condition failed・Editorレイアウト警告・対象外SDK不足は残るが、M06最終試験中の警告は0。Win64 SDK 10.0.22621.0はVALID。
- 作業開始時のGitはクリーン。コード5ファイル追加、既存3ファイル変更、設計書3件更新。Config/Content、Build.cs/Target.cs、M02の解決器、M03 Ocean、M05 Boatコードは変更なし。UI/PIE操作・Windowsパッケージ・自然な1投は未検証。M06は合格、M07へ進める状態。M07〜M18は未着手。D番号の未決仕様を新たに製品仕様へ確定していない。

### M07完了記録（2026-09-13）

- `UTREgiSimulationComponent`、`ATREgiActor`を追加し、Session/Fishingへ固定更新・着底通知・破棄を接続。共通Snapshot/イベント型、M02重量/係数、M03海面/水深、M04更新順、M05 Boat値契約、M06の装備ロック/CastId/寿命を使用する。数値式と境界はFISHING_SYSTEM第4節のM07契約を参照。
- Development Editor Win64通常ビルド成功（終了コード0、約11.9秒、10アクション）。Internal UnrealHeaderToolが8生成ファイルを書き出し、新規Actor/Component/Test、変更したSession/Fishing/M06 Test、生成コードの実コンパイルとDLLリンクを確認。最新判定だけではない。
- `TipRun.M07`全6件（F02WeightAndZeroSinker、F03DepthsAndSingleBottomTransition、LargeFixedStep、FrameRatesAndPause、CastAndSessionLifetime、F12InvalidDataAndEnvironment）成功。重量差と0g、異なる平底水深、海底貫通防止、着底通知一度、ポーズ、30/60/120fps同一結果、旧CastId/破棄Session、不正値/環境喪失、描画からの逆流防止を確認した。
- 必要な回帰8件（M06全5件、M02.F01Combinations、M03.O03DepthQueries、M05.B05RodAnchorAndSnapshotOrder）成功。M04固定更新はM07の描画fps/ポーズ試験と上記接続試験で検証。全14件の試験警告・エラー0、プロセス終了コード0。M06の共通World fixtureをヘッダへ抽出し、「深度積分なし」の旧assertだけをM07導入後の落下確認へ更新した。
- 残存ビルド警告は既存のMSVC 14.51.36257推奨範囲外とUnreal5_6互換include順序。試験開始前のEngine起動には既知のCondition failed 19件、Editorレイアウト警告1件、対象外プラットフォームSDK不足の出力がある。試験World/各試験に由来する警告は0件。Win64 SDK 10.0.22621.0はVALID。これら環境出力の解消は未実施。
- ログは`Saved/Logs/M07Build.log`、`M07Tests.log`、結果は`Saved/Automation/M07/index.json`（Git除外）。コード6ファイル追加・既存5ファイル変更、GAME_DESIGN/FISHING_SYSTEM/ROADMAP更新。Config・Content・Build.cs・Target.cs・TRBaseは変更なし。
- M07は合格、M08へ進める状態。M08〜M18は未着手。F02/F12のライン制約はM08、斜面の統合シナリオはM16に残す。描画ActorはNullRHIで座標を検証し、PIEの目視・Windowsパッケージは未実施。D番号の製品仕様・後続受入基準は変更していない。

### M08完了記録（2026-09-13）

- EgiSimulationの水平指数応答、重量沈下とライン投影、移動先Ocean再照会、長さ/角度/張力代理値を実装。Sessionは問い合わせと描画へ渡す最終海面を接続し、Fishingは接触/離底イベントを所有する。新クラス/共通型/調整DataAssetは追加せず、既存の凍結データを使用。数値式とM10以降に保持する契約はFISHING_SYSTEM第4節のM08記録を参照。
- 初回Development Editor Win64ビルド成功（終了コード0、11.80秒、9アクション）。Internal UHTを実行（約1.86秒、反射宣言の変更がないため生成ファイルの書換え0件）。変更したEgiSimulation/Fishing/Session、M07/M08試験、ModuleのC++実コンパイルとDLLリンクを確認した。最新判定だけではない。
- 初回Automationは20件中19件成功。B07の試験用Session登録に必須の入力callbackがなく、登録拒否で1件失敗した。試験に空の有効callbackを設定し、離底と移動先海域外によるSession中断も追加。試験コード再コンパイル・DLLリンク成功（終了コード0、6.26秒、4アクション）。Runtimeのエラー隠蔽や警告抑制は行っていない。
- 最終`TipRun.M08`7件（F04CurrentAndWeight、F05WeightDriftBalance、F12LineBoundsAndLargeStep、DestinationAndInvalidValues、B07FixedBoatIntegration、SessionLifetimeAndDestination、BottomContactAndRelease）すべて成功。各試験の警告・エラー0、プロセス終了コード0。
- 回帰14件は初回で全件成功・警告/エラー0（M07全6件、旧M06全5件、M02.F01Combinations、M03.O03DepthQueries、M05.B05RodAnchorAndSnapshotOrder）。その後の変更はM08試験だけのため、成功済み回帰は再実行していない。M04固定更新はM08のB07試験で実際のBoatと接続し、Pause・30/60/120fps一致を確認した。
- 重量比較はPrototype/Testのゲーム近似。35/80gの1秒後沈下/水平応答を比較し、同一の張ったラインと船速では1/60秒後に30gが約0.001065m上昇、35gがほぼ変化なし、80gが約0.009569m下降した。これは局所的な釣合い応答の検証で、長時間の製品バランス・BITE評価・実測校正ではない。2秒固定ステップと最短/最大ライン、ゼロ距離、不正値/解なし、旧CastId/破棄も確認。
- ビルドの既存MSVC 14.51.36257推奨範囲外・Unreal5_6互換include順序の警告は残る。Engine起動時の既知のCondition failed 19件、Editorレイアウト警告1件、対象外SDK不足の出力と、各試験の警告0件を区別する。Win64 SDK 10.0.22621.0はVALID。M08由来の残存警告なし。
- ログは`Saved/Logs/M08Build.log`、`M08BuildFinal.log`、`M08Tests.log`、`M08TestsFinal.log`。回帰結果は`Saved/Automation/M08/index.json`、最終M08は`Saved/Automation/M08Final/index.json`（Git除外）。コード1ファイル追加・6ファイル変更、GAME_DESIGN/FISHING_SYSTEM/ROADMAP更新。既存未追跡のequipment_revision.patchを保持。Config/Content・Build.cs/Target.cs・M03/M05実装は変更なし。
- M08は合格、M09へ進める状態。M09〜M18には未着手。D04/D07/D08の最新仕様を維持し、旧AutoStay設定撤去・Shakuri/Stay操作・Retrieveと一投ごとの装備変更・深度速度/接触Snapshot公開はM10、レンジ評価はM11、BITE倍率はM12に残す。NullRHIによる数値/Actor検証であり、PIE目視・入力HUD・パッケージ起動は未実施。

### M09完了記録（2026-09-13）

- `ATRPlayerController`、`UTRInputConfigDataAsset`、`ETRPlayerAction`、`FTRInputBinding`、`FTRHUDSnapshot`、`ATRHUD`、`UTRFishingHUDWidget`、最小接続用`ATRGameModeBase`を追加。SessionConfigに起動参照/初期位置、Sessionに入力安全API/処理通知/表示コピーを追加した。M01の型、M02の装備、M03の海、M04の時計/Queue、M05の船、M06の寿命、M07/08のエギ数値を使用し、計算式をUIやGameModeへ移していない。
- Enhanced InputのStarted/Completed/Canceledから既存コマンドを送り、Tick→Sequence順、CastId/Session登録世代/破棄防御を維持。Pause/フォーカス喪失/Resultで保持を解除し、再開時に動作を勝手に再送しない。Retrieve停止だけは有効な同Castへの安全停止として保持できる。詳細はUI_SPEC第7節。
- HUDはCastId、Fishing State、Egi Depth、Water Depth、Vertical Velocity（world Z、上向き正）、Line Length/Angle、Tension代理値、Boat Velocity、Current、Egi/Sinker/総重量を表示。表示APIは固定更新済みのコピーを返し、数値を変更しない。Squid State/BITE Window/RangeError/RangeStabilityは未追加。
- M09は7試験成功: U02EnhancedPressRelease、FixedOrderAndFrameRates、CastSessionAndControllerLifetime、PauseFocusAndHeldInput、HUDReadOnlySnapshot、InputConfigAndStartup、PrototypeAssets。Enhanced PlayerInputの実イベント注入、30/60/120fps、古い要求、破棄、保持解除、読取非破壊、不正な起動、Blueprintと内部所有Input参照の保存/別プロセス再読込を含む。
- 回帰13件成功: M04全6件（新規SessionConfig参照が時計専用データを壊さないことも確認）、M06全5件、M08のB07FixedBoatIntegration/SessionLifetimeAndDestination。最終20件すべてエラー・警告0件。保存用試験も警告0件。試験World/Controller由来の残存警告なし。
- UE5.8.2 / MSVC14.51.36257 / Windows SDK10.0.22621.0。初回M09Build.logでUHTが17ファイルを生成、入力/HUDを含むC++9アクションとDLLリンク成功。M09BuildFinal.logでもUHTを実行。追加試験のTSubclassOf型指定エラー1件を修正し、最終M09BuildVerified.logでC++再コンパイル/リンク成功。Prototype Blueprintの設定保存不備は変更通知＋再コンパイルで修正し、最終再読込試験成功。
- 既存MSVC推奨範囲外・Unreal5_6互換include順序の注意は残る。Engine起動時の既知のCondition failed（Error表記）19件、Editorレイアウト警告1件、対象外SDK不足の出力は各試験のエラー・警告0件と区別する。Win64 SDKはVALID。M09変更由来の残存警告なし。
- 証跡は`Saved/Automation/M09Final/index.json`、`Saved/Automation/M09Assets/index.json`、`Saved/Logs/M09TestsFinal.log`、`M09Assets.log`、`M09Build.log`、`M09BuildFinal.log`、`M09BuildVerified.log`（Git除外）。最終20件成功をJSONで確認し、プロセス終了コードだけで成功判定していない。
- 新規Prototype SessionConfigとGameMode BlueprintをUEのシリアライザで保存した。World SettingsでGameMode Overrideへ指定する手順をUI_SPEC第7節に記載。Config/既存Content/既存Mapは未変更、既存未追跡`equipment_revision.patch`も保持。UnrealEd依存はEditorターゲットのアセット生成試験に限定し、Runtimeへ含めない。
- **M09合格、M10へ進める。M10〜M18は未着手。** 合格はコード・データ・自動試験の範囲。PIE目視、解像度/DPI、実キーボード/ゲームパッド、アプリ切替の実機確認、パッケージ起動は未実施。Shakuri/TensionFall/Stay、Retrieve実動作、D08の装備変更/ロック移行、旧AutoStay設定撤去はM10以降に残す。未実装コマンドがキューで拒否されることを操作完了と扱っていない。

変更ファイル（23件、既存equipment_revision.patchは対象外）:

| 区分 | ファイル |
|---|---|
| 新規型・入力データ | `Source/TipRunFishingUE5/Public/Data/TRHUDSnapshot.h`、`Public/Data/TRInputConfigDataAsset.h`、`Private/Data/TRInputConfigDataAsset.cpp` |
| 起動設定更新 | `Source/TipRunFishingUE5/Public/Data/TRSessionConfigDataAsset.h`、`Private/Data/TRSessionConfigDataAsset.cpp` |
| Game新規 | `Source/TipRunFishingUE5/Public/Game/TRGameModeBase.h`、`Private/Game/TRGameModeBase.cpp`、`Public/Game/TRPlayerController.h`、`Private/Game/TRPlayerController.cpp` |
| Session更新 | `Source/TipRunFishingUE5/Public/Game/TRFishingSessionActor.h`、`Private/Game/TRFishingSessionActor.cpp` |
| UI新規 | `Source/TipRunFishingUE5/Public/UI/TRHUD.h`、`Private/UI/TRHUD.cpp`、`Public/UI/TRFishingHUDWidget.h`、`Private/UI/TRFishingHUDWidget.cpp` |
| 試験新規 | `Source/TipRunFishingUE5/Private/Tests/TRInputHUDTests.cpp` |
| 依存更新 | `Source/TipRunFishingUE5/TipRunFishingUE5.Build.cs`、`TipRunFishingUE5.uproject` |
| アセット新規 | `Content/TipRun/Prototype/Data/DA_TR_M09Session_Prototype.uasset`、`Content/TipRun/Prototype/BP_TR_M09GameMode_Prototype.uasset` |
| 設計記録更新 | `Docs/GAME_DESIGN.md`、`Docs/UI_SPEC.md`、`Docs/ROADMAP.md` |

表中のPublic/Privateで始まる省略パスは、すべて`Source/TipRunFishingUE5/`配下。
### M10完了記録（2026-09-13）

- M10完了時点では自動試験範囲で合格と判断した。その後のユーザーPIEで品質不合格となり、M11へ進める判断は撤回。M10.5合格が必要。M11〜M18は未着手。操作の技術契約はFISHING_SYSTEM第8節、入力と次投変更手順はUI_SPEC第8節に記録した。Squid AI/Attack/Bite/Hook/Fight/RangeError・RangeStability評価は実装していない。
- FishingComponentに1入力1シャクリ、予約・一連の回数、Jerking→TensionFall→Stay、Re-Fall、Retrieve開始/停止を追加。EgiSimulationへリフトと巻取り、残留リフトの指数減衰・完了、回収終点と1回だけのRetrieved、補正後深度速度・海底/海面接触を追加した。海底接触中のTensionFall要求はBottomを優先する。M08の潮応答・重量沈下・船ドリフト/ライン制約を継続して使用する。
- SessionはStartFishingでロックせず、固定更新外の候補検証・メッシュ準備とDeploy時のコピー/ロックを分離。回収終了で船上帰還/結果/前投装備を確定し、NextCastでReadyへ進むと変更可。次のDeployで新重量・係数・メッシュを使い再ロックする。Abort/EndFishingは船上帰還を代用しない。古いCast/登録世代の入力を拒否し、終端通知を一度だけ発行する。
- SnapshotにDepthVelocityMps、bBottomContact/bSurfaceContact、投内/一連/予約のシャクリ回数、StateEnteredTick、RangeObservationSecondsを追加。後者はTF/Stayの観測時間であり安定時間の判定ではない。世界速度と深度速度の符号・補正/速度上限を区別し、コピー取得で正本を更新しない。Controllerに内部確認用TRSetEquipment Execを追加し、HUDへ今回の回数・接触・装備変更可否を表示する。
- AutoStayDelaySの宣言/検証/時刻換算と旧前提の試験を撤去。既存Prototype FishingTuningをUEのSavePackageで移行し、新しいTensionLiftDecayPerS=12/s、TensionLiftCompletionMps=0.05m/sだけを試験値として追加した。既存の重量曲線・設定を維持し、保存ファイルに旧項目名がないことを確認した。通常のAutomationは読込だけで、移行は明示の`-TRMigrateM10Prototype`付き`TipRun.M10.TuningMigration`で実行した。
- M10 Automation 8件成功: TuningMigration、F06OneInputOneJerk、F07F18ReFallAndSeries、F08InputPriorityAndCancellation、F16RetrieveEquipmentLoop、F17PauseFocusAndSnapshot、FixedFrameRatesAndResults、F19DepthVelocityAndContact。1/10/11/20シャクリ・予約取消、TF完了・Stay再操作、海底への再Fall、回収開始/停止/完了、0g/次投90g、同Tickの装備変更拒否、次投メッシュ更新、前投結果の保持、古いCast、Pause/フォーカス、30/60/120fpsの状態列と終了時刻一致、深度速度・接触・読取非破壊を確認。
- 必要な回帰は計32件成功: M07全6件、M08全7件、M09全7件、改訂D08に期待値を合わせたM06全5件、移行対象M02全6件、M01.TimeConversionの1件。M07/M08/M09の再投寿命試験はAbortを架空の船上帰還として使わず、Retrieve完了を経て次投を開始する構成へ修正。M09のRetrieve受付期待値も実装済みに更新した。旧仕様の成功を新仕様の成功として流用していない。
- 初回は全40件成功。最終レビュー後に海底接触優先/終端通知処理と同Tick変更・メッシュ・重複通知の検証を補強し、影響するM10＋M06〜M09の33件を再実行して全件成功。変更のないM02/汎用時刻7件は初回成功を採用。すべての試験エラー・警告は0件、プロセス終了コード0。Prototype移行単独試験も成功。
- UE5.8.2 / MSVC14.51.36257 / Windows SDK10.0.22621.0。初回UHTで14生成ファイルを書き出し、15 C++コンパイルアクションとDLLリンクを含む18アクション成功。最終もUHT、変更C++の実コンパイル、DLLリンク、Development Editor Win64ビルド成功。up-to-date判定だけではない。今回コンパイル/試験エラーは発生していない。
- M10変更由来の残存警告なし。既存MSVC推奨範囲外、Unreal5_6互換include順序、Engine起動時のCondition failed（Error表記）19件、Editorレイアウト警告1件、対象外SDK不足は残る。これらを各試験のエラー・警告0件と区別する。Win64 SDKはVALID。
- 証跡: `Saved/Logs/M10Build.log`、`M10BuildFinal.log`、`M10Migration.log`、`M10Tests.log`、`M10TestsFinal.log`。結果: `Saved/Automation/M10Migration/index.json`、`M10/index.json`、`M10Final/index.json`（いずれもGit除外）。試験結果JSONとUHT/Compile/Linkの実処理を確認した。
- 未検証: PIE目視、実キーボード/ゲームパッド・コンソール操作、解像度/DPI、Windowsパッケージ、実測/製品バランス校正。今回合格は固定更新のコード・データ・自動試験の範囲。製品値を確定せず、レンジの安定判定・BITE評価はM11/M12へ残す。
- 既存の`TipRunFishingUE5.uproject`差分と未追跡`equipment_revision.patch`は保持し、M10の変更一覧から除外。Config・Build.cs/Target.cs・Boat/Ocean実装・既存Mapには変更なし。AGENTS/設計書の更新はM10の実装状態と技術契約の記録であり、後続AIのゲーム仕様を変更していない。

#### M10変更ファイル一覧

計29件。

- `AGENTS.md`
- `Content/TipRun/Prototype/Data/DA_TR_FishingTuning_Prototype.uasset`
- `Docs/FISHING_SYSTEM.md`
- `Docs/GAME_DESIGN.md`
- `Docs/ROADMAP.md`
- `Docs/SQUID_AI.md`
- `Docs/UI_SPEC.md`
- `Source/TipRunFishingUE5/Private/Data/TREquipmentData.cpp`
- `Source/TipRunFishingUE5/Private/Data/TRFishingTuningDataAsset.cpp`
- `Source/TipRunFishingUE5/Private/Fishing/TREgiSimulationComponent.cpp`
- `Source/TipRunFishingUE5/Private/Fishing/TRFishingComponent.cpp`
- `Source/TipRunFishingUE5/Private/Game/TRFishingSessionActor.cpp`
- `Source/TipRunFishingUE5/Private/Game/TRPlayerController.cpp`
- `Source/TipRunFishingUE5/Private/Tests/TREgiTests.cpp`
- `Source/TipRunFishingUE5/Private/Tests/TREquipmentDataTests.cpp`
- `Source/TipRunFishingUE5/Private/Tests/TRFishingOperationsTests.cpp`
- `Source/TipRunFishingUE5/Private/Tests/TRInputHUDTests.cpp`
- `Source/TipRunFishingUE5/Private/Tests/TRLineTests.cpp`
- `Source/TipRunFishingUE5/Private/Tests/TRSessionTestFixture.h`
- `Source/TipRunFishingUE5/Private/Tests/TRSessionTests.cpp`
- `Source/TipRunFishingUE5/Private/UI/TRFishingHUDWidget.cpp`
- `Source/TipRunFishingUE5/Private/UI/TRHUD.cpp`
- `Source/TipRunFishingUE5/Public/Data/TRFishingTuningDataAsset.h`
- `Source/TipRunFishingUE5/Public/Data/TRHUDSnapshot.h`
- `Source/TipRunFishingUE5/Public/Data/TRSnapshots.h`
- `Source/TipRunFishingUE5/Public/Fishing/TREgiSimulationComponent.h`
- `Source/TipRunFishingUE5/Public/Fishing/TRFishingComponent.h`
- `Source/TipRunFishingUE5/Public/Game/TRFishingSessionActor.h`
- `Source/TipRunFishingUE5/Public/Game/TRPlayerController.h`

## 4. 受入シナリオ

### A: 成功する1投

平底・テストイカ1体・初期エギ3.5号35g（試験シンカー0g）で開始 → 竿先付近投入 → 自動FreeFall → 着底 → 3回シャクリ → TensionFall → 過渡処理終了でStay・狙いレンジ維持 → 自然AIでApproach/Attack/Bite → 0.10秒以上0.55秒未満でHook → テンションを休止で下げつつ巻く → Landing → Caughtと固定重量表示 → 次投。3回は試験入力例でありゲームの固定回数/上限ではない。

自然AIシナリオにはseedと最大sim実行時間を記録する。自然乱数が失敗した試行を隠さず、別の制御シナリオでフック成功経路を確実に検証する。成功するseedだけを使った試験を確率バランス評価と呼ばない。

### B: 早い/遅い/無入力

試験用BITEを成立条件を満たして発行 → Early、Late、Expiredを別々に試す → Caution → Cooldown → 再反応可能。60Hzの開始100なら105/106/132/133を検証。MISSではCastIdを保持しStayまたは再Fallを操作できる。早合わせ連打でHITにならず、回収完了で結果が1回発行される。

### C: レンジと再フォール

同じ海で重量・潮・船速度をそれぞれ変更（重量はRetrieve完了・Cast終了・船上帰還後の次投準備で調整）→ 釣合い維持/上昇/下降の深度、角度、RangeError/RangeStability、BITE計算確率を比較。他の確率要素を揃え、維持が最良、上昇/下降が低下することを確認。直前の投の観測を基にエギ/シンカーを調整して次投のレンジ安定化を比較する → StayからFall → 着底 → 再度誘い。旧BITEが復活しない。

10/11/12/20回のシャクリを全て受理し、後続StayでBITE確率倍率を比較する。11回から極端に低下し、回数が増えて回復しない。Fall/Stay切替だけでは減衰が消えず、新たな一連のシャクリ後はその回数に更新される。活性3段階、距離、レンジ差、RangeStability、Exposure、Stay時間を同じにして比較する。

### D: 終了と障害

回収で釣果なし、Bite中断、Fight中断、イカ破棄、船が海域外、Widget再生成、レベル終了、100回再投。結果重複・無効参照・入力保持がない。

ファイトは安全な巻き/休止で成功、過大テンションの連続超過でバラシを独立検証。超過が途切れたら連続時間が0へ戻る。同Tickの成功/バラシ競合ではバラシを優先。バラシ後はStayで再開し回収結果へEscapedを残す技術契約を確認する。

### E: 時間の一貫性

同じTick入力列を描画30/60/120fpsで再生。イベント列と結果が同じ、浮動小数点は試験許容誤差内。Pause/再開とcatch-up上限超過を含む。実際の人間入力の時刻量子化は別途プレイテストする。

## 5. 実装時ログと完了報告

開発ログはCastId、SimTick、StateFrom/To、CommandResult、EgiDepthM、LineAngle、JerkCount/SeriesJerkCount/StayPenaltyJerkCount、JerkBiteMultiplier、DepthVelocityMps/境界接触、RangeErrorM/RangeStability01/RangeHoldScore/RangeHoldBiteMultiplier、SquidId/State/ActivityLevel、RangeScore/Exposure/StayElapsedS、BiteToken、HookReason、FightTension/OverTensionTicks/Progress、Outcomeを追跡できるようにする。毎Tick出力は明示した試験記録時のみ。

各タスクの完了時に「変更内容・実施した試験・結果・未実施の理由・残るD番号」を報告する。ビルドできない環境でコンパイル済みと書かない。設計変更が必要なら対応する文書も同じ変更単位で修正する。

## 6. Alpha以降の段階

0. **Alpha着手前にD10/D11とD12の未指定部分の製品仕様を再決定する。マウス基本操作はM10.5の決定を維持する。** 重量分布、3種Cue/大型アタリ、製品HUD/入力をMVP暫定仕様から無断昇格させない。
1. Rで導入する簡易操船/釣り切替を基に、本格操船/地形探索を個別設計する。BoatSnapshot契約は維持。
2. 保留D14とD15のAlpha拡張（波/実海域の位置別環境場）を設計し、地形Provider、エリア選択等を追加。D15のMVP決定を未決へ戻さない。
3. 実測/経験者レビューでD05/D07を調整し、軽重・潮・レンジの比較記録を残す。
4. Alpha前に再決定したD11/D12に従い3種アタリ、ロッド表現、製品HUDを実装。
5. D09を更新して詳細ファイトと取り込みを追加。
6. D17を決定して装備・SHOP・保存方式/データ移行を設計してから実装。
7. D18を決定してSteam配布・実績・大会を別タスク化。外部連携は釣果イベントの購読側へ置き、釣りロジックの成否を外部サービスに依存させない。

## 7. M10.5 Prototype Realism Revision — 正式品質ゲート

状態（2026-09-17）: **A〜F実装/自動検証完了。Gは最新手動PIEでゲームプレイ品質不合格。Rの設計のみ完了、実装未着手。HおよびM11以降は保留。** 証跡は保持し、第8節のRをH前の正式ゲートとする。

### Codex実装単位

各単位は実装前に差分/依存/試験を確認する。Public/PrivateのパスはすべてSource/TipRunFishingUE5配下を指す。列挙した新型/アセットは計画であり現存を意味しない。

| ID | 小タスク・追加/変更予定 | 依存 | 単独受入 |
|---|---|---|---|
| M10.5-A（完了） | Data/TRSnapshots、TROceanAreaDataAsset、Ocean/TROceanWorldSubsystem。風/表層潮/深度Profile、単位/設定リビジョン/コピー契約、環境純粋試験 | M01〜M03 | R01。Constantと層別場の独立評価、既存無効海/寿命回帰 |
| M10.5-B（完了） | Data/TRBoatTuningDataAsset、Boat/TRBoatDriftComponent/TRBoatPawn。風＋表層潮応答、抗力/慣性、取付Transform | A、M04/M05 | R02。解析応答と同/逆/直交、Boat固定更新回帰 |
| M10.5-C（完了） | Data/TRSnapshots/TRFishingTuningDataAsset、Fishing/TREgiSimulationComponent/TREgiActor、Session接続。WorldPosition正本、需要繰出し、ライン抗力/制約、水平距離、旧値の読取互換 | A/B、M07/M08 | R03/R04/R05。30m着底・27装備・空間レンジの純粋/Session試験、描画座標回帰 |
| M10.5-D（完了） | Fishing/TRRodControlComponent（予定新規）、Data/TRRodTuningDataAsset（予定新規）、TRInputConfigDataAsset、Game/TRPlayerController/TRFishingSessionActor/TRSimulationWorldSubsystem。Axis2Dキュー、RodSnapshot、Pitch/Yaw/あおり、一入力一動作 | B/C、M09/M10 | R06/R11。旧Boolean固定数検証を意味/型別へ移行、入力/寿命/回数回帰 |
| M10.5-E（完了） | Data/TRTypes/TREvents等の該当共通コマンド/イベント型、Fishing/TRFishingComponent/TREgiSimulationComponent、Game/TRFishingSessionActor、操作Tuning。通常解放→Stay、Quick状態/期限 | C/D、M06/M10 | R07/R08/R09のAPI・数値試験合格。2026-09-15依頼を優先: 通常完了はResult→NでReady/解除、Quick完了は直接Ready/解除。GUI部分はF/G/Hへ残す |
| M10.5-F（実装・自動検証完了） | UI/TRFishingHUDWidget/TRHUD、Data/TRHUDSnapshot、Game/TRPlayerController、Session/Fishingの操作可否読取口。日本語ガイド/装備パネル/拒否理由/色分離/カーソルContext | D/E | R09/R10のAPI・UMG・入力試験合格。F 6件＋関連回帰27件成功。解像度/DPI/日本語欠字・実マウスの目視確認と保存移行はG/Hへ残す |
| M10.5-G（品質不合格） | Game/TRGameModeBase、TRSessionConfigDataAsset、Prototype設定/入力/メッシュ参照の明示移行、L_TR_M105_PrototypeをUEで作成。最小船/竿/エギ/ライン/方向表示 | A〜F | 保存後の別プロセス再読込、Levelを開きPlayだけで起動。旧M09設定を無言で新係数扱いしない |
| M10.5-R（設計のみ） | 第8節のMode/Camera/Sequence/Slack/品質改訂 | A〜F、G不合格分析 | R1〜R9自動検証＋ユーザー手動PIE合格（未実施） |
| M10.5-H（保留） | Private/TestsのM10.5 Automation/Functional Test、必要回帰、PIEチェック票/結果をDocsへ記録、調整はPrototype限定 | R9手動合格＋明示依頼 | R01〜R12とR改訂契約、UHT/実C++/Development Editor Win64成功、ユーザーPIE再評価合格 |

A/B/C/D/Eは各段階で反射変更を通常ビルド/UHTと関連Automationで確認する。F/GはUI/アセットの保存・読込と目視を追加する。Hで変更由来の警告を解消し、未解消の既存警告と区別する。既存テスト名だけを通過させず、変えた契約の期待値・理由を記録する。M11のAIや評価計算をテスト用に仮実装しない。

### 試験条件と期待値

以下は再現性を得るための**Prototype/Test受入値**。製品値/実測値ではない。設定リビジョン、風/潮/重量/竿/初期位置、入力Tick列、seed、固定dt、各係数と許容誤差を試験結果へ記録する。期待値を試験対象関数から逆算して作らない。数値調整が必要なら結果を見て合否線を緩めず、条件と改訂理由をレビュー可能にする。

標準落下シナリオ案: 平底30m、初速0、表層〜底まで同じ潮、潮速0.4/0.7/1.0 knot、独立した水平風2m/s（ユーザー未指定の試験値）、風に対して潮が同方向/逆方向/直交。船首は横風を受ける固定方位、初期竿先は海面上1.5m、竿直下海面から投入。27装備組合せ×3潮速×3方向=243ケース。別途無風/無潮、層別流、強い風の防御シナリオを分離し、標準値を実測範囲の断言にしない。

| ID | 試験 | 期待・判定基準 |
|---|---|---|
| R01 環境 | 0.4/0.7/1.0 knot、風/潮独立、同/逆/直交、層別/位置委譲/不正設定 | 換算は1852/3600基準で誤差1e-6m/s以下。例の層0m=(0.2,0,0)、15m=(0,0.4,0)、30m=(-0.2,0,0)なら7.5m=(0.1,0.2,0)、端点も一致。無風と無潮を独立に扱い、NaN/重複深度/Provider破棄を拒否 |
| R02 船 | 独立風＋表層潮、ゼロ/初速/慣性変更、大dt | BOAT式の独立解析解と位置/速度を比較（1e-4m、1e-4m/sの試験許容）。有限慣性で1Tick後が入力速度へ瞬間一致しない。慣性変更は応答時間に寄与、方向合成は入力方向で変化 |
| R03 FreeFall | 標準243ケース、0g含む全装備 | 各ケースで300 sim秒以内に30mへ着底する試験上限。初回着底までLine<80mを要求。深度超過<=1e-3m、D<=L+1e-3m。無条件繰出しなしはD一定/既存余長十分の別試験でdeltaL=0。長さclampで着底不能にして通さない。着底時間・水平移動・相対距離・L/D/Slack/角・船/エギ速度・風/潮/重量を記録 |
| R04 重量/レンジ | 同一形状係数で30/35/40g＋各シンカー、同じ初期世界位置/竿/L/風/潮 | 静水自由ラインで総重量増に応じた沈下曲線の差。別の一定環境・非境界の共通初期幾何で軽量/中間/重量過多を比較し、10 sim秒のStayで軽量は深度0.2m以上減、中間は全観測深度幅0.1m以下、重量過多は0.2m以上増を試験案とする。中間だけ係数/位置/潮を差替えない。適切な試験環境はCで校正・固定しGのPIE比較にも同一設定を使う。全ケース接触なし、レンジスコア/BITE評価は作らない |
| R05 空間/ライン | 船とエギで異なる入力場、深度潮/ライン抗力を独立変更、竿移動、大dt | 非退化条件で船/エギ速度が異なり、水平距離/深度/角度が変化。ライン抗力係数0/正の比較で寄与を分離。D<=L+1e-3m、海面/海底貫通<=1e-3m、NaN/発散なし。1/60、1/30、0.25、1秒の各刻みの安全性を確認（異なる刻み間の厳密一致は要求しない） |
| R06 マウス/竿 | 右1/3/11/20押下・長押し、Axis2D、極値、連続入力、姿勢復帰 | 押下数=実動作数（明示取消分を除外）、保持連打なし。基準Yaw/PitchとTip Transform一致、合成姿勢も範囲内、あおり後は最新マウス基準へ復帰。作用を二重加算しない。Shakuri→TF→Stay、FreeFall/Bottom無入力でAutoStayなし |
| R07 通常回収 | 左保持/解放/再押下、途中停止 | 保持中のみL減少。解放コマンド境界でWorldPosition/Velocity/Lを維持、角は同じ端点から導出。次ステップからStay相当の運動、底ならBottom優先。船直下スナップ/FreeFall繰出しなし。通常完了は一度だけ帰還/Result（ロック維持）、NでReady/解除 |
| R08 Quick | Q、長さ2/30/80/100m、再Q/左解放/F/Hook、Pause/破棄/旧Cast | 全Lで同じDurationTicks、終了1Tick前は未完、終了Tickで一度だけRetrieved/Onboard/Ready。Attack/Bite対象不適格、Hook拒否。途中停止不可、Pause中期限不進行。Session破棄後の完了0件。長いLはD<=Lかつ有効な海面/海底/設定上限を満たす初期状態を直接構成する隔離試験であり標準FreeFallの異常を許可する意味ではない |
| R09 装備 | 初投Ready、投中/回収中/Quick/Pause、両回収後Ready、古いUI要求 | Ready/船上だけ27組合せ変更成功。投中拒否、次Deployで重量/係数/メッシュが新値へ、再ロック。前投結果は旧装備を保持。GUIだけで完了でき、拒否理由を日本語表示 |
| R10 UI/表示 | 日本語、操作案内、ラベル/値色、1280×720/1920×1080・拡大率100/150%、UIクリック | 必須全項目が読める。UIクリックによる回収/シャクリ0件。Snapshot反復取得で時計/位置/キュー不変。水中無効/Quick/船上を0深度で偽装しない。日本語フォント欠落/文字切れなし |
| R11 決定性/寿命 | 同一固定Tick/Sequenceのマウスdeltaと押下列を30/60/120描画fpsで再生、Pause/フォーカス/登録解除 | 状態/回数/終端Tick/装備結果は一致、位置/L/深度差<=1e-3m、角差<=1e-4rad。Pause中不進行、復帰時の保留delta/自動巻取りなし、旧Cast/世代/破棄対象への配送なし。人間入力の時刻差は別のPIE評価 |
| R12 PIE品質 | Gの保存済みLevelで初見操作、落下→複数シャクリ→Stay→再Fall→通常回収停止→Quick→装備変更→再投 | 外部コマンド説明なしで操作ガイドから一連操作が可能。R03/R04の代表条件を画面から選べる検証用導線を持ち、上昇/下降/非境界安定・水平距離・ラインの関係を目視記録。ユーザーの再現性/操作性再評価が合格。未確認は合格に含めない |

R04の安定幅は純粋な試験観測基準で、M11の製品RangeStabilityしきい値ではない。R03の標準比較では竿入力/シャクリ/巻取りなし。R04では初期化直後の測定開始Tickと入力列を固定し、都合の良い瞬間だけを切り取らない。速度・抗力係数の校正はC/Gの成果物として保存し、対照ケースにも同じ値を使用する。

### 既存機能への影響と回帰

- M01/M02: WorldPosition/Rod/環境/Quickコマンドの値契約、SI換算、設定検証と一投コピー。旧プロパティを二重正本にせず、明示アセット移行・再読込を検証する。
- M03: 一定潮評価をField問い合わせへ。海底Flat/無効値/Provider寿命は保持、風0固定/全深度同潮の期待はモード別へ変更。
- M04/M05: 固定順は維持しRodをFishing内へ追加、船へ風/表層潮と慣性。CastId/Sequence/Pause/登録解除/catch-up試験を必要範囲で回帰。
- M07/M08: 鉛直＋XY正本をWorldPositionへ、需要繰出し/ライン抗力/相対幾何へ。着底一度/貫通防止/旧Cast拒否は維持。旧無条件Payoutや速度上書きの数値一致を新仕様の要件にしない。
- M09: Boolean9種固定の検証・キーボード中心・英語Text表示を改訂。入力順/保持解除/UI読取非破壊を回帰し、実マウス/フォーカスとUI導線を追加確認。
- M10: 1入力1動作/過渡完了Stay/再Fall/装備凍結を維持。Retrieve解放と回収後Readyを変更、Quick追加。既存の回数/旧Cast/装備履歴/終端一度/AutoStay不存在を回帰。

### M11への進行条件

A〜H実装完了、設定移行/保存済みLevelの再読込成功、UHT生成・実C++コンパイル・Development Editor Win64成功、R01〜R11と必要回帰成功、M10.5変更由来の警告解消、R12ユーザーPIE再評価合格をすべて満たす。残存の既存警告・製品値未校正・実測未検証を区別して報告する。自動試験だけの合格、装備変更未確認、80m問題のclamp隠蔽では通過不可。今回の設計更新ではいずれの実行結果も新たに確定していない。

### M10.5-A完了記録（2026-09-14）

- A合格。Bへ進める環境API/設定/互換性を確認した。B〜H、M11以降は未着手であり、M10.5全体のPIE品質合格ではない。今回の風は環境問い合わせの値だけで、Boat/Egi/Input/UIの挙動を改訂していない。
- 追加型: ETRCurrentFieldMode、FTRCurrentDepthKey、FTREnvironmentField、TREnvironmentUnits。既存FTROceanAreaSettings/FTROceanSample/UTROceanWorldSubsystemを拡張。UTROceanAreaDataAssetの既存Validate/IsDataValid経路で新項目も検証する。詳細なAPI・revision 1/2の契約はOCEAN_SYSTEM第9節を正本とする。
- A Automation 5件すべて成功: UnitsAndDirections、ProfileAndFrozenSettings、ValidationAndLifetime、LegacyAndSerialization、SpatialDelegationAndFrameRates。0.4/0.7/1.0 knotと逆換算、風/潮同方向・逆方向・直交、無風/無潮の独立性、層の端点/中間/海底clamp/末端、初期化コピー、revision拒否、非有限/不正Profile/Provider破棄、既存保存Prototype読込、revision 2のメモリシリアライズ再読込、位置/深度/Tick評価委譲、固定60Hzを描画30/60/120fpsで駆動した60サンプル列一致、Pause/読取非破壊を確認。
- 回帰はM03全7件、M05全6件、M08全7件の計20件成功。海/船/ラインの共有Snapshot変更と旧設定互換性が対象。旧風非寄与のBoat試験はB未着手のため維持し、新しい風応答の合格と読み替えない。
- 計25件、失敗/未実行0、各試験のerrors/warnings=0、プロセス終了コード0。UE5.8.2、MSVC14.51.36257、Windows SDK10.0.22621.0（Win64 VALID）。UHTは12生成ファイルを書き出し、変更Data/Ocean・新規評価器/試験・モジュールを含む5 C++コンパイル、LIB/DLLリンク、metadataの計8アクション成功。up-to-date確認だけではない。
- A由来の残存警告なし。既存のMSVC推奨版14.50.35717との差、Unreal5_6互換include順序、Engine起動時Condition failed（Error表記）19件、Editorレイアウト警告1件、対象外SDK不足は残る。試験開始前の既存ログと、各試験のエラー/警告0を区別する。通常権限でUBT起動が進まず停止後、実行権限付き通常ビルドで成功した。
- 証跡: Saved/Logs/M105ABuild.log、Saved/Logs/M105ATests.log、Saved/Automation/M105A/index.json（Git除外）。今回Content/Config/Build.cs/Target.csと既存Boat/Fishing/Input/UI実装は変更なし。新Prototype資産の保存移行/LevelはG、船の風応答はBに残す。AではPIE目視/実測校正/Windowsパッケージは実施していない。

変更ファイル（Source内はSource/TipRunFishingUE5配下）:

- Public/Data/TROceanTypes.h
- Public/Data/TRSnapshots.h
- Public/Data/TREnvironmentUnits.h（新規）
- Public/Ocean/TREnvironmentField.h（新規）
- Public/Ocean/TROceanWorldSubsystem.h
- Private/Data/TROceanAreaDataAsset.cpp
- Private/Ocean/TREnvironmentField.cpp（新規）
- Private/Ocean/TROceanWorldSubsystem.cpp
- Private/Tests/TREnvironmentTests.cpp（新規）
- AGENTS.md
- Docs/GAME_DESIGN.md
- Docs/OCEAN_SYSTEM.md
- Docs/ROADMAP.md

### M10.5-B完了記録（2026-09-14）

- B合格。Aの環境APIを変更せず、船体軸ごとに風/表層潮を別評価する応答、方向別受風倍率、慣性/抗力、速度と位置の同一解による固定更新を実装した。HeadingとVelocityは独立。C向けには既存RodTipと拡張Boat Snapshotを供給する。具体式・単位・互換revisionはBOAT_SYSTEM第8節を正本とする。
- ModelRevision=2は明示選択、新係数は未校正のTest値。既存保存Prototypeはrevision 1の旧モデルを維持し、単位の異なる旧係数を暗黙変換しない。Content/Config/Ocean/Fishing/Egi/Input/UIは変更なし。保存資産の移行とPIE再評価はG/Hへ残す。速度上限は新モデルの候補拒否・停止用であり、過剰速度をclampして合格にしない。
- B Automation 5件成功: AnalyticResponseAndContributions、WindCurrentHeadingAndRepresentativeSpeeds、InertiaChangesAndNumericalGuards、ValidationSerializationAndFrozenTuning、FixedFramesRodAndSessionLifetime。R02とB依存範囲の固定更新・寿命・M08接続を確認。3代表潮速はAのknot変換を利用、風/潮同・逆・直交とHeading依存を比較した。
- 回帰26件成功: M10.5-A 5、M03 7、M04 6、M05 6、M08 2（B07FixedBoatIntegration、SessionLifetimeAndDestination）。旧モデルの互換回帰に加え、新Bモデルを実Sessionへ接続し、Boat→Egiの同Tick/ライン長整合を検証した。M08の計算式・期待値は変更していない。
- 最終31件すべて成功、失敗/未実行0、各試験errors/warnings=0、プロセス終了コード0。初回の30成功/1失敗はテストWorld破棄直後のGC前に弱参照無効化を期待していた寿命試験に起因。DestroyWorld後のGCを実行し、Actorへの残存参照が回収を妨げないことを確認するよう修正した。数値の合否線は変更していない。
- UE5.8.2 / Visual Studio 2026 Community / MSVC14.51.36257 / Windows SDK10.0.22621.0（Win64 VALID）。通常UHTは9生成ファイルを書き出し、Boat/Data/新規試験/Moduleの4実C++コンパイル＋LIB/DLL/metadata、計7アクション成功。寿命試験修正後も試験C++実コンパイル＋リンクを成功確認。up-to-date確認のみではない。
- B由来の残存警告0。既存のMSVC推奨版との差、Unreal5_6互換include順序、Engine試験開始前のCondition failed（Error表記）19件、Editorレイアウト警告1件、Win64以外のSDK不足は残る。試験結果内のエラー/警告0とは区別する。
- 証跡（Git除外）: Saved/Logs/M105BBuild.log、M105BBuildFinal.log、M105BTests.log（初回）、M105BTestsFinal.log、Saved/Automation/M105BFinal/index.json。PIE/実測/パッケージ未実施。Cへ進めるBoat側APIは揃ったが、C〜H・M11以降は未着手、M10.5全体品質ゲートとM11進行保留を維持する。
- 変更ファイル: Source/TipRunFishingUE5/Public/Data/TRBoatTuningDataAsset.h、Public/Data/TRSnapshots.h、Private/Data/TRBoatTuningDataAsset.cpp、Private/Boat/TRBoatDriftComponent.cpp、Private/Tests/TRBoatWindTests.cpp（新規）、AGENTS.md、Docs/BOAT_SYSTEM.md、Docs/GAME_DESIGN.md、Docs/ROADMAP.md。

### M10.5-C完了記録（2026-09-14）

- C合格。WorldPositionを唯一の位置正本とし、互換Depth/XYを導出。EgiModelRevision=2はA深度潮、3点の水中ライン抗力、重量別終端沈下への3D応答、Bの移動竿先による拘束、需要繰出しを実装する。既存入力キュー/操作状態遷移は維持。詳細API・単位・設定・数値防御はFISHING_SYSTEM第10節を参照する。
- C試験6件すべて成功: Standard243FreeFalls、WaterWeightsAndDemand、StayRangeBalance、DepthFieldLineDragAndBottom、SafetyAndSnapshotContract、FixedFramesPauseSessionAndVisual。R03/R04/R05のC範囲を確認した。新しいWorldPosition初期化は旧Depth/XYのコピーを要求しない。Snapshot読取/古いCast/Reset済み対象/Session破棄・World GC、Pause、30/60/120fps一致、m→cm描画接続を確認。
- R03: 27装備（30/35/40g＋0gを含む9シンカー）×0.4/0.7/1.0 knot×風に同方向/逆方向/直交=243条件。全条件が300 sim秒以内に30mへ着底、全観測TickでLine<80m、海底貫通なし、D<=L+1e-5m。最大Line=39.299694m、最長50.550秒。上限設定は200mであり80m clampを使っていない。各条件の時間、XY、相対距離、L/D/Slack/Angle、船/エギ速度、潮・重量をAutomationのInfoへ記録した。
- R04: 10秒全観測、同一環境/初期幾何/応答係数で30gのDepth変化=-0.378585m、35g=-0.004354m（全幅0.008663m）、40g=+0.341138m。上昇0.2m以上・中間全幅0.1m以下・下降0.2m以上の事前基準を満たし、全ケースで境界接触なし。制御竿先軌道を使う純粋試験であり、製品の最適重量やBの代表風2m/sからの自動船速を確定したものではない。試験条件はFISHING_SYSTEMに記録、GのPIE環境校正は別途必要。
- R05: 深度Profileで水平応答が反転、エギ地点の潮0でも水中ライン抗力の独立寄与を確認。0.5/3/30mのBottom通知一度、試験callbackによる移動先水深変化で貫通なし。製品の傾斜地形モードは追加していない。1/60・1/30・0.25・1秒刻み、無効海/Inf/NaN設定/竿先ワープ/過大刻み拒否、十分な余長で追加0、繰出し速度不足時の拘束を確認。
- 必要回帰49件成功: A 5、B 5、M03 7、M05 6、M06 5、M07 6、M08 7、位置・描画接続変更に対するM10操作8。旧係数revision 1の保存資産読込と従来挙動を回帰、新revision 2はCの専用試験と実Sessionで検証した。M09/M10入力コード、A/Bの環境/船計算は変更していない。
- 最終55件成功、失敗/未実行0、各試験errors/warnings=0、終了コード0。初回47件も成功。投入直後の導出値、水平距離、世界位置初期化/繰出し上限の追加確認後に最終ビルド・55件を再実行した。
- UE5.8.2 / Visual Studio 2026 Community / MSVC14.51.36257 / Windows SDK10.0.22621.0（Win64 VALID）。UHT実行、初回のData/空間計算含む実C++コンパイル成功。最終UHTは7生成ファイル、7 C++コンパイル＋LIB/DLL/metadataの計10アクション成功。up-to-dateのみではない。
- C由来の残存警告0。既存MSVC推奨版との差、Unreal5_6互換include順序、試験開始前のEngine Condition failed（Error表記）19件、レイアウト警告1件、対象外SDK不足は残る。個々の試験のエラー/警告0とは区別する。
- 証跡（Git除外）: Saved/Logs/M105CBuild.log、M105CBuildTests.log、M105CBuildFinal.log、M105CTests.log、M105CTestsFinal.log、Saved/Automation/M105CFinal/index.json。PIE目視/実測校正/Windowsパッケージは未実施。保存Prototypeはrevision 1の互換運動・旧繰出しで、新モデルのPIEはGの明示資産移行後に再評価する。Cの新モデル試験合格を旧Prototypeの80m問題解消済みと読み替えない。
- Dに渡す空間Snapshotと竿先接続は用意できた。D〜H・M11以降には着手していない。M10.5全体品質ゲートとM11進行保留を維持する。

変更ファイル（Source内はSource/TipRunFishingUE5配下）:

- Public/Data/TRSnapshots.h
- Public/Data/TRFishingTuningDataAsset.h
- Public/Fishing/TREgiSimulationComponent.h
- Private/Data/TRFishingTuningDataAsset.cpp
- Private/Fishing/TREgiSimulationComponent.cpp
- Private/Fishing/TREgiSpatialSimulation.cpp（新規）
- Private/Fishing/TREgiActor.cpp
- Private/Fishing/TRFishingComponent.cpp（投入Snapshotのみ）
- Private/Game/TRFishingSessionActor.cpp（初期竿先の受渡しのみ）
- Private/Tests/TREgiSpatialTests.cpp（新規）
- AGENTS.md
- Docs/FISHING_SYSTEM.md
- Docs/GAME_DESIGN.md
- Docs/ROADMAP.md

### M10.5-D完了記録（2026-09-14）

- D合格。UTRRodControlComponent、UTRRodTuningDataAsset/FTRRodParameters、FTRRodSnapshotを追加。Mouse Axis2Dと右クリックStartedを既存Input Queueへ接続し、Boat→Rod→Cの固定順で動く竿先をライン拘束へ渡す。基準姿勢/一時あおりを分離し、既存Jerkカウント/予約/TF→Stayを維持。Rod有効時は旧Lift/Reelを0にして二重作用を除いた。Eの左マウス/回収解放/Quick/UIは実装していない。
- DataAsset・時間/単位・式・境界の正本はFISHING_SYSTEM第11節、Enhanced Input/設定参照と保存資産移行の範囲はUI_SPECのD追記を参照する。Cの物理実装、A/B環境/船係数、M09/M10の製品UIや回収仕様は変更なし。Session内の既存FishingフェーズへRod更新を追加し、Coordinatorに竿/物理式を置かない。
- D Automation 5件成功: AimQueueClampAndTransform、ClickProfileQueueAndLine、PauseFocusCastAndLifetime、FixedFramesAndSequence、ConfigurationAndEnhancedMapping。XY感度とClamp、凍結設定、船位置/方位/竿長の座標合成、QuaternionとDirection、1/3/11/20クリック=実動作数、保持再送拒否、PendingJerk、全動作のJerking→TF通知、Stay復帰、Bottomからの引上げ、旧JerkReel非寄与（L不変）、Cへの同Tick接続を確認。
- 同TickのAim→Jerk→Aimで開始時Baseと最新Baseを区別し、最新Baseへの復帰と30/60/120fpsの同じ固定入力列のRod/Egi/Line一致を確認。Ready→Deployの旧Cast delta拒否、Pauseの時計停止、Focus後の保持再演なし、未実行予約取消、旧登録/旧Cast拒否、Session/World破棄後のGC回収を検証した。InputConfigのMouse2D/RMB存在とLMB不在、Enhancedバインド導入、パラメータ不正拒否・メモリシリアライズも確認。
- 回帰49件成功: A 5、B 5、C 6（243落下ケースを含む）、M04 6、M06 5、M08 7、M09 7、M10 8。計54件成功、失敗/未実行0、各試験errors/warnings=0、終了コード0。初回54件も成功し、Bottom試験・終了時Snapshot無効化・起動参照検証を補った後、最終ビルド/54件を再確認した。
- UE5.8.2 / Visual Studio 2026 Community / MSVC14.51.36257 / Windows SDK10.0.22621.0（Win64 VALID）。初回UHT20生成ファイル、Snapshot追加時6生成ファイルを確認。初回11 C++コンパイル＋LIB/DLL/metadataの14アクション、最終は5 C++コンパイル＋リンク等8アクション成功。最終UHTも成功（宣言差分なしで生成書換0）。up-to-dateだけの確認ではない。
- D由来の残存警告0。既存MSVC推奨版との差、Unreal5_6互換include順序、起動時Condition failed（Error表記）19件、Editorレイアウト警告1件、対象外SDK不足は残る。各試験内のエラー/警告0と区別する。
- 証跡（Git除外）: Saved/Logs/M105DBuild.log、M105DBuildTests.log、M105DBuildFinal.log、M105DTests.log、M105DTestsFinal.log、Saved/Automation/M105DFinal/index.json。Automationは入力アダプタ/バインド構成/数値・状態を検証したもので、実マウスPIE/操作感/実測/Windowsパッケージは未実施。
- 保存PrototypeはRod未設定のまま。C revision 2とRod/Input設定の明示割当て・保存移行はGへ残す。Eへ渡すRod/Input/Snapshotは揃ったが、E〜H・M11以降は未着手。M10.5全体品質ゲートとM11進行保留を維持する。

変更ファイル（Source内はSource/TipRunFishingUE5配下、計20 Source＋5 Markdown）:

- Public/Data/TRRodTuningDataAsset.h（新規）
- Private/Data/TRRodTuningDataAsset.cpp（新規）
- Public/Fishing/TRRodControlComponent.h（新規）
- Private/Fishing/TRRodControlComponent.cpp（新規）
- Private/Tests/TRRodTests.cpp（新規）
- Public/Data/TRTypes.h
- Public/Data/TREvents.h
- Public/Data/TRSnapshots.h
- Public/Data/TRHUDSnapshot.h
- Public/Data/TRInputConfigDataAsset.h
- Private/Data/TRInputConfigDataAsset.cpp
- Public/Data/TRSessionConfigDataAsset.h
- Private/Data/TRSessionConfigDataAsset.cpp
- Public/Game/TRSimulationWorldSubsystem.h
- Private/Game/TRSimulationWorldSubsystem.cpp
- Public/Game/TRFishingSessionActor.h
- Private/Game/TRFishingSessionActor.cpp
- Public/Game/TRPlayerController.h
- Private/Game/TRPlayerController.cpp
- Private/Game/TRGameModeBase.cpp
- AGENTS.md
- Docs/FISHING_SYSTEM.md
- Docs/UI_SPEC.md
- Docs/GAME_DESIGN.md
- Docs/ROADMAP.md

### M10.5-E完了記録（2026-09-15）

- E合格。中断地点のgit status/diff、実装、ビルド/試験証跡を確認した結果、Eの18 Sourceファイルは既に実装・検証済みだった。再開後はコードを作り直さず、UI_SPECの入力/データ接続説明と本節の完了記録を補完した。再開前後の18ファイルのSHA256一致を確認。
- 通常回収は左保持（Rバックアップ）から固定Input Queueを経由してラインを短縮し、Cの空間拘束でエギを移動する。解放は位置/速度/ラインを引き継ぐStay相当、通常完了はResultで装備ロック維持、NextCastでReady/解除。Quickは独立状態で固定Tickの所要時間を使い、完了時に直接Ready/解除する。詳細契約と調整値はFISHING_SYSTEM第12節、入力資産の接続はUI_SPEC第9節を正本とする。
- 中断前のE修正には、海面付近のライン拘束の丸め誤差許容と、巻取り時の海面交差幾何に応じた実繰取り量の制限を含む。Egiを船へ直接移動する処理や、Cの速度上限を緩める回避策は追加していない。
- E Automation 7件成功: NormalInputReleaseContinuity、NormalCompletionResultLock、QuickDurationRestrictionsAndUnlock、PauseFocusAndDestruction、QuickEntryStatesAndAbort、FixedFramesAndReadOnlySnapshot、ConfigurationAndSafety。保持/解放と再操作、通常Resultロック、Quickの他入力拒否・途中停止不可・一度だけの完了、即装備変更と次投ロック、Pause/Focus/Cast/寿命、30/60/120fps、有限数、設定検証を確認。長さ2/30/80/100mのQuick試験は初期余長を与えた独立条件であり、標準FreeFallの繰出し量ではない。1.5秒はPrototype試験値で製品値ではない。
- 中断前はE 7件＋回帰54件、計61件成功。再開後はE 7件＋直接関連回帰32件（C 6、D 5、M04 6、M09 7、M10 8）、計39件成功、失敗/未実行0、各試験errors/warnings=0、実行終了コード0。未変更のA/Bは中断前の成功記録を保持し、再開後は再実行していない。
- UE5.8.2 / Visual Studio 2026 Community / MSVC14.51.36257 / Windows SDK10.0.22621.0。中断前のUHTは初回16生成ファイル、Snapshot追加時6生成ファイルを確認。再開後もUHTを強制再実行し成功（宣言変更なし、生成書換0）。コード本文を変更せず、E関連9 C++ファイルをNoUbaで実コンパイルし、リンク等を含む12アクションとDevelopment Editor Win64ビルドが成功。up-to-dateだけの確認ではない。
- E由来の残存警告0。既存MSVC推奨版との差、Unreal5_6互換include順序の案内は残る。試験開始前のEngine Condition failed（Error表記）19件とEditorレイアウト警告1件も残り、各試験内のエラー/警告0とは区別する。
- 証跡（Git除外）: Saved/Logs/M105EFinalBuild.log、M105EFinalTests.log、Saved/Automation/M105EFinal/index.json。再開後: Saved/Logs/M105EResumeBuild.log、M105EResumeTests.log、Saved/Automation/M105EResume/index.json。git diff --check成功。
- Fへ渡す回収/装備Snapshotと入力経路は用意できた。保存Prototypeへの設定移行、実マウスPIE、操作感確認は未実施でG/Hへ残す。F〜H・M11以降は未着手、M10.5全体品質ゲート未合格とM11進行保留を維持する。

変更ファイル（Source内はSource/TipRunFishingUE5配下、計18 Source＋5 Markdown。再開後の本文変更はDocs/ROADMAP.mdとDocs/UI_SPEC.mdのみ）:

- Private/Data/TREquipmentData.cpp
- Private/Data/TRFishingTuningDataAsset.cpp
- Private/Data/TRInputConfigDataAsset.cpp
- Private/Data/TRSessionConfigDataAsset.cpp
- Private/Fishing/TREgiSpatialSimulation.cpp
- Private/Fishing/TRFishingComponent.cpp
- Private/Game/TRFishingSessionActor.cpp
- Private/Game/TRPlayerController.cpp
- Private/Tests/TRRetrievalTests.cpp（新規）
- Public/Data/TREvents.h
- Public/Data/TRFishingTuningDataAsset.h
- Public/Data/TRHUDSnapshot.h
- Public/Data/TRInputConfigDataAsset.h
- Public/Data/TRSnapshots.h
- Public/Data/TRTypes.h
- Public/Fishing/TRFishingComponent.h
- Public/Game/TRFishingSessionActor.h
- Public/Game/TRPlayerController.h
- AGENTS.md
- Docs/FISHING_SYSTEM.md
- Docs/GAME_DESIGN.md
- Docs/ROADMAP.md
- Docs/UI_SPEC.md

### M10.5-F完了記録（2026-09-16、実装・自動検証）

- Fは実装・自動検証範囲で合格。日本語ラベル/値の2列HUD、常時操作ガイド、Tableから生成する装備選択、変更不可理由、UI/釣り入力の排他を実装。現行の操作・表示・API契約はUI_SPEC第9節を正本とし、本節には検証と変更一覧を記録する。G/H・M11以降は未着手。
- 中断前のコードを保持し、最終ビルドから再開した。最終確認で追加した実UMG試験が、プレイヤーコンテキストのないWorldでNativeOnInitializedが省略されレイアウト未生成となる問題を検出。RebuildWidgetでも冪等に生成するよう修正し、期待値を維持して再試験した。
- F Automation 6件成功: JapaneseSnapshotAndGuide、EquipmentTableAndCastLock、QuickReadyAndStaleRequests、EnhancedUIInputConflictAndFocus、WidgetSessionLifetime、NativePanelKeys。日本語主要ラベル、実TextBlockの値/色、水平距離/風/潮/重量、状態/ガイド/未割当入力、3×9装備選択と総重量、投中/Result拒否、Quick帰還後の変更・再投入ロックを確認。UI捕捉中のEnhanced押下/保持とUMGボタン要求が釣り操作を発行しないこと、解放後の再操作、Pause/Focus、古いCast/世代/破棄済みSession、Native Enter/Tab/PとEnter repeat拒否を検証した。
- 必須回帰27件成功: D 5、E 7、M09 7、M10 8。再開後の33件ではFのUMG確認1件だけが失敗し、回帰27件は成功。Widget生成修正後はF全6件＋直接影響するM09全7件の13件を再実行し、すべて成功、各試験errors/warnings=0、未実行0、終了コード0。D/E/M10のコードはこの修正で変更しておらず成功結果を保持。未変更のA〜C全試験は今回再実行していない。
- UE5.8.2 / Visual Studio 2026 Community / MSVC14.51.36257 / Windows SDK10.0.22621.0（Win64 VALID）。初回UHTは5生成ファイル、再開後は4生成ファイル、9 C++コンパイル＋リンク等12アクション成功。Widget修正後もUHTを強制実行し成功（反射宣言変更なし、生成書換0）、6 C++コンパイル＋リンク等9アクションとDevelopment Editor Win64が成功。up-to-dateだけの確認ではない。
- F由来の残存警告0。既存のMSVC推奨版差・Unreal5_6互換include順序案内、起動時Condition failed（Error表記）19件、Editorレイアウト警告1件、対象外SDK不足は残る。各Automation内部のエラー/警告0とは区別する。
- 証跡（Git除外）: Saved/Logs/M105FBuild.log、M105FBuildTests.log、M105FFinalBuild.log、M105FFinalTests.log、M105FWidgetBuild.log、M105FWidgetTests.log。レポート: Saved/Automation/M105F/index.json（初回32件成功）、M105FFinal/index.json（失敗を含む診断記録）、M105FWidget/index.json（修正後13件成功）。git diff --check成功。
- Config/Content/保存済みDataAsset/Levelは変更なし。既存HUDクラスの接続を利用するため、新日本語HUDは既存Prototypeで生成されるが、D/E操作の保存設定を移行済みとは扱わない。Gに渡すAPI/UIは用意できた。保存資産移行、解像度/DPI/日本語欠字の目視確認、実マウス操作感、最終PIE評価は未実施。R10の目視部分とR12、M10.5全体品質ゲートは未合格のまま、M11進行保留を維持する。

変更ファイル（Source内はSource/TipRunFishingUE5配下、13 Source＋3 Markdown）:

- Private/Fishing/TRFishingComponent.cpp
- Private/Game/TRFishingSessionActor.cpp
- Private/Game/TRPlayerController.cpp
- Private/Tests/TRInputHUDTests.cpp（既存表示期待を日本語へ更新）
- Private/Tests/TRPrototypeUITests.cpp（新規）
- Private/UI/TRFishingHUDWidget.cpp
- Private/UI/TRHUD.cpp
- Private/UI/TRPrototypePresentation.cpp（新規）
- Public/Data/TRHUDSnapshot.h
- Public/Fishing/TRFishingComponent.h
- Public/Game/TRFishingSessionActor.h
- Public/Game/TRPlayerController.h
- Public/UI/TRFishingHUDWidget.h
- AGENTS.md
- Docs/UI_SPEC.md
- Docs/ROADMAP.md

### M10.5-G進捗（2026-09-16、保存移行・自動検証済み／手動確認待ち）

- 新規8資産: `/Game/TipRun/Prototype/M105/L_TR_M105_Prototype`、同フォルダの`BP_TR_M105GameMode_Prototype`、`Data/DA_TR_M105{Ocean,Boat,Fishing,Rod,Input,Session}_Prototype`。UEのSavePackageを使用。旧5資産はSHA256一致で変更なし。
- Ocean FieldRevision/Boat ModelRevision/Fishing EgiModelRevisionはすべて2。Rod/Input/Quick設定を明示接続し、Tableは旧装備定義を共用。初期条件は海面Z=0m、水深30m、風+Y 2m/s、表層/深度潮+X 0.7knot（0.360111m/s、Constant field）、船首+X、船初期XY=(0,0)m。風と潮は独立し、Ocean資産で方向/深度プロファイルを変更可能。
- 係数はC/D/E試験を基にしたG用のゲーム近似。BoatのWindResponse=.1、CurrentResponse=1、Drag=1kg/s、Inertia=10kg、Bow/Stern/Side=1/.5/2、安全速度1m/s。Rod長2m、Pitch0〜1.2rad/Yaw±.6rad、感度.01/.02rad、Shakuri .3rad・上げ.15秒/戻し.25秒。通常巻取り1m/s、Quick1.5秒、固定更新1/60秒、最大catch-up8、seed20260916。製品値/実測値には昇格しない。その他の調整値は保存DataAssetを正本とする。
- `ATRPrototypeViewActor`を追加し、Snapshot読取だけの観測カメラ/船/竿/ライン/エギ/深度目盛を接続。`ATRHUD`は当観測Actorがある場合のみ右側配置にする。ConfigはEditorStartupMapのみ追加。通常起動手順はUI_SPECのG節。
- UHT実行（新規生成3ファイル）・変更コードの実C++・Development Editor Win64成功。初回コンパイルの単位変換ヘッダー不足、Level保存のRF_Standalone不足は修正済み。作成済みDataAssetを再生成せずLevel作成だけを再開した。
- 保存後の別プロセスでG 2件（SavedAssetReferences / SavedSetupInputAndEquipmentSmoke）成功。D 5件＋E 7件＋F 6件の関連回帰18件成功。計20件、試験内エラー/警告0。保存参照、revision、専用GameMode/Controller/HUD、入力注入、Quick→Ready、Table選択/次Deployロックを確認。A〜C全統合回帰は依頼どおりHに残す。
- 証跡（生成物・Git対象外）: `Saved/Logs/M105GBuild.log`（UHT）、`M105GBuildFinal.log`/`M105GMapBuild.log`（実コンパイル成功）、`Saved/Automation/M105GRegression/index.json`（20件成功）、`Saved/M105GLegacyHashes.json`（旧資産照合）。ビルドに既知のMSVC非推奨版/旧include順通知あり。試験起動時の既知Engine診断は試験内の警告と区別する。
- 保存MapがEditorStartupMapから開くことは確認済み。PIE操作と1920×1080/2560×1440の視認性/パネル操作確認はユーザーの手動結果待ち。Gはまだ最終合格としない。HおよびM11以降には着手していない。

### GのPIE可視化/UI修正（2026-09-16、手動再確認待ち）

- ユーザー手動結果を受理: 日本語HUD、装備変更/ロック、FreeFall、過大ラインなし、Mouse値、1クリック1Shakuri、Stay、通常回収/解放、再Fall、Quick→Ready/Unlockは正常。一方、3D黒画面、竿/ライン/エギの観測不能、HUD過大、常時装備パネル、revision不明は不合格。初回Automation成功を可視化の合格証拠にしない。
- Gだけ修正。`TRPrototypeViewActor`は一時Debug描画から、Collisionなしの永続StaticMesh Componentへ変更。船/竿/竿先/ライン/エギを色分けし、海面枠・30m海底・背景を追加。新規`/Game/TipRun/Prototype/M105/M_TR_Observation_Prototype`（Unlit/Colorパラメータ）をUEで作成し、保存Levelへ明示参照。近距離Perspectiveカメラと毎フレームのViewTarget確認でGameMode接続後も観測カメラを維持する。旧方式の黒画面原因すべてを実PIEで特定できたとは主張しない。
- RodTip/WorldPositionから表示Transformだけを導出。Simulation/海/船/エギ/ラインの数値コード・Tuning値は変更なし。表示Meshの太さ・色・カメラはPrototype専用。既存G資産8件のうちLevelのみ変更、材質1資産追加。旧revision1資産は維持。
- `TRHUD`/`TRFishingHUDWidget`/`TRPrototypePresentation`をコンパクト化。主要11項目、短い7操作、F1で詳細/Debugキーを折りたたむ。右上幅最大420・通常高さ560 Slate単位（DPI/画面幅上限あり）。Env/Boat/Egi revision欄を常設、実値が2以外なら警告。船上Egiは投入設定と明記。
- `TRPlayerController`のReady/Result自動パネル表示を廃止。初期Closed、Tab/閉じるで開閉、Quick後もClosed。既存入力遮断/保持解除・Session装備APIは維持。新規F1は表示だけを切替。Fの旧自動表示期待を今回の明示仕様に合わせて改訂し、Tab操作を試験へ追加した。
- UHTは4ファイルを生成。初回の表示コードの型エラー/浮動小数リテラル警告を修正後、実C++/Development Editor Win64成功。証跡: `Saved/Logs/M105GVisualBuild.log`（UHT）、`M105GVisualBuild2.log`（成功）。残存は既知の非推奨MSVC 14.51/旧include順通知。G由来のC++警告なし。
- UE保存移行単独1件成功後、別プロセスでG 3件＋D 5/E 7/F 6/M09 7の関連回帰25件、計28件成功。各試験エラー/警告0。`Saved/Automation/M105GVisualRegression/index.json`。追加検証は保存Mesh/材質参照、Snapshot→表示位置/Line端点の整合、Shakuri中の表示Tip移動、描画が時計を進めないこと、初期Closed/Tab、主要表示件数、F1による実UMG行の折りたたみ、revision不一致警告。A〜C全物理回帰/Hは未実施。
- 保存移行は`-TRMigrateM105GView`を付けた`TipRun.M105G.SavedAssetReferences`のみがContentを書込む。通常試験は読取。新規環境で8資産から生成する場合は既存生成フラグに同移行フラグを併用する。保存済みの正しいDataAssetを作り直さない。
- 自動PIEで強制終了するとのユーザー申告を受け、自動Editor入力を停止。保存LevelのEditor読込までは確認したが、修正後の3D視認・マウス動作・1920×1080配置は未確認。手順/正常基準はUI_SPEC末尾。Gの合格は保留、手動結果を得てから判定し、H・M11以降には進まない。

### G追加修正・Shakuri interaction revision（2026-09-17）

- ユーザー手動正常報告: 背景/海面/海底、Boat/Rod/Line/Egi、Mouse Rod、Tab/F1、Rev2、FreeFall、Normal/Quick Retrieve。これらを保持。追加不合格はHUD過大、狭い竿範囲、観測視点/固定基準不足、連続Shakuriの作用低下。
- コードと比較試験で弛み蓄積を確認。巻取りなし5連続の上昇量は約1.536/0.788/0.277/0.049/0m。旧試験は一連で一度の上昇だけを確認していた。通常回収とは独立したReturn同期Pulseを導入し、毎回のライン拘束を検査。現行5連続は約2.313/1.606/2.000/2.000/2.110m（静水・35g・試験初期条件、製品性能値ではない）。ライン短縮は海面制約なしで約2m/Action。2/3回も全Actionで作用し、保持連打なし。
- 最初の試験用0.6m/Actionでは5回目が弱く、上げ中も巻く1.2m/Actionでは毎回の拘束が成立しなかった。受入条件を緩めず、戻し中の短い巻取りへ変更。詳細はFISHING_SYSTEM第13節。海面で旧Locked長が竿先高さを満たせない境界も修正し、既存ReelIn最小長/海面円制約を共用する。新しいEgi直接速度注入なし。
- 表示専用Camera Look、固定グリッド/ブイ、船首/船室とエギ形状、8行/短いガイドのHUD。操作と仮値はUI_SPEC末尾を正本とする。正常revision詳細はF1、不一致警告は通常時も表示する。SimulationはCamera/Gridから変更されない。
- UE保存手段でRod DataAssetと保存Levelを明示更新（`-TRMigrateM105GInteraction`）。旧5資産はSHA256一致を確認。通常のAutomationはContentを書き換えない。既存GameMode/Input/Environment/Boat/Fishing/Equipment参照を維持。
- ビルド成功: `Saved/Logs/M105GInteractionBuild.log`でUHT生成5ファイルと実C++、最終変更は`M105GInteractionFinalBuild.log`で実C++コンパイル/リンク成功。UE5.8.2、Development Editor Win64。途中の試験参照名のコンパイルエラーは修正済み。
- 最終の一意な試験42件が成功: G 6（既存3拡張＋新規3）、D 5、E 7、F 6、M09 7、M10 8、Cの水/需要繰出しと安全Snapshot 2、M04キュー1。Hの全物理統合回帰は実施していない。2/3/5連続の各回の拘束/上昇/ライン短縮、旧Lift/Reel値を極端に変えても結果不変、海面/浅場/底、30/60/120fps、Pause/Focus、通常回収とのSequence競合を確認。
- 証跡: `Saved/Automation/M105GInteractionRegression/index.json`のF以外36件成功。Fでは2件不一致を検出し、詳細値の確認をF1へ移し、保持試験の左右Actionを同じ入力フレームへ注入するよう修正（別フレームでは他方の擬似Releaseを発生させていた）。`Saved/Automation/M105GInteractionUIFinal/index.json`でF全6件成功。合格済み試験内のエラー/警告0。UIクリック遮断の期待値は維持した。
- 残存は既知の非推奨MSVC/旧include順通知とEditor起動時のCondition failed 19件・Layout通知。Gの試験内エラーと混同しない。`git diff --check`成功。修正後PIE/1920×1080の目視は未実施、ユーザー手動再評価が必要。コード/自動検証は合格、G全体の最終合否は保留。H・M11以降未着手。

今回の主な変更ファイル（以前のG差分も保持）:

- `Data/TRRodTuningDataAsset.h/.cpp`、`Fishing/TRRodControlComponent.h/.cpp`、`Fishing/TRFishingComponent.cpp`、`Fishing/TREgiSpatialSimulation.cpp`、`Game/TRFishingSessionActor.cpp`。
- `Game/TRPlayerController.h/.cpp`、`Game/TRPrototypeViewActor.h/.cpp`、`UI/TRFishingHUDWidget.h/.cpp`、`UI/TRPrototypePresentation.cpp`、`UI/TRHUD.cpp`。
- `Tests/TRRodTests.cpp`、`Tests/TRPrototypeSetupTests.cpp`、`Tests/TRPrototypeUITests.cpp`。
- `Content/TipRun/Prototype/M105/Data/DA_TR_M105Rod_Prototype.uasset`、`Content/TipRun/Prototype/M105/L_TR_M105_Prototype.umap`。
- `AGENTS.md`、`Docs/GAME_DESIGN.md`、`Docs/FISHING_SYSTEM.md`、`Docs/UI_SPEC.md`、`Docs/ROADMAP.md`。Configは今回追加変更なし。

## 8. M10.5-R Gameplay Architecture Revision — H前の再実装ゲート

2026-09-17: **Gはゲームプレイ品質不合格**。A〜Fの基盤とGの保存資産/自動検証結果は保持するが、最新手動PIEの不自然な視点/釣り座、操船ループ欠如、速いドリフト、連続Shakuri不安定/過剰回収、視認品質、F1異常を正式な不合格理由とする。旧「G手動結果待ち」は履歴。今回は設計文書のみ更新し、以下のR実装は全て未着手。H/M11も未着手。

### 小タスクと依存関係

以下の型名は予定名。各行を独立依頼単位とし、既存正しいコードを作り直さない。Sourceパスは`Source/TipRunFishingUE5`配下のPublic/Privateを指す。

|単位|依存|対象・変更予定|Automationの受入/必要回帰|手動確認・停止境界|
|---|---|---|---|---|
|R1 Mode architecture|A〜F|Game/TRPlayerModeComponent新設、TRFishingSessionActor/TRGameModeBase、DataのMode要求/Snapshot|Navigation/Fishing、ModeEpoch、同Tick順、古いCast/Session、Pause、投中退出拒否、Normal Result/Quick Ready。M04/M06/E寿命回帰|モードと拒否理由の確認。推進・Camera・Sequenceはまだ追加しない|
|R2a Navigation motion|R1/B|Boat/TRBoatNavigationComponent新設、TRBoatDriftComponent、Boat Tuning、固定配送|単一積分、推力/舵、慣性、Heading/Velocity分離、釣り移行で推進0/速度連続、無効環境。B/M04該当回帰|推進/操舵を記録で確認。三人称品質はR2b待ち|
|R2b Navigation camera/input|R2a|Game/TRPlayerCameraManager新設、TRPlayerController、Input/Camera設定、TRPrototypeViewActorのViewTarget所有解除|Mouse LookがBoat正本を書かない、Input Context排他、Focus/Pause、30/60/120fps。D/M09入力回帰|三人称で移動・船首選択が自然。Fishing視点はR3bへ|
|R3a Fishing stations|R1/R2a/C/D|Data/TRFishingStationDataAsset新設、Boat/Rod/Session設定接続|左右舷の位置/方位、船移動後Eye/Mount/Tip整合、投中舷変更拒否、選択参照不備。CのTip接続/D回帰|左右舷識別。カメラ自然さはR3bへ|
|R3b First-person camera|R2b/R3a|CameraManager、Camera Tuning、Rod/Reel表示参照|選択舷Eye、角度制限、表示のみ補間、ViewTarget競合なし|Rod/Reel/海面中心、真上真下なし。入力仕上げはR4|
|R4 Fishing input/aim|R3b|Controller、Input設定、RodControl、Modeキュー接続|Mouse基準Aim/Camera読取、Snap余裕、UI遮断、保持解除、Cast/ModeEpoch/30-120fps。D/E/M09/F関連回帰|通常釣りにShift不要。右/左操作維持。Sequenceは旧挙動と明示|
|R5 Shakuri Sequence|R4|Fishing/TRShakuriSequenceComponent新設、TRFishingComponent/TRRodControlComponent、Snapshot|1/2/3/5要求、Repeat拒否、Recoverと予約順、終了時だけTF、無AutoStay、割込/寿命。D/M10/E関連回帰|状態管理まで。R6前に保存Prototypeへ新Sequenceを有効化して物理合格としない|
|R6 Slack-aware Reel|R5/C|TREgiSpatialSimulationのライン要求、Sequence接続、Rod/Fishing Tuning|全Jerkの因果対照試験、必要余長だけ回収、5回過剰回収なし、拘束/境界/有限数、30-120fps、左との競合。C該当/D/E/M10回帰|間隔違いの1〜5回すべて作用。固定2mや直接速度注入で代用不可|
|R7 Drift measurement/tuning|R2a/R3a/B|Prototype Boat設定、測定Snapshot/試験、必要な表示|0.4/.7/1 knot、風/潮方向別、10/30/60秒変位、停止/航行後比較、決定性。B関連回帰|固定基準に対する体感評価。式を無条件に置換せず係数調整を記録|
|R8a F1/HUD fix|F/G、独立着手可|Controller/Widget/TRHUD/TRPrototypePresentation、必要ならConfig/DefaultInput.iniの限定除外|実入力経路1押下1toggle、UI/Pause/Close、ViewMode等不変、20往復、HUD値。F/M09回帰|紫表示なし/閉じられる実キー確認。Engine設定編集禁止|
|R8b Visual/readability|R3b/R4/R8a|表示Actor、Prototype Mesh/Material/Level、モード別HUD|Snapshot→表示、釣り座/Rod/Reel/Line参照、Asset load、UI/Input非干渉|塗りの船/左右舷/海中/固定基準、1080p/1440p。物理値を表示都合で変更しない|
|R9 Integration / PIE gate|R1〜R8全て|保存R Prototypeの明示参照移行、GameMode/Input/Session接続、統合試験と記録|保存後別プロセスload、全モード往復、投入〜回収〜装備〜再投入、全Jerk、古い世代/破棄、関連機能統合|下記全項目をユーザー手動確認。Hの全統合試験を先取りしない|

推奨主順序はR1→R2a→R2b→R3a→R3b→R4→R5→R6→R7→R8b→R9。R8aは独立して早期実施でき、R7は依存成立後に別単位で実施できる。複数単位を暗黙に一括実装しない。R5/R6の純粋な状態試験は可能だが、片方だけで新Shakuriの実プレイ合格とはしない。

### 共通の品質境界

- 各実装単位は着手前差分確認→必要なコード/設定だけ変更→新規試験＋直接影響する回帰→文書へ証跡。C++変更時はUHT・変更コードの実コンパイル・Development Editor Win64を行う。Contentのみでも保存/別プロセス読込と参照検証、常にgit diff --checkを行う。
- A〜Fを毎回全部再実行しない。正本の環境/船/エギ契約に変更があれば該当試験を追加。A〜Cを含む全物理統合回帰はHに残す。R9はRで変えたゲームループの統合を省略しない。
- 新しい設定はPrototype/Test値として管理。旧資産の意図しない上書きを避け、R用コピーまたは明示移行を選び参照を検証する。Environment/Boat/Egiのrevision 2をRという理由だけで3へ変更しない。Rod旧固定Pulseとの互換/新設定選択を明示し、保存移行をR9で確認する。
- Sequenceの詳細式/調整項目/試験案はFISHING_SYSTEM第14節、船の測定はBOAT_SYSTEM第9節、UIの操作/目視手順はUI_SPEC末尾が正本。テスト値を製品値へ昇格しない。
- Gで成功した旧固定Pulseの試験は履歴として保存。Rの仕様変更による置換理由と新受入を明記し、テストを通すためだけに期待値を緩めない。操作感はAutomation成功だけで合格にしない。

### 最終ユーザー手動PIEゲート

|確認|合格条件|
|---|---|
|Navigation|海域を移動しポイントと船首を決められる|
|三人称|Mouse周囲確認と操船が競合せず自然|
|Fishing Side|左右舷を選べ、竿元/目線が選択側にある|
|一人称|Rod/Reel/海面が自然。Shift観測を要求しない|
|1〜5回Shakuri|毎回Rod→Line→Egi作用。Held自動連打なし|
|過剰回収|5回程度で船上へ回収されず、各回の必要Slackだけ巻く|
|TF/Stay|Sequence終了後に自然に移行。AutoStayなし|
|ドリフト|Headingと方向を分離。10/30/60秒変位と体感が許容される|
|視認品質|Boat/左右舷/Rod/Reel/Line/Egi/水面/水中/海底/固定基準を識別|
|HUD/F1|1080p/1440pで視界を塞がず、紫表示なし/詳細を閉じられる|
|既存ループ|Normal解放、Re-Fall、Quick Ready、装備変更/再Lockが維持|
|寿命/切替|UI/Focus/Pause/Mode変更で残留入力なし。Cast中操船拒否|

ビルド識別・保存Map/設定・環境・入力間隔・左右舷・解像度と結果を記録する。自動PIEは再実行せずユーザーが手動評価する。不合格はRの該当単位へ戻す。**R自動検証＋全手動合格の明示報告＋次の明示依頼**がH開始条件。R合格だけでM11へ進まない。今回の設計更新をR実装完了やPIE合格と記録しない。
