# R6 Slack-aware Reel / Effective Retrieve

2026-10-08。対象はM10.5-R6のみ。ユーザーがR5のAutomation・最終手動PIEを正式合格と確認。R6の実装/自動受入は成功、今回のGPU描画・OS実入力による手動PIEは未実施。R7以降/H/M11は未着手。

## 実装の正本

Requested/Actual/Slack/Taut/Unrealized/Netの正式定義、処理順、予算/幾何とSnapshot時点はFISHING_SYSTEM末尾のR6現行契約を参照。`TRLineGeometry` がR5C spanを共有し、`TRReelResolver` が端点を変更せず配分とLineを返す。ShakuriのTick距離要求とNormalのReelMps×dtが同じ経路を通る。

Reel処理はrevision 2の既存Spatial Substep内に限定。1/60秒以下へ分割、有限数/位置/速度/海面/海底/Line拘束の既存防御を維持。Taut予算は既存MaxEgiSpeedからRodTip速度と候補Egi速度を予約した残り。Slackの短縮をEgi牽引速度へ加算せず、Taut分だけを既存拘束の速度整合に使用する。Safety threshold、Egi係数、Rod/Camera/Envelope/FOV/感度、環境、名目0.8m、全DataAsset/Config/Contentは変更していない。旧revision 1のEgi積分は変更しない。

Line増加、Reel短縮、最終ネット変化、float上向き公開丸めを分離。Geometryで巻けない部分は未実現として記録し、後続Tickへ強制繰越しない。高度なDrag/slip/リール物理は未実装のゲーム近似。Netが増加でもそのTickでReel実績が正になる場合がある。

## Runtime構成・新規試験

保存 `/Game/TipRun/Prototype/M105/L_TR_M105_Prototype` とGameMode/Session/Input/Rod/Environment設定をロード。実Game World、GameInstance、LocalPlayer、Viewport、PlayerController、CameraManager、Session、Fixed Queue、Sequence、Rod、Egi、Snapshot、PrototypeViewActor/実Meshへ通常World Tickを通す既存fixtureを使用。Rod solverやEgi端点の手動適用、試験のための保存Asset上書きは行わない。

R6新規は4試験、Runtime 26保存World条件＋Resolver Unit。

| 試験 | 条件・受入 |
| --- | --- |
| Runtime.SlackTautAndPartial | 両舷×rich/near-taut/partial=6条件。実Mouse/右クリック、実Mesh一致、固定Root/2m、Actual配分、正常継続 |
| Runtime.NormalReleaseRefallAndQuick | 両舷2条件。左保持1秒、Release後要求0、F payout分離、Q固定回収→Ready/Unlock、Root/Base/2m/Camera不変、Insert読取副作用なし |
| Runtime.OffscreenStressAndFixedTickAccounting | 両舷×10/20/50回×30/60/120fps=18条件。旧R5Cと同じ投入/視線/入力Tick列。Publish登録の読取専用観測者で全成功Spatial Tickの配分/予算/Line収支を検査。Deploy初期化Tick（Reel未評価）は対象外 |
| Unit.GeometryAndAllocation | Slack2m・要求0.8・端点予算0で全量Slack、taut予算0.2で未実現0.6、R5C海面交差span、Hold要求0 |

全TickでActual=Slack+Taut、Actual<=Requested、Requested=Actual+Unrealized、Taut<=既存予算、Net=Payout+Geometry−Actual+PublicationDeltaを検査。配分等式1e−9m、累積等式/収支1e−8m、拘束は既存1e−5m、実Mesh端点誤差<1e−7m。固定入力列/状態イベントと全Reel累積/最終Lineは3fpsで完全一致、Egi位置は1e−8m以内。

## 0.8m Actionの結果

richの準備は保存TableのEgi_4＋Sinker_50をReadyで選択、実MouseでBase34°→−9°、投入/既存Action完了による実際の戻し弛みを使用。調整値やLineへの注入なし。R5保存profileのUpDemandFraction=0を保持し、Recoverで要求を発行する。

| 条件 | 舷 | 開始弛み / 最初のDemand時弛み m | Slack消費 m | Taut m | Actual m | Unrealized m |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| rich（対象の2回目Action） | Port | 1.770001 / 1.221436 | 0.800000 | 0 | 0.800000 | 丸め約8e−15 |
| rich（対象の2回目Action） | Starboard | 1.778360 / 1.232081 | 0.800000 | 0 | 0.800000 | 丸め約8e−15 |
| near-taut | Port | 0.100000 / 0 | 0.571637 | 0.016096 | 0.587733 | 0.212267 |
| near-taut | Starboard | 0.100000 / 0 | 0.571327 | 0.016263 | 0.587589 | 0.212411 |
| partial（高いBase/短Line） | Port | 0.100000 / 0 | 0.420674 | 0.037432 | 0.458105 | 0.341895 |
| partial（高いBase/短Line） | Starboard | 0.100000 / 0 | 0.415519 | 0.037331 | 0.452850 | 0.347150 |

