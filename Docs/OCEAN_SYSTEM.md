# 海・水深・地形・潮流技術設計

2026-09-17 M10.5-R設計改訂: A〜F基盤は保持。最新手動PIEでGはゲームプレイ品質不合格。Rは設計/実装分割のみ完了し、実装未着手。最新契約は本書末尾のM10.5-R節を優先。以前のG合否保留・固定Pulse・観測カメラ等は履歴。R自動検証とユーザー手動合格後もHへ自動進行しない。H/M11以降は保留。

2026-09-14 M10.5-A完了: 環境の風/表層潮/深度別潮、位置依存評価口、knot換算を実装。UHT生成・C++実コンパイル・Development Editor Win64成功、A 5件＋M03/M05/M08回帰20件成功、各試験エラー/警告0。B〜HおよびM11以降は未着手。M10.5全体のPIE品質ゲートは未合格。下の2026-09-13設計のみの記録は履歴。

2026-09-13 M10.5設計改訂（実装未着手）: M00〜M10の実装・自動試験成功は履歴として保持するが、ユーザーのM10後PIE評価は再現性・操作性の品質不合格。M11への進行はM10.5品質ゲート合格まで保留する。本書のM10.5改訂契約を旧記述より優先し、M03〜M10完了記録は旧実装の証跡として読む。今回はMarkdownのみ更新し、改訂機能の実装・ビルド・試験は行っていない。

関連: [全体正本](GAME_DESIGN.md)、[船](BOAT_SYSTEM.md)、[釣り](FISHING_SYSTEM.md)。描画の海と判定用の海を分離する。

更新: v0.2 / 2026-09-12。D15はMVP決定、D14はMVP非対象・非ブロック。無効サンプルの防御は維持し、岸/根掛かり等のゲームルールには拡張しない。

## 1. 責務とクラス

| クラス / 継承元 | 責務・主要Component | 主要変数 | 主要関数 |
|---|---|---|---|
| `UTROceanWorldSubsystem : UWorldSubsystem` | 現在エリアの環境を問い合わせる世界単位サービス。Componentなし | AreaData:TObjectPtr、EnvironmentTimeS:double、OceanState、Provider弱参照 | `InitializeArea()`、`SetSimulationTime()`、`SampleOcean(Query) -> FTROceanSample`、`ShutdownArea()` |
| `ATRSeabedProviderActor : AActor` | 海底の解析式を提供。SceneRoot、任意の仮Mesh | SeabedMode、FlatDepthM、SlopeXY、OriginXYM | `SampleBottomDepth(XY) -> FTRDepthSample`、`InitializeFromArea()` |
| `UTROceanAreaDataAsset : UDataAsset` | エリアの範囲・海面・海底・環境条件 | AreaId、BoundsMin/MaxXYM、SurfaceZ_M、Seabed設定、CurrentMps:FVector | `IsDataValid()` |

SubsystemはGame/PIEで使用する。独立したTickは持たず、Coordinatorから時刻を渡す。SeabedProviderはMVPの平底/斜面を扱い、将来高さグリッドや専用traceへ中身を交換する。UE WaterプラグインをMVPの依存にしない。

依存はDataとUE基礎機構のみ。船・エギ・イカから呼ばれるが、それらを検索したり動かしたりしない。レベル配置したProviderはGameModeから明示登録し、GetAllActorsOfClassを毎Tick実行しない。

## 2. 問い合わせ契約

`FTROceanQuery`: PositionXYM:FVector2D、DepthM:float、SimTick:int64。深度は海面から下向き正。世界座標cmから変換して呼ぶ。

`FTROceanSample`:

| フィールド | 型・単位 | 意味 |
|---|---|---|
| bValid | bool | 環境が利用可能か |
| InvalidReason | ETRSampleError | NotInitialized / OutsideArea / InvalidDepth / MissingProvider / InvalidQuery（非有限XY・負のTick）。有効時はNone |
| AreaId / SeasonId | FName | エリア識別と将来用季節識別。MVPのSeasonIdはNone、参照不要 |
| SampleTick | int64 | 問い合わせたsim時刻 |
| SurfaceZ_M | float、m | ワールド基準の海面Z |
| BottomDepthM | float、m | 当地の海面から海底まで、正 |
| BottomNormal | FVector、正規化 | 海底法線、+Z寄り |
| CurrentMps | FVector、m/s | 世界軸の潮速度、+Z上 |
| WindMps | FVector2D、m/s | 将来用。MVP出力は常に0、物理計算で使用しない |

同Tick・同位置・同設定なら同結果を返す。海底より深い試行位置を補正できるよう、有限な非負DepthMは海底を超えていても水深情報を返す。流速評価の深度だけ0..BottomDepthへclampする。負値/NaNはInvalidDepth。海面Zを求めたいだけの船はDepthM=0で問い合わせる。

