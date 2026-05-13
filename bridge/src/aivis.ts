import { randomUUID } from "node:crypto";
import { writeFile } from "node:fs/promises";
import path from "node:path";
import { config } from "./config.js";

const contentTypes: Record<string, string> = {
  wav: "audio/wav",
  flac: "audio/flac",
  mp3: "audio/mpeg",
  aac: "audio/aac",
  opus: "audio/ogg"
};

export type SynthesizedAudio = {
  id: string;
  url: string;
  path: string;
  contentType: string;
};

export type SynthesisOptions = {
  outputFormat?: string;
  outputSamplingRate?: number;
};

function ttsPayload(text: string, options: SynthesisOptions = {}) {
  const outputFormat = options.outputFormat ?? config.aivisOutputFormat;
  const payload: Record<string, unknown> = {
    model_uuid: config.aivisModelUuid,
    text,
    use_ssml: config.aivisUseSsml,
    use_volume_normalizer: true,
    output_format: outputFormat,
    output_sampling_rate: options.outputSamplingRate ?? config.aivisOutputSamplingRate,
    output_audio_channels: "mono",
    speaking_rate: config.aivisSpeakingRate,
    emotional_intensity: config.aivisEmotionalIntensity,
    tempo_dynamics: config.aivisTempoDynamics,
    volume: config.aivisVolume,
    leading_silence_seconds: 0.0,
    trailing_silence_seconds: 0.1,
    line_break_silence_seconds: 0.25
  };

  if (config.aivisSpeakerUuid) payload.speaker_uuid = config.aivisSpeakerUuid;
  if (config.aivisStyleName) {
    payload.style_name = config.aivisStyleName;
  } else if (config.aivisStyleId !== "") {
    payload.style_id = Number(config.aivisStyleId);
  }

  return payload;
}

export async function synthesizeSpeech(text: string, options: SynthesisOptions = {}): Promise<SynthesizedAudio> {
  const outputFormat = options.outputFormat ?? config.aivisOutputFormat;
  const res = await fetch(`${config.aivisApiBaseUrl}/v1/tts/synthesize`, {
    method: "POST",
    headers: {
      Authorization: `Bearer ${config.aivisApiKey}`,
      "Content-Type": "application/json"
    },
    body: JSON.stringify(ttsPayload(text, options))
  });

  if (!res.ok) {
    throw new Error(`Aivis synthesis failed: HTTP ${res.status} ${await res.text()}`);
  }

  const audio = Buffer.from(await res.arrayBuffer());
  const extension = outputFormat === "opus" ? "ogg" : outputFormat;
  const id = randomUUID();
  const filePath = path.join(config.audioDir, `${id}.${extension}`);
  await writeFile(filePath, audio);

  return {
    id,
    path: filePath,
    url: `${config.publicBaseUrl}/api/audio/${id}.${extension}`,
    contentType: res.headers.get("content-type") ?? contentTypes[outputFormat] ?? "application/octet-stream"
  };
}
