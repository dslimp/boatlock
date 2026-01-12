#include <Arduino.h>

// Определение пинов


#define MOTOR_PWM_PIN    7     // ШИМ-сигнал
#define MOTOR_DIR_PIN1   6     // Направление 1
#define MOTOR_DIR_PIN2   10    // Направление 2

void setup() {
  // Настройка пинов на выход
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN1, OUTPUT);
  pinMode(MOTOR_DIR_PIN2, OUTPUT);

  // Остановим мотор на старте
  digitalWrite(MOTOR_DIR_PIN1, LOW);
  digitalWrite(MOTOR_DIR_PIN2, LOW);
  analogWrite(MOTOR_PWM_PIN, 0);
}

void loop() {
  // Вперёд на 50% мощности
  digitalWrite(MOTOR_DIR_PIN1, HIGH);
  digitalWrite(MOTOR_DIR_PIN2, LOW);
  analogWrite(MOTOR_PWM_PIN, 10);
  delay(3000);

  analogWrite(MOTOR_PWM_PIN, 0);
  delay(1000);

  // Вперёд на 50% мощности
  digitalWrite(MOTOR_DIR_PIN1, HIGH);
  digitalWrite(MOTOR_DIR_PIN2, LOW);
  analogWrite(MOTOR_PWM_PIN, 20);
  delay(3000);
  // Стоп
  analogWrite(MOTOR_PWM_PIN, 0);
  delay(1000);

  // Назад на 75% мощности
  digitalWrite(MOTOR_DIR_PIN1, LOW);
  digitalWrite(MOTOR_DIR_PIN2, HIGH);
  analogWrite(MOTOR_PWM_PIN, 30);
  delay(3000);


  // Вперёд на 50% мощности
  digitalWrite(MOTOR_DIR_PIN1, HIGH);
  digitalWrite(MOTOR_DIR_PIN2, LOW);
  analogWrite(MOTOR_PWM_PIN, 20);
  delay(3000);

  // Стоп
  analogWrite(MOTOR_PWM_PIN, 0);
  delay(1000);
}