richのログ累積1.6mは準備Actionも含む2回分であり、対象1Actionの結果は表の0.8m。Near-tautは最初のReel要求時にSlack0、その後Rod Recoverによる弛みも同じAction内で生じる。Partialは巻けない量を未実現として残し、Jerk受付可能な状態を維持。全条件Technical Abort0、不自然な追加端点ワープなし（位置差速度は既存上限内）。

Normal保持の1秒要求1mは両舷ともSlack0.546685539＋Taut0.453314461=Actual1m、Unrealizedは丸め約3.54e−15。既存速度DataAssetを変更していない。Release後の新規Demandは0、位置/Lineを引き継ぐStay。Re-FallはPayout>0/Actual0、Qは共有Reelを追加実行せずReady/Unlockへ戻る。

## 10/20/50 Stress・旧50回の再評価

下表は30/60/120fpsすべて同一（m）。全入力数/完了数正確、最終Queue0/Temporary0/Final==Base、固定Grip/Base/2m、Technical Abort0/永久Lock0/NaN/Inf0、最終Depth0。

| 舷 | 回数 | Requested | Slack消費 | Taut | Actual | Unrealized | Net Line変化 | 最終Line |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Port | 10 | 8 | 7.565202219 | 0.113169062 | 7.678371281 | 0.321628719 | +0.020248055 | 1.528769016 |
| Port | 20 | 16 | 13.284076150 | 0.272854645 | 13.556930795 | 2.443069205 | +0.015403032 | 1.523923993 |
| Port | 50 | 40 | 30.438672891 | 0.753056532 | 31.191729423 | 8.808270577 | +0.007954836 | 1.516475797 |
| Starboard | 10 | 8 | 7.568453856 | 0.097789783 | 7.666243638 | 0.333756362 | +0.021893859 | 1.530414820 |
| Starboard | 20 | 16 | 13.280132312 | 0.257839911 | 13.537972223 | 2.462027777 | +0.019433260 | 1.527954221 |
| Starboard | 50 | 40 | 30.415524492 | 0.735005436 | 31.150529928 | 8.849470072 | +0.011156201 | 1.519677162 |

50回の最終RequiredはPort1.51647570287m、Starboard1.51967716142m。累積Payoutは両舷7.1169742956m、Geometry増加は24.0825092072m / 24.0444975242m。NetにReel以外の増減が含まれるため「約31mだけエギが浮上/Lineがネット短縮」と解釈しない。

旧R5CのActual31.1360685825 / 31.139349699mに対し、R6の定義では31.1917294227 / 31.1505299283m（差+0.0556608402 / +0.0111802293m）。以前は各Tickの正のLine開始−終了（必要span増加を相殺した値）をSequenceへ反映していた。現在は幾何増加を分離したスプール適用実績。またSlack分を牽引速度へ足さないこと、Taut残量の速度予算、短縮候補の海面評価、保守的float公開により軌跡/幾何増加自体も変わる。定義の変更だけによる差と断定しない。係数/環境/Sequenceは保持。

## 関連回帰・警告・採用元

最終採用126件成功（新規4＋関連122）、errors0/FAIL0/notRun0。単一126件の一括成功ではなく、`Saved/Automation/R6Final/index.json` の非R6 122件と `R6Verified/index.json` の新R6 4件を試験別に採用。採用一覧は `Saved/Automation/R6AcceptedResults.json`。

R5の8件（1/2/3/5・Hold・Up/Recover予約・名目0.8m・最新Base・Pending・Pause/Focus/UI/寿命）、R5A4件、R5B4件、R5C4件、R4A1〜A5、R3/R4/D/E/F/G/M04/M09/M10を回帰。共有Spatialへ変更したためC/M07/M08も追加。無関係なA/B全環境・船試験やR7/R9統合は未実施。

Normal42は全42正常Result、Technical Abort0/Active0。Intentional NaN→Aborted→N→Ready、古いCast/登録世代拒否も成功。Intentional異常の安全停止は成功回収へ変更していない。

