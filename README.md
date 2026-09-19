# Arrietty Cesium

UE 5.8 と Cesium の実世界地形を使う、ローイングと人力飛行のフィットネスシミュレーターです。
同じリポジトリに **row / fly の独立したUEプロジェクト**を保持します。

## 起動

```powershell
./row.ps1 "Koh Hong"                       # 海でローイング
./row.ps1 "中禅寺湖"                        # 湖でローイング
./row.ps1 "中禅寺湖" -mag 5                 # 地形上の移動を5倍にする
./reco.ps1                                # 世界のおすすめ水面31か所から番号で選択
./reco.ps1 14                             # 14番をコメント・半径・倍率付きで通常起動
./reco.ps1 -List                          # 一覧だけ。通信・地形ダウンロードなし
./fly.ps1 "Funafuti International Airport Tuvalu"  # 地上から離陸
./fly.ps1 "富士山" -StartMode Air           # 地表から100m上で発進
./fly.ps1 "富士山" -StartMode Air -StartAglM 200
./fly.ps1 "富士山" -StartMode Air -StartAglM 100 -mag 10
```

場所を表示した後、`Y`を入力すると準備・起動します。`N`、空入力、入力終了は起動せず終了します。
山岳で地上発進に適した場所がない場合は、`-StartMode Air`を指定してください。
場所の解釈を更新する場合は`-RefreshPlace`を指定します。

おすすめ水面の範囲は出発点からの半径です。倍率は等倍10 km/hの漕ぎ方で約10分の直進を
目安に設定しています。岸が先にある場合を除きます。詳細と出典は
[おすすめ水面](docs/ROW_RECO.ja.md)・[カタログ](reco.json)を参照してください。

通常はインターネット接続が必要です。Cesiumの地形と衛星画像を読み込みます。
RowのPS4/WT901入力では選択中のOpenXRランタイムを使用します。Quest 3はMeta Quest Link /
Air LinkとMeta OpenXR、VIVEはSteamVR OpenXRです。従来のTracker入力にはSteamVRが必要です。
Flyは選択中のOpenXRランタイムを使用します。Quest 3ではMeta Quest Link / Air Linkへ接続し、
Meta側を有効にします。標準の自転車入力は有線操作盤とBLEで、SteamVR・VIVEは不要です。
rowとflyの実機モードは同時に起動できません。機器設定は移植時にそれぞれへ引き継ぎ済みです。

実機を接続しない確認:

```powershell
./row.ps1 "Lake Bled" -Demo
./fly.ps1 "富士山" -StartMode Air -Offline
./fly.ps1 "Funafuti International Airport Tuvalu" -Offline -SmokeTest -Headless
```

`-Offline`は実機・VRを使わない意味です。Cesium地形にはネット接続を使います。
`-Demo`はflyでは`-Offline`の別名です。自動走行・飛行には`-SmokeTest`を指定します。

## Row

従来の`run.ps1`が`row.ps1`になりました。引数・ローイング・VR校正・操舵・水面・音は維持します。

- Enter: 開始・一時停止・再開。2秒の準備、1秒の静止、バーの往復2回で校正します。
- 校正中のEnterで進捗を取り消しません。テンキー0で中止できます。
- 左右への体の移動で操舵。頭の向きだけでは曲がりません。左右8cmは直進域です。
- テンキー0: 終了・記録・開始位置へ戻ります。
- 機器の負荷設定は読み取りのみ。心拍不明は`--`です。

PS4コントローラをバーの加速度・ジャイロ入力にする場合は、**□で開始・休止・再開、
△で中止・終了・開始位置へ戻る**操作です。L1/R1で通常旋回、L2/R2で強い旋回を行い、
離すと直進します。Quest 3/VIVEとも同じ操作で、PS4モードでは頭の移動では曲がりません。
走行中のPS4通信断は、有効な受信とHMD追跡が戻れば自動復帰します。手動休止・終了は維持します。
設定、取り付け、検証の限界は[PS4でのローイング](docs/ROW_PS4.ja.md)を参照してください。

購入済みの Waterline Gen 4 をローカル導入した環境では、その波と水面素材を使用します。
`-LegacyWater` で従来の水面へ戻せます。導入と対応範囲は
[Waterline 水面](docs/ROW_WATERLINE.ja.md)を参照してください。VR 実機確認は未完了です。

