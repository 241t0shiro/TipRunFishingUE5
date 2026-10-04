# R4A-4 Fishing Action Boundary / Spatial Abort / Lock Recovery

2026-10-04。R4A-4のみ。R4全体は正式不合格のまま。A5、R5以降/H/M11は未着手。既存の未コミット差分を保持。今回のConfig/Content変更なし。

## 修正前の再現・分類

修正前 `Saved/Automation/R4A4Before/index.json` と診断のみ追加した `R4A4DiagnosticBefore/index.json` は同じ42条件で34 Retrieved / 8 Aborted / Active0。舷変更の6条件は初期Starboard→Portの30/60/120fpsで3 Abort。入力列・安全閾値を変更せず再現した。

42入力列: Pose {0,-1,+1} × Deploy保持 {1,4 frame} × Jerk後delay {0,2,5,9,15,24,32 frame}。非ゼロPoseはReadyでRaw MouseY=100×Poseを24 frame、4 frame待機。Enterを所定frame、右down1frame→up、所定delay、左down、最大360 frameまたは結果確定まで、左up後3frame。各ケース名に全条件を含める。

| Pose | Deploy frame | Jerk→Retrieve delay | 異常Tick / Jerk開始Tick | Egi移動m | 実Step速度m/s | Rod速度m/s |
|---|---:|---|---|---:|---:|---:|
| -1 | 1 | 9 / 15 / 24 / 32（各1件、計4件） | 61 / 53 | 0.365487115 | 21.9292269 | 4.88991194 |
| -1 | 4 | 9 / 15 / 24 / 32（各1件、計4件） | 64 / 56 | 0.361199876 | 21.6719926 | 4.88940853 |

全8件は同じ海面・ライン幾何不整合の分類。Cast1、before=Jerking、after=Result/Aborted、PendingJerk0、Reeling0。Retrieve開始前、Shakuri Up中に発生し、ReturnのReel Pulseはまだ0。最大Egi速度10m/sと移動量20mの既存防御は正常に機能している。Rod移動は約0.0815mで滑らかだが、Rod高さが旧Line1mを超えたとき `Line=max(MinLine,SurfaceHeight)` が高さのみを満たし、海面とライン球の交差円を半径0へ潰す。旧Egi水平位置約0.36mを竿先直下へ補正してしまう。旧Correction値0はこのsurface-circle補正を加算していなかったためで、補正がない証拠ではない。

舷変更後の既知3件も同じ分類。Rod高さは2.073→2.167m（30fps）、1.958→2.073m（60/120fps）。旧Line2.085/2.043/2.022mから高さだけへ増やし、水平距離約0.19〜0.22mを消してしまった。実Step速度12.9407 / 12.1522 / 11.3150m/s。Station Root/New Side/Deploy初期Rodを二重所有する不具合はこの列では検出されなかった。舷変更は再現のトリガーであり、真因は短いラインと海面境界の処理。

各11ケースの入力識別、Cast/Tick/State、Base/Final、Temporary offset、Previous/Current Rod/Egi、速度、Line長、補正、Queue、受付/終了前後は `Saved/R4A4Baseline/BeforeAbortDetails.txt` と修正前JSONへ保存。この8件のAbort直前RetrieveHeld=falseは、Retrieve DownがAbort後に発行される同一入力列からも確認できる（終了前の最終ActionはJerk）。Controller/Heldを含むFrame診断は再実行で更新されるため、前回の実Frameファイル自体を永続証跡と扱わない。診断追加だけのビルド/試験を先に実行し、動作修正前に分類した。

## 修正した境界契約

