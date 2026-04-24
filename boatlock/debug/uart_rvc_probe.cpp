#include <Arduino.h>

namespace {
#if defined(BOATLOCK_RVC_RX_PIN)
constexpr int kRvcRxPin = BOATLOCK_RVC_RX_PIN;
#else
constexpr int kRvcRxPin = 12;
#endif

#if defined(BOATLOCK_COMPASS_RESET_PIN)
constexpr int kResetPin = BOATLOCK_COMPASS_RESET_PIN;
#else
constexpr int kResetPin = -1;
#endif

constexpr uint32_t kBaud = 115200;
constexpr unsigned long kNoDataLogMs = 2000;
constexpr unsigned long kStatsLogMs = 5000;
constexpr unsigned long kFrameReadTimeoutMs = 20;

HardwareSerial rvcSerial(1);
unsigned long lastByteMs = 0;
unsigned long lastNoDataLogMs = 0;
unsigned long lastStatsLogMs = 0;
uint32_t bytesSeen = 0;
uint32_t framesOk = 0;
uint32_t framesBadChecksum = 0;
uint32_t framesShort = 0;
uint32_t syncDrops = 0;
int syncState = 0;

const char* levelName(int level) {
  return level == LOW ? "LOW" : "HIGH";
}

bool pulseReset() {
  if (kResetPin < 0) {
    return false;
  }
  pinMode(kResetPin, OUTPUT);
  digitalWrite(kResetPin, HIGH);
  delay(10);
  digitalWrite(kResetPin, LOW);
  delay(100);
  digitalWrite(kResetPin, HIGH);
  delay(800);
  return true;
}

int16_t readInt16Le(const uint8_t* data) {
  return static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                              (static_cast<uint16_t>(data[1]) << 8));
}

void printHexByte(uint8_t value) {
  if (value < 16) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

bool readFrame(uint8_t* frame, size_t size) {
  const unsigned long start = millis();
  size_t pos = 0;
  while (pos < size && millis() - start <= kFrameReadTimeoutMs) {
    if (rvcSerial.available() > 0) {
      frame[pos++] = static_cast<uint8_t>(rvcSerial.read());
      ++bytesSeen;
      lastByteMs = millis();
    } else {
      delay(1);
    }
  }
  if (pos != size) {
    ++framesShort;
    Serial.printf("[RVC_PROBE] short frame bytes=%u expected=%u\n",
                  static_cast<unsigned>(pos),
                  static_cast<unsigned>(size));
    return false;
  }
  return true;
}

void parseFrame(const uint8_t* frame) {
  uint8_t checksum = 0;
  for (int i = 0; i < 16; ++i) {
    checksum = static_cast<uint8_t>(checksum + frame[i]);
  }
  if (checksum != frame[16]) {
    ++framesBadChecksum;
    Serial.printf("[RVC_PROBE] bad checksum calc=0x%02X got=0x%02X raw=", checksum, frame[16]);
    for (int i = 0; i < 17; ++i) {
      printHexByte(frame[i]);
      if (i < 16) {
        Serial.print(' ');
      }
    }
    Serial.println();
    return;
  }

  ++framesOk;
  const uint8_t index = frame[0];
  const float yaw = readInt16Le(frame + 1) * 0.01f;
  const float pitch = readInt16Le(frame + 3) * 0.01f;
  const float roll = readInt16Le(frame + 5) * 0.01f;
  const float accelX = readInt16Le(frame + 7) * 0.0098067f;
  const float accelY = readInt16Le(frame + 9) * 0.0098067f;
  const float accelZ = readInt16Le(frame + 11) * 0.0098067f;
  Serial.printf("[RVC_PROBE] frame ok=%lu idx=%u yaw=%.2f pitch=%.2f roll=%.2f accel=%.2f,%.2f,%.2f\n",
                static_cast<unsigned long>(framesOk),
                index,
                yaw,
                pitch,
                roll,
                accelX,
                accelY,
                accelZ);
}
} // namespace

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.printf("[RVC_PROBE] start rx=%d tx=none baud=%lu reset_pin=%d\n",
                kRvcRxPin,
                static_cast<unsigned long>(kBaud),
                kResetPin);
  Serial.println("[RVC_PROBE] wire BNO TX -> ESP RX, common GND, PS1/GND, PS0/3V3, BOOTN/HIGH");
  const bool resetPulsed = pulseReset();
  Serial.printf("[RVC_PROBE] reset pulse=%d\n", resetPulsed ? 1 : 0);
  rvcSerial.begin(kBaud, SERIAL_8N1, kRvcRxPin, -1);
  lastNoDataLogMs = millis();
  lastStatsLogMs = millis();
}

void loop() {
  const unsigned long now = millis();
  while (rvcSerial.available() > 0) {
    const uint8_t value = static_cast<uint8_t>(rvcSerial.read());
    ++bytesSeen;
    lastByteMs = millis();

    if (syncState == 0) {
      syncState = (value == 0xAA) ? 1 : 0;
      if (syncState == 0) {
        ++syncDrops;
      }
      continue;
    }

    if (syncState == 1) {
      if (value != 0xAA) {
        syncState = (value == 0xAA) ? 1 : 0;
        ++syncDrops;
        continue;
      }

      uint8_t frame[17] = {0};
      if (readFrame(frame, sizeof(frame))) {
        parseFrame(frame);
      }
      syncState = 0;
    }
  }

  if (bytesSeen == 0 && now - lastNoDataLogMs >= kNoDataLogMs) {
    lastNoDataLogMs = now;
    Serial.printf("[RVC_PROBE] no bytes rx=%d rx_level=%s elapsed_ms=%lu\n",
                  kRvcRxPin,
                  levelName(digitalRead(kRvcRxPin)),
                  static_cast<unsigned long>(now));
  }

  if (now - lastStatsLogMs >= kStatsLogMs) {
    lastStatsLogMs = now;
    const unsigned long age = lastByteMs == 0 ? 0 : now - lastByteMs;
    Serial.printf("[RVC_PROBE] stats bytes=%lu ok=%lu bad_checksum=%lu short=%lu sync_drops=%lu rx_level=%s last_byte_age_ms=%lu\n",
                  static_cast<unsigned long>(bytesSeen),
                  static_cast<unsigned long>(framesOk),
                  static_cast<unsigned long>(framesBadChecksum),
                  static_cast<unsigned long>(framesShort),
                  static_cast<unsigned long>(syncDrops),
                  levelName(digitalRead(kRvcRxPin)),
                  static_cast<unsigned long>(age));
  }

  delay(1);
}