`-mag`（正式名`-Magnification`）で地形上の移動を1～10倍にできます。省略時は1倍、
`-mag 2.5`のような小数も使えます。等倍で10km/hの漕ぎ方なら、5倍では約50km/h相当です。
計器の速度（倍率指定時は`WORLD km/h x5`など）と距離は倍率後の値です。
漕ぐ力の計算、校正、左右への体の移動による旋回速度、波・音の強さは従来通りです。
旋回半径は倍率に応じて大きくなり、航行範囲の境界へも早く到達します。
水面の高さと曲率は維持し、岸・島・航行範囲は倍率後の進路全体で確認します。

ローイングマシンの負荷は**1～16**です。受信した負荷をゲーム用の出力補正に使います。
`-mag`はこの負荷補正とは別の移動倍率です。本体から0 Wを受信したときは、
負荷11～16でも推進力を加えません。負荷が未受信・範囲外・3秒以上更新されないときは
`LOAD -- (x1)`となります。アプリから負荷を変更しません。

バーのTracker 3.0の代わりにWT9011DCLのBLE加速度・姿勢通知も使用できます。
IMUモードでは正面を向いてバーを前へ出して静止し、最初に手前へ引いて2往復で校正します。
設定と検証範囲は[WT9011DCLによるバー検出](docs/ROW_IMU.ja.md)を参照してください。
HMDの左右操舵は維持し、IMUの通信断では停止します。

```powershell
./row.ps1 "Lake Bled" -WaterLevelM 475     # 湖面の海抜標高を明示
./row.ps1 "Koh Hong" -Volume 0.5 -RadiusKm 5
./row.ps1 "中禅寺湖" -PrepareOnly
./row.ps1 "Koh Hong" -ResolveOnly
```

海と湖に対応し、川・河川流・潮汐には対応しません。標準の航行範囲は開始点から半径3kmと
取得済み水域の共通部分です。`-RadiusKm`は1～10km。境界で停止します。
湖面の海抜標高にEGM96補正を加えて楕円体高にし、開始地点の平均水面をUEのZ=0へ対応させます。
原点から離れた水面は地球曲率とジオイド傾斜に沿います。未知の湖面を海抜0mとして扱いません。
湖の出発地点は原則として岸から150m離します。湖内の平坦な地形面が既知の湖面より高い場合は、
5点のばらつき25cm以内・高さのずれ50m以内に限り、Rowの地形表示と衝突面を下げて合わせます。
湖面標高は変更しません。条件を満たさない地形や未取得の高さは、引き続き開始を止めます。

## Fly

Arrietty-UE58の飛行計算・機器処理・計器・操作を引き継ぎ、風景をCesiumに変更しています。
Blender、Secret-World、移植元のチェックアウトは起動・再構築に不要です。

1. 場所をYで確認したらHMDを装着し、地形の準備を待ちます。日時を指定する場合は起動引数を使うか、準備中に編集してApplyします。
2. **地形準備後、自動で機器準備に進みます。Pキーは不要です。** 未適用の日時変更がある場合は適用まで待機します。
3. 自転車の正面を見て左スティックを中央にし、**Button 1**で整列・走行開始します。その瞬間の視線を前進方向にします。
4. 地上発進では**Button 2**で飛行モードへ入り、漕いで速度を上げ、**Button 3+4**でピッチを上げて離陸します。
5. **Esc**で機器を停止してCSVを保存し、準備画面へ戻ります。停止後は自動再開しません。再開は画面の「Resume preparation after stop」、ウィンドウを閉じると終了します。

QuestとVIVEは同じUI・開始手順です。VIVEのHMDを選んでも、標準の操舵は左Joystick 1で、ハンドル用トラッカーは使いません。

| 操作 | 動作 |
|---|---|
| Button 1 | 開始時はHMD整列・操舵中央確認。開始後は確認済みの軌跡を約2m戻る |
| R | 今見ている方向を新しい前進方向にする。位置・経過時間を維持 |
| Button 2 | 地上／人力飛行。空中では地上モードへ戻せない |
| Button 3 / 4 | 左／右ロールを1度変更 |
| Button 3+4 | 80ms以内の同時押しでピッチを1度上げる |
| Button 5 | 音声ブリッジのPTT |
| Button 6 | 押下中のT2負荷3% |
| Joystick 2 | 倒す1操作でピッチ／ロールを1度変更。SWでリセット |
| Joystick 1（左） | 左右で地上の操舵／飛行中のラダー。倒している間、倒した量に応じて旋回 |
| Joystick 1 SW | 従来の飛行調整を選択・確定。調整中の左右は値変更に使い、ラダーは中央固定 |

