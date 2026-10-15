# LUFS Meter

OBSの音声ソースと配信マスターを表示する、コンパクトなLUFS / True Peakメーターです。
Windows x64 / OBS Studio 32向け。別の音声プラグインは必要ありません。

[![Windows x64をダウンロード](https://img.shields.io/badge/Download-Windows_x64-81ada4?style=for-the-badge)](https://github.com/sayaka-aiart/LUFS-Meter/releases/latest/download/LUFS-Meter-Windows-x64.zip)

**[最新版ZIPを直接ダウンロード](https://github.com/sayaka-aiart/LUFS-Meter/releases/latest/download/LUFS-Meter-Windows-x64.zip)** ·
[リリース一覧](https://github.com/sayaka-aiart/LUFS-Meter/releases) ·
[詳しい使い方](README_JA.md)

配布ZIPの中身は **obs-lufs-meter.dll** と **導入方法.txt** の2点です。

| 横表示 | 縦表示 |
| --- | --- |
| ![横表示](https://raw.githubusercontent.com/sayaka-aiart/LUFS-Meter/v0.3.0/docs/images/horizontal.png) | ![縦表示](https://raw.githubusercontent.com/sayaka-aiart/LUFS-Meter/v0.3.0/docs/images/vertical.png) |

True Peakのみ（黄色開始値を−12 dBTPにした例）

![True Peakのみ](https://raw.githubusercontent.com/sayaka-aiart/LUFS-Meter/v0.3.0/docs/images/truepeak.png)

画像はv0.3.0の検証用OBSでテスト音を計測した画面です。

## できること

- 配信トラックのマスターと、選んだ音声ソースを個別に計測。
- 瞬間 / 3秒平均 / 全体平均のLUFSと、True Peak（dBTP）を表示。
- 設定で **LUFSのみ / LUFS＋True Peak / True Peakのみ** を切り替え。
- 両方表示では横メーターは上下、縦メーターは左右に配置。青緑の太いバーがLUFS、青紫の細いバーがTrue Peak。
- LUFSの目標値は既定 **−14 LUFS**。3 LU手前から黄色、目標を超えると赤。
- True Peakの黄色開始値は設定で変更可能（既定 **−6 dBTP**）。0 dBTPを超えると赤。
- True Peakの **MAXホールド** を数値とバーの印で表示。歯車の左のリセットボタンでクリア。
- ソースごとの目標、表示選択、並べ替え、横・縦表示を右上の歯車で設定。
- libebur128を内蔵。音声を変更しない計測専用プラグイン。

## インストール

1. 上のボタンから **LUFS-Meter-Windows-x64.zip** をダウンロードして展開します。
2. OBSを終了し、展開した **obs-lufs-meter.dll** を、
   OBSインストール先の **obs-plugins/64bit/** へコピーします。
3. OBSを起動し、**ドック → LUFS Meter** を開きます。

通常インストールのコピー先は C:/Program Files/obs-studio/obs-plugins/64bit/ です。
QtやOBS本体のDLLを追加コピーする必要はありません。
削除するときはOBSを終了して、このプラグインのDLLだけを取り除きます。

GitHubが自動生成する「Source code」のZIPは開発用です。
インストールには **LUFS-Meter-Windows-x64.zip** を選んでください。

## 計測の範囲

マスターは配信先に選ばれたOBS音声トラックの、エンコード前の合成音を測ります。
ソース別はフィルター後の音声と現在のOBS音量を使います。
ソースのLUFSを足し算してマスターを作る処理はありません。
対応はモノラル / ステレオです。

[測定仕様・制限](README_JA.md#測定範囲) · [検証記録](docs/VERIFICATION.md)

## 開発

[Windowsでビルド](docs/BUILDING.md) · [リリース方法](docs/RELEASING.md) · [変更履歴](CHANGELOG.md)

計測部分だけならQt/OBSなしでテストできます。

    cmake -S . -B build-core -DBUILD_OBS_PLUGIN=OFF
    cmake --build build-core --config Release
    ctest --test-dir build-core -C Release --output-on-failure

## ライセンス

独自コードは **GPL-2.0-or-later**。libebur128はMIT、
同梱のqueue.hはBSD-3-Clauseです。
ライセンス全文と著作権表示はDLL内にも収録しています。
対応ソースとライセンスは[別ZIPでダウンロード](https://github.com/sayaka-aiart/LUFS-Meter/releases/latest/download/LUFS-Meter-Source-and-Licenses.zip)できます。

[LICENSE](LICENSE) · [Third-party notices](THIRD_PARTY_NOTICES.md)
