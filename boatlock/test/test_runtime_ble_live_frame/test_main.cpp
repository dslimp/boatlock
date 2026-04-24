#include "RuntimeBleLiveFrame.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

static uint16_t readU16(const std::vector<uint8_t>& bytes, size_t offset) {
  return (uint16_t)bytes[offset] | ((uint16_t)bytes[offset + 1] << 8);
}

static uint32_t readU32(const std::vector<uint8_t>& bytes, size_t offset) {
  return (uint32_t)bytes[offset] |
         ((uint32_t)bytes[offset + 1] << 8) |
         ((uint32_t)bytes[offset + 2] << 16) |
         ((uint32_t)bytes[offset + 3] << 24);
}

void test_runtime_ble_live_frame_encodes_stable_header_and_scaled_fields() {
  RuntimeBleLiveTelemetry telemetry;
  telemetry.mode = "ANCHOR";
  telemetry.status = "WARN";
  telemetry.lat = 55.454227;
  telemetry.lon = 37.560818;
  telemetry.anchorLat = 55.450001;
  telemetry.anchorLon = 37.560002;
  telemetry.anchorHeadingDeg = 12.3f;
  telemetry.distanceM = 42.25f;
  telemetry.headingDeg = 359.9f;
  telemetry.holdHeading = true;
  telemetry.secPaired = true;
  telemetry.secNonce = 0x0123456789ABCDEFULL;

  const std::vector<uint8_t> frame = runtimeBleEncodeLiveFrame(telemetry, 7);

  TEST_ASSERT_EQUAL_UINT8('B', frame[0]);
  TEST_ASSERT_EQUAL_UINT8('L', frame[1]);
  TEST_ASSERT_EQUAL_UINT8(2, frame[2]);
  TEST_ASSERT_EQUAL_UINT8(1, frame[3]);
  TEST_ASSERT_EQUAL_UINT16(7, readU16(frame, 4));
  TEST_ASSERT_EQUAL_UINT16(RUNTIME_BLE_FLAG_HOLD_HEADING | RUNTIME_BLE_FLAG_SEC_PAIRED,
                           readU16(frame, 6));
  TEST_ASSERT_EQUAL_UINT8(2, frame[8]);
  TEST_ASSERT_EQUAL_UINT8(1, frame[9]);
  TEST_ASSERT_EQUAL_UINT16(123, readU16(frame, 26));
  TEST_ASSERT_EQUAL_UINT16(4225, readU16(frame, 28));
  TEST_ASSERT_EQUAL_UINT16(3599, readU16(frame, 30));
  TEST_ASSERT_EQUAL_UINT8(70, frame.size());
}

void test_runtime_ble_live_frame_maps_reasons_and_reject_codes() {
  RuntimeBleLiveTelemetry telemetry;
  telemetry.statusReasons =
      "NO_GPS,DRIFT_FAIL,COMM_TIMEOUT,NO_HEADING,GPS_DATA_STALE,"
      "GPS_POSITION_JUMP,GPS_HDOP_MISSING";
  telemetry.secReject = "AUTH_REQUIRED";

  const std::vector<uint8_t> frame = runtimeBleEncodeLiveFrame(telemetry, 1);
  const uint32_t reasons = readU32(frame, 57);

  TEST_ASSERT_EQUAL_UINT8(4, frame[56]);
  TEST_ASSERT_BITS_HIGH(RUNTIME_BLE_REASON_NO_GPS, reasons);
  TEST_ASSERT_BITS_HIGH(RUNTIME_BLE_REASON_DRIFT_FAIL, reasons);
  TEST_ASSERT_BITS_HIGH(RUNTIME_BLE_REASON_COMM_TIMEOUT, reasons);
  TEST_ASSERT_BITS_HIGH(RUNTIME_BLE_REASON_NO_HEADING, reasons);
  TEST_ASSERT_BITS_HIGH(RUNTIME_BLE_REASON_GPS_DATA_STALE, reasons);
  TEST_ASSERT_BITS_HIGH(RUNTIME_BLE_REASON_GPS_POSITION_JUMP, reasons);
  TEST_ASSERT_BITS_HIGH(RUNTIME_BLE_REASON_GPS_HDOP_MISSING, reasons);
  TEST_ASSERT_BITS_LOW(RUNTIME_BLE_REASON_NO_COMPASS, reasons);
}

void test_runtime_ble_live_frame_scaling_clamps_before_cast() {
  TEST_ASSERT_EQUAL_INT32(INT32_MAX,
                          runtimeBleScaleSigned(1.0e100, 10000000.0,
                                                INT32_MIN, INT32_MAX));
  TEST_ASSERT_EQUAL_INT32(INT32_MIN,
                          runtimeBleScaleSigned(-1.0e100, 10000000.0,
                                                INT32_MIN, INT32_MAX));
  TEST_ASSERT_EQUAL_UINT32(UINT16_MAX,
                           runtimeBleScaleUnsigned(1.0e100, 100.0, UINT16_MAX));
  TEST_ASSERT_EQUAL_UINT32(0, runtimeBleScaleUnsigned(-1.0, 100.0, UINT16_MAX));
  TEST_ASSERT_EQUAL_INT32(0,
                          runtimeBleScaleSigned(NAN, 10000000.0,
                                                INT32_MIN, INT32_MAX));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_runtime_ble_live_frame_encodes_stable_header_and_scaled_fields);
  RUN_TEST(test_runtime_ble_live_frame_maps_reasons_and_reject_codes);
  RUN_TEST(test_runtime_ble_live_frame_scaling_clamps_before_cast);
  return UNITY_END();
}