首を振っても進路は変わりません。左スティックを中央へ戻すとラダー入力は0になります。
右スティック・Button 3/4によるロール操作は従来通りで、ラダーと併用できます。
HMD追跡が無効、または操作盤の受信が0.5秒以上途切れた間は移動と風量を止めます。
開始・R整列・再接続・飛行調整終了の後は、一度左スティックを中央へ戻してください。
中央15%は直進域です。最大入力は従来のハンドル最大舵角と同じで、飛行特性は変えません。
心拍計の接続は開始条件ではありません。
Offlineでは上下矢印で模擬速度、左右矢印で操舵、数字列1～8で操作盤を入力できます。

`config/fly.local.json`の`steering_input`は標準`"joystick1"`です。
左スティックの上下が操舵になる場合は`joystick1_steering_axis`を`"x"`から`"y"`へ、
左右が逆なら`joystick1_steering_invert`を`true`へ変更し、Flyを再起動してください。
従来のVIVEハンドル操舵は`steering_input: "vive"`で選択でき、その場合はSteamVRとVIVEの追跡が必要です。
Quest 3の設定・実機チェックは[FlyのOpenXRと操作盤](docs/FLY_OPENXR.ja.md)を参照してください。

### 発進と高さ

- 地上発進では指定地点から約1km以内の候補を調べ、周囲50mの標高差とOSM水域から平坦な陸地を選びます。
  地形の解像度や水域境界によって発進できない場合があります。滑走路を人工的に平坦化しません。
- 空中発進は指定地点の地表から`-StartAglM`（標準100m、10～3000m）上です。初速24km/hで飛行状態に入ります。
- 選ばれた発進地点の地表がUEのZ=0です。内部高度は発進地点の楕円体高からの差で、負の値も許容します。
- 計器の**MSL**は海抜高度、**AGL**はCesium地形面からの高さです。飛行中、山に合わせて機体を自動上昇させません。
- 緩い陸地への穏やかな下降は着地します。急斜面・水域・障害物への接触は停止し、Button 1で復帰します。
  復帰先も標高・水域を再確認し、確認できる履歴がなければEscで再開してください。
- 飛行範囲は開始地点から半径10km（`-RadiusKm`で1～10km）。地形が未取得の進行先では待機します。

Cesium World Terrainの最詳細標高を周囲120m四方・10m間隔で先読みし、経路を1m以下の間隔で検査します。
描画された地形への前方スイープも併用します。これは地形データの解像度による近似で、細かな道路・建物・樹木を
再現する地上自転車シミュレーターではありません。高所の空気密度による難易度変更は行わず、元の飛行特性を維持します。
flyの水面はCesium側の地形・衛星画像の表示です。row独自の波はrowで維持しています。

### 移動倍率

`-mag`（正式名`-Magnification`）で、地形に対する移動を1～10倍にできます。
省略時は1倍です。`-mag 2.5`のような小数も指定できます。

- 前後・左右・上下の**毎フレームの移動差分**に倍率を掛けます。滑空経路の傾きを保ちます。
- 初期対地高度は変わりません。`-StartAglM 100 -mag 10`でも地表100mから開始します。
- 漕ぐ出力、飛行計算上の対気速度・失速、姿勢や旋回の変化速度、実機の負荷・風量は等倍です。
  地上走行にも移動倍率が掛かります。旋回角速度を保つため、地形上の旋回半径は大きくなります。
- `AIR`は飛行計算上の対気速度、`WORLD`は倍率適用後の水平移動速度です。
  水平飛行の`AIR 20 km/h`は、10倍なら`WORLD 200 km/h`相当になります。
  `DIST`は地形上の移動距離、`V/S`は倍率適用後の上昇・下降率です。
- 衝突は倍率適用後の経路で検査します。急な下降は衝突扱いになり、未取得の地形では停止して待ちます。
  1倍を超える場合は10mの標高間隔を保ったまま、取得範囲を240m四方に広げて進行方向を先読みします。
- 半径10kmの飛行範囲は変わりません。倍率を上げると境界へ早く到達します。

まず`-mag 3`～`5`で体感を確認し、山岳の広い景色では`-mag 10`も試せます。
移動倍率と速度・距離表示に加え、人力グライダーの音を再生します。

### 飛行の音

風2種類、ペダル伝達機構、足踏みプロペラ、車輪、接地の6種類を使用します。
漕ぐと伝達機構とプロペラが鳴り、漕ぐのをやめると減衰します。**滑空中は風音が続きます。**
風は`AIR`速度に連動し、`-mag`によって音量やピッチは変わりません。
車輪音は地上走行時、接地音は着地時に1回だけ鳴ります。
準備中・地形待ち・追跡喪失・停止中は消音します。

