#include <Arduino.h>

namespace {
constexpr int kProbePins[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
    16, 17, 18, 21, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
};

int lastLevels[sizeof(kProbePins) / sizeof(kProbePins[0])] = {0};
unsigned long lastSnapshotMs = 0;

const char* levelName(int level) {
  return level == LOW ? "LOW" : "HIGH";
}

void printSnapshot() {
  Serial.print("[GPIO_PROBE] snapshot");
  for (size_t i = 0; i < sizeof(kProbePins) / sizeof(kProbePins[0]); ++i) {
    Serial.printf(" %d=%s", kProbePins[i], levelName(digitalRead(kProbePins[i])));
  }
  Serial.println();
}
} // namespace

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("[GPIO_PROBE] start");
  Serial.println("[GPIO_PROBE] touch the unknown pin to GND through 1k..10k only");
  Serial.println("[GPIO_PROBE] do not short unknown pins to 3V3 or 5V");

  for (size_t i = 0; i < sizeof(kProbePins) / sizeof(kProbePins[0]); ++i) {
    pinMode(kProbePins[i], INPUT_PULLUP);
    lastLevels[i] = digitalRead(kProbePins[i]);
  }
  printSnapshot();
}

void loop() {
  const unsigned long now = millis();
  for (size_t i = 0; i < sizeof(kProbePins) / sizeof(kProbePins[0]); ++i) {
    const int level = digitalRead(kProbePins[i]);
    if (level != lastLevels[i]) {
      lastLevels[i] = level;
      Serial.printf("[GPIO_PROBE] change pin=%d level=%s ms=%lu\n",
                    kProbePins[i],
                    levelName(level),
                    now);
    }
  }
  if (now - lastSnapshotMs >= 5000 || now < lastSnapshotMs) {
    lastSnapshotMs = now;
    printSnapshot();
  }
  delay(25);
}
