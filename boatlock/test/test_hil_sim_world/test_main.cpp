#include "HilSimWorld.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_boat_world_reset_sets_pose_and_clears_velocity() {
  hilsim::BoatSim2D world;
  world.state().vxBody = 1.0f;
  world.state().vyBody = -2.0f;

  world.reset(3.0f, -4.0f, 270.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.0f, world.state().xM);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -4.0f, world.state().yM);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 270.0f, world.state().psiDeg);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, world.state().vxBody);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, world.state().vyBody);
}

void test_boat_world_current_moves_quiet_boat() {
  hilsim::BoatSim2D world;
  world.reset(0.0f, 0.0f, 0.0f);
  ActuatorCmd cmd;
  hilsim::Vec2 current;
  current.x = 0.5f;
  current.y = -0.25f;

  world.step(2.0f, cmd, current);

  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, world.state().xM);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.5f, world.state().yM);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, world.state().vxBody);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, world.state().vyBody);
}

void test_boat_world_applies_clamped_thrust_and_turn_rate() {
  hilsim::BoatSim2D world;
  hilsim::BoatWorldConfig cfg;
  cfg.vMaxMps = 1.2f;
  cfg.tauV = 1.0f;
  cfg.tauPsi = 1.0f;
  cfg.maxTurnRateDegS = 90.0f;
  world.setConfig(cfg);
  world.reset(0.0f, 0.0f, 0.0f);

  ActuatorCmd cmd;
  cmd.thrust = 2.0f;
  cmd.steerDeg = 90.0f;
  cmd.stop = false;
  world.step(1.0f, cmd, hilsim::Vec2());

  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.2f, world.state().vxBody);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.2f, world.state().xM);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 90.0f, world.state().psiDeg);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boat_world_reset_sets_pose_and_clears_velocity);
  RUN_TEST(test_boat_world_current_moves_quiet_boat);
  RUN_TEST(test_boat_world_applies_clamped_thrust_and_turn_rate);
  return UNITY_END();
}
