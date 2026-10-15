# LUFS Meter 0.1.1

配布ZIPをコンパクトに整理しました。Windows x64 / OBS Studio 32向けです。

## ダウンロード

**LUFS-Meter-Windows-x64.zip** には次の2点だけが入っています。

- obs-lufs-meter.dll
- 導入方法.txt

対応ソースとライセンス全文は **LUFS-Meter-Source-and-Licenses.zip** から取得できます。
DLLにもライセンス全文・著作権表示を収録しています。SHA256SUMS.txtで両ZIPを検証できます。

## 導入方法

1. OBSを終了します。
2. ZIPを展開し、obs-lufs-meter.dll をOBSの obs-plugins/64bit/ へコピーします。
   通常は C:/Program Files/obs-studio/obs-plugins/64bit/ です。
3. OBSを起動し、**ドック → LUFS Meter** を開きます。

右上の歯車でソース・目標値・測定時間・縦横表示を設定します。
初期値は3秒平均、目標−14 LUFSです。

初期の試作版です。ローカルOBSでのソース・マスター計測とUIテストを実施しています。
配信サービスへの実接続や長時間稼働は未検証です。
