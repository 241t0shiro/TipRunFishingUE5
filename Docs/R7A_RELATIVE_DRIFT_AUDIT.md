# R7A Boat / Egi / Current Relative Drift Audit

2026-10-08。ユーザーがR6のAutomationと最終手動PIEを正式合格と確認。今回はR7Aの診断・実保存Runtime監査のみ。R7Bの調整/H/M11は未着手。ゲーム近似を測定したもので、実船の校正・製品バランス値の確定ではない。

## 結論

保存標準PrototypeではBoatとEgiの速度は同一ではない。Stay20秒後のBoat=(0.171481,0.181818)m/s、Egi=(0.275533,0.072623)m/s、Boat−Egi相対速度の大きさ0.150832m/s。Boatは風の+Y、潮の+Xを別に受け、Egiは深度潮+Xへ応答し、taut時にラインを介して部分追従する。Boat velocityの直接加算/全成分コピー、Boatへの深度潮適用はない。

一方、保存Oceanは**FieldRevision2だがCurrentMode=Constant**。表層/深度/ライン3点で同じ(0.360111111,0,0)m/s（0.7knot）を返す。責務が別でも深度差は設定上0である。自然な相対運動はあり、Portの船−Egi水平距離13.337→17.024m、竿先−Egi水平距離10.840→14.510mと増えるが、鉛直差30.033→29.387mより小さいため海面角70.153→63.722°で、20秒窓内に45°へ達しない。

これは「完全同速度に強制するコード欠陥」の再現ではなく、保存環境・応答・幾何・観測時間の組み合わせによる角度不足。GPUの見え方やユーザーPIEでの装備/経過時間そのものは今回再評価していないため、主観的な『ほぼ一致』の原因を一つへ断定しない。90gでは相対速度0.030m/sまで小さくなることも別途観測した。

## 実Runtime構成・記録の時点

保存 `/Game/TipRun/Prototype/M105/L_TR_M105_Prototype`、BP_TR_M105GameMode_Prototype、保存Session/Ocean/Boat/Fishing/Rod/Input/Equipmentをロード。GameInstance/LocalPlayer/Controller/CameraManager/Station/PrototypeView/実Meshと通常World Tick→Input Queue→Fixed Simulation→Publishを使用。Enter/右クリックは実Viewport InputKey。Physics/端点/速度を直接設定しない。

baseline両舷35g＋Port30/40g、A〜E×30/35/40g、非理想2環境＋90g=22Cast World。別に30/60/120fpsの固定Tick入力リプレイ3World。環境を変える試験は保存OceanのTransient複製を既存Ocean/Providerへ明示初期化し、既存Mode入力で通常のFishing velocity resetを通す。係数は保存Boat/Egiからそのまま使い、元Assetを書き換え/保存しない。

全成功Spatial Tickを後から登録したPublish観測者で記録。CSVにState/位置/速度/環境/相対速度/幾何/ライン配分/補正/抗力/実サンプルを格納。Boat velocityは解析的終端速度、Egi Snapshot velocityは当Tick位置差/dt（平均）、内部Motion velocityは別欄。この時間定義差を同速度不具合と混同しない。

Egi DriftObservationは最後の数値substepの入力/候補/拘束後値。CurrentAtEgiDepthMpsは最終到着位置の潮であり入力点と区別する。BoatはTick開始XY、深度0で環境を評価。ライン3点は実際に抗力へ適用された水中点のみで、FreeFall slack/DragRate0なら空配列=未サンプル。存在しない点を0潮として捏造しない。

各状態遷移はevents.csvにも記録。Deploying/FreeFallは同Tick、最終Recover→TensionFall→Stayも同Tick。TFの数値過渡がこの条件で直ちに完了するため、独立したTF Publish行は存在しない。eventTickとlastPublishedTickを明示して、その場で読めるSnapshotを記録し、TF用の別積分値を捏造しない。Deploy初期化の相対幾何は未評価で、0の欄を計測値として使用しない。

生データ: `Saved/Automation/R7AObservations/<条件>.csv`、`.events.csv`、`.txt`（保存参照/実係数/イベントTick）。180sim秒の着底待ち上限を事前設定したが、全22Castが到達した。Stay開始から正確に1200固定Tick=20秒を観測した。

