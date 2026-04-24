#include "RuntimeUiSnapshot.h"

#include <math.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_build_runtime_ui_snapshot_keeps_base_values() {
  const RuntimeUiSnapshot snapshot =
      buildRuntimeUiSnapshot(true, 11, true, 1.4f, 6.5f, 82.0f, true, false, 2, 95.0f, 4.2f, 13.0f, 37);

  TEST_ASSERT_TRUE(snapshot.gpsFix);
  TEST_ASSERT_EQUAL(11, snapshot.satellites);
  TEST_ASSERT_TRUE(snapshot.gpsFromPhone);
  TEST_ASSERT_EQUAL_FLOAT(1.4f, snapshot.gpsHdop);
  TEST_ASSERT_EQUAL_FLOAT(6.5f, snapshot.speedKmh);
  TEST_ASSERT_EQUAL_FLOAT(82.0f, snapshot.heading);
  TEST_ASSERT_TRUE(snapshot.headingValid);
  TEST_ASSERT_FALSE(snapshot.headingFromPhone);
  TEST_ASSERT_EQUAL(2, snapshot.compassQuality);
  TEST_ASSERT_EQUAL_FLOAT(95.0f, snapshot.bearing);
  TEST_ASSERT_EQUAL_FLOAT(4.2f, snapshot.distanceM);
  TEST_ASSERT_EQUAL_FLOAT(13.0f, snapshot.diffDeg);
  TEST_ASSERT_EQUAL(37, snapshot.motorPwmPercent);
}

void test_apply_hil_sim_ui_snapshot_overrides_runtime_fields() {
  RuntimeUiSnapshot snapshot =
      buildRuntimeUiSnapshot(false, 0, false, NAN, 0.0f, 0.0f, false, false, 0, 0.0f, 0.0f, 0.0f, 0);

  hilsim::HilScenarioRunner::LiveTelemetry live;
  live.valid = true;
  live.gnss.valid = true;
  live.gnss.sats = 14;
  live.gnss.hdop = 0.8f;
  live.speedMps = 1.5f;
  live.heading.valid = true;
  live.heading.headingDeg = 10.0f;
  live.bearingDeg = 40.0f;
  live.errTrueM = 5.5f;
  live.command.thrust = 0.735f;

  const bool applied = applyHilSimUiSnapshot(&snapshot, live, 123.0f);

  TEST_ASSERT_TRUE(applied);
  TEST_ASSERT_TRUE(snapshot.gpsFix);
  TEST_ASSERT_EQUAL(14, snapshot.satellites);
  TEST_ASSERT_FALSE(snapshot.gpsFromPhone);
  TEST_ASSERT_EQUAL_FLOAT(0.8f, snapshot.gpsHdop);
  TEST_ASSERT_EQUAL_FLOAT(5.4f, snapshot.speedKmh);
  TEST_ASSERT_TRUE(snapshot.headingValid);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, snapshot.heading);
  TEST_ASSERT_EQUAL(3, snapshot.compassQuality);
  TEST_ASSERT_EQUAL_FLOAT(40.0f, snapshot.bearing);
  TEST_ASSERT_EQUAL_FLOAT(5.5f, snapshot.distanceM);
  TEST_ASSERT_EQUAL_FLOAT(30.0f, snapshot.diffDeg);
  TEST_ASSERT_EQUAL(74, snapshot.motorPwmPercent);
}

void test_apply_hil_sim_ui_snapshot_uses_fallback_when_heading_invalid() {
  RuntimeUiSnapshot snapshot = buildRuntimeUiSnapshot(true,
                                                      8,
                                                      false,
                                                      1.1f,
                                                      4.0f,
                                                      88.0f,
                                                      true,
                                                      false,
                                                      3,
                                                      70.0f,
                                                      3.0f,
                                                      -18.0f,
                                                      20);

  hilsim::HilScenarioRunner::LiveTelemetry live;
  live.valid = true;
  live.gnss.valid = false;
  live.gnss.sats = 0;
  live.gnss.hdop = NAN;
  live.speedMps = 0.0f;
  live.heading.valid = true;
  live.heading.headingDeg = NAN;
  live.bearingDeg = 140.0f;
  live.errTrueM = 12.0f;
  live.command.thrust = 0.0f;

  const bool applied = applyHilSimUiSnapshot(&snapshot, live, 222.0f);

  TEST_ASSERT_TRUE(applied);
  TEST_ASSERT_FALSE(snapshot.gpsFix);
  TEST_ASSERT_FALSE(snapshot.headingValid);
  TEST_ASSERT_EQUAL_FLOAT(222.0f, snapshot.heading);
  TEST_ASSERT_EQUAL(0, snapshot.compassQuality);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, snapshot.diffDeg);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_build_runtime_ui_snapshot_keeps_base_values);
  RUN_TEST(test_apply_hil_sim_ui_snapshot_overrides_runtime_fields);
  RUN_TEST(test_apply_hil_sim_ui_snapshot_uses_fallback_when_heading_invalid);
  return UNITY_END();
}
