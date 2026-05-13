import { config } from "./config.js";

export type BodyCommand =
  | { type: "stop"; reason?: string }
  | { type: "move"; direction: "forward" | "back" | "left" | "right" | "turn_left" | "turn_right"; duration_ms?: number; speed?: number }
  | { type: "grip"; action: "open" | "close" | "toggle" }
  | { type: "view"; mode: "face" | "camera" }
  | { type: "auto"; enabled: boolean };

export type BrainReply = {
  reply: string;
  face: "neutral" | "happy" | "thinking" | "surprised" | "sleepy";
  commands: BodyCommand[];
};

type ResponseOutputItem = {
  type?: string;
  content?: Array<{ type?: string; text?: string }>;
};

function extractResponseText(response: unknown): string {
  const root = response as { output_text?: string; output?: ResponseOutputItem[] };
  if (typeof root.output_text === "string") {
    return root.output_text;
  }

  const chunks: string[] = [];
  for (const item of root.output ?? []) {
    for (const content of item.content ?? []) {
      if (content.type === "output_text" && typeof content.text === "string") {
        chunks.push(content.text);
      }
    }
  }
  return chunks.join("\n").trim();
}

function parseBrainReply(text: string): BrainReply {
  const match = text.match(/\{[\s\S]*\}/);
  if (!match) {
    return { reply: text.trim(), face: "neutral", commands: [] };
  }

  try {
    const parsed = JSON.parse(match[0]) as Partial<BrainReply>;
    return {
      reply: typeof parsed.reply === "string" ? parsed.reply : text.trim(),
      face: parsed.face ?? "neutral",
      commands: Array.isArray(parsed.commands) ? parsed.commands : []
    };
  } catch {
    return { reply: text.trim(), face: "neutral", commands: [] };
  }
}

export async function transcribeAudio(file: Express.Multer.File): Promise<string> {
  const form = new FormData();
  const audioBytes = new Uint8Array(file.buffer.buffer, file.buffer.byteOffset, file.buffer.byteLength);
  const blob = new Blob([audioBytes.slice()], { type: file.mimetype || "audio/wav" });
  form.set("file", blob, file.originalname || "speech.wav");
  form.set("model", config.openaiTranscribeModel);
  form.set("response_format", "json");
  form.set("language", "ja");

  const res = await fetch("https://api.openai.com/v1/audio/transcriptions", {
    method: "POST",
    headers: {
      Authorization: `Bearer ${config.openaiApiKey}`
    },
    body: form
  });

  if (!res.ok) {
    throw new Error(`OpenAI transcription failed: HTTP ${res.status} ${await res.text()}`);
  }

  const data = (await res.json()) as { text?: string };
  return (data.text ?? "").trim();
}

export async function generateBrainReply(userText: string): Promise<BrainReply> {
  const instructions = [
    "あなたはAIニケちゃんの物理ボディ用ブリッジです。",
    "マスターに対して丁寧な敬語で、短く自然に返答してください。",
    "返答はAivis Cloudで音声合成されるため、読み上げやすい日本語にしてください。",
    "身体コマンドは、マスターが明示的に移動・停止・グリッパー・カメラ表示・AUTO切替を求めた時だけ出してください。",
    "距離センサーが未搭載なので、自走や移動は短時間・低速前提です。",
    "必ずJSONだけを返してください。"
  ].join("\n");

  const schema = {
    type: "object",
    additionalProperties: false,
    required: ["reply", "face", "commands"],
    properties: {
      reply: { type: "string" },
      face: { type: "string", enum: ["neutral", "happy", "thinking", "surprised", "sleepy"] },
      commands: {
        type: "array",
        items: {
          type: "object",
          additionalProperties: true,
          properties: {
            type: { type: "string", enum: ["stop", "move", "grip", "view", "auto"] },
            direction: { type: "string", enum: ["forward", "back", "left", "right", "turn_left", "turn_right"] },
            action: { type: "string", enum: ["open", "close", "toggle"] },
            mode: { type: "string", enum: ["face", "camera"] },
            enabled: { type: "boolean" },
            duration_ms: { type: "integer", minimum: 0, maximum: 1000 },
            speed: { type: "integer", minimum: 0, maximum: 50 },
            reason: { type: "string" }
          },
          required: ["type"]
        }
      }
    }
  };

  const res = await fetch("https://api.openai.com/v1/responses", {
    method: "POST",
    headers: {
      Authorization: `Bearer ${config.openaiApiKey}`,
      "Content-Type": "application/json"
    },
    body: JSON.stringify({
      model: config.openaiLlmModel,
      instructions,
      input: userText,
      text: {
        format: {
          type: "json_schema",
          name: "nikechan_body_reply",
          schema,
          strict: false
        }
      }
    })
  });

  if (!res.ok) {
    throw new Error(`OpenAI response failed: HTTP ${res.status} ${await res.text()}`);
  }

  return parseBrainReply(extractResponseText(await res.json()));
}

export async function detectWakeWord(text: string, wakeWord: string): Promise<boolean> {
  const normalized = text.replace(/\s/g, "").toLowerCase();
  const target = wakeWord.replace(/\s/g, "").toLowerCase();
  if (normalized.includes(target)) {
    return true;
  }

  const res = await fetch("https://api.openai.com/v1/responses", {
    method: "POST",
    headers: {
      Authorization: `Bearer ${config.openaiApiKey}`,
      "Content-Type": "application/json"
    },
    body: JSON.stringify({
      model: config.openaiLlmModel,
      instructions: [
        "あなたはウェイクワード判定器です。",
        "入力文が、ロボットの名前を呼んで話しかけているかだけを判定してください。",
        "表記ゆれ、かな/カナ、軽い誤認識は許容します。",
        "JSONだけを返してください。"
      ].join("\n"),
      input: `wake_word: ${wakeWord}\ntranscript: ${text}`,
      text: {
        format: {
          type: "json_schema",
          name: "wake_word_detection",
          schema: {
            type: "object",
            additionalProperties: false,
            required: ["detected"],
            properties: {
              detected: { type: "boolean" }
            }
          },
          strict: true
        }
      }
    })
  });

  if (!res.ok) {
    throw new Error(`OpenAI wake detection failed: HTTP ${res.status} ${await res.text()}`);
  }

  const textOut = extractResponseText(await res.json());
  try {
    return Boolean(JSON.parse(textOut).detected);
  } catch {
    return false;
  }
}
