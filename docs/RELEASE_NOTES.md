# LUFS Meter 0.3.0

True PeakのMAXホールドと、黄色になる数値の設定を追加しました。
Windows x64 / OBS Studio 32向けです。

## 変更点

- ソース別・配信マスターのTrue PeakにMAXホールドを追加。小さいMAX表示とバー上の印で確認できます。
- MAXはミュート中も保持し、右上のリセットボタンでクリアします。
- 歯車 → 表示 →「True Peak 黄色」で黄色の開始値を設定できます（−60～0 dBTP、初期値−6）。
- 現在値・バー・MAXに同じ境界値を適用し、次回起動時にも保存します。赤は0 dBTP超です。
- LUFSのみ / LUFS＋True Peak / True Peakのみ、横・縦表示に対応します。

## ダウンロード・導入

**LUFS-Meter-Windows-x64.zip** の中身は **obs-lufs-meter.dll** と **導入方法.txt** の2点です。

1. OBSを終了します。
2. DLLを C:/Program Files/obs-studio/obs-plugins/64bit/ へコピーします。
   OBSを別の場所に入れている場合は、その中の obs-plugins/64bit/ です。
3. OBSを起動し、**ドック → LUFS Meter** を開きます。

対応ソース・ライセンス全文は **LUFS-Meter-Source-and-Licenses.zip** から取得できます。
SHA256SUMS.txtで両ZIPを検証できます。
