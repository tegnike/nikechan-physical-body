#include <Arduino.h>
#include <M5CoreS3.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "esp_heap_caps.h"
#include "esp_camera.h"

namespace {
constexpr uint8_t kRoverAddress = 0x38;
constexpr uint8_t kPortASda = 2;
constexpr uint8_t kPortAScl = 1;
constexpr int8_t kJogSpeed = 35;
constexpr uint32_t kJogMs = 350;
constexpr uint32_t kFaceFrameMs = 120;
constexpr uint32_t kStatusFrameMs = 1000;
constexpr size_t kRecordSampleRate = 17000;
constexpr size_t kRecordMaxSeconds = 10;
constexpr size_t kRecordMaxSamples = kRecordSampleRate * kRecordMaxSeconds;
constexpr size_t kRecordMaxBytes = kRecordMaxSamples * sizeof(int16_t);
constexpr size_t kRecordChunkSamples = 320;
constexpr uint32_t kVoiceStartTimeoutMs = 5000;
constexpr uint32_t kSilenceEndMs = 800;
constexpr uint32_t kMinSpeechMs = 500;
constexpr uint32_t kPrerollMs = 250;
constexpr size_t kPrerollSamples = kRecordSampleRate * kPrerollMs / 1000;
constexpr uint32_t kVoiceThreshold = 900;
constexpr uint32_t kSilenceThreshold = 520;
constexpr size_t kWakeRecordMaxSeconds = 3;
constexpr uint16_t kBg = 0x0821;
constexpr uint16_t kPanel = 0x18E3;
constexpr uint16_t kAccent = 0x57EA;
constexpr uint16_t kWarn = 0xFBE0;
constexpr uint16_t kInk = 0xFFFF;
constexpr uint16_t kFace = 0xFFE8;
constexpr uint16_t kEye = 0x10A2;
constexpr uint16_t kCheek = 0xFC9F;

TwoWire RoverWire(1);
WebServer server(80);
Preferences prefs;

enum class ViewMode { Face, Camera };

ViewMode viewMode = ViewMode::Face;
bool roverReady = false;
bool cameraReady = false;
bool gripperOpen = false;
bool autonomousEnabled = false;
bool wakeEnabled = false;
bool wakeListening = false;
int8_t lastX = 0;
int8_t lastY = 0;
int8_t lastZ = 0;
uint32_t motionUntilMs = 0;
uint32_t lastFaceMs = 0;
uint32_t lastStatusMs = 0;
uint32_t lastTouchMs = 0;
uint32_t autonomousNextMs = 0;
String lastAction = "boot";
String bridgeUrl = "http://192.168.4.2:8787";
String wifiSsid;
String wifiPassword;
String wakeWord = "ニケちゃん";
String lastTranscript;
String lastReply;

struct WAVHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t fileSize = 0;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmtSize = 16;
  uint16_t audioFormat = 1;
  uint16_t numChannels = 1;
  uint32_t sampleRate = kRecordSampleRate;
  uint32_t byteRate = kRecordSampleRate * sizeof(int16_t);
  uint16_t blockAlign = sizeof(int16_t);
  uint16_t bitsPerSample = 16;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t dataSize = 0;
};

int8_t clampSpeed(int value) {
  return static_cast<int8_t>(max(-100, min(100, value)));
}

bool writeBytes(uint8_t reg, const uint8_t* data, size_t length) {
  RoverWire.beginTransmission(kRoverAddress);
  RoverWire.write(reg);
  for (size_t i = 0; i < length; ++i) {
    RoverWire.write(data[i]);
  }
  return RoverWire.endTransmission() == 0;
}

bool writeByte(uint8_t reg, uint8_t value) {
  return writeBytes(reg, &value, 1);
}

bool roverPresent() {
  RoverWire.beginTransmission(kRoverAddress);
  return RoverWire.endTransmission() == 0;
}

void setMotorRaw(int8_t m1, int8_t m2, int8_t m3, int8_t m4) {
  const uint8_t buffer[4] = {
      static_cast<uint8_t>(m1),
      static_cast<uint8_t>(m2),
      static_cast<uint8_t>(m3),
      static_cast<uint8_t>(m4),
  };
  writeBytes(0x00, buffer, sizeof(buffer));
}

void stopMotors() {
  lastX = 0;
  lastY = 0;
  lastZ = 0;
  motionUntilMs = 0;
  setMotorRaw(0, 0, 0, 0);
  lastAction = "stop";
}

