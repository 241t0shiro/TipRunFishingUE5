# TipRunFishingUE5 — Codex実装規約

2026-10-05 R5C: 手動の約5m→10連打→水面で下/左右操作による停止を、実保存Runtimeで両舷6条件のTechnical Abort/Result/RecoveryAvailableとして再現。A4海面必要span整合のReelIn限定を撤去しHold等でも評価。閾値/係数/Camera/環境/0.8mとAbort/N契約は保持。UHT7生成・実C++/Development Editor Win64成功、最終採用150件成功（新規4＋関連146）、外部HTTP警告6。R5正式不合格・修正後手動PIE待ち。R6以降/R7/H/M11未着手。証跡/途中FAIL/最小3項目は[R5C記録](Docs/R5C_OFFSCREEN_REPEATED_SHAKURI.md)。以下は履歴。

2026-10-05 R5B: R5A手動PIEはCamera Down時のMouse操作不合格。保存RuntimeでViewport Clamp由来の別軸移動を修正前FAILとして再現（Shakuri yawは0）。Mouseをunbounded投影＋後方/無効投影時の接平面Fallbackへ改訂し、Base Envelope/Action縦平面/Root/Length2m/Sequence/0.8mを保持。UHT6生成・実C++/Development Editor Win64成功、最終採用91件（新規4＋関連87）成功、正常42条件Technical Abort0、外部HTTP警告3。Config/Content20ファイルhash不変。R5は正式不合格・修正後手動PIE待ち、R6以降/R7/H/M11未着手。[R5B記録](Docs/R5B_CAMERA_DOWN_CONTROL.md)を現行証跡として優先。以下は履歴。

2026-10-04 R5A: R5最終手動PIEのCamera Down不具合により正式合格保留。Camera投影でTemporaryを作る旧経路とBase EnvelopeによるAction抑制を撤去し、Station-local Base＋Temporary elevationと独立Action安全上限へ改訂。Root/Base/Length2m、Sequence/名目0.8m、A4安全契約は維持。UHT3生成・実C++/Development Editor Win64成功、最終採用87件（新規4＋関連83）成功、外部HTTP警告3。修正後の手動PIE待ち。R6以降/R7/H/M11未着手。現行契約と証跡は[R5A記録](Docs/R5A_CAMERA_INDEPENDENCE.md)。以下は各時点の履歴。

2026-10-04 R5実装・自動受入完了: ユーザーがR4をAutomation・Runtime Integration・最終手動PIEまで正式合格と確認。R5はUp/Recover予約Sequence＋最大1turn/Action・名目0.8mの限定巻取り要求を保存Rodへ明示適用。固定Root/Base/Length2m、A4 Pending/技術復旧を維持。UHT生成・実C++/Development Editor Win64成功、最終採用101件（新規8＋関連93）成功・errors0、外部HTTP警告4。今回R5手動PIE未実施。R6以降/R7/H/M11は未着手。現行契約はFISHING_SYSTEM末尾、証跡/限界/変更一覧は[R5実施記録](Docs/R5_SHAKURI_SEQUENCE.md)。以下のR4不合格/手動待ち記録は各時点の履歴。

2026-10-04 R4A-5: 固定Station-local Root/BaseDirection/2mを維持し、保存Rodへopt-inのErgonomic Envelope（yaw±40/pitch-10〜35度）を追加。Camera/Grip/FOV80/感度/Action係数は保持。保存実Runtime受入は新規5＋関連88、採用93件成功（外部HTTP警告1件）、正常42条件はTechnical Abort0。投影長は同一A1列で比100.67→1.92257、広域720姿勢で435.991〜1382.100px（比3.17002）。UHT生成・実C++/Development Editor Win64成功。表示端点最大差約5.03e-15m。手動6項目未実施、R4全体は正式不合格を維持。証跡/限界/境界挙動/手順はDocs/R4A5_RUNTIME_ACCEPTANCE.md。R5以降/H/M11未着手。以下は履歴。

