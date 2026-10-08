# M10.5-R5 Tip-run Shakuri Sequence + Reel Turn Demand

最新状態（2026-10-04）: この記録の後、ユーザー最終PIEでCamera DownによりShakuriが消える問題を確認し、R5正式合格を保留した。Sequence/.8m/1turnは維持し、Temporary poseのCamera依存とBase Envelope適用をR5Aで修正した。旧Camera独立の自動成功は受入の代用にしない。修正前後の実Runtime証拠・現行座標契約・手動再確認は[R5A記録](R5A_CAMERA_INDEPENDENCE.md)。以下の数値は修正前R5の履歴。

2026-10-04。作業開始時のGitはclean（HEAD fbf766e）。ユーザーがR4のAutomation・Runtime Integration・最終手動PIEを正式合格と確認したため、過去A1〜A5の不合格/手動待ちは当時の履歴として保持する。今回R5のみ。実装・自動受入は成功、今回のR5 GUI/主観操作感は未確認。R6以降/R7/H/M11には着手していない。

## 正本・Sequence・要求

SessionがUTRShakuriSequenceComponentをDefaultSubobjectとして所有。独立Actor Tickなし。Controller→Session Queue→Tick/Sequence/CastId/ModeEpoch/登録世代検証→Fishing PrepareStep→Sequence Advance→Rod Step→Egi/Line→Publish→実Presentationの順。

Fishingは受付・予約回数と外側Jerking状態、Sequenceは固定Tick Phaseと名目要求/実績、Rodは固定Station-local Root/BaseDirection/Length・Temporary Final、Egiはライン短縮と空間拘束を所有する。Queuedは処理結果enum末尾へ追加し、既存値を変更しない。外側Jerking中にQueued数を増やすだけで即次Actionへ飛ばさない。

1右Started=1要求。Base→Up（9Tick）→Recover（15Tick）→予約があれば次Up、予約なしならTensionFall→数値過渡完了で即Stay。各Queued Action間にTF/Stayを挟まない。無入力/連打猶予タイマーなし。Mouse中もBaseだけ更新、Action終了で最新Baseへ正確に戻す。Root/Length2mは固定、表示Rod/Line端点は既存SnapshotのFinal Tipで一致する。

RodTuning.Parameters.Sequenceはopt-in。保存PrototypeをUE SavePackageで明示移行:
- NominalRetrievePerHandleTurnM=0.8m、HandleTurnsPerShakuri=1.0（最大1turn）。
- UpDemandFraction01=0、残りをRecoverへ均等配分。0/50/100%の調整反映と設定凍結も試験。
- 既存Up 0.15秒/Return 0.25秒と振幅を維持。製品値/実測値へ昇格しない。
- 旧ShakuriReelSpeedMps/Secondsは保存R5で0/0へ撤去（旧8m/s×0.25秒=2m）。非opt-in旧fixtureの互換経路は維持しR5受入根拠にはしない。

各Up開始時に1turn/0.8mを当該Actionと投内合計へ計上。実行前に取消した予約は実行済みDemandに数えない。Phase内Tickへ配分する予算であり、実効回収のノルマではない。未実現量はR5で繰越/強制回収しない。

## 実効上限と安全性

FTREgiActionに限定要求を渡し、要求→LineLength短縮→既存LineConstraint→Egiの順を維持。直接Egi位置/速度注入なし。既存海面交差円/必要幾何長で実短縮が減ることを許容する。

追加配分試験でUpへ全量を配ると、Rod速度6.25669m/s＋Reel要求5.33333m/sで、拘束後移動11.3063m/s（既存上限10）となるSpatial.LineCorrectionExceededを検出。閾値/振幅/時間を緩めず、限定要求だけに既存速度予算からRod速度と候補Egi速度を予約した残量を適用した。各サブステップの要求は min(PhaseDemand/Count, max(0, MaxEgiSpeed-RodSpeed-CandidateEgiSpeed)*dt)。安全な追加spool要求の技術予算であり、R6のSlack目標/張力/Drag/slip評価ではない。残量0ならその要求を実行しない。Normal Retrieveにはこの限定予算を適用しない。

