# Fly: OpenXRと自転車の操作盤

映像とHMD姿勢はUEのOpenXR、自転車の入力は有線ESP32操作盤とBLEのT2から取得します。
標準ではVIVEハンドル・OpenVR・SteamVRを自転車入力に使いません。
Quest 3単体で動くAndroidアプリではなく、PCのUE映像をLinkで表示する構成です。

## Quest 3での確認

1. PCのMeta Horizon LinkとQuest 3をLinkケーブルまたはAir Linkで接続し、HMD内でLinkへ入ります。
2. Metaを有効なOpenXRランタイムにします。このPCでは別途用意した`openxr-switch`を使えます。
   Fly自身はランタイム設定を変更しません。切り替え後はFlyを再起動します。
3. 自転車の操作盤をUSB接続し、T2を起動します。
4. `./fly.ps1 "富士山" -StartMode Air`などで起動し、場所を確認して明示的にYを入力します。
5. HMDを装着したまま地形準備を待ちます。機器準備は自動で進むのでPは不要です。
   `READY | BUTTON 1: ALIGN AND START`が表示されたら、正面を見て左スティック中央でButton 1。
   `STR J1 READY`と`HMD OK`を確認します。
6. 小さな左右入力から確認します。中央でラダー0、左で左旋回、右で右旋回になることを確認します。
   次に右スティック／Button 3・4で傾け、ラダーを加えた旋回も確認します。
7. Rの向き合わせ、操作盤切断時の停止、再接続後の中央確認、Escの機器停止も確認します。

VIVEのHMDも同じUI・自動準備・Button 1開始です。HMDを切り替えても操舵設定は変わらず、
標準のJoystick 1を使用し、ハンドル用トラッカーは不要です。
地形の読み込み失敗、ブリッジ未接続、日時の未適用がある間は自動準備を開始しません。
Escや通信監視による停止後は自動再開しません。準備画面の「Resume preparation after stop」から再開します。
`-PrepareOnly`は従来通り地形確認だけで終了します。

公式の接続手順: [Meta: アプリ開発でLinkを使う](https://developers.meta.com/horizon/documentation/unreal/unreal-link/?locale=ja_JP)。
UEの対応: [Epic: OpenXR](https://dev.epicgames.com/documentation/unreal-engine/developing-for-head-mounted-experiences-with-openxr-in-unreal-engine)。
2026-09-19のローカル確認ではWindowsの32/64ビットOpenXR登録はMetaでした。
これはHMDの映像表示・接続品質・実機操舵の合格を意味しません。

## 左スティック

左をJoystick 1、右をJoystick 2とします。Joystick 1の中央15%は直進域、
それ以外は倒した量に比例します。最大舵角は従来と同じ15度です。
地上では自転車の操舵、飛行中は既存のラダーとして働き、エルロンの旋回に加算されます。
スティックを戻してもエルロンで指定したバンクは残ります。

配線／取り付け方向は実機で確認してください。`config/fly.local.json`へ設定を追加できます。
既存の機器設定は保持します。

```json
{
  "steering_input": "joystick1",
  "joystick1_steering_axis": "x",
  "joystick1_steering_invert": false
}
```

- 上下が操舵になる場合: axisを`"y"`へ。
- 左右が逆の場合: invertを`true`へ。標準では選択軸の正値を右にしています。
- VIVEハンドルを再使用する場合: inputを`"vive"`へ。従来の`steering_serial`も保持しています。

変更後はFlyを再起動します。省略時は上記の標準値を使います。
環境変数`ARRIETTY_STEERING_INPUT`、`ARRIETTY_JOYSTICK1_STEERING_AXIS`、
`ARRIETTY_JOYSTICK1_STEERING_INVERT`がある場合は設定ファイルより優先します。

## 待機と飛行調整

- `J1 CENTER`: スティックを中央へ戻してください。Button 1開始・R整列の後にも確認します。
- `J1 LOST`: 新しい操作盤入力がありません。移動・風量・飛行音が停止します。
  再接続後、倒したままでは再開しません。一度中央へ戻します。
- `J1 TUNE`: SWによる従来の飛行調整中です。左右で調整値を変え、ラダーは0に固定します。
  既存の調整4項目をSWで進めて終了すると、再度中央確認が必要です。

HMD追跡、Button 1で確定する前進方向、Rの向き合わせ、地形準備ゲート、
Button 6のT2負荷操作、PTT、Esc／ウィンドウ終了時の機器停止は維持します。
実機の左右方向・中心のばらつき・旋回の快適さは、オフラインテストでは判定できません。
