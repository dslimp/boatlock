#include "BnoRvcFrame.h"

#include <unity.h>

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

void buildFrame(uint8_t* frame) {
  for (size_t i = 0; i < 17; ++i) {
    frame[i] = 0;
  }
  frame[0] = 7;
  putI16(frame, 1, 12345);
  putI16(frame, 3, -1234);
  putI16(frame, 5, 9000);
  putI16(frame, 7, 100);
  putI16(frame, 9, -200);
  putI16(frame, 11, 1000);
  finishChecksum(frame);
}

} // namespace

void test_valid_frame_is_decoded() {
  uint8_t frame[17];
  buildFrame(frame);

  BnoRvcSample sample;
  TEST_ASSERT_TRUE(bnoRvcParseFrame(frame, sizeof(frame), &sample));
  TEST_ASSERT_EQUAL_UINT8(7, sample.index);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 123.45f, sample.yawDeg);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -12.34f, sample.pitchDeg);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, sample.rollDeg);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.98067f, sample.accelXMps2);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -1.96134f, sample.accelYMps2);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 9.8067f, sample.accelZMps2);
}

void test_bad_checksum_is_rejected() {
  uint8_t frame[17];
  buildFrame(frame);
  frame[16] = static_cast<uint8_t>(frame[16] + 1);

  BnoRvcSample sample;
  TEST_ASSERT_FALSE(bnoRvcParseFrame(frame, sizeof(frame), &sample));
}

void test_stream_parser_resyncs_and_decodes_payload() {
  uint8_t frame[17];
  buildFrame(frame);
  const uint8_t streamPrefix[] = {0x00, 0xAA, 0x00, 0xAA, 0xAA};

  BnoRvcStreamParser parser;
  BnoRvcSample sample;
  bool parsed = false;
  for (uint8_t byte : streamPrefix) {
    parsed = parser.push(byte, &sample);
    TEST_ASSERT_FALSE(parsed);
  }
  for (uint8_t byte : frame) {
    parsed = parser.push(byte, &sample);
  }

  TEST_ASSERT_TRUE(parsed);
  TEST_ASSERT_EQUAL_UINT8(7, sample.index);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 123.45f, sample.yawDeg);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_frame_is_decoded);
  RUN_TEST(test_bad_checksum_is_rejected);
  RUN_TEST(test_stream_parser_resyncs_and_decodes_payload);
  return UNITY_END();
}