限定要求のfloat公開ライン境界を上向き丸めし、追加ULP短縮を防ぐ。Actualはその固定Tickの公開ライン前後差の非負部分を加算。Tick/Action/投内Actual<=Requestedを検証。根拠を超える値なら既存Aborted経路へ技術停止し、成功へ偽装しない。

Normal Retrieveは左Heldの独立連続操作。Action中はBase復帰までPending、早期Releaseで解除。Re-FallもPending。これらの新要求で未実行Jerk予約を取消す。Quickは即釣り評価を止めて限定要求を解除し、既存Rod Temporaryだけ自然完了する。Pause/Focusで予約/Heldを解除、Pause中Tick/pose/予算は不変。Intentional Safety AbortはAbortedのまま、Nによる復旧と旧入力拒否を維持。

## Snapshot / Insert

FTRShakuriSequenceSnapshot: CastId/Tick、Phase、SequenceIndex/CompletedCount/QueuedCount、RequestedHandleTurns/RequestedRetrieveM/ActualRetrieveMのcurrent/total、TickDemandM、PendingRetrieve/PendingReFall。Base/Final/Root/LengthはHUD.Rodを共用。Insert詳細へだけ追加し、通常HUD/キー/Mouse感度/DPI/Camera/FOV/Grip/環境は変更しない。将来SettingsのSensitivity X/Yまたは倍率要件はUI_SPECへ文書化のみ。

## Runtime受入・結果

保存 /Game/TipRun/Prototype/M105/L_TR_M105_Prototype、保存GameMode/SessionConfig/Rod Assetをロードした独立Game World。実LocalPlayer/Controller/Queue/CameraManager/FOV/ViewRect/Station/Rod/Egi/PrototypeViewActor/実Mesh。キーはViewport InputKey→Enhanced Input、固定Tickリプレイは同じController event adapter。World TickとEngine共通Tickableを通し、実Mesh端点とSnapshotを比較する。Solver直呼び/合成Camera/手動Presentationを受入根拠にしない。NullRHIのためOS物理Mouse/GPU/主観視覚は未検証。自動GUI PIEは起動しない。

最終採用 **101 Success / 0 Fail / 0 notRun / errors0**。新規R5 8件、関連93件。単一実行101件ではなく、試験別採用元をSaved/Automation/R5AcceptedResults.jsonに記録。R5Acceptance（100件中99成功）を基礎に、最終R5FinalSafety（55件全成功、R5新規8を含む）で重複を置換して101件。

新規8件:
1. SavedSequenceTuning: 別プロセス保存Assetロード、opt-in/0.8m/1turn/旧Pulse0、有限・範囲検証、Runtime接続。
2. OneTwoThreeFiveClicks: 両舷×3fps×1/2/3/5=24条件。毎Frame固定Root/Base/2m長/実Mesh端点、名目上限、Phase/予約/最終TF-Stay、左Held非生成、正常Abort0。
3. LongHoldAndStates: 右4秒Heldは1回、Release→再Startedで次1回、Ready/Quickの拒否、Quick Ready/Unlock。
4. BaseAimAndMouseDuringSequence: 両舷×中央/左右/上下、開始Base復帰・実行中Mouseで最新Baseへ戻る。
5. ActionInterruptionsPauseFocus: 両舷×Retrieve/PendingRelease/F/Quick/Pause/Focus。Base境界で移行、永久Lock/正常Abort0。
6. FixedTickSequenceReplay: 同一初期Worldから同Tick Controller入力5回。30/60/120でCommand/状態列、Sequence5、要求5turn/4m、Egi最終位置/実績一致。
7. StateSurfaceBudgetAndLifetime: 同TickDeploy→JerkはDeploying拒否、FreeFall/Bottom/通常Retrieveから受理、海面で名目未達許容、終了Sessionへ旧Cast入力不可/更新停止。
8. PhaseDistributionAndFrozenTuning: Up配分0/50/100%とLive asset変更（メモリのみ・保存なし）に対する凍結設定。

| クリック数 | Sequence / nominal turns | Requested total |
|---|---|---|
| 1 | 1 / 1 | 0.8m |
| 2 | 2 / 2 | 1.6m |
| 3 | 3 / 3 | 2.4m |
| 5 | 5 / 5 | 4.0m |

