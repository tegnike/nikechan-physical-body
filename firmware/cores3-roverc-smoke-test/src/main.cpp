#include <Arduino.h>
#include <Wire.h>

namespace {
constexpr uint8_t kRoverAddress = 0x38;
constexpr uint8_t kPortASda = 2;
constexpr uint8_t kPortAScl = 1;
constexpr int8_t kJogSpeed = 35;
constexpr uint32_t kJogMs = 450;

uint32_t motionUntilMs = 0;
bool gripperOpen = false;

int8_t clampSpeed(int value) {
  return static_cast<int8_t>(max(-100, min(100, value)));
}

bool writeBytes(uint8_t reg, const uint8_t* data, size_t length) {
  Wire.beginTransmission(kRoverAddress);
  Wire.write(reg);
  for (size_t i = 0; i < length; ++i) {
    Wire.write(data[i]);
  }
  return Wire.endTransmission() == 0;
}

bool writeByte(uint8_t reg, uint8_t value) {
  return writeBytes(reg, &value, 1);
}

bool roverPresent() {
  Wire.beginTransmission(kRoverAddress);
  return Wire.endTransmission() == 0;
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
  setMotorRaw(0, 0, 0, 0);
  motionUntilMs = 0;
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
}

void jog(int8_t x, int8_t y, int8_t z) {
  setSpeed(x, y, z);
  motionUntilMs = millis() + kJogMs;
}

void setServoAngle(uint8_t servoIndex, uint8_t angle) {
  writeByte(static_cast<uint8_t>(0x10 + servoIndex), angle);
}

void printHelp() {
  Serial.println();
  Serial.println("CoreS3 SE -> RoverC Pro smoke test");
  Serial.println("Commands:");
  Serial.println("  f/b/l/r : short forward/back/left/right jog");
  Serial.println("  q/e     : short rotate left/right jog");
  Serial.println("  s       : stop");
  Serial.println("  g       : toggle gripper servo 0");
  Serial.println("  o/c     : open/close servo 0");
  Serial.println("  p       : probe RoverC I2C address 0x38");
  Serial.println("  h       : help");
  Serial.println();
}

void handleCommand(char command) {
  switch (command) {
    case 'f':
      jog(0, kJogSpeed, 0);
      break;
    case 'b':
      jog(0, -kJogSpeed, 0);
      break;
    case 'l':
      jog(-kJogSpeed, 0, 0);
      break;
    case 'r':
      jog(kJogSpeed, 0, 0);
      break;
    case 'q':
      jog(0, 0, -kJogSpeed);
      break;
    case 'e':
      jog(0, 0, kJogSpeed);
      break;
    case 's':
      stopMotors();
      break;
    case 'g':
      gripperOpen = !gripperOpen;
      setServoAngle(0, gripperOpen ? 60 : 10);
      break;
    case 'o':
      gripperOpen = true;
      setServoAngle(0, 60);
      break;
    case 'c':
      gripperOpen = false;
      setServoAngle(0, 10);
      break;
    case 'p':
      Serial.println(roverPresent() ? "RoverC detected at 0x38" : "RoverC not detected");
      break;
    case 'h':
    case '?':
      printHelp();
      break;
    default:
      break;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(kPortASda, kPortAScl, 100000);
  printHelp();

  if (roverPresent()) {
    Serial.println("RoverC detected at 0x38");
    stopMotors();
    setServoAngle(0, 10);
  } else {
    Serial.println("RoverC not detected. Check SDA/SCL/5V/GND wiring and RoverC power switch.");
  }
}

void loop() {
  while (Serial.available() > 0) {
    handleCommand(static_cast<char>(Serial.read()));
  }

  if (motionUntilMs != 0 && static_cast<int32_t>(millis() - motionUntilMs) >= 0) {
    stopMotors();
  }
}