無効サンプルの数値は使わない。OutsideAreaで深度0を返して「着底」と誤判定させない。境界上は内側扱い（Min<=XY<=Max）とし、それ以外はOutsideArea。複数エリアを同時に重ねる仕様はMVPではない。

## 3. 水深と海底

`ETRSeabedMode`: Flat / PlaneSlope。MVPの通常シーンはFlat、試験はPlaneSlopeも使用する。

`BottomDepthM(x,y) = FlatDepthM + SlopeX*(x-OriginX) + SlopeY*(y-OriginY)`

Slopeはm/m。定義域全体で水深が正になることをデータ検証する。海底ワールドZは `SurfaceZ_M-BottomDepthM`、法線は `normalize(SlopeX,SlopeY,1)`。深さ増加方向と海底Z下降方向の符号を取り違えない。

描画用海底メッシュはこの値を元に置き、衝突meshを第二の水深正本にしない。将来Landscape/高さマップを採用するときはProviderのみ変更し、問い合わせAPIは維持する。毎エギ毎Tickの複雑な物理traceは、必要性を測ってから導入する。

## 4. M03までの潮・季節・天候（M10.5で風/深度別潮の制限を置換）

MVP決定(D15): 一定水平潮、固定海面。CurrentMps.z=0。風・波の物理影響は対象外。MVPのAreaDataは非ゼロの鉛直潮を検証エラーにし、深度/時刻により水平潮を変化させない。WindMpsは契約互換用の0出力だけを残し、風設定DataAssetを要求しない。

将来の層別潮: 深度キーと流速を持つ配列へ差し替え、間を線形補間する。実装はAlpha以降。季節・天候シミュレーションの所有者は将来のGame側環境サービスとする。D07に従い季節補正はMVPのAIで使用せず、SeasonIdはNoneでよい。

波のメッシュ上下をSurfaceZへ反映しない。将来波が釣りへ影響する場合は、見た目の法線・波高とは別にゲーム用海面変位と周波数を定義してから導入する。船が読む潮は海面、エギとイカが読む潮は各深度である。

## 5. ライフサイクル・Blueprint

`ETROceanState`: Uninitialized → Ready → Unloaded。設定不正ならError。Ready以外のSampleはbValid=false。

InitializeAreaは設定/Provider検証後にReadyへ遷移。エリア変更は活動中の投をGame側で終了させてからUnload→Initialize。古いAreaData・Providerへの弱参照を解除する。MVPにエリアロード画面は不要。

DataAsset項目はクラス表の通り。BlueprintReadOnly: 現在AreaId、海面、状態。BlueprintCallable: `SampleOceanForDisplay()` の読み取りラッパー。Blueprintから流速や水深の正本を書き換えない。Providerの平底/斜面設定はAreaData由来で初期化し、Actor配置値との二重設定を避ける。

## 6. M03までのテスト（風0固定の期待はM10.5で置換）

| ID | 入力 | 期待 |
|---|---|---|
| O01 | 平底30mの複数XY | すべて30m（テスト用値） |
| O02 | SlopeX=0.1、OriginからXを10m増やす | 深度+1m、法線の符号一致 |
| O03 | 深度が海底より大きい | 水深情報は有効、流速評価深度は海底でclamp |
| O04 | 境界上/微小量外側 | 前者有効、後者OutsideArea |
| O05 | 初期化前/Provider破棄後 | bValid=false、偽の着底なし |
| O06 | 100cm↔1m、MVP設定の鉛直潮 | 換算一回、Current.z=0を要求、非ゼロ鉛直潮を設定検証で拒否 |
| O07 | 同Tick同入力を繰返す | 同一Sample、乱数使用なし |
| O08 | エリアUnload/再初期化 | 古いエリア値を参照しない |
| O09 | 深度/時刻を変更して同じMVP AreaをSample | 水平潮一定、WindMps=0、SeasonId=None、風/季節アセットなしでReady |

海底表示と判定面の一致はデバッグ断面/深度ラベルで目視する。リアリティ評価に用いる地形・潮の実データの入手元は未決であり、MVP試験データを実在エリアとして表示しない。

## 7. M03実装・検証記録（実装2026-09-12 / 最終確認2026-09-13）

