# Nikechan Body Bridge

CoreS3にAPIキーを持たせず、Mac/サーバー側で音声認識・LLM・音声合成を行うブリッジ。

## 使用API

調査日: 2026-05-13

- OpenAI Speech to Text: `POST https://api.openai.com/v1/audio/transcriptions`
  - 初期モデル: `gpt-4o-mini-transcribe`
  - 公式ドキュメント: https://developers.openai.com/api/docs/guides/speech-to-text
- OpenAI Responses API: `POST https://api.openai.com/v1/responses`
  - 初期モデル: `gpt-5.4-mini`
  - 公式ドキュメント: https://developers.openai.com/api/reference/resources/responses/methods/create
  - モデル情報: https://developers.openai.com/api/docs/models/gpt-5.4-mini
- Aivis Cloud API TTS: `POST https://api.aivis-project.com/v1/tts/synthesize`
  - 公式OpenAPI: https://api.aivis-project.com/v1/openapi.json
  - APIドキュメント: https://api.aivis-project.com/v1/docs

## セットアップ

```bash
cd /Users/user/WorkSpace/nikechan-physical-body/bridge
npm install
cp .env.example .env
```

`.env` に最低限以下を設定する。`bridge/.env` は秘密値を含むためGit管理しない。

```bash
OPENAI_API_KEY=...
AIVIS_API_KEY=...
AIVIS_MODEL_UUID=...
```

AivisのモデルUUIDはAivisHubまたはAivis Cloud APIダッシュボードで確認する。

## 起動

```bash
npm run dev
```

起動後、ブラウザで `http://localhost:8787` を開くとテキスト入力で疎通確認できる。

## API

### `GET /health`

設定不足と使用モデルを確認する。

### `POST /api/text`

テキスト入力で会話パイプラインをテストする。

```bash
curl -s http://localhost:8787/api/text \
  -H 'Content-Type: application/json' \
  -d '{"text":"こんにちは、前に少し進んで"}' | jq
```

レスポンス:

```json
{
  "input_text": "こんにちは、前に少し進んで",
  "reply": "こんにちは、マスター。少しだけ前に進みます。",
  "face": "happy",
  "commands": [
    {"type": "move", "direction": "forward", "duration_ms": 350, "speed": 35}
  ],
  "audio_url": "http://localhost:8787/api/audio/....mp3",
  "audio_content_type": "audio/mpeg"
}
```

`audio_format` と `audio_sampling_rate` を指定すると、返答音声の形式をリクエスト単位で変更できる。CoreS3からは `wav` / `16000` を使う。

```bash
curl -s http://localhost:8787/api/text \
  -H 'Content-Type: application/json' \
  -d '{"text":"こんにちは","audio_format":"wav","audio_sampling_rate":16000}' | jq
```

### `POST /api/audio`

CoreS3やブラウザから録音音声を送る。multipart/form-data の `audio` フィールドに `wav` / `mp3` / `webm` 等を入れる。

```bash
curl -s http://localhost:8787/api/audio \
  -F 'audio=@/path/to/speech.wav;type=audio/wav' \
  -F 'audio_format=wav' \
  -F 'audio_sampling_rate=16000' | jq
```

### `POST /api/wake`

短い録音からウェイクワードを判定する。デフォルトのウェイクワードは `ニケちゃん`。

```bash
curl -s http://localhost:8787/api/wake \
  -F 'audio=@/path/to/wake.wav;type=audio/wav' \
  -F 'wake_word=ニケちゃん' | jq
```

レスポンス:

```json
{
  "transcript": "ニケちゃん、聞いて",
  "wake_word": "ニケちゃん",
  "detected": true
}
```

## CoreS3連携方針

1. CoreS3が短い音声を録音する。
2. `POST /api/audio` に送信する。
3. ブリッジが文字起こし、返答生成、Aivis音声合成を実行する。
4. CoreS3は `audio_url` を取得してWAVを再生し、`commands` をRoverC制御に反映する。

安全のため、移動コマンドはマスターが明示した時だけ生成する。距離センサーが届くまではAUTOや移動は短時間・低速に限定する。

CoreS3ファーム側は、顔の中央タップまたはWeb UIの `Talk` で録音を開始する。固定秒数ではなく、RMS音量で発話開始を検出し、800ms程度の無音継続で話し終わりと判定する。最大録音時間は10秒、発話開始待ちは5秒。CoreS3は音声再生しやすいように `audio_format=wav` と `audio_sampling_rate=16000` を指定してbridgeへ送る。

ウェイクワードモードを有効にすると、CoreS3は短い発話候補を `POST /api/wake` へ送り、`ニケちゃん` と呼ばれた時だけ本会話録音に進む。
