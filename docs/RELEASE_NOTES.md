# LUFS Meter 0.2.0

ソース別と配信マスターにTrue Peak表示を追加しました。Windows x64 / OBS Studio 32向けです。

## 変更点

- 設定の表示項目で **LUFSのみ / LUFS＋True Peak / True Peakのみ** を選択できます。
- 横メーターは上下、縦メーターは左右に配置。LUFSは太い青緑、True Peakは細い青紫で区別できます。
- 表示設定は次回起動時にも保存します。
- True Peakは直近約0.4秒の最大値をdBTPで表示。−6 dBTPから黄色、0 dBTP超で赤です。
- 初期表示は従来どおりLUFSのみです。

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
