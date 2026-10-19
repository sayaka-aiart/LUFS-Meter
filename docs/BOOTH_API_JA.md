# AI連携API

Python 3.10以降・PowerShell 7以降。追加のPythonパッケージは不要です。
このフォルダーを丸ごと保持してください。

## MCP

`lufs_meter_mcp.py` はローカルの標準入出力（stdio）MCPサーバーです。
AIアプリが起動します。単独で実行した際に入力待ちになるのは正常です。
通常のOBSではJSON出力先を自動選択します。ポータブル版・複数OBSでは
`--file "readings.jsonの絶対パス"` を指定してください。
OBS接続先は `--endpoint ws://127.0.0.1:4455`、PowerShellは
`--pwsh "pwsh.exeの絶対パス"` で変更できます。
パスワードは起動したプロセスの `OBS_WEBSOCKET_PASSWORD` 環境変数から取得します。

| ツール | 引数・内容 |
| --- | --- |
| meter_read | source（UUID/正確な名前）またはmaster=true。省略すると全メーター |
| meter_sample | seconds=1〜30（既定30）、interval=0.5〜5（既定0.5）、source/master。区間を計測 |
| obs_inspect | source省略でOBS入力一覧。source指定で音量・ミュート・フィルター |
| obs_set_volume | source、volume_db（−60〜＋6）、apply=true |
| obs_set_filter | source、filter_name、settings（設定オブジェクト）、apply=true |
| obs_create_filter | source、filter_name、filter_kind、settings、apply=true |
| obs_restore | backup_path（以前の変更で返されたパス）、apply=true |
| meter_reset | apply=true。全LUFS履歴とTrue Peak MAXをリセット |

設定変更では自動的に新しいバックアップを保存してから実行し、OBSから結果を読み直します。
保存先の既定値は `%LOCALAPPDATA%/LUFS-Meter/backups`。
`--backup-dir "絶対パス"` で変更でき、設定変更の返答に実際の保存先が含まれます。
リセットは測定履歴だけを消し、OBSの音量やフィルター設定は変えません。
MCP経由の変更は、出力が新鮮で対象OBSのプロファイル・シーンコレクションが一致し、
対象ソースが測定JSON内に存在する場合に実行します。LUFS Meterの表示対象へ追加してください。
同じプロファイル・シーンコレクションを複数OBSで使う場合は、接続先ポートとJSONパスを明示してください。

例：obs_set_volume の引数（音量フェーダー値でありLUFS目標値ではありません）

```json
{"source":"ソースUUID","volume_db":-9,"apply":true}
```

## コマンドから使う場合

MCP対応のない、ローカルコマンドを実行できるAIでも使用できます。
配布フォルダーで実行してください。

```powershell
python -X utf8 API/read_meter.py
python -X utf8 API/read_meter.py --master
python -X utf8 API/read_meter.py --source "マイク" --samples 60 --interval 0.5
pwsh -NoProfile -File API/obs_control.ps1 -Action Inspect
pwsh -NoProfile -File API/obs_control.ps1 -Action Inspect -Source "マイク"
pwsh -NoProfile -File API/obs_control.ps1 -Action ResetMeters -Apply
```

設定変更時は `-Apply` と毎回新しい `-BackupPath` が必要です。
読み取りAPIは古い・停止・無効のデータをエラーにします。
各行のstatus、fresh、lufsReady、truePeakAvailable、nullも確認してください。
表示はLUFSのみでも、JSONには全LUFSモード・True Peak・MAXが出力されます。
VST独自画面など、OBS WebSocketに公開されない設定は操作できません。

## プロトコル・設定の参照先

- MCP標準入出力: https://modelcontextprotocol.io/specification/2025-06-18/basic/transports
- Codex MCP設定: https://developers.openai.com/codex/mcp/
- OBS WebSocket: https://github.com/obsproject/obs-websocket/blob/master/docs/generated/protocol.md
- ソース・詳細: https://github.com/sayaka-aiart/LUFS-Meter

APIコードはGPL-2.0-or-laterです。ライセンス全文:
https://github.com/sayaka-aiart/LUFS-Meter/blob/main/LICENSE