## 現在の式とコード経路

### Boat

`Private/Game/TRSimulationWorldSubsystem.cpp::StepRegisteredBoat`が船の現位置XY/TickでOceanを取得する。`Private/Ocean/TROceanWorldSubsystem.cpp:69`が指定深度潮、深度0表層潮、独立風を別に評価する。

`Private/Boat/TRBoatDriftComponent.cpp:29`の各船首/横軸で、Fishing中は

`I dv/dt = Kw(w-v) + Kc(c_surface-v) - Drag*v`

`Kw=WindResponse×Bow/Stern/SideScale`、`K=Kw+Kc+Drag`、`rate=K/I`、`target=(Kw*w+Kc*c_surface)/K`。

`v_next=v_prev+(target-v_prev)*(1-exp(-rate*dt))`。位置も同じ指数解を積分する。風/潮/抵抗のSnapshot寄与は当Tickの積分Δvであり、終端速度の成分や『目標速度』ではない。FishingはEngine/Assist0。Navigation式/Mode解除契約は変更しない。

保存値: Wind=(0,2)m/s、Surface=(0.360111111,0,0)、Heading0、WindResponse0.1/CurrentResponse1/Drag1 kg/s、Inertia10kg、Bow1/Stern0.5/Side2。定常Xは0.360111111/2.1=0.171481、Yは0.2×2/2.2=0.181818。風による横流しは小さすぎて消失しているわけではない。

### Egi・水中ライン

`Private/Fishing/TREgiSpatialSimulation.cpp:55`はEgiの現WorldPositionから深度を導出し、毎substepそのXY/深度の潮を取得。水平直接応答r=装備Profile.HorizontalResponsePerS、鉛直はVerticalResponsePerS。Target=(CurrentAtDepth.X,Y,−SinkSpeed×StateSinkScale+ResidualLift)。直接BoatV加算はない。

同77〜106行:

`wetLength=straightDistance*(1-wetStart)`

`dragRate = LineDragKgPerMS*wetLength*Transfer/totalMassKg`

`forcing = sum_i (current_i - rodVelocity*(1-t_i))/3`

`endWeight=sum_i t_i/3`

`rate=r+dragRate*endWeight`

`equilibrium=(r*target+dragRate*forcing)/rate`

指数解でV/候補位置を進め、ライン球/海面/海底へ整合。tautでは同191行の `outward=dot(V−RodVelocity,N)+actualTautReelMps` が正なら法線方向成分だけを除く。Boat/Rod motionがラインを通して影響する正式経路で、Egi全ベクトルをBoat速度へ置換しない。

水中代表点はwet segmentの1/6、1/2、5/6中点。各点XY/深度で再問い合わせし、風やBoat Surface sampleを流用しない。Baseline20秒末尾の深度4.647/13.940/23.233mすべて同じ0.360111111m/sなのはConstant設定による。

保存35g: SinkSpeed0.700000048m/s、水平応答1.875/s、鉛直2/s、LineDrag0.0001 kg/(m s)、TautTransfer0.05/SlackTransfer0、SlackAllowance0.1m、StaySinkScale0.2。末尾wetLength31.091m、dragRate0.004442/s、endWeight0.525666、line側rate寄与0.002335/s（直接1.875の約0.125%）。この条件でLine drag過大は支持されない。拘束補正0.000065646m/Tick、候補水平V=(0.278154,0.070371)、内部拘束後=(0.275524,0.072632)と部分的な追従を確認。『過剰』かどうかは未確定の受入値によるためバグと断定しない。

## 診断角度・相対速度の正式定義

RodTip→Egiの `H=horizontal separation`、`V=abs(vertical separation)`、`D=sqrt(H²+V²)` から `LineAngleToSurfaceDeg=atan2(V,H)`、0〜90°。45°ならH=V。実LineLength、Required（海面span/MinLineを含む）、Slackとは別。既存LineAngleRadは鉛直からの角度として互換維持（正常下方配置では両角の和90°）。端点一致/非有限/未評価/異Tickなら診断無効。

`RelativeHorizontalVelocity=BoatVelocity.XY−EgiVelocity.XY`。船−Egi水平分離ベクトルnについて `SeparationRate=dot(normalize(n),RelativeVelocity)`。相対速度の大きさだけでは距離増減を判定しない。ゼロ距離の瞬間の径方向率は0とし時系列を併記する。Boat/Egi位置m、速度m/s、角度degree、表示境界だけcm。