1. 海面に接する巻取りの既存幾何式 `sqrt(height² + max(0,horizontalRadius-reelRate*dt)²)` を、動くRodに必要なライン下限として使う。旧PreviousLineによる上限制約を撤去し、上がったRodと海面の同時接触を満たす。Egi位置/速度を上限へClampせず、異常検出の速度/移動量/非有限判定を一切緩めない。Surface円補正もCorrection診断へ加算する。Reel量、Shakuri振幅/時刻、Drag、最小/最大Line、環境値は保持。
2. Rod profile使用時にShakuri中のRetrieveStartedをPendingとして受理。現在Up/Return完了TickまでStateを置換しない。Final==BaseになるTickでRetrieving開始。左ReleaseまたはPause/Focusの安全停止で保留Retrieveを解除。Jerkが後から来れば既存「新Actionが回収を置換」のSequence規約を維持。Fallも同じBase復帰境界を使用。Rod未設定の旧M10経路は既存即時遷移を維持する。
3. Quickは既存仕様通り直ちにQuickRetrievingへ入り他Commandを拒否し、水中評価を停止。進行中のRod一時profileだけは元の開始Tickから自然に完了し、State enum置換を理由に一瞬でBaseへ戻さない。Quick時間/結果/Ready/Unlockは変更しない。R5 Sequenceは実装していない。
4. RodのProfileStartTickは既存Shakuri開始時に記録する一時時刻。BaseDirectionLocalへ焼き込まない。Snapshotの同じFinal Tip間をEgiが使用し、無効時のCamera/Base代替を挿入しない。GripとLength不変。Abort/明示Resetではprofileを消去する。

## 明示的な技術復旧

技術Abortは従来どおりResult/Aborted、Onboard=false、Lock=true。明示AbortCast/EndFishingのSession.ExplicitAbortは技術復旧対象から除外し、既存の中断/船外契約を維持。自動帰還やRetrievedへの変更はしない。Fishing Mode内のこの状態でN（NextCast/ResetCast）を明示受理すると、技術復旧として水中Actor/数値を片付け、Onboard=true/Ready/Unlockを確定する。LastResult.Outcomeと結果CastId、装備記録、DetailedFailure/Abort直前診断は保持し、新しい終端通知を送らない。

Recoveryは現在CastIdを未活動値へ戻し、登録を更新する。PendingJerk/保留Retrieve/Fall/Reeling/profileを解除し、旧Queue/すでに抽出された旧登録への配送を無効にする。Controllerは既存登録/Cast変更監視でHeldを解除する。次Deployは単調増加の新CastIdを使用する。ModeEpoch、Side、Base、Grip、Length、Cameraを変更しない。Boat Position/Heading/Driftにも触れない。

通常RetrievedはResult→N Ready、QuickRetrievedは直接Readyという既存契約を維持。技術復旧が通常Retrievedになったとは報告しない。Release後に回収条件へ達しなければCastは継続する。

## 診断/HUD

複合 `Spatial.ExcessiveMotionOrNonFinite` をPositionNonFinite / VelocityNonFinite / EgiTravelExceeded / LineCorrectionExceeded / EgiConstraintVelocityExceededへ分割。Candidate段階はPositionNonFinite / VelocityNonFinite / EgiCandidateVelocityExceeded、Rod段階はRodVelocityNonFinite / RodTipDeltaExceeded。既存のOcean/Line/収束エラー分類は保持。閾値は従来値。

Insert詳細にはRecoveryAvailable/PendingAction/Temporary offset、前後Rod、RodTipDelta/RodVelocity、前後Egi/velocity、LineRequiredBeforeCorrection/LineCurrent/LineCorrection/Tensionを追加。通常表示の大きさは変更せず、Abort時の操作案内だけ「シミュレーション異常：回収状態をリセットしてください / N：安全復旧」を表示する。

## 検証結果

**最終採用112件すべて成功（新規3件＋関連109件）、試験内errors/warnings=0、未実行0。** 単一実行を全成功と書き換えていない。Runtime最終版の関連109件は`R4A4Final2/index.json`、新規Pending/Replayの最終結果は`R4A4Acceptance/index.json`、強化した異常復旧/旧入力検証の最終結果は`R4A4RecoveryFinal/index.json`。採用元を各試験ごとに`Saved/Automation/R4A4AcceptedResults.json`へ記録した。

