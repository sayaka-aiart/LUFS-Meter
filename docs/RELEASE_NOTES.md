# LUFS Meter 0.4.0

Codexや外部ツールから測定値を読み取り、OBSの調整につなげるための外部連携を追加しました。
Windows x64 / OBS Studio 32向けです。

## 使い方

1. 歯車 → **外部連携** → **JSON出力** をオンにします。
2. **パスをコピー** でreadings.jsonの保存先を取得します。
3. ソースZIPの **scripts/read_meter.py** からLUFS・True Peak・MAXを読み取れます。

JSONは500msごとに更新し、更新時刻・ミュート・測定準備・OBS停止などの状態も出力します。
表示のオン・オフに関係なくLUFSの全測定モードとTrue Peakを取得できます。

**scripts/obs_control.ps1** はOBS WebSocketを使い、音量・フィルターの取得、変更、
バックアップからの復元、計測リセットに対応します。
プラグイン自身が自動でOBSの音声設定を変更する動作はありません。
詳しくはリポジトリの **docs/EXTERNAL_CONTROL.md** を参照してください。

## ダウンロード・導入

**LUFS-Meter-Windows-x64.zip** の中身は従来どおり **obs-lufs-meter.dll** と **導入方法.txt** の2点です。
OBSを終了し、DLLをOBSの **obs-plugins/64bit/** へコピーして起動します。

連携スクリプト・対応ソース・ライセンスは **LUFS-Meter-Source-and-Licenses.zip** に含まれます。
SHA256SUMS.txtで両ZIPを検証できます。
