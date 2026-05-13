# nikechan-physical-body

AIニケちゃんの物理ボディ動作プロジェクト。

このリポジトリは、最初の購入品である M5Stack CoreS3 SE と RoverC Pro を使って、移動・把持・遠隔操作の最小実験から始める。

## 購入済みハードウェア

- M5Stack RoverC Pro: メカナムホイール4輪の全方向移動ベース、サーボ式グリッパー、I2C制御、内蔵/交換式 16340 700mAh バッテリー
- M5Stack CoreS3 SE: ESP32-S3、Wi-Fi、タッチ液晶、スピーカー、デュアルマイク、microSD、RTC。CoreS3 SE 本体にはバッテリーなし
- M3 6mm ネジ + ワッシャー
- Grove 4-pin 2mm to 2.54mm male wires
- M3 12mm ナイロンスタンドオフ

詳細は [docs/hardware-assessment.md](docs/hardware-assessment.md) を参照。

## 最初の到達点

購入品だけで現実的に狙える最初の物理ボディは、以下の構成。

- CoreS3 SE を頭脳・表示・音声I/O・Wi-Fi通信役にする
- RoverC Pro を足回り・グリッパー役にする
- CoreS3 SE の PORT.A I2C から RoverC Pro の I2C/HAT 端子へ配線して制御する。ただし両端のコネクタ形状確認が必要
- USB給電中は PC/ホストからシリアルコマンドで安全に短距離ジョグ動作を確認する

## 初期ファームウェア

`firmware/cores3-roverc-smoke-test` は、CoreS3 SE から RoverC Pro を I2C で動かすための最小 PlatformIO プロジェクト。

`firmware/cores3-nikechan-body` は、追加購入した通常CoreS3向けの本命ファーム。顔表示、Webリモコン、タッチ操作、RoverC Pro制御、カメラプレビューを含む。
顔の中央をタップすると3秒録音し、`bridge/` に送ってOpenAI + Aivis Cloudの返答音声を再生する。

```bash
cd /Users/user/WorkSpace/nikechan-physical-body/firmware/cores3-nikechan-body
pio run
pio run -t upload
pio device monitor
```

PlatformIO が未導入の場合は、先に `python3 -m pip install platformio` などで導入する。

## 配線メモ

CoreS3 SE PORT.A の Grove 線色は以下。

| Grove線 | CoreS3 SE PORT.A | RoverC Pro 側で対応させる信号 |
| --- | --- | --- |
| Black | GND | GND |
| Red | 5V | 5V |
| Yellow | G2 / SDA | SDA |
| White | G1 / SCL | SCL |

購入済みの Grove-to-male 線は、Grove側を CoreS3 SE または RoverC Pro の Groveソケットへ挿せるが、反対側は 2.54mm male ピンなので、もう一方の Groveソケットへ直接は挿せない。現物で RoverC Pro 側に使えるピン穴/ヘッダがなければ、Grove-to-Grove ケーブル、Groveブレークアウト、またはメス-メスジャンパ経由の接続を用意する。

RoverC Pro は本来 M5StickC/M5StickC PLUS 直挿し用なので、初回は低速・短時間・車輪を浮かせた状態で確認する。

通常CoreS3到着後の手順は [docs/bring-up-checklist.md](docs/bring-up-checklist.md) を参照。

## 会話ブリッジ

`bridge/` は、CoreS3から音声を受け取り、OpenAIで音声認識・返答生成、Aivis Cloud APIで音声合成を行うMac/サーバー側ブリッジ。

詳細は [docs/body-bridge.md](docs/body-bridge.md) を参照。