2026-10-04 R4A-4: 同一42条件の8Abortと舷変更後3Abortを、Shakuri上昇中の海面/短Line交差円の不整合へ分類。必要幾何長の整合、既存profileのBase復帰までRetrieve/FallをPending、技術Abort後の明示N復旧を実装。安全閾値/係数/Config/Contentは保持、LastResult=Abortedを保持。最終採用は新規3件＋関連109件成功、試験内警告0。同一42条件は28回収/14操作可能Stay/Abort0、舷変更6条件成功。UHT生成・実C++/Development Editor Win64成功。投影長10.9937〜1106.74pxの視覚問題は残り、R4全体は正式不合格。証跡/限界/変更一覧はDocs/R4A4_ACTION_BOUNDARY_RECOVERY.md。A5/R5以降/H/M11未着手。以下は履歴。

2026-10-04 R4A-3: Mouse Commandの実Camera/Projection/ViewRect観測で現在Base Tipを投影し、DeltaからStation-local BaseDirectionへ確定。固定Grip/Length・Camera-only不変性・実Mesh/Line端点契約を維持し旧±0.28入力矩形を撤去。UHT生成・実C++/Development Editor Win64成功。最終85件は83成功（外部HTTP警告付き1件含む）/2既知Lock試験FAIL。新規4件/直接回帰72件は成功。舷変更後3条件は残存、従来42条件では34回収/8Abortへ変化。安全閾値/Abort/Egi/Shakuri係数は未変更。広い操作域で投影長の極端な短縮も観測し、R4正式不合格を維持。証跡と制限はDocs/R4A3_MOUSE_DELTA_INTEGRATION.md。R4A-4以降/R5以降/H/M11未着手。以下は履歴。

2026-10-04 R4A-2完了記録: Rodの永続正本を固定Station-local Grip・BaseDirection・LengthMへ移行し、Camera-only更新から切断。中断前9/30の最終UHT生成/実C++/Development Editor Win64・81試験の証跡を再確認。80成功/1失敗、関連72件成功、試験内警告0。旧Camera-only FAILはPASSへ変化し、同じ42条件Lock probeは全Retrieved。追加の舷変更後Shakuriでは3条件の安全停止/Lockを検出し、指示どおり未修正でR4A-4証拠として保持。Mouseは入力時の凍結Cameraを使う暫定互換でありR4A-3完成版ではない。R4正式不合格を維持。現行正本はFISHING_SYSTEM末尾、証跡/限界/変更一覧はDocs/R4A2_STATION_LOCAL_POSE.md。R4A-3以降/R5以降/H/M11未着手。以下は各時点の履歴。

2026-09-30 R4A-1: R4はユーザー手動PIEで正式不合格。今回はRuntime観測・診断・Failing Integration Testのみ。保存Mapの実Game World/Active Camera/Visual Mesh経路でCamera-onlyによるRod移動をExpected FAILとして再現。Pure Mouse軸ずれとLockは未再現であり、解消扱いにしない。Rod方式/係数/Config/Contentは今回未変更。結果と限界はDocs/R4A1_RUNTIME_OBSERVATION.md。R4A-2、R5以降/H/M11は未着手。以下のR4自動成功は受入の代用にならない履歴。

2026-09-29 R4 Screen最終確認: 旧Geometry方式はユーザー手動PIE不合格。Camera画面基準の2D Control＋固定Grip/固定長球面へ改訂し、Quick中のRod追従停止/完了時初期姿勢Fallbackを修正。UHT・実C++/Development Editor Win64成功、新規4件＋関連44件の最終結果成功。水平FOVを実Viewportにも明示。旧World軸試験だけで合格にせず、画面再投影と手動PIEを受入とする。操作感の正式合否は手動待ち、R5以降/H/M11未着手。現行契約/手順はFISHING_SYSTEM/UI_SPEC末尾、証跡はROADMAP末尾。

