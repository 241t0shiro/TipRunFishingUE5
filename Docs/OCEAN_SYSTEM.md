# 海・水深・地形・潮流技術設計

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

## 4. 潮・季節・天候

MVP決定(D15): 一定水平潮、固定海面。CurrentMps.z=0。風・波の物理影響は対象外。MVPのAreaDataは非ゼロの鉛直潮を検証エラーにし、深度/時刻により水平潮を変化させない。WindMpsは契約互換用の0出力だけを残し、風設定DataAssetを要求しない。

将来の層別潮: 深度キーと流速を持つ配列へ差し替え、間を線形補間する。実装はAlpha以降。季節・天候シミュレーションの所有者は将来のGame側環境サービスとする。D07に従い季節補正はMVPのAIで使用せず、SeasonIdはNoneでよい。

波のメッシュ上下をSurfaceZへ反映しない。将来波が釣りへ影響する場合は、見た目の法線・波高とは別にゲーム用海面変位と周波数を定義してから導入する。船が読む潮は海面、エギとイカが読む潮は各深度である。

## 5. ライフサイクル・Blueprint

`ETROceanState`: Uninitialized → Ready → Unloaded。設定不正ならError。Ready以外のSampleはbValid=false。

InitializeAreaは設定/Provider検証後にReadyへ遷移。エリア変更は活動中の投をGame側で終了させてからUnload→Initialize。古いAreaData・Providerへの弱参照を解除する。MVPにエリアロード画面は不要。

DataAsset項目はクラス表の通り。BlueprintReadOnly: 現在AreaId、海面、状態。BlueprintCallable: `SampleOceanForDisplay()` の読み取りラッパー。Blueprintから流速や水深の正本を書き換えない。Providerの平底/斜面設定はAreaData由来で初期化し、Actor配置値との二重設定を避ける。

## 6. テスト

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