void setSpeed(int8_t x, int8_t y, int8_t z) {
  if (z != 0) {
    x = static_cast<int8_t>(x * (100 - abs(z)) / 100);
    y = static_cast<int8_t>(y * (100 - abs(z)) / 100);
  }

  const int8_t m1 = clampSpeed(y + x - z);
  const int8_t m2 = clampSpeed(y - x + z);
  const int8_t m3 = clampSpeed(y - x - z);
  const int8_t m4 = clampSpeed(y + x + z);
  setMotorRaw(m1, m2, m3, m4);
  lastX = x;
  lastY = y;
  lastZ = z;
}

void jog(int8_t x, int8_t y, int8_t z, const String& label) {
  autonomousEnabled = false;
  setSpeed(x, y, z);
  motionUntilMs = millis() + kJogMs;
  lastAction = label;
}

void setServoAngle(uint8_t servoIndex, uint8_t angle) {
  writeByte(static_cast<uint8_t>(0x10 + servoIndex), angle);
}

void setGripper(bool open) {
  gripperOpen = open;
  setServoAngle(0, gripperOpen ? 60 : 10);
  lastAction = gripperOpen ? "grip open" : "grip close";
}

void drawButton(int x, int y, int w, int h, const char* label, uint16_t color) {
  CoreS3.Display.fillRoundRect(x, y, w, h, 8, color);
  CoreS3.Display.drawRoundRect(x, y, w, h, 8, kInk);
  CoreS3.Display.setTextDatum(middle_center);
  CoreS3.Display.setTextColor(kInk, color);
  CoreS3.Display.drawString(label, x + w / 2, y + h / 2);
}

void drawStatusBar() {
  CoreS3.Display.fillRect(0, 0, 320, 30, kPanel);
  CoreS3.Display.setTextDatum(middle_left);
  CoreS3.Display.setTextColor(kInk, kPanel);
  CoreS3.Display.drawString(roverReady ? "Rover:OK" : "Rover:NG", 8, 15);
  CoreS3.Display.drawString(cameraReady ? "Cam:OK" : "Cam:NG", 92, 15);
  CoreS3.Display.drawString(wakeEnabled ? "Wake:ON" : "Wake:OFF", 166, 15);
  CoreS3.Display.setTextDatum(middle_right);
  CoreS3.Display.drawString(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString(), 312, 15);
}

void drawFace() {
  CoreS3.Display.fillRect(0, 30, 320, 150, kBg);
  CoreS3.Display.fillRoundRect(70, 42, 180, 126, 36, kFace);
  CoreS3.Display.fillCircle(118, 98, 16, kEye);
  CoreS3.Display.fillCircle(202, 98, 16, kEye);
  CoreS3.Display.fillCircle(123, 92, 5, kInk);
  CoreS3.Display.fillCircle(207, 92, 5, kInk);
  CoreS3.Display.fillCircle(102, 126, 12, kCheek);
  CoreS3.Display.fillCircle(218, 126, 12, kCheek);
  CoreS3.Display.drawArc(160, 122, 34, 18, 20, 160, kEye);

  CoreS3.Display.setTextDatum(middle_center);
  CoreS3.Display.setTextColor(kInk, kBg);
  CoreS3.Display.drawString("Nikechan Body", 160, 190);
  CoreS3.Display.drawString(lastAction, 160, 212);
  if (lastReply.length() > 0) {
    CoreS3.Display.drawString(lastReply.substring(0, 18), 160, 232);
  } else {
    CoreS3.Display.drawString("tap face to talk", 160, 232);
  }

  drawButton(8, 196, 58, 36, "CAM", cameraReady ? kAccent : kPanel);
  drawButton(254, 196, 58, 36, "AUTO", autonomousEnabled ? kWarn : kPanel);
}

void drawTouchPad() {
  drawButton(121, 198, 78, 34, "FWD", kPanel);
  drawButton(121, 236, 78, 34, "BACK", kPanel);
  drawButton(39, 236, 78, 34, "LEFT", kPanel);
  drawButton(203, 236, 78, 34, "RIGHT", kPanel);
  drawButton(8, 196, 58, 36, "CAM", cameraReady ? kAccent : kPanel);
  drawButton(254, 196, 58, 36, "AUTO", autonomousEnabled ? kWarn : kPanel);
}

void renderFaceView(bool full) {
  if (full) {
    CoreS3.Display.fillScreen(kBg);
    CoreS3.Display.setFont(&fonts::FreeSans9pt7b);
  }
  drawStatusBar();
  drawFace();
  drawTouchPad();
}

