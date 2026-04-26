#include "HilSimRunner.h"
#include <unity.h>

using hilsim::HilScenarioRunner;
using hilsim::SimScenario;

void setUp() {}
void tearDown() {}

static bool findScenario(const char* id, SimScenario* out) {
  if (!out) return false;
  const auto scenarios = hilsim::defaultScenarios();
  for (const SimScenario& s : scenarios) {
    if (s.id == id) {
      *out = s;
      return true;
    }
  }
  return false;
}

static HilScenarioRunner::Result runScenario(const SimScenario& s) {
  HilScenarioRunner runner;
  TEST_ASSERT_TRUE(runner.start(s));
  int guard = 0;
  while (runner.isRunning() && guard < 100000) {
    runner.stepOnce();
    ++guard;
  }
  TEST_ASSERT_TRUE(guard < 100000);
  return runner.result();
}

static bool hasEvent(const HilScenarioRunner::Result& r, const char* token) {
  for (const auto& ev : r.events) {
    if (ev.code == token || ev.details == token) {
      return true;
    }
  }
  return false;
}

void test_deterministic_repeatable_for_same_seed() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S1_current_0p4_good", &s));

  const auto r1 = runScenario(s);
  const auto r2 = runScenario(s);

  TEST_ASSERT_EQUAL(r1.pass, r2.pass);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, r1.metrics.p95ErrorM, r2.metrics.p95ErrorM);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, r1.metrics.maxErrorM, r2.metrics.maxErrorM);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, r1.metrics.timeInDeadbandPct, r2.metrics.timeInDeadbandPct);
}

void test_dropout_produces_stale_and_failsafe_events() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S4_gnss_dropout_3s", &s));
  const auto r = runScenario(s);

  bool hasStale = false;
  bool hasFailsafe = false;
  for (const auto& ev : r.events) {
    if (ev.code == "GPS_DATA_STALE" || ev.details == "GPS_DATA_STALE") {
      hasStale = true;
    }
    if (ev.code == "FAILSAFE_TRIGGERED" || ev.details == "FAILSAFE_TRIGGERED") {
      hasFailsafe = true;
    }
  }

  TEST_ASSERT_TRUE(hasStale);
  TEST_ASSERT_TRUE(hasFailsafe);
}

void test_control_loop_stall_produces_timeout_event() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S8_control_loop_stall", &s));
  const auto r = runScenario(s);

  bool hasLoopTimeout = false;
  for (const auto& ev : r.events) {
    if (ev.code == "CONTROL_LOOP_TIMEOUT" || ev.details == "CONTROL_LOOP_TIMEOUT") {
      hasLoopTimeout = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(hasLoopTimeout);
}

void test_s0_hold_still_good_passes_expectations() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S0_hold_still_good", &s));
  const auto r = runScenario(s);
  printf("S0 metrics: pass=%d reason=%s p95=%.3f max=%.3f deadband=%.2f sat=%.2f\n",
         (int)r.pass,
         r.reason.c_str(),
         r.metrics.p95ErrorM,
         r.metrics.maxErrorM,
         r.metrics.timeInDeadbandPct,
         r.metrics.timeSaturatedPct);
  TEST_ASSERT_TRUE_MESSAGE(r.pass, "S0 should pass baseline hold expectations");
  TEST_ASSERT_TRUE(r.metrics.p95ErrorM <= s.expect.p95ErrorMaxM + 1e-4f);
  TEST_ASSERT_TRUE(r.metrics.maxErrorM <= s.expect.maxErrorMaxM + 1e-4f);
  TEST_ASSERT_TRUE(r.metrics.timeInDeadbandPct + 1e-4f >= s.expect.timeInDeadbandMinPct);
}

void test_s1_current_0p4_good_passes_expectations() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S1_current_0p4_good", &s));
  const auto r = runScenario(s);
  printf("S1 metrics: pass=%d reason=%s p95=%.3f max=%.3f deadband=%.2f sat=%.2f\n",
         (int)r.pass,
         r.reason.c_str(),
         r.metrics.p95ErrorM,
         r.metrics.maxErrorM,
         r.metrics.timeInDeadbandPct,
         r.metrics.timeSaturatedPct);
  TEST_ASSERT_TRUE_MESSAGE(r.pass, "S1 should pass baseline current expectations");
  TEST_ASSERT_TRUE(r.metrics.p95ErrorM <= s.expect.p95ErrorMaxM + 1e-4f);
  TEST_ASSERT_TRUE(r.metrics.maxErrorM <= s.expect.maxErrorMaxM + 1e-4f);
  TEST_ASSERT_TRUE(r.metrics.timeInDeadbandPct + 1e-4f >= s.expect.timeInDeadbandMinPct);
}

void test_s2_current_0p8_hard_passes_expectations() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S2_current_0p8_hard", &s));
  const auto r = runScenario(s);
  printf("S2 metrics: pass=%d reason=%s p95=%.3f max=%.3f deadband=%.2f sat=%.2f\n",
         (int)r.pass,
         r.reason.c_str(),
         r.metrics.p95ErrorM,
         r.metrics.maxErrorM,
         r.metrics.timeInDeadbandPct,
         r.metrics.timeSaturatedPct);
  TEST_ASSERT_TRUE_MESSAGE(r.pass, "S2 should pass hard-current expectations");
  TEST_ASSERT_TRUE(r.metrics.p95ErrorM <= s.expect.p95ErrorMaxM + 1e-4f);
  TEST_ASSERT_TRUE(r.metrics.maxErrorM <= s.expect.maxErrorMaxM + 1e-4f);
  TEST_ASSERT_TRUE(r.metrics.timeInDeadbandPct + 1e-4f >= s.expect.timeInDeadbandMinPct);
  TEST_ASSERT_TRUE(r.metrics.timeSaturatedPct <= s.expect.timeSaturatedMaxPct + 1e-4f);
}