| 受入項目 | 修正前 | 修正後／採用結果 |
|---|---|---|
| 同一42条件 | 34 Retrieved / 8 Aborted | 28 Retrieved / 14 Release後Stay継続 / **Technical Abort0** |
| 舷変更既知6条件 | Starboard→Portの3fpsでAbort | 6成功、Abort0、Side/Root/Base/Final/Deploy初期条件整合 |
| Pending Retrieveと早期Release | Jerkingを即時置換可能 | 30/60/120fps×保持/Releaseの6条件成功、Base復帰Tickで開始／Releaseで取消 |
| 意図的異常と明示復旧 | 永久Lockの可能性 | 両舷×3fpsの6条件でNonFiniteを検出→Aborted保持→N Ready/Onboard/Unlock、新投可能 |
| 旧入力拒否 | 診断対象 | 旧CastId/ModeEpoch/登録世代のRetrieve/Jerk/Mouseが新投へ漏れない |
| 固定Tickリプレイ | 診断対象 | 30/60/120fpsでCommand結果・状態遷移Tick列一致、Pause/Focusで保留/Held解除 |
| R4A-3 Runtime | 既存成功契約 | Station-local/Camera不変/両舷/実Mesh投影/固定長/Quick不変を維持 |

42条件の入力列は変更していない。14継続ケースは下寄りRodで既存MinLine1mへ達し、通常回収の船上条件へ6秒内に到達せずRelease→Stayとなったもの。Pendingなし、Jerk/Fall/Quick受付可能を確認し、元の入力列の**後に別の追試**としてQ→Ready/Unlockを確認した。42全Retrievedとは報告せず、回収速度・最小Line・船上条件を変更して結果を合わせない。

意図的異常はテスト限定friendからEgi.MotionVelocityMpsにNaNを設定し、通常World Tickの既存安全検査を通す。Aborted通知1回・船外/Lockを確認後、実ControllerのN→Queue→固定消費で復旧。LastResult=Abortedと詳細理由、Side/Base/Root/Length、Camera相対Transform、Modeを保持。Boat位置/Heading/Velocityは復旧Command境界で完全一致（後続の自然Drift積分とは区別）。旧登録の予約Queueは0、新登録には旧RetrieveStarted/Jerk/RodAimなし。120fpsではAbort後ControllerのHeld解除が新登録へ安全なRetrieveStoppedを1件送る場合があり、旧予約入力と区別して確認した。

Rod連続性試験は最終Base一致、固定Length、回収開始Tick=`Shakuri開始Tick＋既存Up/Return Tick`、早期Release後の遅延回収なしを検査。観測最大Sample速度7.55m/s、60/120fpsの最大Frame差約0.1258m、30fpsは1Frameに2固定Tickを含み約0.2473m。Frame差を1固定Tickのワープとして誤分類しない。Quickは進行中profileの自然復帰を維持し、技術Abortでのみ一時状態を明示消去する。

継続投影測定（見かけ品質の合格判定ではない）:
- A1と同じ連続操作列・両舷×3fps: **10.9937〜1106.74px、最大/最小約100.67**。Camera Lookを含む既知の極端な短縮が残る。
- A3広域端点列: 1080p換算115.978943〜1343.59215px、比11.5847939。別の操作列なので上記範囲を置き換えない。
- A3実Mesh投影の交差軸差最大約0.0000610352px、実Mesh/Snapshot差約1.61e-15m。両軸2000往復・両舷/Heading/解像度の契約を維持。

関連109件はA1/A2/A3 Runtime、R1/R2/R3/R4、D/E/F/G、M04/M06/M09/M10、B/C（243落下条件を含む）。変更の影響を受けた旧Gの即時Retrieve期待はPending契約へ更新。旧M10のRod未設定経路と明示AbortCast契約は維持して回帰確認した。R9/Hの統合タスクは開始していない。