## 保存標準30m Cast（Port・3.5号35g・シンカーなし）

| Phase | Tick | 投入後s | Depth m | Boat H m | Rod H m | V m | 海面角° | Line m | Required m | Slack m | Boat Vxy m/s | Egi Vxy m/s |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| FreeFall | 25 | 0.017 | 0.000 | 3.057 | 0.000 | 1.509 | 89.995 | 1.575 | 1.509 | 0.066 | (0.007, 0.008) | (0.006, 0.000) |
| BottomContact | 2626 | 43.367 | 30.000 | 13.772 | 11.281 | 31.509 | 70.301 | 33.573 | 33.467 | 0.106 | (0.171, 0.182) | (0.360, 0.000) |
| Jerking | 2627 | 43.383 | 30.000 | 13.776 | 11.285 | 31.509 | 70.294 | 33.573 | 33.469 | 0.105 | (0.171, 0.182) | (0.360, 0.000) |
| Stay | 2651 | 43.783 | 28.524 | 13.337 | 10.840 | 30.033 | 70.153 | 32.773 | 31.929 | 0.844 | (0.171, 0.182) | (-0.499, 0.695) |

DeployingはTick24、着底2626、Jerking2627、TF/Stay2651。海面Z0、RodTipZ1.508521m。環境/重量は全Castで凍結、表層/深度潮0.360111111m/s、風(0,2)m/s。TFとStayの同Tickイベント計測値は最初のStay行と同じであり、TF専用の物理stepではない。

## Stay20秒時系列（Port）

| Stay後s | Boat XY m | Egi XY m | Boat H m | Rod H m | 海面角° | Depth m |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | (6.724, 7.169) | (15.749, -2.651) | 13.337 | 10.840 | 70.153 | 28.524 |
| 1 | (6.895, 7.350) | (15.727, -2.342) | 13.113 | 10.610 | 69.858 | 27.419 |
| 2 | (7.067, 7.532) | (16.029, -2.294) | 13.300 | 10.796 | 69.515 | 27.390 |
| 3 | (7.238, 7.714) | (16.380, -2.287) | 13.550 | 11.046 | 69.160 | 27.508 |
| 4 | (7.410, 7.896) | (16.739, -2.286) | 13.809 | 11.305 | 68.805 | 27.644 |
| 5 | (7.581, 8.078) | (17.098, -2.286) | 14.071 | 11.566 | 68.454 | 27.784 |
| 6 | (7.753, 8.259) | (17.459, -2.286) | 14.332 | 11.827 | 68.108 | 27.924 |
| 7 | (7.924, 8.441) | (17.819, -2.286) | 14.594 | 12.088 | 67.767 | 28.064 |
| 8 | (8.096, 8.623) | (18.179, -2.286) | 14.855 | 12.350 | 67.430 | 28.204 |
| 9 | (8.267, 8.805) | (18.539, -2.286) | 15.117 | 12.611 | 67.099 | 28.344 |
| 10 | (8.439, 8.987) | (18.899, -2.286) | 15.378 | 12.872 | 66.772 | 28.484 |
| 11 | (8.610, 9.169) | (19.229, -2.261) | 15.601 | 13.095 | 66.450 | 28.535 |
| 12 | (8.782, 9.350) | (19.515, -2.198) | 15.766 | 13.259 | 66.136 | 28.463 |
| 13 | (8.953, 9.532) | (19.799, -2.134) | 15.929 | 13.421 | 65.826 | 28.391 |
| 14 | (9.125, 9.714) | (20.082, -2.069) | 16.090 | 13.581 | 65.519 | 28.318 |
| 15 | (9.296, 9.896) | (20.364, -2.002) | 16.250 | 13.740 | 65.213 | 28.246 |
| 16 | (9.468, 10.078) | (20.644, -1.935) | 16.408 | 13.897 | 64.911 | 28.173 |
| 17 | (9.639, 10.259) | (20.924, -1.866) | 16.564 | 14.053 | 64.610 | 28.099 |
| 18 | (9.811, 10.441) | (21.202, -1.796) | 16.719 | 14.206 | 64.312 | 28.026 |
| 19 | (9.982, 10.623) | (21.479, -1.725) | 16.872 | 14.359 | 64.016 | 27.952 |
| 20 | (10.153, 10.805) | (21.755, -1.653) | 17.024 | 14.510 | 63.722 | 27.878 |