2026-09-28 R4 Geometry: 固定LengthMをSnapshotへ公開しStation Basisから毎Tick再計算。表示Root/Tipを同一Rod Snapshotへ統一。UHT8生成・実C++/Development Editor Win64成功、新規Geometry3件＋関連34件成功。旧Simulationにも固定長契約があり、PIE症状の原因/解消は断定せず手動再確認待ち。R5以降/H/M11未着手。証跡/限界はROADMAP末尾。

2026-09-25 R4: R3はユーザーが自動/手動PIEとも正式合格。R4はFishing WASD視線とMouse Rodを独立化し、保存Inputへ明示接続。UHT7生成・実C++/Development Editor Win64成功、R4 4件＋関連回帰43件の最終結果成功。R4操作感/実Mouse captureは手動PIE待ち。Shakuri/Reel/環境係数は保持、R5以降/H/M11未着手。現行操作はUI_SPEC末尾、証跡はROADMAP末尾。

2026-09-25 R3 Cancel修正: 手動PIEでFishing内舷選択の取消後に観測Camera/操作不能となる不具合を受け、選択開始元と変更前Sideを明示化。R3 Cancel入力をSession終了へ流さず、開始元のMode/Context/Camera/Sideを維持する。UHT6生成・実C++/Development Editor Win64成功、R3 6件＋関連回帰27件の最終結果成功。今回取消経路の手動再PIE待ち。R4以降/H/M11未着手。

2026-09-24 R3最終修正: 既存の両舷FPS構図/Gunwale/Rod/Reel/Line/Mouse Look/Drift追従はユーザー手動合格。Fishing Ready内のC釣り座変更を追加し、Mode/船動態を維持。UHT7生成・実C++/Development Editor Win64成功、R3 5件＋R1/R2/F/G回帰27件成功。今回C経路の手動再確認待ち。R4入力/R5-R6巻取り/R7環境の補足は文書のみで未実装。R4以降/H/M11未着手。

2026-09-24 R3: ユーザーがR1/R2を正式合格。R3は毎回の舷選択、舷別Rod実接続、一人称Fishing Cameraと簡易Reel/Gunwaleを追加。R3実装・自動検証は成功、手動PIE構図は未確認。現行操作/手順はUI_SPEC末尾、証跡はROADMAP末尾。R4以降/H/M11未着手。

2026-09-19 R2 Mode Transition/Boost最終改訂: ユーザー指示により旧Velocity/慣性保持契約を廃止。Fishing確定時に動的速度と全Navigation応答を解除し、位置/Headingを保持して環境Driftを0から再形成する。Shiftは物理ReleasedとContextのCompleted/Canceledを分離し、左右両キー解放まで再入力待ち。Navigation HUDにOFF/ON/再入力待ち。UHT4生成・実C++/Development Editor Win64成功、R2 14件＋回帰28件の最終結果成功。今回の実PIE未確認、R2正式合否保留。R3以降/H/M11未着手。以下の速度継承/保持解除成功記録は旧契約の履歴。最新契約と再手順は各設計書末尾を参照。

2026-09-19 R2追加2点: Fishing→Navigation後のRod/Tip/Line/Egi表示をModeで遮断し再利用、Navigation専用Shift Boostを固定入力/推力応答へ追加。保存Input/Navigationのみ明示更新。UHT11生成・実C++・Development Editor Win64成功、R2 12件＋関連回帰29件成功、試験内エラー/警告0。今回の表示/Boost手動再PIEは未実施、R2正式合否保留。R3以降/H/M11未着手。現行操作はUI_SPEC末尾、計算はBOAT_SYSTEM末尾、証跡はROADMAP末尾。以下は履歴。

2026-09-18 R2最終修正: ユーザー手動で操船/Camera/Enter遷移/Deploy等は合格。残る旋回時の横滑り、Fishing退出入力欠落、F1競合を修正。推進中だけ横減衰補助（Prototype 1.5/s）、Fishing ReadyからEでNavigation、詳細HUDはInsertへ変更しプロジェクト設定で旧F1 Wireframe割当を除外。UHT8生成・実C++・Development Editor Win64成功、R2 9件＋関連回帰35件成功、試験内エラー/警告0。今回3点の手動再確認は未実施、R2正式合否保留。R3以降/H/M11未着手。現行キー/再試験はUI_SPEC末尾、計算はBOAT_SYSTEM末尾、証跡はROADMAP末尾。以下は履歴。

