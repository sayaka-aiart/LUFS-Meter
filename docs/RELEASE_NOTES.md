# LUFS Meter 0.5.0

画面のデザインを新しくしました。Live Audio Assistと同じ見た目に揃えています。
計測の処理、JSON出力、保存済みの設定は変わりません。
Windows x64 / OBS Studio 32向けです。OBS 33.0.0でも読み込みと計測の表示を確認しました。

## 変わったところ

- 色、数字のフォント、上部バー、カード、ボタン、設定画面のタブを新しくしました。
- True PeakをLED風のメーターにしました。警告値までは緑、それより上は黄色、0 dBTPを超えると全体が赤です。
- MAXの印を白い線にしました。LUFSのバーは青緑です。
- 設定画面の数値欄から増減ボタンを外しました。直接入力か↑↓キーで変更できます。
- ソースZIPに、MCPサーバー、AI連携APIの説明、MCP設定のひな形を追加しました。

## ダウンロード・導入

**LUFS-Meter-Windows-x64.zip** の中身は従来どおり **obs-lufs-meter.dll** と **導入方法.txt** の2点です。
OBSを終了し、DLLをOBSの **obs-plugins/64bit/** へコピーして起動します。

連携スクリプト・対応ソース・ライセンスは **LUFS-Meter-Source-and-Licenses.zip** に含まれます。
SHA256SUMS.txtで両ZIPを検証できます。