最初の1〜2秒はShakuriの既存過渡Velocityで一度浅くなる。その後重さで沈下し、約11秒からtaut拘束で徐々に上昇する。Depthが完全固定でないことは正式契約。末尾Line32.773445m、Required32.773445m、Slack0。StarboardはBoat H9.824→13.273m、Rod H10.863→14.502m、海面角70.153→63.736°、Depth28.588→27.882m。舷によるBoat中心とのoffsetと竿先間幾何を区別する。

## A〜E試験環境（Transientのみ）

すべて保存30m/Heading0/Boat/Egi係数を維持。A〜D Surface=(0.36,0)m/s。A〜C Wind=(2,0)、D Wind=(-2,0)、E無風無潮。DepthProfileの0/30mキーはA=(0.36,0)/(0.36,0)、B=(0.36,0)/(0.04,0)、C=(0.36,0)/(0,-0.20)、D=(0.36,0)/(0.15,0)、E=(0,0)/(0,0)。成分線形補間。0.36m/sは試験用で、保存0.7knotの0.360111111とは区別する。

下表は35g、各自身の最初のStayから20秒。

| Case | Bottom秒 | Boat Vxy | Egi Vxy | 相対速m/s | Boat H開始→末 m | 海面角開始→末° | Depth変化m |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A | 43.367 | (0.224, 0.000) | (0.311, -0.000) | 0.087 | 7.686→9.642 | 77.803→74.241 | 0.382 |
| B | 43.367 | (0.224, 0.000) | (0.067, -0.000) | 0.158 | 3.057→4.012 | 89.994→83.914 | 0.740 |
| C | 43.367 | (0.224, 0.000) | (0.052, -0.138) | 0.221 | 7.042→11.015 | 82.151→73.940 | -0.014 |
| D | 43.367 | (0.076, 0.000) | (0.111, -0.000) | 0.035 | 8.836→9.843 | 75.583→73.998 | 0.603 |
| E | 43.383 | (0.000, 0.000) | (0.000, -0.000) | 0.000 | 3.057→3.057 | 90.000→90.000 | 0.914 |

Cの末尾3ライン点Currentは(0.303247,-0.031530)/(0.189741,-0.094589)/(0.076234,-0.157648)m/s、Egiは(0.019493,-0.189170)。実点の異なる深度から別値が返っていることを独立Ocean再問い合わせと照合した。Bも0.308207/0.204622/0.101037と深度差が出る。Boat Surfaceは0.36のまま。

AはEgiがBoatより+Xへ速く、BではBoatが先行するが、Drop中の相対位置が近づいてStay開始Rod H約0.003mから始まるため20秒ではまだほぼ鉛直。Cは相対距離が育つ。Dは風が潮に逆らいBoat速0.076m/s、Eは流れ不足でRod Hほぼ0・海面角90°。『同方向』だけでは良いドリフトを判定できない。

## 保存同一環境の重量比較

30/35/40gはいずれもシンカーなし。90gは既存4号40g＋既存Sinker50、追加調整値なし。

| 総重量g | Bottom秒 | Stay Depth開始→末 m | Depth変化m | Boat H開始→末 m | 末海面角° | 末相対速m/s |
| --- | --- | --- | --- | --- | --- | --- |
| 30 | 50.500 | 28.564→27.688 | -0.877 | 15.150→18.823 | 60.808 | 0.150 |
| 35 | 43.367 | 28.524→27.878 | -0.646 | 13.337→17.024 | 63.722 | 0.151 |
| 40 | 38.017 | 28.501→28.045 | -0.456 | 11.979→15.630 | 66.072 | 0.149 |
| 90 | 17.183 | 28.466→29.172 | 0.705 | 6.467→7.731 | 80.620 | 0.030 |

30gほど落下に時間が掛かり、相対距離が育つ時間が長い。40gは早く着底するためその時点の距離が小さい。初期姿勢/一回のShakuriも同じだが、Stay開始時の位置/速度まで同じではないため、この比較を『重量だけの定常平衡曲線』と主張しない。90gは沈下側、30gはより上昇側。最適重量/重量過多の製品境界は未確定。A〜Eの30/35/40g全15結果はCSVに保存。

