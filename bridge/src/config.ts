import { mkdir } from "node:fs/promises";
import path from "node:path";
import dotenv from "dotenv";

dotenv.config();
if (process.env.BODY_BRIDGE_ENV_FILE) {
  dotenv.config({ path: process.env.BODY_BRIDGE_ENV_FILE, override: false });
}

const rootDir = path.resolve(new URL("../../", import.meta.url).pathname);

const useAivisStyleName = (process.env.NEXT_PUBLIC_AIVIS_CLOUD_USE_STYLE_NAME ?? "").toLowerCase() === "true";

export const config = {
  port: Number(process.env.PORT ?? 8787),
  publicBaseUrl: process.env.PUBLIC_BASE_URL ?? "http://localhost:8787",
  openaiApiKey: process.env.OPENAI_API_KEY || process.env.NEXT_PUBLIC_OPENAI_API_KEY || "",
  openaiLlmModel: process.env.OPENAI_LLM_MODEL ?? "gpt-5.4-mini",
  openaiTranscribeModel: process.env.OPENAI_TRANSCRIBE_MODEL ?? "gpt-4o-mini-transcribe",
  aivisApiKey: process.env.AIVIS_API_KEY || process.env.AIVIS_CLOUD_API_KEY || "",
  aivisApiBaseUrl: (process.env.AIVIS_API_BASE_URL ?? "https://api.aivis-project.com").replace(/\/$/, ""),
  aivisModelUuid: process.env.AIVIS_MODEL_UUID || process.env.NEXT_PUBLIC_AIVIS_CLOUD_MODEL_UUID || "",
  aivisSpeakerUuid: process.env.AIVIS_SPEAKER_UUID ?? "",
  aivisStyleId: process.env.AIVIS_STYLE_ID ?? (useAivisStyleName ? "" : process.env.NEXT_PUBLIC_AIVIS_CLOUD_STYLE_ID ?? ""),
  aivisStyleName: process.env.AIVIS_STYLE_NAME ?? (useAivisStyleName ? process.env.NEXT_PUBLIC_AIVIS_CLOUD_STYLE_NAME ?? "" : ""),
  aivisOutputFormat: process.env.AIVIS_OUTPUT_FORMAT ?? "mp3",
  aivisOutputSamplingRate: Number(process.env.AIVIS_OUTPUT_SAMPLING_RATE ?? 44100),
  aivisSpeakingRate: Number(process.env.AIVIS_SPEAKING_RATE ?? process.env.NEXT_PUBLIC_AIVIS_CLOUD_SPEED ?? 1.0),
  aivisEmotionalIntensity: Number(process.env.AIVIS_EMOTIONAL_INTENSITY ?? process.env.NEXT_PUBLIC_AIVIS_CLOUD_INTONATION_SCALE ?? 1.0),
  aivisTempoDynamics: Number(process.env.AIVIS_TEMPO_DYNAMICS ?? process.env.NEXT_PUBLIC_AIVIS_CLOUD_TEMPO_DYNAMICS ?? 1.0),
  aivisVolume: Number(process.env.AIVIS_VOLUME ?? 1.0),
  aivisUseSsml: (process.env.AIVIS_USE_SSML ?? "false").toLowerCase() === "true",
  audioDir: path.join(rootDir, "tmp", "bridge-audio")
};

export async function ensureRuntimeDirs() {
  await mkdir(config.audioDir, { recursive: true });
}

export function missingRequiredConfig() {
  const missing: string[] = [];
  if (!config.openaiApiKey) missing.push("OPENAI_API_KEY");
  if (!config.aivisApiKey) missing.push("AIVIS_API_KEY");
  if (!config.aivisModelUuid) missing.push("AIVIS_MODEL_UUID");
  return missing;
}
