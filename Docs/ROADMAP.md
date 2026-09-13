# ロードマップ・Codex向けMVP実装順序

関連: [全体正本と要決定事項](GAME_DESIGN.md)、[実装規約](../AGENTS.md)。**M00〜M07は完了済み。M08〜M18は未着手。** 工数・発売日・担当者は未決。

更新: v0.2 / 2026-09-12。MVP決定済みD01〜D09/D13/D15を前提とする。D03は同日の補足「回数上限なし、10回超で後続StayのBITE確率を極端に低下」を優先。D10〜D12はMVP暫定仕様を使い製品仕様をAlpha前に再決定、D14/D16〜D18はMVP非ブロック。

## 1. フェーズ

| フェーズ | 実装範囲 | 完了条件 | 持ち込まないもの |
|---|---|---|---|
| MVP | 仮船/一定水平潮/1投、上限なしシャクリと10回超減衰、自動Stay、3段階活性AI、0.10〜0.55秒受付、テンション/進捗ファイト、固定重量結果 | Windowsで自然な1投と境界・バラシ・回収試験が再現できる | SHOP、自由操船、風/波の物理、季節補正、高度描画、Steam実績 |
| Alpha | 自由操船、ポイント探索、地形差、複数エリア、潮の差、ロッドCue改善、実測調整 | エリア選択から釣果まで反復して遊べる | 未決の経済・大会を先行実装しない |
| 将来版 | 季節/天候、SHOP/装備、詳細ファイト/取り込み、セーブ、Steam配布/実績、大会 | 各仕様決定後の個別受入基準 | オンラインや大会形式を推測で追加しない |

拡張点は契約を維持して差し替える。Ocean Provider→地形、Boatの推進成分→操船、装備Snapshot→SHOP、OnCastCompleted→保存/実績。MVPから空のショップ/大会/実績サービスを用意する必要はない。

## 2. 実装開始時の前提

1. `AGENTS.md` と7設計書を読む。最初の実装タスクを選び、関連D番号の状態を確認する。
2. 対象リポジトリが `TipRunFishingUE5` であることを確認する。UE 5.8.2 / C++プロジェクトは作成済みであり、既存構成を維持する。
3. ローカルUE 5.8.2、Windows用コンパイラ/SDK、プロジェクト生成・ビルド手段を確認。導入不足は不足内容を報告する。
4. MVP決定と使用承認済みのMVP暫定仕様は再承認を求めない。未指定の倍率/係数は調整項目として区別し、試験例は `Prototype/Test` アセットへ隔離する。D14/D16〜D18の保留でMVPを止めない。

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
| 09 M08 | 水平潮応答、ライン長/球面制約、船追従 | M07、D02/D05 | F04/F05/B07。重量・潮・船を一変数ずつ比較 |
| 10 M09 | Enhanced Input接続と最小デバッグHUD（状態/深度/装備） | M06/07、D12 | U02、入力が各1回、数値とSnapshot一致 |
| 11 M10 | 連続シャクリと一連回数、0.8秒AutoStay、再Fall、回収 | M08/09、D03/D04/D13 | F06/F07/F08/F17/F18、上限なし、回数保持/新しい一連リセット |
| 12 M11 | テストイカ1体、3段階活性、距離/レンジ/Exposure/STAY時間 | M10、D01/D07 | S01/S13/S14、季節データ不要 |
| 13 M12 | Attack/Bite、10回超減衰、Caution/Cooldown、仲裁、Hook最小API | M11、D03/D07 | S02〜S04/S07/S08/S15〜S17。非StayのBITEゼロ、回数に応じ強い減衰 |
| 14 M13 | Hook受付初期0.10/0.55秒、早/適正/遅判定、仮Cue1種 | M12、D06、D11暫定 | S05/S06/S09/S11/S12、S18のCue部分。HIT/MISS重複なし |
| 15 M14 | Fightテンション/進捗、継続超過バラシ、対象解放、Landing、固定重量 | M13、D09、D10暫定 | F10/F13/F14/S18、安全な巻きとバラシ、二重結果なし |
| 16 M15 | 内部HUD、Result/次投、MISS継続、回収終了、入力解除/破棄 | M14、D13、D12暫定 | F09/F11/F15/U01/U03/U04/U06/U09〜U12 |
| 17 M16 | 斜面シナリオ、無効環境、開発用試験シナリオ/ログ | M15 | O02/O07/O08/B06/F12 |
| 18 M17 | 自然AIでの1投統合、制御入力による各結末のFunctional Test | M16 | 下記MVP受入シナリオをすべて実行 |
| 19 M18 | Windows Developmentパッケージ、起動/入力/結果/再投確認 | M17 | Editor非依存の起動、30/60/120fps比較、ログに致命的エラーなし |