## 非理想条件と角度分布

FastBoatはWind=(0,8)、Surface=(0.36,0)、30mDepth=(0.15,0)。StrongDepthはWind=(0,2)、Surface=(0.36,0)、30mDepth=(1,0)。どちらもTransient試験のみ、既存安全限界内。

| 条件 | 海面角開始→末° | Depth変化m | Boat H開始→末 m | 末Boat速m/s | 末Egi速m/s |
| --- | --- | --- | --- | --- | --- |
| FastBoat_TestOnly | 47.435→37.449 | -4.964 | 31.019→36.316 | 0.747 | 0.551 |
| StrongDepthCurrent_TestOnly | 53.180→40.035 | -5.589 | 24.339→30.976 | 0.250 | 0.405 |

これらでは40〜50°へ入るが5m前後の大きい上昇を伴うため、角度だけでGoodとは判定できない。強いDepth CurrentではEgiが先行する。流れすぎ/流れ不足/軽量上昇/重量沈下を残す必要がある。

下表は各20秒観測窓の全固定Tick、中央値は並べた中央の1値。

| 条件 | 最小° | 中央値° | 最大° | 40〜50°滞在割合 |
| --- | --- | --- | --- | --- |
| Baseline_Port | 63.722 | 66.772 | 70.153 | 0.000% |
| Baseline_Starboard | 63.736 | 66.782 | 70.153 | 0.000% |
| CaseA_Egi_3_5 | 74.241 | 75.955 | 77.803 | 0.000% |
| CaseB_Egi_3_5 | 83.914 | 86.916 | 89.994 | 0.000% |
| CaseC_Egi_3_5 | 73.940 | 78.079 | 82.151 | 0.000% |
| CaseD_Egi_3_5 | 73.998 | 74.719 | 75.583 | 0.000% |
| CaseE_Egi_3_5 | 90.000 | 90.000 | 90.000 | 0.000% |
| FastBoat_TestOnly | 37.449 | 42.075 | 47.435 | 71.524% |
| StrongDepthCurrent_TestOnly | 40.035 | 45.731 | 53.180 | 80.183% |
| Baseline_90g_TestEquipment | 80.620 | 81.371 | 83.090 | 0.000% |

## 原因候補の分類

| 候補 | 観測・分類 |
| --- | --- |
| Boat/Egiが同じ値をsample | 保存Constantでは潮が同値。ただし別XY/depth問い合わせ。Profile Caseで別値を使用することを実検証。設定上の主因 |
| BoatV直接加算 | 該当なし。実コードと候補/拘束後値から否定 |
| Lineが全速度をコピー | 該当なし。法線方向の片側拘束だけ。末尾相対速度0.151m/sが残る |
| Line拘束の部分追従 | 確認。必要な物理契約。90gでは結果として近い速度になるが、過剰かは今後の受入で評価 |
| Line drag強すぎ | 保存35gではrate寄与0.125%程度で支持なし。単に増減して角度を合わせない |
| 深度Profile差実質0 | 保存はConstant、厳密に差0。revision2だけでは深度Profile有効を保証しない |
| Wind寄与小さすぎ | 保存横流しY0.182m/sで潮によるX0.171と同程度。全体で無視できるほど小さくない |
| Boat Current過剰追従 | 係数/抵抗によりXは表層の約47.62%。即時一致/100%追従ではない |
| Egi Current応答強すぎ | 35g r=1.875/sで半減時間約0.370s。slackでは0.36へ戻る。程度の適否はR7B。応答率を下げるだけではslackの定常Target自体は変わらない |

## なぜ標準20秒で45°にならないか

末尾Rod H14.510m / V29.387m=0.49375、海面角63.722°。現在の鉛直差を保つなら45°に必要なHは約29.387mで、現在の倍程度。現在のtaut Line32.773mを保持する場合はH=V≈23.175m、Depth≈21.666mが45°幾何となり、現Depth27.878mから約6.2m浅くなる。角度だけ合わせるとレンジを犠牲にする。

