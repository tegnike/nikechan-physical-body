import express from "express";
import cors from "cors";
import multer from "multer";
import path from "node:path";
import { access } from "node:fs/promises";
import { config, ensureRuntimeDirs, missingRequiredConfig } from "./config.js";
import { detectWakeWord, generateBrainReply, transcribeAudio } from "./openai.js";
import { synthesizeSpeech } from "./aivis.js";

const app = express();
const upload = multer({
  storage: multer.memoryStorage(),
  limits: {
    fileSize: 25 * 1024 * 1024
  }
});

app.use(cors());
app.use(express.json({ limit: "1mb" }));

function synthesisOptionsFromBody(body: unknown) {
  const input = body as { audio_format?: string; audio_sampling_rate?: number | string } | undefined;
  const requestedFormat = typeof input?.audio_format === "string" ? input.audio_format.toLowerCase() : undefined;
  const outputFormat = requestedFormat && ["wav", "flac", "mp3", "aac", "opus"].includes(requestedFormat)
    ? requestedFormat
    : undefined;
  const parsedRate = Number(input?.audio_sampling_rate);
  const outputSamplingRate = Number.isFinite(parsedRate) && parsedRate > 0 ? parsedRate : undefined;
  return { outputFormat, outputSamplingRate };
}

app.get("/", (_req, res) => {
  res.type("html").send(`<!doctype html>
<html lang="ja">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Nikechan Body Bridge</title>
  <style>
    body { margin: 0; font-family: system-ui, sans-serif; background: #111719; color: #f3f6f2; }
    main { max-width: 720px; margin: auto; padding: 24px; }
    textarea, input, button { font: inherit; }
    textarea { width: 100%; min-height: 96px; box-sizing: border-box; border-radius: 8px; padding: 12px; }
    button { border: 0; border-radius: 8px; padding: 12px 16px; background: #9ee6d6; color: #10201d; font-weight: 700; cursor: pointer; }
    pre { white-space: pre-wrap; background: #1d272b; border-radius: 8px; padding: 14px; }
    audio { width: 100%; margin-top: 12px; }
  </style>
</head>
<body>
<main>
  <h1>Nikechan Body Bridge</h1>
  <p>テキストで会話パイプラインをテストします。音声入力は <code>POST /api/audio</code> に <code>audio</code> フィールドで送ります。</p>
  <textarea id="text">マスターです。こんにちは、今の状態を教えて。</textarea>
  <p><button id="send">送信</button></p>
  <pre id="out">Ready</pre>
  <audio id="audio" controls></audio>
</main>
<script>
document.querySelector("#send").addEventListener("click", async () => {
  const out = document.querySelector("#out");
  const audio = document.querySelector("#audio");
  out.textContent = "Sending...";
  audio.removeAttribute("src");
  const res = await fetch("/api/text", {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify({ text: document.querySelector("#text").value })
  });
  const json = await res.json();
  out.textContent = JSON.stringify(json, null, 2);
  if (json.audio_url) audio.src = json.audio_url;
});

</script>
</body>
</html>`);
});

app.get("/health", (_req, res) => {
  res.json({
    ok: missingRequiredConfig().length === 0,
    missing: missingRequiredConfig(),
    models: {
      llm: config.openaiLlmModel,
      transcription: config.openaiTranscribeModel
    },
    aivis: {
      base_url: config.aivisApiBaseUrl,
      output_format: config.aivisOutputFormat,
      model_uuid_configured: Boolean(config.aivisModelUuid)
    }
  });
});

app.post("/api/text", async (req, res, next) => {
  try {
    const text = typeof req.body?.text === "string" ? req.body.text.trim() : "";
    if (!text) {
      res.status(400).json({ error: "text is required" });
      return;
    }

    const brain = await generateBrainReply(text);
    const audio = await synthesizeSpeech(brain.reply, synthesisOptionsFromBody(req.body));
    res.json({
      input_text: text,
      reply: brain.reply,
      face: brain.face,
      commands: brain.commands,
      audio_url: audio.url,
      audio_content_type: audio.contentType
    });
  } catch (error) {
    next(error);
  }
});

app.post("/api/audio", upload.single("audio"), async (req, res, next) => {
  try {
    if (!req.file) {
      res.status(400).json({ error: "audio file field is required" });
      return;
    }

    const transcript = await transcribeAudio(req.file);
    if (!transcript) {
      res.status(422).json({ error: "transcription returned empty text" });
      return;
    }

    const brain = await generateBrainReply(transcript);
    const audioFormat = typeof req.body?.audio_format === "string" ? req.body.audio_format : undefined;
    const audioSamplingRate = typeof req.body?.audio_sampling_rate === "string" ? Number(req.body.audio_sampling_rate) : undefined;
    const audio = await synthesizeSpeech(brain.reply, {
      outputFormat: audioFormat,
      outputSamplingRate: audioSamplingRate
    });
    res.json({
      transcript,
      reply: brain.reply,
      face: brain.face,
      commands: brain.commands,
      audio_url: audio.url,
      audio_content_type: audio.contentType
    });
  } catch (error) {
    next(error);
  }
});

app.post("/api/wake", upload.single("audio"), async (req, res, next) => {
  try {
    if (!req.file) {
      res.status(400).json({ error: "audio file field is required" });
      return;
    }

    const wakeWord = typeof req.body?.wake_word === "string" && req.body.wake_word.trim()
      ? req.body.wake_word.trim()
      : "ニケちゃん";
    const transcript = await transcribeAudio(req.file);
    const wake = transcript ? await detectWakeWord(transcript, wakeWord) : { detected: false, utterance: "" };
    let brain = null;
    let audio = null;
    if (wake.detected && wake.utterance) {
      brain = await generateBrainReply(wake.utterance);
      audio = await synthesizeSpeech(brain.reply, synthesisOptionsFromBody(req.body));
    }
    res.json({
      transcript,
      wake_word: wakeWord,
      detected: wake.detected,
      utterance: wake.utterance,
      reply: brain?.reply,
      face: brain?.face,
      commands: brain?.commands ?? [],
      audio_url: audio?.url,
      audio_content_type: audio?.contentType
    });
  } catch (error) {
    next(error);
  }
});

app.get("/api/audio/:file", async (req, res, next) => {
  try {
    const safeName = path.basename(req.params.file);
    const filePath = path.join(config.audioDir, safeName);
    await access(filePath);
    res.sendFile(filePath);
  } catch (error) {
    next(error);
  }
});

app.use((error: unknown, _req: express.Request, res: express.Response, _next: express.NextFunction) => {
  const message = error instanceof Error ? error.message : String(error);
  console.error(message);
  res.status(500).json({ error: message });
});

await ensureRuntimeDirs();
app.listen(config.port, () => {
  console.log(`Nikechan body bridge listening on ${config.publicBaseUrl}`);
});
