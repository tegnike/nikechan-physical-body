# Bring-up Checklist

CoreS3通常版とRoverC Proが届いた後の初回確認手順。

## 追加購入品

- M5Stack CoreS3 ESP32S3 IoT Development Kit K128
- Grove 両端ケーブル 4-pin 20cm

距離センサーは在庫切れのため保留。初期ファームには「簡易散歩」モードを入れているが、障害物回避なしなので床上では必ず監視する。

## 配線

1. CoreS3 の PORT.A と RoverC Pro の I2C/Grove ポートを両端Groveケーブルで接続する。
2. RoverC Pro の電源スイッチを入れる。
3. CoreS3 は初回USB-CでPCに接続する。
4. 最初のモーター確認は車輪を浮かせた状態で行う。

## ファームウェア

```bash
cd /Users/user/WorkSpace/nikechan-physical-body/firmware/cores3-nikechan-body
pio run
pio run -t upload
pio device monitor
```

PlatformIOが未導入の場合:

```bash
mkdir -p /Users/user/WorkSpace/nikechan-physical-body/tmp
python3 -m venv /Users/user/WorkSpace/nikechan-physical-body/tmp/pio-venv
/Users/user/WorkSpace/nikechan-physical-body/tmp/pio-venv/bin/pip install platformio
/Users/user/WorkSpace/nikechan-physical-body/tmp/pio-venv/bin/pio run
```

## 操作

起動後、CoreS3が `NikeBody-XXXX` というWi-Fiアクセスポイントを作る。

1. Macでbridgeを起動する。
2. PCまたはスマホで `NikeBody-XXXX` に接続する。
3. ブラウザで `http://192.168.4.1` を開く。
4. CoreS3を同じLANに参加させる場合は、Wi-Fi SSID/Passwordを保存する。
5. Bridge URLにMacのURLを保存する。例: `http://192.168.1.23:8787`
6. Forward / Back / Left / Right / Stop / Grip を押して短時間ジョグ動作を確認する。
7. Camera preview でCoreS3内蔵カメラの表示を確認する。
8. 顔の中央をタップ、またはWeb UIの `Talk` を押して会話を確認する。
9. ハンズフリーで使う場合は、Web UIのWake wordを確認し、`Toggle wake word` で常時待受をONにする。

会話時は、CoreS3が発話開始を待ち、話し終わりの無音を検出して録音を区切り、bridgeの `POST /api/audio` へ送信する。bridgeはOpenAIで音声認識と返答生成を行い、Aivis CloudでWAV音声を生成してCoreS3へ返す。CoreS3は返答WAVをスピーカーで再生し、返ってきた身体コマンドをRoverCへ反映する。

ウェイクワード待受をONにすると、CoreS3はマイク音量を低負荷に監視し、声らしい音量を検出した時だけ短い音声をbridgeの `/api/wake` に送る。bridgeが `ニケちゃん` と呼ばれたと判定した場合だけ、本会話録音へ進む。

USBシリアルからも操作できる。

| キー | 動作 |
| --- | --- |
| `f` | 前進 |
| `b` | 後退 |
| `l` | 左移動 |
| `r` | 右移動 |
| `q` | 左旋回 |
| `e` | 右旋回 |
| `s` | 停止 |
| `g` | グリッパー開閉 |

## 安全メモ

- 初回は必ず車輪を浮かせる。
- `AUTO` は障害物センサーなしのランダム低速動作。床上で使う場合は手で止められる距離で試す。
- CoreS3とRoverCはまず別電源扱いにする。RoverCからCoreS3への給電流用は現物確認後に判断する。
- 音声コマンドで移動させる場合も、距離センサーが届くまでは短距離・低速のみ。
