#include "AnchorControlLoop.h"

#include <unity.h>

using namespace simctl;

class FakeClock : public IClock {
public:
  unsigned long now = 0;
  unsigned long now_ms() const override { return now; }
};

class FakeGnss : public IGnssSource {
public:
  GnssFix fix;
  GnssFix getFix() override { return fix; }
};

class FakeHeading : public IHeadingSource {
public:
  HeadingSample heading;
  HeadingSample getHeading() override { return heading; }
};

class FakeActuator : public IActuator {
public:
  ActuatorCmd last;
  int calls = 0;
  void apply(const ActuatorCmd& cmd) override {
    last = cmd;
    calls++;
  }
};

static GnssFix goodFix() {
  GnssFix fix;
  fix.valid = true;
  fix.xM = 0.0f;
  fix.yM = 0.0f;
  fix.sats = 12;
  fix.hdop = 1.0f;
  fix.ageMs = 0;
  return fix;
}

static HeadingSample goodHeading() {
  HeadingSample heading;
  heading.valid = true;
  heading.headingDeg = 0.0f;
  heading.ageMs = 0;
  return heading;
}

static AnchorControlLoop makeLoop(FakeGnss& gnss,
                                  FakeHeading& heading,
                                  FakeActuator& actuator,
                                  FakeClock& clock,
                                  const ControllerConfig& cfg) {
  AnchorControlLoop loop(&gnss, &heading, &actuator, &clock);
  loop.setConfig(cfg);
  loop.reset();
  return loop;
}

void setUp() {}
void tearDown() {}

void test_good_gnss_entry_accepts_zero_timestamp_sample() {
  FakeClock clock;
  FakeGnss gnss;
  FakeHeading heading;
  FakeActuator actuator;
  gnss.fix = goodFix();
  heading.heading = goodHeading();

  ControllerConfig cfg;
  cfg.minGoodTimeToEnterMs = 2000;
  cfg.controlLoopTimeoutMs = 10000;
  cfg.sensorTimeoutMs = 10000;
  AnchorControlLoop loop = makeLoop(gnss, heading, actuator, clock, cfg);

  loop.step(0.0f, 0.0f);
  TEST_ASSERT_FALSE(loop.isAnchorActive());

  clock.now = 2000;
  loop.step(0.0f, 0.0f);
  TEST_ASSERT_TRUE(loop.isAnchorActive());
}

void test_sensor_timeout_accepts_zero_timestamp_sample() {
  FakeClock clock;
  FakeGnss gnss;
  FakeHeading heading;
  FakeActuator actuator;
  gnss.fix = goodFix();

  ControllerConfig cfg;
  cfg.minGoodTimeToEnterMs = 10000;
  cfg.controlLoopTimeoutMs = 10000;
  cfg.sensorTimeoutMs = 1500;
  AnchorControlLoop loop = makeLoop(gnss, heading, actuator, clock, cfg);

  loop.step(0.0f, 0.0f);
  TEST_ASSERT_FALSE(loop.isSafeStopped());

  clock.now = 1500;
  loop.step(0.0f, 0.0f);
  TEST_ASSERT_TRUE(loop.isSafeStopped());
  TEST_ASSERT_EQUAL((int)StopReason::SENSOR_TIMEOUT, (int)loop.stopReason());
}

void test_control_loop_timeout_accepts_zero_timestamp_sample() {
  FakeClock clock;
  FakeGnss gnss;
  FakeHeading heading;
  FakeActuator actuator;
  gnss.fix = goodFix();
  heading.heading = goodHeading();

  ControllerConfig cfg;
  cfg.minGoodTimeToEnterMs = 10000;
  cfg.controlLoopTimeoutMs = 200;
  cfg.sensorTimeoutMs = 10000;
  AnchorControlLoop loop = makeLoop(gnss, heading, actuator, clock, cfg);

  loop.step(0.0f, 0.0f);
  TEST_ASSERT_FALSE(loop.isSafeStopped());

  clock.now = 201;
  loop.step(0.0f, 0.0f);
  TEST_ASSERT_TRUE(loop.isSafeStopped());
  TEST_ASSERT_EQUAL((int)StopReason::CONTROL_LOOP_TIMEOUT, (int)loop.stopReason());
}

void test_bad_gnss_exit_accepts_zero_timestamp_sample() {
  FakeClock clock;
  FakeGnss gnss;
  FakeHeading heading;
  FakeActuator actuator;
  gnss.fix = goodFix();
  heading.heading = goodHeading();

  ControllerConfig cfg;
  cfg.minGoodTimeToEnterMs = 0;
  cfg.badTimeToExitMs = 1500;
  cfg.controlLoopTimeoutMs = 10000;
  cfg.sensorTimeoutMs = 10000;
  AnchorControlLoop loop = makeLoop(gnss, heading, actuator, clock, cfg);

  loop.step(0.0f, 0.0f);
  TEST_ASSERT_TRUE(loop.isAnchorActive());

  gnss.fix.valid = false;
  loop.step(0.0f, 0.0f);
  TEST_ASSERT_FALSE(loop.isSafeStopped());

  clock.now = 1500;
  loop.step(0.0f, 0.0f);
  TEST_ASSERT_TRUE(loop.isSafeStopped());
  TEST_ASSERT_EQUAL((int)StopReason::GPS_DATA_STALE, (int)loop.stopReason());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_good_gnss_entry_accepts_zero_timestamp_sample);
  RUN_TEST(test_sensor_timeout_accepts_zero_timestamp_sample);
  RUN_TEST(test_control_loop_timeout_accepts_zero_timestamp_sample);
  RUN_TEST(test_bad_gnss_exit_accepts_zero_timestamp_sample);
  return UNITY_END();
}