採用試験内警告6件はEngine外部HTTP generate_204の3秒timeout（A5 Boundary/Sweep、R5C Replay/Cadence/Repeated/Surface）。新R6試験4件は警告0。既存MSVC推奨版差/IncludeOrder通知、起動時非Win64 SDK/Python通知は残る。R6由来コンパイル警告なし。

## 途中FAILと修正

R6Firstは新Runtime3件FAIL。Port連打の正常Actionが海面へ達する短縮候補を、短縮前だけで判定していたため安全停止。ProspectiveLineを用いてR5C海面条件を評価し、既存閾値を緩めず解決した。Near/richの準備状態、Quickの未取得Camera/古いEgi状態参照も試験fixtureを実Controller/実Stateへ修正。

R6Second/Thirdではrichの「入力前に弛み>=0.8」の準備が未成立。R6Finalは関連122成功だがR6 2FAIL（rich準備と、Deploy初期化Tickを成功Spatial Tickと混同した観測）。R6FixtureFinalではrichの最初のDemandだけを0.8以上と仮定したが、要求はRecoverに分配されるため未成立。期待を下げず、既存Actionの戻しで弛み1.77m以上を作る実Runtime準備へ変更し、対象ActionでSlack0.8/Taut0を検証した。観測者はReel.Tick==現在Tickの成功Spatialだけを対象とする。最終R6Verifiedは全4件成功。途中FAILログ/レポートは削除していない。

## Build / リポジトリ保全

UE5.8.2 / MSVC14.51.36257 / Windows SDK10.0.22621.0。R6Build1はInternal UHT（WarningsAsErrors）で10生成ファイルを書き出し、新反射型と変更Runtime/試験を実C++/Link。R6Build4は最終Production Spatialと試験の実コンパイル、R6RichBuildは最終Runtime fixtureを実コンパイル/LinkしDevelopment Editor Win64成功。up-to-dateだけを根拠にしていない。

開始時Git clean。今回Source11ファイル＋Markdown6ファイル、Config/Contentの差分/追加/削除0。保存Prototypeを通常ロードし、Asset保存/移行はしていない。git diff --check成功。自動GUI PIEは起動せず、NullRHI Runtime Integrationと手動評価を区別する。

変更ファイル:

- Public/Data/TRReelTypes.h（新規enum/Snapshot）
- Public/Data/TRSnapshots.h（Egi Reel）
- Public/Data/TRShakuriSequenceTypes.h（配分/名目未実現）
- Public/Fishing/TRShakuriSequenceComponent.h
- Private/Fishing/TRReelResolver.h（新規共有Geometry/Resolver）
- Private/Fishing/TREgiSpatialSimulation.cpp
- Private/Fishing/TRFishingComponent.cpp
- Private/Fishing/TRShakuriSequenceComponent.cpp
- Private/Game/TRFishingSessionActor.cpp
- Private/Tests/TRReelResolverTests.cpp（新規Unit）
- Private/Tests/TRRuntimeRodIntegrationTests.cpp（新規Runtimeのみ。旧試験の期待値は保持）
- AGENTS.md、Docs/GAME_DESIGN.md、Docs/FISHING_SYSTEM.md、Docs/UI_SPEC.md、Docs/ROADMAP.md、本記録

## 手動PIE（最大5項目）

1. Editor再起動→保存Prototype→PIE→Enter/AまたはD/Enter→投入。右1/2/3/5クリックと右Hold。入力数どおりAction、最後だけTF/Stay、Insertで名目0.8m/回・Actual<=Requested・配分を確認。両舷で実施。
2. 深度約5mから下/左右を見てRodを画面外へ出す。10/20/50連打後もN不要、Queue/Temporary0、最新Base復帰、Line/速度/Failure正常。ActualとNetが異なってよいことを確認。
3. 左保持→Release→再保持。要求/実績SourceがNormalになり、Release後新要求0、位置/Line連続、Holdが勝手に再開しないこと。F再FallはPayoutのみ。
4. Shakuri途中の左/F Pending、Pause/Focus/Tabを確認。復帰後の誤連打/保持回収なし、技術Abortなし。
5. Q→Ready/Unlock→装備変更→次投入。Quickが長時間の通常巻取りへ変わらず、Root/Base/2m/Cameraが保持されること。弛み/張り/未実現値がInsertで追えること。

技術的なR6実装・自動受入は合格。実OS入力/GPU表示/操作感のR6正式受入はユーザー手動確認待ち。R7へ渡す基盤はあるが今回着手せず、次の明示依頼に従う。