2026-09-18 R2手動指摘への修正: Navigationの釣り開始入力欠落、モード非対応の固定HUDガイド、遅い操船Prototype値を修正。Navigation Enter=釣り開始、Fishingで押し直したEnter=投入。推力/旋回だけを再調整し自然Drift係数は維持。保存Input/Navigationを明示更新。UHT3生成ファイル・実C++・Development Editor Win64成功、R2 6件＋R1/B/M09/F回帰22件成功、試験内エラー/警告0。手動再PIEは未実施、操作感合否は保留。今回の契約/結果はUI_SPEC・BOAT_SYSTEM・ROADMAP末尾を優先。R3以降/H/M11未着手。

2026-09-18 R2実装・自動検証完了: Navigation専用固定入力、推力/操舵と既存Driftの単一積分、三人称Cameraを追加。保存PrototypeへNavigation DataAssetと独立Input Contextを明示接続。UHT19生成ファイル・実C++・Development Editor Win64成功、R2 4件＋R1/B/D/M04/M09回帰27件成功、試験内エラー/警告0。手動PIE未確認で操作感の合否は保留。計算はBOAT_SYSTEM末尾、手動手順はUI_SPEC末尾、証跡/変更一覧はROADMAP末尾。R3以降/H/M11は未着手。以下は各時点の履歴。

2026-09-18 R1実装・自動検証完了: 初期Navigationと明示Fishing/Navigation固定Command、Session所有Mode、ModeEpoch/拒否条件/Snapshot/入力Context接続を実装。UHT16生成ファイル・実C++・Development Editor Win64成功。R1 4件＋関連回帰32件成功、試験内エラー/警告0。今回PIEは未実施。Sideは未選択を許容する型のみ、操船/Camera/Sequenceは未実装。R2以降/H/M11未着手。APIはGAME_DESIGN末尾、証跡はROADMAP末尾、開発確認方法はUI_SPEC末尾を参照。以下のR設計のみ/G記録は履歴。

2026-09-17 M10.5-R設計改訂: 最新のユーザー手動PIEによりGはゲームプレイ品質不合格。過去の自動試験成功を取り消すものではないが、合格の代用にはしない。今回更新したのは設計文書のみ。Rの実装は未着手。Navigation/Fishingの分離、三人称操船/左右舷一人称釣り、Shakuri Sequenceと弛み量に応じた回収を設計した。D16の簡易操船は今回の明示依頼で対象内へ変更。現行設計はGAME_DESIGN第11節、BOAT_SYSTEM第9節、FISHING_SYSTEM第14節、UI_SPEC末尾、ROADMAP第8節を優先する。Gの固定Reel Pulse、Shift観測カメラ、F1正常記録は旧実装の履歴。Rの自動試験とユーザー手動PIE合格後もHへ自動進行しない。H・M11以降は保留。

2026-09-17 G追加修正: 手動確認済みの表示/入力を保持し、通常HUD8行、Shift+Mouse観測/Home復帰、世界固定5mグリッド/ブイ、Rod可動域拡大、Rod Snap＋戻し中Reel Pulseを実装。連続Shakuriの弛み蓄積を再現し2/3/5回それぞれの作用を試験。UHT・実C++・Development Editor Win64成功、G 6件＋関連回帰36件の最終結果は成功、試験内エラー/警告0。PIEは自動起動せず今回分の手動再評価待ち。G最終合否保留、H・M11以降未着手。現行操作はUI_SPEC末尾、計算契約はFISHING_SYSTEM第13節。以下は履歴。