void test_s9_nan_heading_injection_passes_as_expected_failsafe() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S9_nan_heading_injection", &s));
  const auto r = runScenario(s);

  bool hasNanEvent = false;
  bool hasFailsafe = false;
  for (const auto& ev : r.events) {
    if (ev.code == "INTERNAL_ERROR_NAN" || ev.details == "INTERNAL_ERROR_NAN") {
      hasNanEvent = true;
    }
    if (ev.code == "FAILSAFE_TRIGGERED" || ev.details == "FAILSAFE_TRIGGERED") {
      hasFailsafe = true;
    }
  }

  TEST_ASSERT_TRUE_MESSAGE(r.pass, "S9 should pass when NaN triggers expected failsafe");
  TEST_ASSERT_TRUE(r.metrics.nanDetected);
  TEST_ASSERT_TRUE(hasNanEvent);
  TEST_ASSERT_TRUE(hasFailsafe);
}

void test_s12_compass_dropout_triggers_sensor_timeout_failsafe() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S12_compass_dropout_5s", &s));
  const auto r = runScenario(s);
  TEST_ASSERT_TRUE_MESSAGE(r.pass, "S12 should pass expected failsafe path");
  TEST_ASSERT_TRUE(hasEvent(r, "COMPASS_LOST_EMU"));
  TEST_ASSERT_TRUE(hasEvent(r, "SENSOR_TIMEOUT"));
  TEST_ASSERT_TRUE(hasEvent(r, "FAILSAFE_TRIGGERED"));
}

void test_s14_power_loss_emits_loss_restore_and_failsafe() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S14_power_loss_recover", &s));
  const auto r = runScenario(s);
  TEST_ASSERT_TRUE_MESSAGE(r.pass, "S14 should pass expected power-loss failsafe path");
  TEST_ASSERT_TRUE(hasEvent(r, "POWER_LOSS_EMU"));
  TEST_ASSERT_TRUE(hasEvent(r, "POWER_RESTORED_EMU"));
  TEST_ASSERT_TRUE(hasEvent(r, "FAILSAFE_TRIGGERED"));
}

void test_s16_display_loss_events_without_control_failure() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S16_display_loss_transient", &s));
  const auto r = runScenario(s);
  TEST_ASSERT_TRUE_MESSAGE(r.pass, "S16 should stay pass with display-only outage");
  TEST_ASSERT_TRUE(hasEvent(r, "DISPLAY_LOST_EMU"));
  TEST_ASSERT_TRUE(hasEvent(r, "DISPLAY_RESTORED_EMU"));
}

void test_rf_water_scenarios_are_in_default_catalog() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("RF0_oka_normal_55lb", &s));
  TEST_ASSERT_TRUE(findScenario("RF1_volga_spring_flow_80lb", &s));
  TEST_ASSERT_TRUE(findScenario("RF2_rybinsk_fetch_55lb", &s));
  TEST_ASSERT_TRUE(findScenario("RF3_ladoga_storm_abort", &s));
  TEST_ASSERT_TRUE(findScenario("RF4_baltic_gulf_drift", &s));
}

void test_rf_wave_wake_packets_emit_events() {
  SimScenario oka;
  TEST_ASSERT_TRUE(findScenario("RF0_oka_normal_55lb", &oka));
  const auto okaResult = runScenario(oka);
  TEST_ASSERT_TRUE_MESSAGE(okaResult.pass, "RF0 should pass with wake packet event");
  TEST_ASSERT_TRUE(hasEvent(okaResult, "WAKE_PACKET_EMU"));

  SimScenario rybinsk;
  TEST_ASSERT_TRUE(findScenario("RF2_rybinsk_fetch_55lb", &rybinsk));
  const auto rybinskResult = runScenario(rybinsk);
  TEST_ASSERT_TRUE_MESSAGE(rybinskResult.pass, "RF2 should pass with chop packet events");
  TEST_ASSERT_TRUE(hasEvent(rybinskResult, "CHOP_PACKET_EMU"));
}

void test_all_default_scenarios_pass() {
  const auto scenarios = hilsim::defaultScenarios();
  TEST_ASSERT_TRUE_MESSAGE(!scenarios.empty(), "defaultScenarios() must not be empty");
  for (const auto& s : scenarios) {
    const auto r = runScenario(s);
    if (!r.pass) {
      char msg[192];
      snprintf(msg, sizeof(msg), "scenario %s should pass, reason=%s", s.id.c_str(), r.reason.c_str());
      TEST_FAIL_MESSAGE(msg);
    }
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_deterministic_repeatable_for_same_seed);
  RUN_TEST(test_dropout_produces_stale_and_failsafe_events);
  RUN_TEST(test_control_loop_stall_produces_timeout_event);
  RUN_TEST(test_s0_hold_still_good_passes_expectations);
  RUN_TEST(test_s1_current_0p4_good_passes_expectations);
  RUN_TEST(test_s2_current_0p8_hard_passes_expectations);
  RUN_TEST(test_s9_nan_heading_injection_passes_as_expected_failsafe);
  RUN_TEST(test_s12_compass_dropout_triggers_sensor_timeout_failsafe);
  RUN_TEST(test_s14_power_loss_emits_loss_restore_and_failsafe);
  RUN_TEST(test_s16_display_loss_events_without_control_failure);
  RUN_TEST(test_rf_water_scenarios_are_in_default_catalog);
  RUN_TEST(test_rf_wave_wake_packets_emit_events);
  RUN_TEST(test_all_default_scenarios_pass);
  return UNITY_END();
}