- 実装済み: `UTROceanAreaDataAsset`、`ATRSeabedProviderActor`、`UTROceanWorldSubsystem`。M03はFlatのみ。第3節のPlaneSlopeと斜面の表示・試験はM16で追加する。現時点のETRSeabedModeはFlatだけを定義し、それ以外の値は検証で拒否する。
- AreaDataの値は`FTROceanAreaSettings Settings`にまとめ、AreaId、XY範囲、固定海面、FlatDepthM、一定水平潮を保持する。未設定深度0は検証不合格。範囲は各軸min < max、問い合わせは境界を含む。有限数・正の水深・海底ワールドZの有限性・水平潮Z=0をIsDataValidでも検証する。
- 初期化でProviderとSubsystemに設定を値コピーし、Editorによる元アセットの編集が問い合わせ結果を変えないようにした。SubsystemはAreaDataを所有参照、同じWorldのProviderを弱参照で保持し、明示登録する。Provider再設定時は構成リビジョンが変わり、古い登録からの問い合わせをMissingProviderで拒否する。Shutdown/Deinitializeで参照・設定・環境時刻を解除する。
- ProviderはSceneRootだけを持ち、ActorのTransformや描画・衝突から水深を算出しない。両クラスとも独立Tickなし。SetSimulationTimeは外部から渡された有限・非負の秒を保持するだけで、固定時計・入力キュー・CoordinatorはM04に残す。
- SampleOceanは初期化前・破棄済みProvider・境界外・不正深度・非有限XY・負のTickを明示的に無効化する。海底より深い有限の非負深度も問い合わせ可能。MVPの潮は評価深度に依存せず、底を超えた問い合わせでも海底位置での潮と同じ一定値を返す。WindMps=0、SeasonId=None。SampleOceanForDisplayは同じ結果を返す読取専用ラッパー。
- NullRHIのAutomationでO01/O03/O04/O05/O06と設定検証・World種別・設定コピー・時刻非依存の7件が成功。初回O05のテストWorld context警告に対し登録・解除を追加し再ビルド後、2026-09-13に全7件を再実行して成功・警告0・エラー0・終了コード0を確認した。30m等は一時的なTestデータで、Content・Configやマップは追加/変更していない。描画と海底面の目視比較、斜面、船や釣りとの接続は未実施。

## 8. M10.5の風・表層潮・深度別潮契約（A実装済み）

D15のユーザー改訂により、第2/4節の「WindMps常に0」「深度に依存しない」は旧一定場の履歴となる。海面/海底/有効性/寿命の契約は維持する。

| 問い合わせ | 入力 | 出力と利用者 |
|---|---|---|
| SampleWindAtLocation相当 | 世界XYm、SimTick | WindVelocityMps（水平方向）、有効性。Boatへ供給 |
| CurrentAtLocationAndDepth相当（SampleOceanから委譲可） | 世界XYm、DepthM、SimTick | CurrentMps（水平方向）、有効性。Boatは深度0、Egiは自位置、ラインは水中サンプル位置で使用 |
| SurfaceCurrent | 同じ位置の深度0問い合わせ | 別の速度設定を持たず、深度潮Fieldの表層評価から導出 |
| SampleOcean | 既存FTROceanQuery | 海面/海底/接触用データ、当該深度潮、必要な風。各値の位置とTickを明示 |

WindVector/CurrentVectorは流れて行く方向を表す。内部正本は速度ベクトルm/sとし、Speedはその大きさ、Directionはゼロ速度時に方向なしとして導出する。設定UIが方向＋速度を受ける場合は検証後に一度だけベクトルへ変換し、速度を二重に掛けない。気象学的な「風が吹いて来る方位」を表示する際は180度反転し、HUDに「風：流れる向き」等の基準を明示する。

M10.5の最小FieldはConstantとDepthProfile。DepthProfileは深度mの昇順キーと水平流速ベクトルで表層/中層/底潮を表す。深度0キー必須、深度重複/非有限/負値を拒否。キー間はベクトル成分を線形補間（角度の単純補間はしない）、範囲外は端値。評価深度のみ0〜当地水深へ制限し、海底超過の試行位置も海底情報を返す既存契約を保持する。一定風0は正常、非ゼロ風も正常。風設定未移行と意図的な無風を区別する設定リビジョンを持ち、旧アセット移行を明示する。

Wind FieldとCurrent Fieldの評価責務は別にし、XY/深度/Tickを省略しない。M10.5の通常FieldはXY方向に一様でよいが、試験用評価器で異なるXYに異なる値を返し、呼出側が現在位置を渡していることを確認する。将来の地形/岸/岬/湾による場は評価器/データの拡張で扱い、Boat/Fishingへ地形判定を埋め込まない。流体ソルバーや複数海域UIは作らない。

DataAssetは既存UTROceanAreaDataAssetを拡張し、設定を初期化時にコピー。必要な値型はDataに置く。海域外、Provider破棄、設定不正はbValid=falseのまま、無効風/潮を都合よく0へ置き換えない。海底Flatは維持し、斜面/実地形は既存の後続範囲。

