#include "BNO08xCompass.h"
#include "BNO08xMath.h"

#include <unity.h>
#include <climits>

namespace {

void putI16(uint8_t* frame, size_t pos, int16_t value) {
  frame[pos] = static_cast<uint8_t>(value & 0xFF);
  frame[pos + 1] = static_cast<uint8_t>((static_cast<uint16_t>(value) >> 8) & 0xFF);
}

void finishChecksum(uint8_t* frame) {
  uint8_t checksum = 0;
  for (size_t i = 0; i < 16; ++i) {
    checksum = static_cast<uint8_t>(checksum + frame[i]);
  }
  frame[16] = checksum;
}

void buildStream(uint8_t* stream) {
  stream[0] = 0xAA;
  stream[1] = 0xAA;
  uint8_t* frame = stream + 2;
  for (size_t i = 0; i < 17; ++i) {
    frame[i] = 0;
  }
  frame[0] = 9;
  putI16(frame, 1, 12345);
  putI16(frame, 3, 500);
  putI16(frame, 5, -250);
  finishChecksum(frame);
}

} // namespace

void setUp() {
  mockSetMillis(0);
}

void tearDown() {}

void test_heading_event_at_zero_has_real_age_and_quality() {
  HardwareSerial serial(2);
  BNO08xCompass compass;
  uint8_t stream[19];
  buildStream(stream);

  compass.begin(serial, 12, 115200);
  mockSetMillis(0);
  serial.inject(stream, sizeof(stream));
  const BNO08xCompassReadResult read = compass.read();

  TEST_ASSERT_TRUE(read.headingEvent);
  TEST_ASSERT_EQUAL_UINT8(3, compass.getHeadingQuality());
  TEST_ASSERT_EQUAL(0UL, compass.lastHeadingEventAgeMs(0));
  TEST_ASSERT_EQUAL(100UL, compass.lastHeadingEventAgeMs(100));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 123.45f, compass.getRawAzimuth());
}

void test_heading_age_survives_unsigned_millis_wrap() {
  HardwareSerial serial(2);
  BNO08xCompass compass;
  uint8_t stream[19];
  buildStream(stream);
  const unsigned long eventMs = ULONG_MAX - 50UL;

  compass.begin(serial, 12, 115200);
  mockSetMillis(eventMs);
  serial.inject(stream, sizeof(stream));
  TEST_ASSERT_TRUE(compass.read().headingEvent);

  TEST_ASSERT_EQUAL(100UL, compass.lastHeadingEventAgeMs(eventMs + 100UL));
  TEST_ASSERT_EQUAL(100UL, compass.lastAnyEventAgeMs(eventMs + 100UL));
}

void test_hardware_reset_clears_event_state() {
  HardwareSerial serial(2);
  BNO08xCompass compass;
  uint8_t stream[19];
  buildStream(stream);

  compass.attachResetPin(13);
  compass.begin(serial, 12, 115200);
  serial.inject(stream, sizeof(stream));
  TEST_ASSERT_TRUE(compass.read().headingEvent);

  TEST_ASSERT_TRUE(compass.hardwareReset());
  TEST_ASSERT_EQUAL_UINT8(0, compass.getHeadingQuality());
  TEST_ASSERT_EQUAL(0xFFFFFFFFUL, compass.lastHeadingEventAgeMs(millis()));
}

void test_quaternion_heading_math_normalizes_yaw() {
  const float half = 0.70710678f;
  const BNO08xEulerDeg euler = bno08xEulerFromQuaternion(0.0f, 0.0f, half, half);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, euler.yaw);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, euler.pitch);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, euler.roll);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 350.0f, bno08xNormalize360(-10.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -170.0f, bno08xNormalize180(190.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, bno08xNormalize360(NAN));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, bno08xNormalize180(INFINITY));
}

void test_rvc_backend_rejects_sh2_calibration_commands() {
  BNO08xCompass compass;
  TEST_ASSERT_FALSE(compass.sh2Enabled());
  TEST_ASSERT_FALSE(compass.startDynamicCalibration(false));
  TEST_ASSERT_FALSE(compass.saveDynamicCalibration());
  TEST_ASSERT_FALSE(compass.setDcdAutoSave(true));
  TEST_ASSERT_FALSE(compass.tareHeadingNow());
  TEST_ASSERT_FALSE(compass.saveTare());
  TEST_ASSERT_FALSE(compass.clearTare());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_heading_event_at_zero_has_real_age_and_quality);
  RUN_TEST(test_heading_age_survives_unsigned_millis_wrap);
  RUN_TEST(test_hardware_reset_clears_event_state);
  RUN_TEST(test_quaternion_heading_math_normalizes_yaw);
  RUN_TEST(test_rvc_backend_rejects_sh2_calibration_commands);
  return UNITY_END();
}
