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
| ![横表示](https://raw.githubusercontent.com/sayaka-aiart/LUFS-Meter/v0.4.0/docs/images/horizontal.png) | ![縦表示](https://raw.githubusercontent.com/sayaka-aiart/LUFS-Meter/v0.4.0/docs/images/vertical.png) |

True Peakのみ（黄色開始値を−12 dBTPにした例）

![True Peakのみ](https://raw.githubusercontent.com/sayaka-aiart/LUFS-Meter/v0.4.0/docs/images/truepeak.png)

画像はv0.4.0の検証用OBSでテスト音を計測した画面です。

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

## Codex・外部ツールとの連携

歯車 → **外部連携** → **JSON出力** で、ソース別・マスターのLUFS、True Peak、MAXを500msごとに読み出せます。
読み取り用スクリプトと、OBS WebSocketで音量・フィルターを操作するスクリプトを同梱しています。

[外部連携の使い方](docs/EXTERNAL_CONTROL.md) · [連携スクリプト入りソースZIP](https://github.com/sayaka-aiart/LUFS-Meter/releases/latest/download/LUFS-Meter-Source-and-Licenses.zip)

### Codexへのプロンプト例

JSON出力をオンにし、下の2つのパスを自分の環境に置き換えてからCodexへ貼り付けます。
OBS WebSocketの接続・認証設定は[外部連携の使い方](docs/EXTERNAL_CONTROL.md)を参照してください。

```text
OBSの配信音量を、LUFS Meterの測定値を使って調整してください。

連携スクリプトの場所：<ソースZIPを展開したフォルダー>/scripts
測定JSON：<LUFS Meterの外部連携画面でコピーしたreadings.jsonのパス>
OBS WebSocket：ws://127.0.0.1:4455
認証：OBS_WEBSOCKET_PASSWORD環境変数を使用。パスワードは返答やログへ表示しないでください。

目標（必要に応じて変更してください）：
・マイクは発話中の3秒平均（Short-term）で−17〜−16 LUFSを目安に、声を前に出す。
・BGMとゲーム音は小さめにして、マイクの声がはっきり聞こえるバランスを優先する。
・配信マスターの全体平均（Integrated）は−14 LUFS以下に抑える。−14 LUFSは上限であり、届かせるために全体を持ち上げない。
・代表的な発話＋BGM＋ゲーム音の区間では、マスターの3秒平均も−14 LUFS以下を目安に確認する。
・変更後のTrue Peak MAXは−1 dBTP以下を目安にする。
・マイクの声を基準に、BGM・ゲーム音との音量差も調整する。3つを同じLUFSへ揃えない。
・BGMは声の後ろで控えめに聞こえ、ゲーム音は内容が分かりつつ、効果音で声が隠れないバランスにする。
・ゲーム内の会話やムービーも聞かせたいので、プレイ中の効果音とは分けて確認する。

進め方：
1. docs/EXTERNAL_CONTROL.mdと各スクリプトの使い方を確認してください。
   read_meter.pyで数値を読み、obs_control.ps1のInspectで現在の音量・フィルターを取得してください。
   ソースUUID、プロファイル、シーンコレクションから、同じOBSを対象にしていることを確認してください。
   マイク・BGM・ゲーム音に該当するソースを特定し、役割とUUIDを記録してください。
   ゲーム音にBGMが含まれる場合は、別ソースのBGMと二重に数えないでください。
2. 変更前の音量・フィルター設定を、日時付きの新しいバックアップへ保存してください。
   配信マスターに送られているソースを対象にし、変更は音量・ゲイン・コンプレッサー・リミッターの必要な項目に絞ってください。
3. 計測リセットを実行し、30秒の計測開始を案内してください。
   私は実際の配信と同じBGM・ゲーム音を再生し、普段の声と少し大きい声で話します。
   その区間のソース別・マスターのLUFS、True Peak、MAXを記録してください。
   声が出ている区間の3秒平均（Short-term）を使い、マイク−BGM、マイク−ゲーム音の差をLUで比較してください。
   無言の区間を含む全体平均だけで、マイクが小さいと判断しないでください。
   普通のプレイ、効果音の大きい場面、ゲーム内会話を確認し、場面ごとの問題を分けてください。
4. 数値を根拠に、必要な調整を実行してください。
   設定変更にはobs_control.ps1の-Applyと、毎回新しい-BackupPathを使用してください。
   音量やゲインは一度に1〜2 dB程度ずつ変更し、変更後の設定を読み直して確認してください。
   無音、ミュート、測定準備中、古いJSONの値を根拠にゲインを上げないでください。
5. 変更後はResetMetersでLUFS履歴とTrue Peak MAXをリセットし、同じ条件で再計測してください。
   マイクの目安、声とBGM・ゲーム音の音量差、マスターの上限を満たすか比較してください。
   マスターが−14 LUFSを超える場合は、まずBGM・ゲーム音を下げて声を保ち、必要ならマイクの大声も抑えてください。
   悪化した変更はバックアップから逆順に復元してください。
   一時的な声の大小を追い続けず、通常の配信で安定する設定にまとめてください。
   音量差だけでは声の聞き取りやすさを断定せず、必要なら短い録音や私の聞こえ方でも確認してください。
6. 最後に、変更した設定、変更前後のソース別・マスターのLUFS・True Peak MAX、
   マイクとBGM・ゲーム音の音量差、バックアップの場所を簡潔に報告してください。

この依頼は、上記の範囲での設定変更・再計測・必要な復元まで含みます。
手順の説明だけで終わらず、接続できる範囲で実際に確認・調整してください。
```

LUFSやTrue Peakだけでは、声質や歪みの聴感までは判定できません。
EQなどの音質調整も依頼する場合は、比較できる録音も用意してください。

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
