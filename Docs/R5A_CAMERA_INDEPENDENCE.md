# R5A — Shakuri Camera Independence Repair

2026-10-04。R4はユーザーの正式合格を維持。R5は最終手動PIEのCamera Down指摘により正式合格保留。今回はR5Aのみ実装・自動検証を完了。R6以降/R7/H/M11には着手しない。修正後のGUI PIE・OS Mouse・GPU描画・主観演出は未確認であり、Runtime成功で手動合格を代用しない。

## 原因と経路

Right Mouse Started → ATRPlayerController::ActionStarted → Session Input Queue（Tick/Sequence/CastId/ModeEpoch/登録世代）→ Fishing PrepareStep → UTRShakuriSequenceComponent::Advance → UTRRodControlComponent::Step → Final Snapshot → ATRPrototypeViewActor::ApplyPresentation → 実Rod/Line Mesh。

Camera依存はSequenceではなくRod Stepに残っていた。旧StepはBaseをDeriveCompatibilityControlで投影し、ResolveScreenPose(...Offset...,bDirectProjection=true)でtan(Offset)/tan(FOV/2)の画面上向き量を加算。YをScreen.MaxProjectedY=0.5へ制限し、Camera Ray/SphereでFinal Tipへ戻していた。Base投影が上限へ達すると一時上昇量がほぼ0になる。さらにFinalにもBase用ConstrainLocalDirection（Envelope pitch上端35度）を適用し、上端Baseからの動作も潰していた。

参照Cameraは毎TickのActive Cameraではなく、最後の非ゼロMouse AimでApplyAimが保存したCompatibilityCameraLocal/Rotation/FOV。Camera-only操作でMouseが完全に0なら旧観測を保持するが、下向きCameraで次のMouse sampleが入ると同じStation-local Baseでも振幅が変わる。PIEの小さなMouse入力を伴う条件に対応する再現を追加した。Presentationは既にSnapshotのFinal Root/Tipを使用しており、表示側のCamera振幅補正は原因ではない。

位置（本記録時）: ControllerはPrivate/Game/TRPlayerController.cppのActionStarted/SubmitMouseDelta、SessionはPrivate/Game/TRFishingSessionActor.cpp:285/290、SequenceはPrivate/Fishing/TRShakuriSequenceComponent.cpp:16、RodはPrivate/Fishing/TRRodControlComponent.cpp:15/94/137、表示はPrivate/Game/TRPrototypeViewActor.cpp:129。

## 修正後の正本・安全範囲

RootLocal/BaseRodDirectionLocal/LengthMを保持し、毎固定Tick、BaseのStation-local Yaw/Pitchと既存SmoothstepのTemporary OffsetからFinal Directionを再構成する。Base azimuthに対応するStation横軸まわりの上向き作用であり、Camera Pitch/Yaw、pixel、Viewport、FOVをAction生成に使用しない。Offset=0ならFinalは最新Baseそのものへ完全復帰。Camera投影はMouse Delta確定時の観測と読取診断だけに残す。旧solverはResolveInitialScreenPoseへ改名し、一度のStation初期化だけで使用する。

Base Envelopeは既存yaw±40度/pitch-10〜35度のまま。Temporaryには別DataAsset項目Parameters.ActionSafety.MaxPitchDegを追加。既定75度はPrototype安全候補であり製品値ではない。有限・0より大・85度未満・Base上端以上を検証。ActionはBase azimuthを保持し上向きpitchをこの上限で制限するため、保存Prototypeの外向き半球を維持して背面/船内へ回さない。診断はAction.PitchSafety。保存Assetの未保存新項目にはC++既定75度をロード時に使用し、今回Content保存/移行は行わない。

保存PrototypeのLength2m、Root/TipのSimulation m→Mesh境界cm一回変換、同一SnapshotのRod/Line端点を維持。R5 Sequence/nominal .8m/1turn、Up .15秒/Recover .25秒、振幅 .3rad、限定要求の既存速度予算、Pending/Abort semanticsは変更しない。

## 修正前後の実Runtime計測

保存 /Game/TipRun/Prototype/M105/L_TR_M105_Prototype と保存GameMode/SessionConfig/Rod/Stationをロード。独立Game World、LocalPlayer、SceneViewport、実Controller/CameraManager、Session Queue、固定Simulation、PrototypeViewActor、実Meshを通常World Tick/Engine共通Tickableで観測。キーはInputKey、Mouseは実Controllerの観測付きadapter。両舷で同じBaseを保持し、WASDでCameraを向けた後に微小な可逆Mouse sampleで観測を更新する。Base差許容1e-10、Camera間60固定Tick軌跡差許容1e-9m。Synthetic solver/手動Presentationは受入根拠にしない。

| Camera | 修正前Tip最大変位m（両舷） | 修正後m（両舷） |
|---|---:|---:|
| 正面（pitch約-20.58） | 0.692563857834 | 0.597752529894 |
| 上（pitch約-0.17） | 0.803471481957 | 0.597752529894 |
| 下（pitch約-35.17） | 0.019885240240 | 0.597752529894 |
| 左（yaw約-24.75） | 0.692128780174 | 0.597752529894 |
| 右（yaw約24.75） | 0.692128780174 | 0.597752529894 |

旧Downは正面の約2.87%まで縮小。修正前の同一Base/5視線試験は実FAILし、下向きとの全軌跡差0.672970515032mを記録（R5ABeforeSameBase）。pitch約-45度ではほぼ0も観測したが、その条件はBaseの画面境界も変化したため正式な同一Base比較には使わない（R5ABefore）。