FreeFallでは必要距離＋0.1mを供給するため、差のない深度潮でもEgiが流れる。着底時から既にRod H11.281m、その後の0.8m要求と既存ShakuriでLine/速度が変わる。Stay20秒では相対移動時間が短く、方向・重量・初期位置も影響する。より長いStayで45°へ入るかは今回の20秒窓だけから断定しない。

## R7Bへの具体的提案（未確定・未実装）

1. 第一に保存OceanのConstantを、同じ表層潮でも弱い/異方向の深度Profileへ明示変更する候補を比較する。Wind方向/強さ、Surface/Depthを別々に固定・記録する。Case BのようにDropで相対位置が一度相殺されることも含め、最初のStayだけでなく着底時間を含む全Castで評価する。
2. BoatのWindResponse、SideWindScale、CurrentResponse、Drag、Inertiaを独立Sweepし、定常のBoat−Depth Current相対速度と応答時間を測る。Engine/Nav/Boost係数をFishingへ持ち込まない。速度ClampでGoodへ強制しない。
3. Egi ProfileのSinkSpeedByTotalMass、HorizontalResponseByTotalMass、StaySinkScaleを、同一深度/Line状態からの比較試験で整理する。直接応答率変更は時定数変更であり、定常潮速度の変更ではない点を維持する。
4. LineDragKgPerMS、Taut/SlackTransferは現在弱い。先に環境/Boat/sinkバランスを測り、直線3点近似の寄与を別記録してから必要な場合のみ調整する。拘束による正当な端点整合を切断せず、EgiへBoat速度を加えない。Reel/必要span/安全閾値を角度合わせの手段にしない。
5. RangeError/RangeStability、BITEは未実装のまま。Depth差/DepthVelocity/観測時間を将来評価へ渡し、45°と深度安定を別指標として扱う。

受入案: 代表Good条件を事前固定（30m、特定Wind/Profile/Heading、標準装備、投入/着底/1Actionの固定入力列）。最初のStayは海面角50°超、一定評価窓でBoat側へ相対分離が育ち、例としてStay20〜60秒のどこかで40〜50°へ入り、連続5〜10秒で大きなレンジ変化なく維持する。暫定候補としてその窓のDepth変化幅≤1m、平均絶対DepthVelocity≤0.05m/sを比較検討する。時間/数値/重量はまだ決定しない。現在の結果がこの案へ合格したとは扱わない。

Calmは角度が立ったまま、強いDepth/過大風/重量過多/軽量は角度またはレンジ安定から外れてよい。全部の条件を40〜50°へ強制しない。NaN/Inf/海底貫通/80m超異常繰出し/Technical Abort0は独立した品質ゲートとして維持する。

## Snapshot / Insert HUD

新FTRRelativeDriftSnapshot: 同Tick有効性、Boat/Egi水平Velocity、Boat−Egi相対Velocity、分離径方向率、Boat/Rod水平距離、鉛直差、直線距離、LineAngleToSurfaceDeg。Sessionはコピーから導出するだけで、Simulationへ入力しない。Quick/未活動Cast/未評価/異Tickは無効として表示。

FTREgiDriftObservation: 成功Tickの最後のsubstep番号、実Egi問い合わせ位置/深度/潮、実ライン水中3点の位置/深度/潮、wet長、Transfer、dragRate/endWeight/水平応答、Rod速度、line forcing、equilibrium、拘束前候補V/拘束後内部V。失敗TickのSnapshotを確定しない。Boat Snapshotには環境問い合わせ元Positionを追加、既存Wind/Current/DragのΔvを保持。

Insert詳細へ『相対ドリフト監査（同Tick・読取専用）』を追加。風/表層/深度潮、Boat/Egi/相対V、距離/角/長/Required/Slack/重量、Boat寄与、最後substepの抗力/候補/拘束後を表示。通常HUD/操作案内/レイアウトは維持。3点配列はSnapshot/CSVから取得可能。Camera/Rod/Shakuri/Reelのロジックを表示側へ複製しない。

## Build / Automation / 保全

最終採用**108件すべて成功（新規R7A4＋関連104）、FAIL/errors/notRun0**。単一108件の一括成功ではない。R7AFinalの非R7A87件、R7AVerifiedの新R7A4件、R7AInputRegressionのR3/R4入力17件を試験ID別に採用。`Saved/Automation/R7AAcceptedResults.json`へ採用元を保存。