AutoStayはM10の必須項目。D14の境界ゲーム仕様はM16に含めず、無効サンプルへの技術防御だけ検証する。Steam SDK・配信アップロードはM18に含まない。既存プロジェクトへ適用する場合はM00を環境/構成確認に読み替え、既存コードを作り直さない。

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
- NullRHIのUnrealEditor-Cmdで `TipRun.M01` を実行。Units、Identifiers、TimeConversion、Reflectionの4件すべて成功、各試験の警告・エラー0件、プロセス終了コード0。100cm/1m、ID無効値・Token同一性、0.10/0.55/0.8秒→6/33/48Tick、丸め境界・不正値・オーバーフロー、反射登録・Blueprint読取専用を確認。
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

## 4. 受入シナリオ

### A: 成功する1投

平底・テストイカ1体・初期エギ3.5号35g（試験シンカー0g）で開始 → 竿先付近投入 → 自動FreeFall → 着底 → 3回シャクリ → TensionFall → 無入力0.8秒でAutoStay → 自然AIでApproach/Attack/Bite → 0.10秒以上0.55秒未満でHook → テンションを休止で下げつつ巻く → Landing → Caughtと固定重量表示 → 次投。3回は試験入力例でありゲームの固定回数/上限ではない。

自然AIシナリオにはseedと最大sim実行時間を記録する。自然乱数が失敗した試行を隠さず、別の制御シナリオでフック成功経路を確実に検証する。成功するseedだけを使った試験を確率バランス評価と呼ばない。

### B: 早い/遅い/無入力

試験用BITEを成立条件を満たして発行 → Early、Late、Expiredを別々に試す → Caution → Cooldown → 再反応可能。60Hzの開始100なら105/106/132/133を検証。MISSではCastIdを保持しStayまたは再Fallを操作できる。早合わせ連打でHITにならず、回収完了で結果が1回発行される。

### C: レンジと再フォール

同じ海で重量・潮・船速度をそれぞれ変更 → 深度、角度、好適レンジ滞在時間を比較 → StayからFall → 着底 → 再度誘い。旧BITEが復活しない。

10/11/12/20回のシャクリを全て受理し、後続StayでBITE確率倍率を比較する。11回から極端に低下し、回数が増えて回復しない。Fall/Stay切替だけでは減衰が消えず、新たな一連のシャクリ後はその回数に更新される。活性3段階、距離、レンジ差、Stay時間を同じにして比較する。

### D: 終了と障害

回収で釣果なし、Bite中断、Fight中断、イカ破棄、船が海域外、Widget再生成、レベル終了、100回再投。結果重複・無効参照・入力保持がない。

ファイトは安全な巻き/休止で成功、過大テンションの連続超過でバラシを独立検証。超過が途切れたら連続時間が0へ戻る。同Tickの成功/バラシ競合ではバラシを優先。バラシ後はStayで再開し回収結果へEscapedを残す技術契約を確認する。

### E: 時間の一貫性

同じTick入力列を描画30/60/120fpsで再生。イベント列と結果が同じ、浮動小数点は試験許容誤差内。Pause/再開とcatch-up上限超過を含む。実際の人間入力の時刻量子化は別途プレイテストする。

## 5. 実装時ログと完了報告

開発ログはCastId、SimTick、StateFrom/To、CommandResult、EgiDepthM、LineAngle、JerkCount/SeriesJerkCount/StayPenaltyJerkCount、JerkBiteMultiplier、AutoStayDeadline、SquidId/State/ActivityLevel、RangeScore/Exposure/StayElapsedS、BiteToken、HookReason、FightTension/OverTensionTicks/Progress、Outcomeを追跡できるようにする。毎Tick出力は明示した試験記録時のみ。

各タスクの完了時に「変更内容・実施した試験・結果・未実施の理由・残るD番号」を報告する。ビルドできない環境でコンパイル済みと書かない。設計変更が必要なら対応する文書も同じ変更単位で修正する。

## 6. Alpha以降の段階

0. **Alpha着手前にD10〜D12の製品仕様を再決定する。** 重量分布、3種Cue/大型アタリ、製品HUD/入力をMVP暫定仕様から無断昇格させない。
1. D16を決定して操船/釣り切替と自由移動を実装。BoatSnapshot契約は維持。
2. 保留D14とD15のAlpha拡張（風/波/深度別潮）を設計し、地形Provider、エリア選択等を追加。D15のMVP決定を未決へ戻さない。
3. 実測/経験者レビューでD05/D07を調整し、軽重・潮・レンジの比較記録を残す。
4. Alpha前に再決定したD11/D12に従い3種アタリ、ロッド表現、製品HUDを実装。
5. D09を更新して詳細ファイトと取り込みを追加。
6. D17を決定して装備・SHOP・保存方式/データ移行を設計してから実装。
7. D18を決定してSteam配布・実績・大会を別タスク化。外部連携は釣果イベントの購読側へ置き、釣りロジックの成否を外部サービスに依存させない。
