# Tapo P105 を PowerShell から操作する

PC と P105 が同じ LAN にあり、Tapo アプリで初期設定が完了していることが前提です。
コミュニティ製 [python-kasa](https://github.com/python-kasa/python-kasa) の CLI を使用します。
P105 は対応一覧にありますが、手元のファームウェアとの通信は実機確認が必要です。

## TPAP / Unsupported device が表示された場合

実機 P105(JP) の State で `Encrypt Type: TPAP` と `Unsupported device` が
報告されました。python-kasa 0.10.2 はこの通信方式に対応しておらず、
この段階のエラーはパスワードの正誤を示しません。

Tapo アプリの「マイページ」→「音声アシスタント」→「サードパーティー連携」を
確認してください。英語版では Me → Third-Party Services → Third-Party Compatibility です。
旧版では Tapo Lab 配下の場合があります。有効化後に `./tapo.ps1 State` を再実行します。
これは外部ツールとの互換性を許可する設定で、全ファームウェアでの成功を保証するものではありません。
[TP-Link 公式説明](https://www.tp-link.com/jp/support/faq/4416/)

その後、ユーザーから動作したとの報告がありました。全4台の個別の操作結果は未記録です。

## 準備と操作

```powershell
./tapo.ps1 Setup  # 初回のみ。Python 3.13 が必要
$tapo = Get-Credential -Message 'Tapoアプリのメールアドレスとパスワード'
./tapo.ps1 Discover -Credential $tapo
```

表示された名前・IP とアプリの表示を照合して、対象のプラグを選びます。
以下の IP は説明用です。実際の対象の IP に置き換えてください。

```powershell
# 初回に対象の4台を登録（電源は変更しません）
$plugs = '192.0.2.10','192.0.2.11','192.0.2.12','192.0.2.13'
./tapo.ps1 Configure -Address $plugs

# 以降は4台まとめてが標準
./tapo.ps1 State -Credential $tapo
./tapo.ps1 On    -Credential $tapo
./tapo.ps1 Off   -Credential $tapo

# 1台だけ操作したい場合
./tapo.ps1 On -Address 192.0.2.10 -Credential $tapo
```

認証は `-Credential`、環境変数 `KASA_USERNAME` と `KASA_PASSWORD`、対話入力の順です。
スクリプト自体は認証情報を保存せず、子プロセスの環境変数へ一時的に渡し、終了時に元へ戻します。
パスワードをコマンド引数・Git・チャットに記載する必要はありません。
`Remove-Variable tapo` または PowerShell 終了で変数を破棄できます。
4台のIPは Git 対象外の `config/tapo.local.json` に保存します。
未登録では一括操作を実行しません。Configure は4つの異なるIPを必要とします。

毎回の入力を省く場合、以下を手元の PowerShell で一度実行します。
前半2行が現在の端末への設定、後半2行が次回以降のためのユーザー環境変数への保存です。
パスワード入力は非表示で、コマンド履歴に値を直接書きません。
ただし、保存先のユーザー環境変数は暗号化されません。

```powershell
$env:KASA_USERNAME = Read-Host 'Tapoメールアドレス'
$env:KASA_PASSWORD = [System.Net.NetworkCredential]::new('', (Read-Host 'Tapoパスワード' -AsSecureString)).Password
[Environment]::SetEnvironmentVariable('KASA_USERNAME', $env:KASA_USERNAME, 'User')
[Environment]::SetEnvironmentVariable('KASA_PASSWORD', $env:KASA_PASSWORD, 'User')
./tapo.ps1 State
```

`-WhatIf` は対象表示だけで、認証も通信も実行しません。
通常の On/Off は即時に実行し、続けて状態を読み戻して表示します。
途中で失敗した場合はそこで止まり、先に成功したプラグは元へ戻しません。
自動検出した全機器への一括操作や Row/Fly 起動・終了への自動連動はありません。
Lighthouse の Bluetooth スリープと異なり、P105 の Off は給電を切ります。

機器が見つからない場合、ゲスト Wi-Fi の端末分離やネットワークを確認してください。
複数 NIC がある PC では `-Target` で対象 LAN のブロードキャストアドレスを指定できます。
IP が変わる環境では操作前に再確認するか、ルーターの DHCP 予約を使ってください。

依存パッケージは `.runtime/tapo`（Git 対象外）にインストールします。
導入・CLI 起動・通信なしの引数/認証情報の受け渡しを検証済みです。
`./tools/test_tapo.ps1` は実機通信なしの代替 CLI で4台の既定動作、個別指定、
WhatIf、途中失敗時の停止、認証情報の復元を検証します。
Windows PowerShell と PowerShell 7 の両方で成功しています。
ユーザーから動作報告あり。環境変数による入力省略は通信なしのテストで確認しています。