void renderCameraView() {
  if (!cameraReady || !CoreS3.Camera.get()) {
    CoreS3.Display.fillScreen(kBg);
    drawStatusBar();
    CoreS3.Display.setTextDatum(middle_center);
    CoreS3.Display.setTextColor(kInk, kBg);
    CoreS3.Display.drawString("Camera unavailable", 160, 120);
    drawButton(8, 196, 58, 36, "FACE", kAccent);
    return;
  }
  CoreS3.Display.pushImage(0, 0, CoreS3.Display.width(), CoreS3.Display.height(),
                           reinterpret_cast<uint16_t*>(CoreS3.Camera.fb->buf));
  CoreS3.Camera.free();
  CoreS3.Display.fillRect(0, 0, 320, 26, kPanel);
  CoreS3.Display.setTextDatum(middle_left);
  CoreS3.Display.setTextColor(kInk, kPanel);
  CoreS3.Display.drawString("Camera preview", 8, 13);
  CoreS3.Display.setTextDatum(middle_right);
  CoreS3.Display.drawString("tap left: face", 312, 13);
}

String htmlPage() {
  String page;
  page.reserve(3500);
  page += F("<!doctype html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>");
  page += F("<title>Nikechan Body</title><style>");
  page += F("body{margin:0;background:#101417;color:#f5f7f2;font-family:system-ui,sans-serif}main{max-width:520px;margin:auto;padding:18px}.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}button,a{font:inherit}a{display:block;text-align:center;text-decoration:none;color:#101417;background:#9ee6d6;border-radius:8px;padding:16px;font-weight:700}.muted{color:#aab4b8}.danger{background:#f5cf6a}.wide{grid-column:1/4}.status{background:#1f282d;border:1px solid #334047;border-radius:8px;padding:12px;margin:12px 0}</style></head><body><main>");
  page += F("<h1>Nikechan Body</h1><div class=status>");
  page += "Rover: ";
  page += roverReady ? "OK" : "NG";
  page += " / Camera: ";
  page += cameraReady ? "OK" : "NG";
  page += " / Auto: ";
  page += autonomousEnabled ? "ON" : "OFF";
  page += "<br>Last: " + lastAction + "</div>";
  page += F("<form action='/config' method='get'><label>Bridge URL</label><input name='bridge' value='");
  page += bridgeUrl;
  page += F("' style='box-sizing:border-box;width:100%;padding:12px;border-radius:8px;margin:8px 0'><button type='submit'>Save bridge</button></form>");
  page += F("<form action='/wifi' method='get'><label>Wi-Fi SSID</label><input name='ssid' value='");
  page += wifiSsid;
  page += F("' style='box-sizing:border-box;width:100%;padding:12px;border-radius:8px;margin:8px 0'><label>Wi-Fi Password</label><input name='pass' type='password' placeholder='leave blank to keep current' style='box-sizing:border-box;width:100%;padding:12px;border-radius:8px;margin:8px 0'><button type='submit'>Save Wi-Fi</button></form>");
  page += F("<form action='/wake-config' method='get'><label>Wake word</label><input name='word' value='");
  page += wakeWord;
  page += F("' style='box-sizing:border-box;width:100%;padding:12px;border-radius:8px;margin:8px 0'><button type='submit'>Save wake word</button></form>");
  page += F("<div class=grid>");
  page += F("<span></span><a href='/cmd?m=f'>Forward</a><span></span>");
  page += F("<a href='/cmd?m=l'>Left</a><a class=danger href='/cmd?m=s'>Stop</a><a href='/cmd?m=r'>Right</a>");
  page += F("<span></span><a href='/cmd?m=b'>Back</a><span></span>");
  page += F("<a href='/cmd?m=q'>Turn L</a><a href='/cmd?m=g'>Grip</a><a href='/cmd?m=e'>Turn R</a>");
  page += F("<a class=wide href='/view?m=face'>Face view</a><a class=wide href='/view?m=camera'>Camera preview</a><a class=wide href='/auto'>Toggle simple wander</a>");
  page += F("<a class=wide href='/talk'>Talk</a>");
  page += F("<a class=wide href='/wake'>Toggle wake word</a>");
  page += F("</div><p class=muted>Use only on a lifted robot first. Simple wander has no distance sensor yet.</p></main></body></html>");
  return page;
}

void handleCommandChar(char command) {
  switch (command) {
    case 'f': jog(0, kJogSpeed, 0, "forward"); break;
    case 'b': jog(0, -kJogSpeed, 0, "back"); break;
    case 'l': jog(-kJogSpeed, 0, 0, "left"); break;
    case 'r': jog(kJogSpeed, 0, 0, "right"); break;
    case 'q': jog(0, 0, -kJogSpeed, "turn left"); break;
    case 'e': jog(0, 0, kJogSpeed, "turn right"); break;
    case 's':
      autonomousEnabled = false;
      stopMotors();
      break;
    case 'g':
      setGripper(!gripperOpen);
      break;
    default:
      break;
  }
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void saveBridgeUrl(const String& value) {
  if (!value.startsWith("http://") && !value.startsWith("https://")) {
    return;
  }
  bridgeUrl = value;
  bridgeUrl.trim();
  while (bridgeUrl.endsWith("/")) {
    bridgeUrl.remove(bridgeUrl.length() - 1);
  }
  prefs.putString("bridge", bridgeUrl);
}

void connectStaWifi(uint32_t timeoutMs = 8000) {
  if (wifiSsid.length() == 0) {
    return;
  }
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
  }
}