```powershell
./fly.ps1 "富士山" -StartMode Air -mag 10 -Volume 0.8
```

`-Volume`は0～1、標準0.8、0で消音です。素材の再取り込みと録音テストは
[飛行の音](docs/FLY_AUDIO.ja.md)を参照してください。

```powershell
./fly.ps1 "富士山" -StartMode Air -LocalDate 2026-09-12 -LocalTime 17:00
./fly.ps1 "富士山" -StartMode Air -PrepareOnly -Headless
./fly.ps1 "富士山" -ResolveOnly
```

現地時刻は場所のタイムゾーン・夏時間から決め、飛行中は固定します。
`-PrepareOnly`はUEを実機なしで起動して地形の確認まで行い、終了します。

## 設定・準備

UE 5.8.x、Visual Studio C++ / Windows SDK、Python 3.13が必要です。
Cesium 2.29.1・EGM96を共通管理し、各アプリのビルド出力・Python環境は独立させています。

```powershell
./tools/prepare.ps1             # 両方
./tools/prepare.ps1 -App Row
./tools/prepare.ps1 -App Fly
./tools/test.ps1                # 実機なしの自動テスト
```

- `OPENAI_API_KEY`: 場所検索用。モデルは`gpt-5.4-mini`、`-Model`または`ARRIETTY_OPENAI_MODEL`で変更できます。
- `config/cesium.local.json`: Cesiumの設定。`CESIUM_ION_TOKEN`環境変数が優先されます。
- `config/row.local.json`: ローイングの機器設定。
- `config/fly.local.json`: 自転車・飛行の機器設定。
- 新規環境では各`*.example.json`を対応する`*.local.json`へコピーします。
- rowの音素材は[音の準備](apps/row/sounds/README.md)を参照してください。
- flyは提供された6種類を使用します。[素材と再生の仕様](docs/FLY_AUDIO.ja.md)を参照してください。
  元素材は`wav_fly/`または`apps/fly/sounds/`に置き、準備スクリプトで再生用データを生成します。

個人設定・鍵・地理キャッシュ・ログ・運動記録・生成素材・ダウンロードしたプラグインはGit対象外です。
旧`Arrietty-UE58`と他の隣接プロジェクトは変更していません。

## 保存先と検証

Tapo P105 の手動電源操作は `./tapo.ps1` から利用できます。
設定とコマンドは [Tapo CLI](docs/TAPO.ja.md) を参照してください。

| 用途 | 場所 |
|---|---|
| Rowの実装 | `apps/row/` |
| Flyの実装 | `apps/fly/` |
| 共通地理処理 | `shared/python/arrietty_geo/`、`shared/unreal/` |
| Rowの正常起動ログ | `logs/row/training.log` |
| Rowの運動記録 | `apps/row/unreal/ArriettyCesium/Saved/Sessions/` |
| Flyの正常起動ログ | `logs/fly/training.log` |
| Flyの実機／模擬CSV | `logs/fly/latest-ue-flight.csv` / `latest-ue-offline.csv` |
| 場所・シーン | `cache/places/`、`cache/row/`、`cache/fly/` |

飛行CSVは従来の列を保ち、海抜高度・対地高度・楕円体高・緯度経度・高さ基準を追加しています。
`altitude_m`列は発進地点からの相対高度です。
`movement_magnification`、`world_speed_kmh`、`world_vertical_speed_mps`に倍率と
地形に対する速度を記録します。従来の`speed_kmh`は飛行計算上の速度、`distance_m`は地形上の距離です。

RowのCSVにも`movement_magnification`と`world_speed_kmh`を追加します。
Rowの`speed_kmh`は倍率前の艇の計算速度、`distance_m`は倍率後の距離です。
`resistance_raw`と`resistance_age_s`には、負荷の受信値と最終受信からの秒数を記録します。

現在の検証結果と実機確認の範囲は[検証記録](docs/VALIDATION.md)、設計は[統合仕様](docs/INTEGRATION.md)を参照してください。
公開前に`python tools/check_public_tree.py`と`python tools/check_public_tree.py --history`で検査します。

アプリケーションコードはMIT。Cesium、UE、地形・画像・OSM、同梱wheel、音は各利用条件に従います。
Cesiumの動的な帰属表示をデスクトップとHMDへ表示します。[第三者の権利表記](THIRD_PARTY_NOTICES.md)を参照してください。
