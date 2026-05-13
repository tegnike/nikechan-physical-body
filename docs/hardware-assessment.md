# Hardware Assessment

調査日: 2026-05-13

## 参照した一次情報

- M5Stack RoverC-Pro docs: https://docs.m5stack.com/en/hat/hat_roverc_pro
- RoverC-Pro I2C protocol PDF: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/hat/hat_roverc_pro/K036-B_I2C_PROTOCOL_EN.pdf
- M5Stack CoreS3-SE docs: https://docs.m5stack.com/en/core/M5CoreS3%20SE
- M5Stack M5-RoverC library: https://github.com/m5stack/M5-RoverC
- Botland Grove cable page: https://botland.com.pl/grove-przewody-polaczeniowe/4402-grove-zestaw-5-przewodow-4-pin-2mm-przewody-meskie-254mm20cm-5904422352035.html
- Botland nylon spacer page: https://botland.de/abstandshalter/11164-nylon-12mm-abstandshalter-mit-m3-gewinde-10st-5904422317324.html

## 購入アイテム確認

### M5Stack RoverC Pro

できること:

- 4個の N20 ウォームギアモーターとメカナムホイールによる前後・左右・旋回・斜め移動
- STM32F030C8T6 搭載のベース側 MCU に I2C アドレス `0x38` で指示
- 4輪モーター速度を個別またはベクトル指定で制御
- サーボ式グリッパーの開閉
- 追加サーボインターフェース 2系統
- Grove互換 I2C ポート 2系統による拡張
- LEGO穴を使った軽い構造追加
- 16340 700mAh バッテリーでベース側を駆動

制約:

- 公式には M5StickC / M5StickC PLUS を差し込んで使う前提
- CoreS3 SE とは機械的に直挿しできない
- CoreS3 SE から使う場合は、I2Cの SCL/SDA/5V/GND を配線する。両端がGroveソケットの場合、購入済み Grove-to-male 線だけでは直接接続できない可能性がある

### M5Stack CoreS3 SE

できること:

- ESP32-S3 240MHz、16MB Flash、8MB PSRAM でロボット側アプリを実行
- 2.4GHz Wi-Fi によるローカルWeb UI、HTTP/WebSocket、MQTT等の通信
- 2.0インチ 320x240 タッチ液晶で表情・状態・操作UIを表示
- スピーカー、デュアルマイクによる音声入出力の実験
- microSD にログや設定を保存
- Grove PORT.A の I2C で RoverC Pro を制御

制約:

- CoreS3 SE 本体にはバッテリーがないため、単体で自走させるには USB-C 給電または追加電源が必要
- CoreS3 SE は CoreS3 からカメラ、近接センサー、IMU、磁気センサーを省いたモデル
- 自律移動に必要な距離センサー、カメラ、IMU、エンコーダーは今回の購入品には含まれない

### M3ネジ、ワッシャー、M3 12mmナイロンスタンドオフ

できること:

- CoreS3 SE や軽量な追加プレートを RoverC Pro 近くに固定するための試作
- スタンドオフで基板や小型プレート間に 12mm の空間を作る

制約:

- これだけでは CoreS3 SE と RoverC Pro の完全な筐体固定設計は確定できない
- 必要穴位置や干渉確認は現物採寸が必要

### Grove 4-pin to 2.54mm male wires

できること:

- CoreS3 SE の Grove PORT.A または RoverC Pro 側の Groveソケットから、SCL/SDA/5V/GND を個別に引き出す
- ブレッドボードやピンヘッダ経由の試作配線

制約:

- Grove-to-Grove ケーブルではないため、CoreS3 SE と RoverC Pro の Groveソケット同士をそのまま接続する用途には向かない
- RoverC Pro 側に挿せるピンヘッダ/穴が現物にない場合、Grove-to-Grove ケーブルやGroveブレークアウト等の追加部品が必要
- 走行体では抜けやすいので、動作確認後は固定・抜け止めが必要

## これらのアイテムだけでできること

厳密に購入品だけでできること:

- RoverC Pro の車体を組み、グリッパー付き移動台車として使う
- CoreS3 SE を有線給電して、画面・タッチ・マイク・スピーカー・Wi-Fi の開発機として使う
- CoreS3 SE と RoverC Pro を I2C 接続する。コネクタ形状によっては追加のGroveケーブル/ブレークアウトが必要
- ファームウェアを書き込んだ後、短距離の前後左右移動・旋回・グリッパー開閉を実行する
- Wi-Fi 経由で PC/スマホから簡単なリモコンUIを提供する
- CoreS3 SE の画面にニケちゃんの顔、状態、接続状況、簡易UIを表示する
- スピーカーで短い音声や効果音を鳴らす
- マイク入力を使った音量検知や簡易音声トリガーの実験をする

ただし、実運用には別途ほぼ必須のもの:

- CoreS3 SE への給電用 USB-C ケーブルと電源、または小型バッテリー
- ファーム書き込み用 PC
- CoreS3 SE を RoverC Pro に安定固定するマウント材、結束バンド、両面テープ、3Dプリント部品など
- 走行中に抜けない配線固定

今回の購入品だけでは難しいこと:

- 障害物回避や自己位置推定
- 段差・落下検知
- カメラで人や物体を認識すること
- 長時間の完全ワイヤレス運用
- 腕や首などの多自由度モーション
- 安全な高速走行

## 初期方針

1. まずは車輪を浮かせて I2C 接続確認とモーター/サーボの短時間ジョグを行う。
2. 次に低速の床上走行とグリッパー開閉を確認する。
3. CoreS3 SE の画面に顔/状態UIを出し、Wi-Fi経由のリモコンに進む。
4. 物理固定、電源、センサーを追加して自律性を上げる。