void handleCmd() {
  if (server.hasArg("m") && server.arg("m").length() > 0) {
    handleCommandChar(server.arg("m")[0]);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleView() {
  if (server.arg("m") == "camera") {
    viewMode = ViewMode::Camera;
    lastAction = "camera";
  } else {
    viewMode = ViewMode::Face;
    lastAction = "face";
    renderFaceView(true);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleConfig() {
  if (server.hasArg("bridge")) {
    saveBridgeUrl(server.arg("bridge"));
    lastAction = "bridge saved";
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleWifiConfig() {
  if (server.hasArg("ssid")) {
    wifiSsid = server.arg("ssid");
    wifiSsid.trim();
    prefs.putString("ssid", wifiSsid);
  }
  if (server.hasArg("pass") && server.arg("pass").length() > 0) {
    wifiPassword = server.arg("pass");
    prefs.putString("pass", wifiPassword);
  }
  connectStaWifi();
  lastAction = WiFi.status() == WL_CONNECTED ? "wifi connected" : "wifi saved";
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleWakeConfig() {
  if (server.hasArg("word")) {
    wakeWord = server.arg("word");
    wakeWord.trim();
    if (wakeWord.length() == 0) {
      wakeWord = "ニケちゃん";
    }
    prefs.putString("wakeword", wakeWord);
  }
  lastAction = "wake word saved";
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleAuto() {
  autonomousEnabled = !autonomousEnabled;
  motionUntilMs = 0;
  autonomousNextMs = 0;
  if (!autonomousEnabled) {
    stopMotors();
  } else {
    lastAction = "simple wander";
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleWakeToggle() {
  wakeEnabled = !wakeEnabled;
  prefs.putBool("wake", wakeEnabled);
  lastAction = wakeEnabled ? "wake on" : "wake off";
  if (!wakeEnabled) {
    wakeListening = false;
    CoreS3.Mic.end();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void applyBodyCommands(JsonArray commands) {
  for (JsonObject command : commands) {
    const char* type = command["type"] | "";
    if (strcmp(type, "stop") == 0) {
      autonomousEnabled = false;
      stopMotors();
    } else if (strcmp(type, "move") == 0) {
      const char* direction = command["direction"] | "";
      const int speed = constrain(command["speed"] | kJogSpeed, 0, 50);
      const uint32_t duration = constrain(command["duration_ms"] | static_cast<int>(kJogMs), 0, 1000);
      if (strcmp(direction, "forward") == 0) setSpeed(0, speed, 0);
      else if (strcmp(direction, "back") == 0) setSpeed(0, -speed, 0);
      else if (strcmp(direction, "left") == 0) setSpeed(-speed, 0, 0);
      else if (strcmp(direction, "right") == 0) setSpeed(speed, 0, 0);
      else if (strcmp(direction, "turn_left") == 0) setSpeed(0, 0, -speed);
      else if (strcmp(direction, "turn_right") == 0) setSpeed(0, 0, speed);
      motionUntilMs = millis() + duration;
      lastAction = "voice move";
    } else if (strcmp(type, "grip") == 0) {
      const char* action = command["action"] | "toggle";
      if (strcmp(action, "open") == 0) setGripper(true);
      else if (strcmp(action, "close") == 0) setGripper(false);
      else setGripper(!gripperOpen);
    } else if (strcmp(type, "view") == 0) {
      const char* mode = command["mode"] | "face";
      viewMode = strcmp(mode, "camera") == 0 ? ViewMode::Camera : ViewMode::Face;
    } else if (strcmp(type, "auto") == 0) {
      autonomousEnabled = command["enabled"] | false;
      autonomousNextMs = 0;
      if (!autonomousEnabled) stopMotors();
    }
  }
}

uint32_t rmsLevel(const int16_t* samples, size_t count) {
  uint64_t sumSquares = 0;
  for (size_t i = 0; i < count; ++i) {
    const int32_t sample = samples[i];
    sumSquares += static_cast<uint64_t>(sample * sample);
  }
  return static_cast<uint32_t>(sqrt(static_cast<double>(sumSquares) / count));
}

uint8_t* recordWav(size_t& wavSize, size_t maxSeconds = kRecordMaxSeconds, bool waitForVoice = true) {
  const size_t maxSamples = kRecordSampleRate * maxSeconds;
  const size_t maxBytes = maxSamples * sizeof(int16_t);
  const size_t maxWavSize = sizeof(WAVHeader) + kRecordMaxBytes;
  const size_t allocSize = sizeof(WAVHeader) + maxBytes;
  uint8_t* wav = static_cast<uint8_t*>(heap_caps_malloc(allocSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!wav) {
    wav = static_cast<uint8_t*>(heap_caps_malloc(allocSize, MALLOC_CAP_8BIT));
  }
  if (!wav) {
    return nullptr;
  }

  CoreS3.Speaker.end();
  CoreS3.Mic.begin();
  int16_t* out = reinterpret_cast<int16_t*>(wav + sizeof(WAVHeader));
  int16_t chunk[kRecordChunkSamples];
  int16_t preroll[kPrerollSamples];
  size_t prerollWrite = 0;
  bool prerollFilled = false;
  bool started = !waitForVoice;
  size_t recordedSamples = 0;
  uint32_t listenStartMs = millis();
  uint32_t speechStartMs = 0;
  uint32_t lastVoiceMs = 0;

  while (recordedSamples + kRecordChunkSamples <= maxSamples) {
    CoreS3.Mic.record(chunk, kRecordChunkSamples, kRecordSampleRate);
    const uint32_t level = rmsLevel(chunk, kRecordChunkSamples);
    const uint32_t now = millis();

    if (!started) {
      for (size_t i = 0; i < kRecordChunkSamples; ++i) {
        preroll[prerollWrite++] = chunk[i];
        if (prerollWrite >= kPrerollSamples) {
          prerollWrite = 0;
          prerollFilled = true;
        }
      }

      CoreS3.Display.fillRect(56, 190, 208, 28, kBg);
      CoreS3.Display.setTextDatum(middle_center);
      CoreS3.Display.setTextColor(kInk, kBg);
      CoreS3.Display.drawString("Listening " + String(level), 160, 204);

      if (level >= kVoiceThreshold) {
        started = true;
        speechStartMs = now;
        lastVoiceMs = now;

        if (prerollFilled) {
          for (size_t i = 0; i < kPrerollSamples; ++i) {
            out[recordedSamples++] = preroll[(prerollWrite + i) % kPrerollSamples];
          }
        } else {
          memcpy(out, preroll, prerollWrite * sizeof(int16_t));
          recordedSamples = prerollWrite;
        }
      } else if (now - listenStartMs > kVoiceStartTimeoutMs) {
        CoreS3.Mic.end();
        free(wav);
        wavSize = 0;
        return nullptr;
      }
    }

    if (started) {
      memcpy(out + recordedSamples, chunk, kRecordChunkSamples * sizeof(int16_t));
      recordedSamples += kRecordChunkSamples;
      if (level >= kSilenceThreshold) {
        lastVoiceMs = now;
      }

      const uint32_t elapsedMs = now - speechStartMs;
      CoreS3.Display.fillRect(56, 190, 208, 28, kBg);
      CoreS3.Display.setTextDatum(middle_center);
      CoreS3.Display.setTextColor(kInk, kBg);
      CoreS3.Display.drawString("REC " + String(elapsedMs / 1000.0f, 1) + "s " + String(level), 160, 204);

      if (elapsedMs >= kMinSpeechMs && now - lastVoiceMs >= kSilenceEndMs) {
        break;
      }
    }
    delay(1);
  }

  CoreS3.Mic.end();
  if (recordedSamples < (kRecordSampleRate * kMinSpeechMs / 1000)) {
    free(wav);
    wavSize = 0;
    return nullptr;
  }

  WAVHeader header;
  header.fileSize = 36 + recordedSamples * sizeof(int16_t);
  header.dataSize = recordedSamples * sizeof(int16_t);
  memcpy(wav, &header, sizeof(header));
  wavSize = sizeof(WAVHeader) + recordedSamples * sizeof(int16_t);

  CoreS3.Display.fillRect(70, 190, 180, 28, kBg);
  CoreS3.Display.setTextDatum(middle_center);
  CoreS3.Display.setTextColor(kInk, kBg);
  CoreS3.Display.drawString("Captured " + String((recordedSamples * 1000) / kRecordSampleRate) + "ms", 160, 204);
  return wav;
}

String normalizeAudioUrl(const String& audioUrl) {
  if (audioUrl.startsWith("http://localhost") || audioUrl.startsWith("https://localhost")) {
    const int pathStart = audioUrl.indexOf("/", audioUrl.indexOf("//") + 2);
    if (pathStart >= 0) {
      return bridgeUrl + audioUrl.substring(pathStart);
    }
  }
  return audioUrl;
}

bool downloadAudio(const String& url, uint8_t*& audio, size_t& audioSize) {
  audio = nullptr;
  audioSize = 0;
  HTTPClient http;
  if (!http.begin(url)) {
    return false;
  }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  const int length = http.getSize();
  if (length <= 0 || length > 2 * 1024 * 1024) {
    http.end();
    return false;
  }
  audio = static_cast<uint8_t*>(heap_caps_malloc(length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!audio) {
    audio = static_cast<uint8_t*>(heap_caps_malloc(length, MALLOC_CAP_8BIT));
  }
  if (!audio) {
    http.end();
    return false;
  }
  WiFiClient* stream = http.getStreamPtr();
  size_t offset = 0;
  while (http.connected() && offset < static_cast<size_t>(length)) {
    const size_t available = stream->available();
    if (available) {
      const int read = stream->readBytes(audio + offset, min(available, static_cast<size_t>(length) - offset));
      offset += read;
    } else {
      delay(1);
    }
  }
  http.end();
  audioSize = offset;
  return offset == static_cast<size_t>(length);
}

void playWavBuffer(uint8_t* audio, size_t audioSize) {
  CoreS3.Mic.end();
  CoreS3.Speaker.begin();
  CoreS3.Speaker.setVolume(180);
  CoreS3.Speaker.playWav(audio, audioSize, 1, 0, false);
  while (CoreS3.Speaker.isPlaying()) {
    CoreS3.update();
    delay(10);
  }
  CoreS3.Speaker.end();
  CoreS3.Mic.begin();
}

bool postAudioToBridge(uint8_t* wav, size_t wavSize) {
  const String boundary = "----NikechanCoreS3Boundary";
  String head;
  head += "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"audio\"; filename=\"speech.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";
  String fields;
  fields += "\r\n--" + boundary + "\r\n";
  fields += "Content-Disposition: form-data; name=\"audio_format\"\r\n\r\nwav\r\n";
  fields += "--" + boundary + "\r\n";
  fields += "Content-Disposition: form-data; name=\"audio_sampling_rate\"\r\n\r\n16000\r\n";
  fields += "--" + boundary + "--\r\n";

  const size_t bodySize = head.length() + wavSize + fields.length();
  uint8_t* body = static_cast<uint8_t*>(heap_caps_malloc(bodySize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!body) {
    body = static_cast<uint8_t*>(heap_caps_malloc(bodySize, MALLOC_CAP_8BIT));
  }
  if (!body) {
    return false;
  }

  size_t offset = 0;
  memcpy(body + offset, head.c_str(), head.length());
  offset += head.length();
  memcpy(body + offset, wav, wavSize);
  offset += wavSize;
  memcpy(body + offset, fields.c_str(), fields.length());

  HTTPClient http;
  if (!http.begin(bridgeUrl + "/api/audio")) {
    free(body);
    return false;
  }
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  const int code = http.POST(body, bodySize);
  free(body);
  if (code != HTTP_CODE_OK) {
    lastAction = "bridge error " + String(code);
    http.end();
    return false;
  }

  const String response = http.getString();
  http.end();

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, response);
  if (error) {
    lastAction = "json error";
    return false;
  }

  lastTranscript = String(doc["transcript"] | "");
  lastReply = String(doc["reply"] | "");
  lastAction = lastReply.length() ? "reply received" : "no reply";
  if (doc["commands"].is<JsonArray>()) {
    applyBodyCommands(doc["commands"].as<JsonArray>());
  }

  const String audioUrl = normalizeAudioUrl(String(doc["audio_url"] | ""));
  if (audioUrl.length() == 0) {
    return false;
  }

  uint8_t* audio = nullptr;
  size_t audioSize = 0;
  if (!downloadAudio(audioUrl, audio, audioSize)) {
    if (audio) free(audio);
    lastAction = "audio dl error";
    return false;
  }
  playWavBuffer(audio, audioSize);
  free(audio);
  return true;
}

bool playAudioUrlFromJson(JsonDocument& doc) {
  if (doc["commands"].is<JsonArray>()) {
    applyBodyCommands(doc["commands"].as<JsonArray>());
  }

  const String audioUrl = normalizeAudioUrl(String(doc["audio_url"] | ""));
  if (audioUrl.length() == 0) {
    return false;
  }

  uint8_t* audio = nullptr;
  size_t audioSize = 0;
  if (!downloadAudio(audioUrl, audio, audioSize)) {
    if (audio) free(audio);
    lastAction = "audio dl error";
    return false;
  }
  playWavBuffer(audio, audioSize);
  free(audio);
  return true;
}

enum class WakeResult { NotDetected, DetectedOnly, Answered };

WakeResult postWakeToBridge(uint8_t* wav, size_t wavSize) {
  const String boundary = "----NikechanWakeBoundary";
  String head;
  head += "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"audio\"; filename=\"wake.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";
  String fields;
  fields += "\r\n--" + boundary + "\r\n";
  fields += "Content-Disposition: form-data; name=\"wake_word\"\r\n\r\n";
  fields += wakeWord + "\r\n";
  fields += "--" + boundary + "--\r\n";

  const size_t bodySize = head.length() + wavSize + fields.length();
  uint8_t* body = static_cast<uint8_t*>(heap_caps_malloc(bodySize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!body) body = static_cast<uint8_t*>(heap_caps_malloc(bodySize, MALLOC_CAP_8BIT));
  if (!body) return WakeResult::NotDetected;

  size_t offset = 0;
  memcpy(body + offset, head.c_str(), head.length());
  offset += head.length();
  memcpy(body + offset, wav, wavSize);
  offset += wavSize;
  memcpy(body + offset, fields.c_str(), fields.length());

  HTTPClient http;
  if (!http.begin(bridgeUrl + "/api/wake")) {
    free(body);
    return WakeResult::NotDetected;
  }
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  const int code = http.POST(body, bodySize);
  free(body);
  if (code != HTTP_CODE_OK) {
    http.end();
    return WakeResult::NotDetected;
  }
  const String response = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, response)) {
    return WakeResult::NotDetected;
  }
  lastTranscript = String(doc["transcript"] | "");
  const bool detected = doc["detected"] | false;
  if (!detected) {
    return WakeResult::NotDetected;
  }

  const String utterance = String(doc["utterance"] | "");
  const String reply = String(doc["reply"] | "");
  if (utterance.length() > 0 && reply.length() > 0) {
    lastReply = reply;
    lastAction = "wake answered";
    playAudioUrlFromJson(doc);
    return WakeResult::Answered;
  }

  return WakeResult::DetectedOnly;
}

void startVoiceRoundTrip() {
  if (WiFi.status() != WL_CONNECTED && WiFi.softAPgetStationNum() == 0) {
    lastAction = "connect bridge host";
    renderFaceView(true);
    return;
  }
  autonomousEnabled = false;
  stopMotors();
  lastAction = "listening";
  lastReply = "";
  renderFaceView(true);

  size_t wavSize = 0;
  uint8_t* wav = recordWav(wavSize);
  if (!wav) {
    lastAction = "record alloc error";
    renderFaceView(true);
    return;
  }

  lastAction = "thinking";
  renderFaceView(true);
  const bool ok = postAudioToBridge(wav, wavSize);
  free(wav);
  if (!ok && lastAction == "thinking") {
    lastAction = "talk failed";
  }
  renderFaceView(true);
}

void handleWakeMonitor() {
  if (!wakeEnabled || viewMode == ViewMode::Camera || wakeListening || CoreS3.Speaker.isPlaying()) {
    return;
  }
  static uint32_t lastProbeMs = 0;
  static uint32_t lastDisplayMs = 0;
  static bool micReady = false;
  static int16_t chunk[kRecordChunkSamples];

  if (!micReady) {
    CoreS3.Speaker.end();
    CoreS3.Mic.begin();
    micReady = true;
  }

  if (millis() - lastProbeMs < 30) {
    return;
  }
  lastProbeMs = millis();
  if (!CoreS3.Mic.record(chunk, kRecordChunkSamples, kRecordSampleRate)) {
    return;
  }
  const uint32_t level = rmsLevel(chunk, kRecordChunkSamples);
  if (millis() - lastDisplayMs > 500) {
    lastDisplayMs = millis();
    lastAction = "wake listen " + String(level);
  }
  if (level < kVoiceThreshold) {
    return;
  }

  wakeListening = true;
  micReady = false;
  CoreS3.Mic.end();
  lastAction = "wake checking";
  renderFaceView(true);

  size_t wavSize = 0;
  uint8_t* wav = recordWav(wavSize, kWakeRecordMaxSeconds, false);
  if (!wav) {
    lastAction = "wake record failed";
    wakeListening = false;
    return;
  }
  const WakeResult wakeResult = postWakeToBridge(wav, wavSize);
  free(wav);

  if (wakeResult == WakeResult::Answered) {
    lastAction = "wake answered";
  } else if (wakeResult == WakeResult::DetectedOnly) {
    lastAction = "wake detected";
    renderFaceView(true);
    startVoiceRoundTrip();
  } else {
    lastAction = "wake ignored";
  }
  wakeListening = false;
}

void handleTalk() {
  server.sendHeader("Location", "/");
  server.send(303);
  startVoiceRoundTrip();
}

void setupWifi() {
  String suffix = String((uint32_t)ESP.getEfuseMac(), HEX);
  suffix.toUpperCase();
  String ssid = "NikeBody-" + suffix.substring(max(0, static_cast<int>(suffix.length()) - 4));
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid.c_str());
  connectStaWifi();
  Serial.print("AP SSID: ");
  Serial.println(ssid);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("STA IP: ");
    Serial.println(WiFi.localIP());
  }
}

void setupWeb() {
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/view", handleView);
  server.on("/config", handleConfig);
  server.on("/wifi", handleWifiConfig);
  server.on("/wake-config", handleWakeConfig);
  server.on("/auto", handleAuto);
  server.on("/wake", handleWakeToggle);
  server.on("/talk", handleTalk);
  server.begin();
}

void handleTouch() {
  CoreS3.update();
  const auto t = CoreS3.Touch.getDetail();
  if (!t.state || millis() - lastTouchMs < 250) {
    return;
  }
  lastTouchMs = millis();

  if (viewMode == ViewMode::Camera) {
    if (t.x < 96) {
      viewMode = ViewMode::Face;
      renderFaceView(true);
    }
    return;
  }

  if (t.y < 185) {
    if (t.x >= 60 && t.x <= 260 && t.y >= 40 && t.y <= 180) {
      startVoiceRoundTrip();
    }
    return;
  }
  if (t.x < 70 && t.y < 236) {
    viewMode = ViewMode::Camera;
    lastAction = "camera";
  } else if (t.x > 250 && t.y < 236) {
    autonomousEnabled = !autonomousEnabled;
    autonomousNextMs = 0;
    lastAction = autonomousEnabled ? "simple wander" : "auto off";
    if (!autonomousEnabled) {
      stopMotors();
    }
  } else if (t.x >= 121 && t.x <= 199 && t.y < 236) {
    jog(0, kJogSpeed, 0, "forward");
  } else if (t.x >= 121 && t.x <= 199) {
    jog(0, -kJogSpeed, 0, "back");
  } else if (t.x < 121) {
    jog(-kJogSpeed, 0, 0, "left");
  } else {
    jog(kJogSpeed, 0, 0, "right");
  }
}

void runAutonomousStep() {
  if (!autonomousEnabled || millis() < autonomousNextMs || motionUntilMs != 0) {
    return;
  }

  const uint8_t choice = random(0, 5);
  switch (choice) {
    case 0: setSpeed(0, 24, 0); lastAction = "auto forward"; motionUntilMs = millis() + 500; break;
    case 1: setSpeed(0, 0, 30); lastAction = "auto turn"; motionUntilMs = millis() + 350; break;
    case 2: setSpeed(0, 0, -30); lastAction = "auto turn"; motionUntilMs = millis() + 350; break;
    default: stopMotors(); lastAction = "auto pause"; break;
  }
  autonomousNextMs = millis() + 900;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  auto cfg = M5.config();
  CoreS3.begin(cfg);
  CoreS3.Display.setRotation(1);
  CoreS3.Display.setBrightness(180);
  CoreS3.Display.setFont(&fonts::FreeSans9pt7b);

  RoverWire.begin(kPortASda, kPortAScl, 100000);
  prefs.begin("nike-body", false);
  bridgeUrl = prefs.getString("bridge", bridgeUrl);
  wifiSsid = prefs.getString("ssid", "");
  wifiPassword = prefs.getString("pass", "");
  wakeWord = prefs.getString("wakeword", wakeWord);
  wakeEnabled = prefs.getBool("wake", false);
  roverReady = roverPresent();
  if (roverReady) {
    stopMotors();
    setGripper(false);
  }

  cameraReady = CoreS3.Camera.begin();
  if (cameraReady && CoreS3.Camera.sensor) {
    CoreS3.Camera.sensor->set_framesize(CoreS3.Camera.sensor, FRAMESIZE_QVGA);
  }

  setupWifi();
  setupWeb();
  renderFaceView(true);

  Serial.println("Commands: f/b/l/r/q/e/s/g. Open http://192.168.4.1 on the NikeBody AP.");
}

void loop() {
  server.handleClient();
  handleTouch();

  while (Serial.available() > 0) {
    handleCommandChar(static_cast<char>(Serial.read()));
  }

  if (motionUntilMs != 0 && static_cast<int32_t>(millis() - motionUntilMs) >= 0) {
    stopMotors();
  }

  runAutonomousStep();
  handleWakeMonitor();

  if (viewMode == ViewMode::Camera) {
    renderCameraView();
    delay(20);
    return;
  }

  if (millis() - lastFaceMs > kFaceFrameMs) {
    lastFaceMs = millis();
    drawFace();
  }
  if (millis() - lastStatusMs > kStatusFrameMs) {
    lastStatusMs = millis();
    roverReady = roverPresent();
    drawStatusBar();
  }
}