新規: SavedPrototypeDropToStay（両舷35g＋標準環境30/40g）、DirectionAndWeightAudit（A〜E×3重量）、NonIdealDriftAudit（風強/深度潮強/既存90g）、ReadOnlyAndFixedTickReplay（独立45°校正・異Tick拒否・HUD副作用なし・30/60/120fps）。全22Castは着底/1Sequence/TF/Stayへ到達、20秒観測、Technical Abort0、有限数/ライン整合/サンプル照合成功。理想角/最適重量へ合格させる試験ではない。

関連: R4A1〜A5の21件、旧R4入力/Geometry/Screen11件、R3 Side6件、R5/5A/5B/5Cの20件、R6の4件、D5/E7/F6/G6/B5/M046/M097件。正常42はnormalResults42/aborted0/active0。Intentional NaN→Aborted→N→Ready、古いCast/登録拒否成功。Root/2m/Base/Mouse/Camera/UI/Pause/Focus/Sequence/0.8m/Reel/Quickを保持。無関係な全Egi物理/R9/H統合は実施していない。

採用試験の警告4件はEngine外部HTTP到達確認 `generate_204` の3秒timeout（R5B Extreme、R5C Cadence/Repeated、R7A Baseline）。物理異常ではないが隠していない。起動時に既存Python bScreenControl/ScreenControl名衝突、EditorLayout/非Win64 SDK診断等がある。Buildの既存MSVC推奨版差・IncludeOrder通知を保持。R7A由来C++警告なし。

UE5.8.2 / MSVC14.51.36257 / Windows SDK10.0.22621.0。R7ABuild1ではUHT11生成ファイル（新しいRelativeDrift型含む）と変更Runtime/モジュールを実コンパイル。最初の監査fixtureでTObjectPtrへauto*推論したC++エラーを.Get()で修正し、R7ABuild2成功。R7AFinalBuildとR7AExtendedBuildでは最終監査fixtureを実C++/LinkしDevelopment Editor Win64成功。up-to-dateだけで確認していない。

初回R7AFirstは1成功/2失敗。全Cast物理は正常だったが、TFをPublish時のStateだけで捕捉しようとした試験の欠陥。状態イベントと観測Snapshot Tickを別記録し、TF→Stayの同Tick遷移を正しく観測して同じ状態経路のassertを維持。Safety/物理/期待角度の緩和で通していない。途中FAIL/BuildエラーログはSavedへ保持。

開始時Git clean。Config/Content全20ファイルは開始時SHA256と一致、追加/削除/変更なし。物理式/数値順序/全調整係数/Action/Camera/Rod/Inputは変更せず、計算途中のコピーと成功Snapshotへ診断を追加した。git diff --check成功。

変更Source9ファイル: Public/Data/TRRelativeDriftTypes.h（新規）、TRSnapshots.h、TRHUDSnapshot.h、Private/Boat/TRBoatDriftComponent.cpp、Private/Fishing/TREgiSpatialSimulation.cpp、Private/Game/TRFishingSessionActor.cpp、Private/UI/TRPrototypePresentation.cpp、Private/Tests/TRRuntimeRodIntegrationTests.cpp、TRRelativeDriftRuntimeAudit.inl（新規）。

文書9ファイル: AGENTS.md、本記録、R6_SLACK_AWARE_REEL.md、FISHING_SYSTEM.md、UI_SPEC.md、BOAT_SYSTEM.md、OCEAN_SYSTEM.md、GAME_DESIGN.md、ROADMAP.md。R6正式受入、診断定義、R7A実測/未確定提案、R7B未着手を記録する。Saved/Intermediate生成物はソースへ追加しない。

## 手動確認は任意の観測のみ

保存Prototype→Enter→A/D→Enter→Deploy、着底→右1回→20秒観測→Insert。表の海面角と旧『鉛直から』を区別し、重量35g/シンカーなしを明示する。Boatに追従するCameraで見える相対表示と、固定Grid/CSVのWorld移動を混同しない。今回GUI PIE/GPU/実OS Mouseは実施せず、手動品質合格を宣言しない。R7Aの監査はR7Bで再調整するための測定であり、後続へ自動進行しない。