同一固定Tickリプレイの5回ActualはPort3.51122904m、Starboard3.43137503mで各3fps完全一致。自由キー試験はfpsごとのpress/release Frame間隔・bootstrapが異なるためActualが少し異なる（5回約3.46〜3.52m）。この列を同Tick物理決定性の証拠と混同しない。全24条件で要求上限とSequence/最終Stateは一致。

海面直後は要求0.8mに対しActual0.623786926mで強制達成なし。制御配分試験ではUp/Recover要求が0/0.8、0.4/0.4、0.8/0、Actualは0.490468979/0.446933270/0.154408932m。名目配分は設定どおり、物理実効は予算と幾何で下がる。

関連: A1=4、A2=5、A3=4、A4=3、A5=5、R1=4、R2=14、R3=6、R4=11、D=5、E=7、F=6、G=6、M04=6、M09=7。Camera-only、2,000往復/実投影、Ergonomic 720姿勢、舷選択、UI遮断、Pending、安全停止/N、通常/Quickを確認。正常42ケースと舷変更の回帰はAbort0。Intentional NaN→Safety Abort→N復旧/旧Cast・ModeEpoch・登録世代拒否は維持。無関係なA〜C全物理/R9/H統合は実行しない。

途中失敗は保存: R5Firstの決定性1件とR5AcceptanceのA4旧Replay1件は、fpsごとに起動/投入のFrame数が同じで物理初期状態が違った。初期履歴を60fpsで揃えた後、同固定Tick入力の間だけ描画fpsを変える試験へ修正し、位置/実績/状態の許容を緩めず再実行。R5Final/R5DistributionDiagnosticのUp100%安全停止は上記限定予算で修正。途中ログ/FAILを削除していない。

試験内警告4件は外部Google generate_204 HTTPタイムアウト（A3 RepeatedDeltaReach、A5 EnvelopeReplay/Sweep、R5 Replay各1）。R5コード由来警告0、errors0。起動時の既存Python旧screen_control名重複、Editor layout/環境通知は別に残る。外部接続を成功扱いに書換えてはいない。

## Build・ファイル

UE5.8.2 / MSVC14.51.36257 / Windows SDK10.0.22621.0。通常Development Editor Win64成功。新Sequence/enum/Snapshot/ParametersのUHT生成.cpp/hを実確認（Types/Componentは10/04 20:37生成）。Build1/2/4で変更Runtimeと反射コード、最終Build5で試験、Build6で最終Egi要求処理を実Compile/Link。Live Coding/up-to-date確認だけではない。git diff --check成功。残るビルド通知は既存推奨MSVC14.50.35717との差とIncludeOrder Unreal5_6。

新規Source3:
- Public/Data/TRShakuriSequenceTypes.h。
- Public/Fishing/TRShakuriSequenceComponent.h。
- Private/Fishing/TRShakuriSequenceComponent.cpp。

変更Source11:
- Public/Data/TRRodTuningDataAsset.h、Private/Data/TRRodTuningDataAsset.cpp。
- Public/Data/TREvents.h、TRHUDSnapshot.h、TRTypes.h。
- Public/Fishing/TRFishingComponent.h、Private/Fishing/TRFishingComponent.cpp。
- Private/Fishing/TREgiSpatialSimulation.cpp。
- Public/Game/TRFishingSessionActor.h、Private/Game/TRFishingSessionActor.cpp。
- Private/Tests/TRRuntimeRodIntegrationTests.cpp。

Content1: Content/TipRun/Prototype/M105/Data/DA_TR_M105Rod_Prototype.uassetのみUE SavePackageで移行。Config/Map/Input/Station/Camera/Grip/Environment/Boatの追加変更なし。旧互換資産を無計画に上書きしていない。文書6: AGENTS、GAME_DESIGN、FISHING_SYSTEM、UI_SPEC、ROADMAP、本書。Saved/Intermediate/ログ/生成JSONはソースに追加しない。

## 次の確認

手動はUI_SPEC末尾の最大5項目。今回手動PIEを実施済みとは報告しない。R5の実装/Runtime契約は成功し、R6のSlack-aware Reelへ渡す名目/実績とSequence基盤がある。ただしR6は未実装で、新しい釣り操作全体の実プレイ品質を先取りして合格にしない。R6着手は次の明示依頼で行う。H/M11も未着手。