試験: R01の単位換算、同/逆/直交風潮、層境界/中間点/末端、同Tick同位置の一致、独立したゼロ風/ゼロ潮、無効値/Provider寿命、異なるXYへの委譲を検証。旧O09の一定性はConstantモードに限定し、風0固定を共通不変条件とする試験は置換する。

## 9. M10.5-A 環境実装（2026-09-14、合格）

Aのみ実装。Bの船風応答、Cの世界位置/ライン、D以降の操作/UI、M11以降は未着手。風をOceanが返せることと船が風で流れることを区別する。

- FTROceanAreaSettingsにFieldRevision、CurrentMode（Constant/DepthProfile）、WindMps、CurrentDepthProfileを追加。設定全体を初期化時にコピーする。FTRCurrentDepthKeyは深度mと水平速度m/s。先頭深度0必須、昇順/重複なし、有限数、非負深度、水平流と速度の二乗ノルムが有限であることをRuntime/IsDataValidで検証する。Profileモードでは旧CurrentMpsは使用せず、表層値はProfileの深度0を参照する。
- FieldRevision=1は旧パッケージ互換の明示状態（Constant・無風・Profileなし）。新しい風/Profileを設定するには2を明示選択する。1へ新項目を混在させると検証エラー。既存保存済みM09 Prototypeは1として読み込み、値を勝手に再校正しない。2での無風は意図的な設定として区別する。未知revision/modeを拒否する。
- 既存SampleOcean/InitializeAreaのシグネチャは維持。FTROceanSample.CurrentMpsは従来と同じ呼出側の深度で評価した潮、追加SurfaceCurrentMpsは同じXY/Tickで深度0の潮。WindMpsは独立風、FieldRevisionは読込契約の識別。SeasonIdは引き続きNone。
- SampleCurrentAtLocationAndDepth(Query)、SampleSurfaceCurrent(XY,Tick)、SampleWindAtLocation(XY,Tick)をnative読取APIとして追加。戻り値は既存FTROceanSampleで、有効性/エラー/AreaId/Tickを省略しない。いずれも環境全体の有効性を検証するため、無効な潮または風は有効な一部サンプルとして返さない。Blueprintは既存SampleOceanForDisplayから追加値を読む。
- FTREnvironmentFieldのWindAtLocationとCurrentAtLocationAndDepthを別のconst仮想評価口にする。標準は一様風と一定/層別潮。InitializeAreaWithFieldはrevision 2限定で、不変のnative評価器を共有所有し、Shutdownで解放する。評価器はActor/UObjectを保持せず、設定変更・時計進行・副作用を持たない契約。将来の位置別場のためXY/Depth/Tickを渡し、試験ではXYとTickで変化する評価器を使用する。実海域Fieldや風の物理は追加しない。
- Profileは成分線形補間し、末端を延長。海底超過の試行深度は従来どおり有効に扱い、潮の評価深度だけ海底へ制限する。海域外/Provider破棄/不正問い合わせを既存理由で拒否。評価器が非有限または鉛直流を返す場合もbValid=false（InvalidQuery）であり、NaNを有効値として公開しない。
- TREnvironmentUnits::TryKnotsToMps/TryMpsToKnotsは1852/3600で双方向換算する。符号付き成分を許可し、NaN/Infinity/結果overflowはfalse、出力引数は維持。代表値は0.4→0.205777778、0.7→0.360111111、1.0→0.514444444 m/s。製品バランスの設定ではない。

Aの5試験とM03/M05/M08回帰20件は成功、各試験エラー/警告0。詳細はROADMAPのA完了記録を参照。Content保存移行はGへ残し、Aでは既存保存アセットの互換読込とrevision 2のメモリ上シリアライズ/再読込を試験する。

## M10.5-Rからの利用（2026-09-17、設計のみ）

AのEnvironmentField、Wind/SurfaceCurrent/CurrentAtDepthと単位契約は変更しない。Navigationは船の現在XYで環境を問い合わせる。ポイント確認用の深度潮を表示する場合は評価深度を明示し、存在しないエギの実測潮と混同しない。FishingではCのEgi WorldPositionから深度と潮を評価する。

現行Prototypeは平坦約30mであり、Rのポイント探索は移動位置・環境・船首を選ぶ操作から始める。位置依存問い合わせは維持するが、地形差/海底生成/CFDをRで追加する意味ではない。無効Sampleでは推進/更新を安全停止して異常を通知し、水深0として続行しない。表示用海面/海底Meshやグリッドから環境値へ逆流させない。環境実装の全面改造・A全試験の毎回再実行は要求せず、問い合わせ契約変更時に該当回帰を追加する。