修正後Camera間軌跡差最大6.2821e-16m。両舷対応軌跡一致、Base復帰誤差0、1Tick最大Tip移動約0.0995781891m。専用Down試験はCamera-only/非ゼロMouse観測更新を分け、双方約0.597753m、正面との差最大8.8818e-16m。上端Base35度からFinal52.1887338161度まで動き、左右Baseも角振幅.3rad。Root/Base/2m長不変、外向きX>0、Action上限、実Mesh/Snapshot端点一致を確認。

動作中S+D保持でCameraをyaw55/pitch-65まで動かす試験も追加。両舷×30/60/120fps×Look有無で、5要求の全150固定Tick軌跡差0、5完了/名目4m/最新Base完全復帰。Egi最終位置は1e-9m以内、実巻取りは同舷内完全一致（Port3.92353439331m、Starboard3.93497562408m）。Actualは名目以下であり.8m実効保証へ変更しない。実Mesh/Tip差は全新規試験最大約1.42e-15m。

## 試験・ビルド・警告

新規4件はTipRun.R5A.Runtime.CameraIndependentTrajectory / CameraDownReproduction / BaseEnvelopeActionSeparation / ActiveLookFixedTickTrajectory。保存Asset、実CameraとMouse、全軌跡、上端Base、異常Action設定、両舷、実Presentation、固定Tick観測を検証。

最終採用87 Success / 0 Fail / 0 notRun / errors0。新規4＋関連83（R5=8、A1=4、A2=5、A3=4、A4=3、A5=5、R4=11、R3=6、D=5、E=7、F=6、G=6、M09=7、M04=6）。R5全8件、1/2/3/5/Hold/最終TF-Stay/途中Mouse/最新Base、RetrieveとRe-Fall Pending、早期Release、Quick、UI/Pause/Focus、通常42条件、Intentional Abort/N復旧・旧寿命拒否を回帰。正常操作Technical Abort0で安全閾値を緩めていない。無関係なA〜C全物理/R9/Hは未実施。

採用元はSaved/Automation/R5AAcceptedResults.json。R5AAcceptanceの関連83件と最終R5AFinalの新規4件を採用。単一実行87全成功ではない。途中のActiveLook観測Actorに必要なCommand delegateが未登録で観測0件となったfixtureをbound no-op delegateへ修正し、全150Tickを確認。プロダクトSimulationをこの失敗のために変更していない。途中/修正前FAILは保持する。

関連3件に既存外部HTTP generate_204タイムアウト各1（A5 Boundary/Replay/Sweep）。新規最終4件warning0。初回Focusedは外部音声device初期化警告があり、最終headlessは-nosound指定（Config変更なし）。ビルドは既存MSVC推奨版/include順通知あり、今回コード由来警告なし。

UE5.8.2 / MSVC14.51.36257 / SDK10.0.22621.0。R5ABuild1でUHT3生成ファイル、変更RodControl/RodTuningの実C++/Link、Development Editor Win64成功。最後のfixtureもR5ABuild3で実C++/Link成功。up-to-date確認だけではない。git diff --check成功。Saved/Logs、Saved/Automationの生成証跡はソースへ追加しない。

## 今回の変更ファイル

Source5件: Public/Data/TRRodTuningDataAsset.h、Private/Data/TRRodTuningDataAsset.cpp、Public/Fishing/TRRodControlComponent.h、Private/Fishing/TRRodControlComponent.cpp、Private/Tests/TRRuntimeRodIntegrationTests.cpp。

Markdown7件: 本記録、AGENTS.md、Docs/FISHING_SYSTEM.md、UI_SPEC.md、GAME_DESIGN.md、ROADMAP.md、R5_SHAKURI_SEQUENCE.md。開始前から存在したR5 Sequence/Session/Fishing/Egiコードと保存Rod Assetの差分は保持。R5AによるConfig/Content追加変更なし。Base感度/Clamp、CameraAnchor/FOV、Length、nominal、Egi/Environment tuningは変更しない。

## 手動PIE再確認（最大4項目）

1. Editor再起動→保存L_TR_M105_Prototype→PIE→Enter/AまたはD/Enter→投入・沈下。Mouseを止めたままWASDで正面/上/十分下/左/右を向き右1クリック。見え方が変わってもRod Actionが消えず、InsertのTemporary/Final値とLine作用が進むこと。
2. 下向きでMouseを少し動かした後にも右クリック。中央/左右/上端Baseから自然に上げ戻しし最新Baseへ戻ること。両舷で確認。Root固定/2m長を維持しCameraに合わせて伸縮しないこと。
3. 右1/2/3/5クリックと長押し。入力数どおりUp/Recover、予約間Stayなし、最後のみTF/Stay、1回1turn/名目.8m（5回4m）、Actual<=Requested。Action中Cameraを動かしても動作量が変わらないこと。
4. Action中左Hold/早期Release/F/Q、Tab/Pause/Focus。Pending/正常回収・Quick Ready/Unlock、Base復帰、Technical Abort/永久Lockなし。異常時はInsert詳細理由を記録する。

R5A自動検証完了。R5正式合格はこの手動再確認待ちであり、R6以降への進行許可ではない。

## 2026-10-05 手動不合格とR5Bへの継続

上記はR5A時点の自動証跡。ユーザー手動でShakuri動作/Base復帰は改善したが、Camera DownでMouseの別軸移動/画面外操作不良を確認しR5は正式不合格。追加の原因はBase MouseのViewport Clampで、Shakuri yawではなかった。現行Mouse契約と修正前/後Runtime証跡は[R5B記録](R5B_CAMERA_DOWN_CONTROL.md)。R6以降へ進行しない。
