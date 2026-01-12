#include <Arduino.h>

#define DIR_PIN 5
#define STEP_PIN 4

void setup() {
  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);

  digitalWrite(DIR_PIN, HIGH);  // Направление вращения (HIGH или LOW)
}

void loop() {
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(500);       // Длительность импульса
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(500);       // Пауза между шагами
}