2026-09-16 GのPIE表示/UI修正: ユーザー手動試験で釣り操作は正常、黒い3D画面・大きいHUD・自動表示パネル等は不合格。表示専用Mesh/Unlit材質/近距離カメラ、主要11項目/7操作、F1詳細、Tab装備初期Closed、実Snapshotのrevision表示へ修正。UHT・実C++ビルド成功、G 3件＋D/E/F/M09回帰25件成功、試験内エラー/警告0。自動PIEは強制終了するとの申告で停止。修正後の手動目視はユーザー実施待ち、G最終合否未確定、H・M11以降未着手。現行操作はUI_SPECのG修正再試験節。以下は履歴。

2026-09-16 M10.5-G進行中: 旧5資産を保持してrevision 2専用8資産と保存Levelを追加。UHT・実C++・Development Editor Win64成功、G 2件＋D/E/F回帰18件成功（試験内エラー/警告0）。Editorで保存Levelを開けることを確認。実PIE入力・代表解像度は手動結果待ちで、Gの最終合否は未確定。起動手順はDocs/UI_SPEC.mdのG節、詳細はROADMAPのG進捗を参照。H・M11以降未着手。以下は履歴。

2026-09-16 M10.5-F実装・自動検証完了: 日本語HUD/操作ガイド、Table由来の装備パネル、Sessionの許可/拒否理由とUI入力遮断を実装。UHT・実C++・Development Editor Win64成功、F 6件＋関連回帰27件成功。保存設定移行・解像度/DPI/実マウスPIEの目視評価は未実施。G/H・M11以降は未着手、全体品質ゲートは未合格。現行UI契約はDocs/UI_SPEC.md第9節、証跡はDocs/ROADMAP.mdのF記録を参照。以下は履歴。

2026-09-15 M10.5-E完了: 左保持の通常回収／解放後Stayと、固定TickのQuickRetrievingを分離。今回の明示依頼を優先し、通常完了はResult（ロック維持）→NextCastでReady/解除、Quick完了だけ直接Ready/解除。海面近傍のライン拘束・巻取りを修正。UHT生成・実C++・Development Editor Win64成功、E 7件＋回帰54件成功、各試験エラー/警告0。保存資産移行・PIEは未実施。F〜H・M11以降は未着手、全体品質ゲート未合格。下のA〜D記録は履歴。

2026-09-14 M10.5-D完了: Mouse Axis2D→固定Input Queue→RodControl、右クリック/Spaceの同一Jerk、基準姿勢＋時間プロファイル、RodTip→Cライン接続を実装。Rod有効時は旧Lift/Reelを重ねない。UHT生成・実C++・Development Editor Win64成功、D 5件＋必要回帰49件成功、各試験エラー/警告0。Rod/Input資産は明示設定、既存保存Prototype移行/実マウスPIEは未実施。E〜H・M11以降は未着手、M10.5全体品質ゲートは未合格。以下のA〜C/設計のみの記録は履歴。

2026-09-14 M10.5-C完了: Egi WorldPositionを位置正本へ移行し、revision 2の深度潮/水中ライン抗力/需要繰出し/空間拘束とSnapshotを実装。UHT生成・実C++・Development Editor Win64成功、C 6件（243落下条件含む）＋回帰49件成功、各試験エラー/警告0。標準30mの最大ライン39.299694m、最長50.550秒で着底。保存Prototypeは旧係数revision 1のまま、資産移行/新モデルPIEは未実施。D〜H・M11以降は未着手、M10.5全体の品質ゲートは未合格。以下のA/B完了・設計のみの記録は履歴。

2026-09-14 M10.5-B完了: 風/表層潮を別々に評価する船体方向別応答、抗力/慣性、解析的な固定更新とBoat Snapshot拡張を実装。UHT生成・実C++・Development Editor Win64成功、B 5件＋A/M03/M04/M05/M08回帰26件成功、各試験エラー/警告0。旧保存設定はModelRevision=1で互換維持、新モデルは明示revision 2。保存Prototypeの移行/PIE再評価は未実施。C〜HおよびM11以降は未着手、M10.5全体の品質ゲートは未合格。以下のA完了/設計のみの記録は履歴。

