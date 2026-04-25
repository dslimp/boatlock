#include "HilSimRandom.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_xorshift_same_seed_is_repeatable() {
  hilsim::XorShift32 a(12345);
  hilsim::XorShift32 b(12345);

  for (int i = 0; i < 10; ++i) {
    TEST_ASSERT_EQUAL(a.nextU32(), b.nextU32());
  }
}

void test_xorshift_zero_seed_does_not_stick_at_zero() {
  hilsim::XorShift32 rng(0);

  TEST_ASSERT_NOT_EQUAL(0, rng.nextU32());
  TEST_ASSERT_NOT_EQUAL(0, rng.nextU32());
}

void test_uniform01_stays_inside_unit_interval() {
  hilsim::XorShift32 rng(777);

  for (int i = 0; i < 32; ++i) {
    const float v = rng.uniform01();
    TEST_ASSERT_TRUE(v >= 0.0f);
    TEST_ASSERT_TRUE(v < 1.0f);
  }
}

void test_gauss_zero_sigma_is_zero() {
  hilsim::XorShift32 rng(42);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, rng.gauss(0.0f));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_xorshift_same_seed_is_repeatable);
  RUN_TEST(test_xorshift_zero_seed_does_not_stick_at_zero);
  RUN_TEST(test_uniform01_stays_inside_unit_interval);
  RUN_TEST(test_gauss_zero_sigma_is_zero);
  return UNITY_END();
}
