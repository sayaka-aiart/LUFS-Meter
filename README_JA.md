# LUFS Meter 0.1.1 試作版

LAAを必要としない、Windows x64の独立OBSプラグインです。
配信マスターを上段、選んだ音声ソースを下段に表示します。
計測にはlibebur128 1.2.6を静的に組み込んでいます。

## 導入

1. OBSを終了してください。
2. [配布ZIP](https://github.com/sayaka-aiart/LUFS-Meter/releases/latest/download/LUFS-Meter-Windows-x64.zip)を展開し、obs-lufs-meter.dll
   をOBSのobs-plugins/64bitへコピーします。
   通常は C:/Program Files/obs-studio/obs-plugins/64bit/ です。
3. OBSを起動し、ドック → LUFS Meterを開きます。

通常のOBSへのコピーは自動では実施していません。
アンインストールはOBSを終了して、このDLLだけを削除します。
Qt DLLやLAAのDLLをコピーする必要はありません。

## 操作

- 標準表示は直近3秒の平均（Short-term LUFS）。
- 瞬間（0.4秒、Momentary）と計測中の全体平均（Integrated）へ一括切替。
- 右上の歯車 → ソースで表示対象を指定し、ドラッグで順番を変更。
- 選択状態はシーンコレクション単位のUUIDで保存。名前変更に追従します。
- 初期状態は音声ソースを自動表示。選択を保存した後はチェックしたソースだけを表示。
- 最大32音声ソース。無音、ミュート、音声停止、準備中を表示。
- リセットは全メーターの計測履歴を消します。OBSの音声設定は変更しません。
- 共通の目標は既定−14.0 LUFS。歯車 → 表示で変更できます。
- 歯車 → ソースでソース／マスターを選び、個別の目標を設定できます。
  「共通値」で個別設定を解除できます。
  個別設定は共通値より優先し、シーンコレクションごとに保存します。
- 目標の3 LU手前から目標ちょうどまで黄色、目標を超えると赤。それより
  小さい値は落ち着いた緑。数値とバーの色、バー上の目標マーカーで確認できます。
- 常時表示は名前・LUFS・バー中心。配信トラック、計測状態、個別目標などの
  詳細はマウスを重ねたときに表示します。
- 歯車 → 表示で、測定モード・目標・メーターの縦／横・リセットを操作します。
- 横表示は行ごとの横バー、縦表示はマスターと各ソースを横に並べた縦バー。
  初期値は横。方向と測定モードは次回起動時にも保存されます。
- コンパクト版の表示例は横340×480px、縦480×400px（4ソース）。
  OBSドックとして自由にサイズを変えられ、収まらないソースはスクロールできます。
- 色は選択中のLUFS表示に対する目標との比較で、音声クリッピングの検出ではありません。

## 測定範囲

マスターはOBSの配信に選ばれた音声トラックの合成PCMを測ります。
Simple出力はトラック1、Advanced出力は指定トラックを自動追従。
配信中は実際の出力エンコーダーのトラックを優先。SRT/RISTの複数
音声トラックは個別のマスターカードに表示します。配信停止中は設定の
トラックを測定し、その状態をツールチップで確認できます。VOD専用トラックや録音
専用トラックを通常配信のマスターとして扱いません。

マスターはエンコード前、OBSのミックス出力クランプ後の音です。
配信サービス側の再エンコード・音量補正・視聴端末の音量は含みません。
ソースのLUFSを足し算してマスター値を作る処理はありません。

ソースはフィルターとバランス処理後のPCMを読み、測定用コピーにOBSの
現在の音量ゲインを掛けます。ミュートはOBSのキャプチャ通知に従います。
この方法は通常のスライダー操作を反映しますが、ミックス時点の
サンプル単位のゲイン予約・同期オフセット・シーン遷移の合成まで完全に
再現するものではありません。最終出力の確認はマスターを使ってください。
配信トラックへの割り当て／モニター専用の状態も各ソースに表示します。

OBS音声コールバックでは事前確保したキューへコピーするだけです。
libebur128の計算はワーカーで行い、OBSの元PCMを変更しません。
Integratedはゲート付き累積平均。ヒストグラム方式で履歴メモリーを抑えます。
音声取得の長い停止、フォーマット変更、キュー欠落時には計測を再開します。

## 対応範囲と検証

- OBS 32系 / Windows x64 / モノラル・ステレオ向け。
- Qt 6.8.3 SDKでビルド。OBS側のQtを動的に使用します。
- 検証結果はdocs/VERIFICATION.mdを参照してください。
- 実配信のサービス接続、SRT/RIST、長時間連続稼働は初期版の残課題です。

## ビルド

Visual Studio C++ Build Tools、PowerShell 7、Python、CMakeが必要です。
scripts/fetch_obs.ps1とscripts/fetch_qt.ps1で、公式SDK・ランタイムを
SHA256検証付きでdeps/へ取得します。その後scripts/build.ps1を実行します。
Visual Studioは自動検出し、PC固有の絶対パスは必要ありません。
詳しくは[ビルド手順](docs/BUILDING.md)を参照してください。

```powershell
& .\scripts\fetch_obs.ps1
& .\scripts\fetch_qt.ps1
& .\scripts\build.ps1
python scripts/package.py
```

計測部分のみならQt/OBSなしでビルドできます。

```powershell
cmake -S . -B build-core -G "Visual Studio 16 2019" -A x64 -DBUILD_OBS_PLUGIN=OFF
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

## ライセンス

プラグイン独自コードはGPL-2.0-or-later。GPL全文はLICENSE。
libebur128はMIT、queue.hはBSD-3-Clause。著作権表示とライセンス全文はDLL内にも収録しています。
[対応ソース・ライセンスZIP](https://github.com/sayaka-aiart/LUFS-Meter/releases/latest/download/LUFS-Meter-Source-and-Licenses.zip)を配布版と同じリリースから取得できます。
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)も参照してください。