2026-09-14 M10.5-A完了: 環境の風/表層潮/深度別潮、位置依存評価口、knot換算を実装。UHT生成・C++実コンパイル・Development Editor Win64成功、A 5件＋M03/M05/M08回帰20件成功、各試験エラー/警告0。B〜HおよびM11以降は未着手。M10.5全体のPIE品質ゲートは未合格。下の2026-09-13設計のみの記録は履歴。

2026-09-13 M10.5設計改訂（実装未着手）: M00〜M10の実装・自動試験成功は履歴として保持するが、ユーザーのM10後PIE評価は再現性・操作性の品質不合格。M11への進行はM10.5品質ゲート合格まで保留する。本書のM10.5改訂契約を旧記述より優先し、M03〜M10完了記録は旧実装の証跡として読む。今回はMarkdownのみ更新し、改訂機能の実装・ビルド・試験は行っていない。


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
- D01〜D09/D13/D15はMVP決定。D10/D11とD12の未指定部分は使用承認済みのMVP暫定仕様。D12はM10.5-Rでモード別操作へ改訂。D16の簡易操船・ポイント/船首/釣り舷の選択はR対象内。D14/D17/D18と本格操船は対象外・非ブロック。これらを理由なく再承認待ちへ戻さない。
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

- M10.5ではEgiSimulationのWorldPositionMを位置の唯一の正本とする。DepthMは当地海面から導出するfloat、下向き正。PositionXYMは互換読取値であり二重積分しない。シミュレーションm/s/g/kg、UE境界cm。単位を変数名に書く。
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
- M10.5では風と表層潮の各作用を船体側で評価し、エギ/海中ラインは深度別潮へ応答する。同じ速度を船とエギへコピーしない。Rの簡易操船以外の本格船舶物理、波物理、CFDは対象外。無効環境への技術防御と保留D14のゲーム仕様を混同しない。
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

## 9. M10.5 Prototype Realism Revision

- R1〜R4はユーザー手動を含め正式合格。R5はR5A後の手動Camera Down操作不具合により正式不合格、R5B修正・自動検証済みで手動再確認待ち。A〜Fの基盤を保持し、ROADMAPのR6〜R9は後続の明示依頼ごとに実装する。R全体の手動合格前にHへ進まず、M11以降のAI/ATTACK/BITE/Hook/Fight/Range評価を先行実装しない。
- M10までの自動試験成功と、ユーザーPIEの品質不合格を両方記録する。M10.5は自動試験とPIE再評価が両方合格するまで未完了。旧「M11へ進める」という記録を進行許可に使わない。
- 位置正本の移行、風/表層潮/深度別潮の分離、船体応答、需要に応じたFreeFall繰出し、通常回収停止の連続性を一体で検証する。水深30mで80m以上のラインが出る問題を表示値や80m clampで隠さない。
- 右クリックは1押下1シャクリ。左保持で通常回収、解放で位置/ラインを引き継ぐStay相当、F再Fall、Q Quick Retrieve、Enter投入。マウス竿入力もCastId/登録世代/固定Tick/Sequenceを通し、Actor/Widget Tickから正本を動かさない。
- QuickRetrievingは通常Retrievingと分離。途中停止/Attack/Bite/Hook不可。固定Tick所要時間の完了でCast終了・船上帰還・Readyを一度だけ確定する。Pause中は時間を進めず、Session破棄/明示終了は安全に中断して帰還を捏造しない。
- 日本語Prototype操作案内とReady用装備UIを用意し、Console Commandを必須にしない。UI操作を同時に回収/シャクリとして送らない。SHOPは追加しない。
- 未指定の風速・応答・抗力・慣性・沈下・ロッド範囲・回収時間・受入数値はPrototype/Testの技術調整値。製品値や実測値と区別し、試験失敗を隠すために期待値を変更しない。
