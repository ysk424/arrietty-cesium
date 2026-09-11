# Arrietty-cesium

UE 5.8 と Cesium を使い、好きな海・湖で漕ぐフィットネスゲームです。
Arrietty-row `316f4b2` のボート、ローイング判定、VR 操舵、波、効果音を引き継ぎます。
地形は Cesium World Terrain、衛星画像は Cesium ion、水面は UE の独自波シミュレーションで描きます。

## 起動

```powershell
cd "$env:USERPROFILE/git/arrietty-cesium"
./run.ps1 "Koh Hong"
```

OpenAI が場所を特定すると、例えば次のように表示します。

```text
タイ・クラビ県のホン島ですね？ [Y/N]:
```

`Y` で水域・標高を準備し、UE を起動します。`N`、空入力、入力終了なら起動せず終了します。
違う場所なら、国や地域を加えて再実行してください。同名候補の選択メニューはありません。
初回は地形とシェーダーの読み込みに時間がかかります。準備が終わるまで漕ぎ始めません。

```powershell
./run.ps1 "Lake Bled"
./run.ps1 "中禅寺湖"
./run.ps1 "Koh Hong" -Demo                 # VR・実機なし
./run.ps1 "Koh Hong" -Volume 0.5
./run.ps1 "Lake Bled" -WaterLevelM 475     # 湖面の海抜標高を明示
./run.ps1 "Koh Hong" -RefreshPlace         # 保存済みの場所解釈を更新
./run.ps1 "Koh Hong" -RadiusKm 5           # 局所シーンの航行範囲を拡張
./run.ps1 "Koh Hong" -PrepareOnly          # Y 確認・水域準備まで
./run.ps1 "Koh Hong" -ResolveOnly          # 場所の特定だけ
```

通常はインターネット接続が必要です。場所解釈と水域境界は `cache/` に保存します。
地形・画像は Cesium が実行中にストリーミングします。オフライン用地形の一括取得は行いません。
対象は海と湖です。川の傾斜、河川流、潮位の時間変化には対応していません。
標準の航行範囲は開始地点から半径 3 km と取得済み水域の共通部分で、境界では停止します。
`-RadiusKm` は 1–10 km。地球全体を無制限に航行するモードではありません。

## 操作

SteamVR を起動し、OpenXR ランタイムを SteamVR に設定してください。
MERACH MR-R02 (Q1S)、バー中央の指定 VIVE Tracker、HMD、心拍計は既存版と同じです。

- Enter: 開始・一時停止・再開。2 秒の準備、1 秒の静止、バーの往復 2 回で校正します。
- 校正中の Enter は進捗を取り消しません。テンキー 0 で中止できます。
- 左右に体を寄せると操舵。頭の向きだけでは曲がりません。左右 8 cm の直進域を維持します。
- テンキー 0: 終了・記録・開始位置へ戻る。
- 画面の距離・速度はゲーム内の船の値。心拍不明は `--`。マシンの負荷設定は読み取りのみです。

記録は `unreal/ArriettyCesium/Saved/Sessions/`。正常起動ログは `logs/training.log`。
機器設定は Git 対象外の `settings.local.json` に保存します。
詳しい漕ぎ方・波・音の技術資料は `docs/` にあります。

## 水面と標高

湖面の海抜標高と湖底の高さを区別します。OpenAI の Web 検索で得た湖面標高と出典を保存し、
出典が検索結果に確認できない場合は海抜 0 m に置き換えず、`-WaterLevelM` の指定を求めます。
海は平均海面 0 m を使います。

Cesium の高さは WGS84 楕円体高です。海抜標高に EGM96 ジオイド補正を加え、
湖・海の水面を CesiumGeoreference の原点に対応させます。
さらに局所の地球曲率とジオイド傾斜を近似し、船と UE 水面が同じ平均水面を使います。
波はその平均水面の周囲で変位し、HMD の水平線を波で揺らしません。

OpenStreetMap の湖・海岸線から水域を作り、島を水面から除外します。水域内の低い地形表面も描画から除外し、衛星写真に含まれる静止水面が UE の波と重ならないようにします。
開始位置には岸からの余裕を設け、Cesium の最詳細地形で周辺 5 点の高さを検査します。
この地形検査は湖面標高の代用ではありません。水面が地形の下になる場合やデータ取得に失敗した場合は開始を止めます。
海抜標高は公開資料に基づく代表値で、当日の観測水位ではありません。水位変動のある湖では手動指定が必要になることがあります。

## 設定と準備

初回セットアップでは、以下のキー・機器設定と音素材を用意してから実行します。再構築にも同じコマンドを使えます。

```powershell
./tools/prepare.ps1
```

UE 5.8、Visual Studio C++ / Windows SDK、Python 3.13 が必要です。
Cesium for Unreal 2.29.1、OpenVR、EGM96 補正グリッドを公式配布元から取得し、SHA-256 を検証します。
ボートと水面の素材は UE で生成します。音素材の準備方法は [sounds/README.md](sounds/README.md)。
以前の Lake Bled 地形プロジェクトは不要です。

- OpenAI: 環境変数 `OPENAI_API_KEY`。既定モデルは `gpt-5.4-mini`。
  `-Model` または `ARRIETTY_OPENAI_MODEL` で Web 検索・Structured Outputs 対応モデルに変更できます。
- Cesium: `cesium.example.json` を `cesium.local.json` にコピーして既存トークンを設定。
  `CESIUM_ION_TOKEN` 環境変数があれば優先します。標準の地形 ID は 1、画像 ID は 2。
- 機器: `settings.example.json` を `settings.local.json` にコピーして実機を指定。

キーはコマンドラインやシーン JSON に埋め込みません。設定、キャッシュ、運動記録、UE 素材、プラグインは Git 対象外です。
API・地形サービスの使用量は各アカウントの条件に従います。

## 検証

```powershell
./tools/build_native.ps1
./.venv/Scripts/python.exe -m unittest discover -s tests -p 'test_*.py' -v
./tools/test_setup.ps1
./tools/preview.ps1 -Scene <cache内のscene.json> -Name preview
```

現時点の結果と制約は [docs/VALIDATION.md](docs/VALIDATION.md)。
引き継いだ実装の検証範囲は [docs/UPSTREAM_VALIDATION.md](docs/UPSTREAM_VALIDATION.md) を参照してください。

公開前の検査は `python tools/check_public_tree.py`（ステージ全体）と
`python tools/check_public_tree.py --history`（全ブランチ・タグの履歴）です。

## 出典・ライセンス

アプリケーションコードは MIT。UE、Cesium、衛星画像・地形、OSM、音はそれぞれのライセンス・利用条件が適用されます。
Cesium の動的なデータ帰属表示を使用し、VR では HMD 内にも表示します。
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) を参照してください。

- [OpenAI Responses API Web search](https://developers.openai.com/api/docs/guides/tools-web-search)
- [OpenAI Structured Outputs](https://developers.openai.com/api/docs/guides/structured-outputs)
- [CesiumGeoreference の高さ基準](https://cesium.com/learn/cesium-unreal/ref-doc/classACesiumGeoreference.html)
- [Cesium 地形の高さ取得](https://cesium.com/learn/cesium-unreal/ref-doc/classACesium3DTileset.html)
- [EGM96 グリッド](https://cdn.proj.org/us_nga_egm96_15.tif)
- [OpenStreetMap の帰属](https://www.openstreetmap.org/copyright)