途中実行の失敗も保持する。`R4A4Final2`の新規Replayは通常Retrievedが先に成立しているのにQuick完了を要求したテスト誤り、`R4A4Acceptance`のRecoveryは安全なRetrieveStoppedまで旧Queue残留と誤分類したテスト誤り。最終試験は通常ResultとQuick Readyの契約、旧登録と新登録の入力種別を分けて検証した。RuntimeはBuild5以降変更なし、期待の変更でAbort/安全失敗を成功へ隠していない。

UHT生成済みの`ETRCommandResult::Pending`、SnapshotのTemporary offset、HUD RecoveryAvailableを生成.cppで確認。変更Runtimeの実C++コンパイル/LinkはBuild5、最終試験コードはBuild7で成功。最終空白整形後のBuild8も実Compile/Link/Development Editor Win64成功。git diff --check成功。UE5.8.2 / MSVC14.51.36257 / Windows SDK10.0.22621.0。既存MSVC推奨版14.50.35717とIncludeOrder Unreal5_6の通知は残る。今回由来の試験警告なし。途中実行での外部HTTPタイムアウト警告は最終採用試験で再発していない。

証跡: `Saved/Logs/R4A4DiagnosticBuild.log`、`R4A4Build5.log`、`R4A4Build7.log`、`R4A4Build8.log`、上記各Automation JSON、`Saved/R4A4Baseline/BeforeAbortDetails.txt`。修正前/中間レポートを削除せず保持。自動GUI PIE、実OS Mouse capture、GPU描画、主観操作感は今回未実施。R4A-4の技術受入は成功だが、R4全体は正式不合格。A5開始の基盤は揃ったが、明示依頼まで開始しない。

## 今回の変更ファイル

Source/TipRunFishingUE5配下:
- Public/Data/TRTypes.h（Pending結果を末尾追加）、TRHUDSnapshot.h（Recovery表示）、TRSnapshots.h（一時offset診断）。
- Public/Fishing/TRFishingComponent.h、Private/Fishing/TRFishingComponent.cpp（既存Actionの保留/解除）。
- Public/Fishing/TRRodControlComponent.h、Private/Fishing/TRRodControlComponent.cpp（profile境界保持、明示消去）。
- Public/Fishing/TREgiSimulationComponent.h、Private/Fishing/TREgiSpatialSimulation.cpp（診断、海面Line整合、テスト限定friendでの異常値注入）。
- Public/Game/TRFishingSessionActor.h、Private/Game/TRFishingSessionActor.cpp（明示Recovery/診断/寿命識別）。
- Private/UI/TRPrototypePresentation.cpp（Abort時のみ日本語復旧案内）。
- Private/Tests/TRRuntimeRodIntegrationTests.cpp（既存入力列保持、新Runtime受入）、TRRodTests.cpp（即時Retrieveという旧期待をPending契約へ更新）。
- 文書: 本書、AGENTS.md、FISHING_SYSTEM.md、UI_SPEC.md、ROADMAP.md。

既存の2つの未コミットAssetはSHA256が作業前後一致。旧差分を本タスクで作成したものと混同しない。Config/Content、FOV/Camera anchor/Grip/Length/感度/係数変更なし。

## 手動PIE再確認（自動GUI PIEは起動しない）

1. 保存L_TR_M105_Prototypeから開始し、両舷で下寄りRod→投入直後右クリック→左保持。Shakuri中は巻取り開始を保留し、自然にBaseへ戻ってから回収すること。早期左Releaseでも勝手に後から回収しないこと。
2. 右舷Ready→Cで左舷確定→D視線約1/3秒→投入後約1秒→右クリック。InsertのFailure/Stateを確認し、安全停止しないこと。その後左Release、再Shakuri/F、Q Ready/Unlockを確認。
3. 実際に技術Abort表示が出た場合だけ、診断を記録後NでReady/Onboard/Unlock、Base/Side/視線維持、次Deployを確認する。通常プレイで故意に異常値を作る操作は追加していない。明示N前に装備解除/帰還しないこと。

見かけ長さ/操作感の正式合否は今回判定しない。A5の視覚評価は後続明示依頼まで開始しない。R5/H/M11未着手。
