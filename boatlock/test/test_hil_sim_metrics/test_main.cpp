#include "HilSimMetrics.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_error_histogram_reports_zero_without_samples() {
  hilsim::SimErrorHistogram hist;
  TEST_ASSERT_EQUAL(0, hist.samples());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, hist.p95(9.0f));
}

void test_error_histogram_p95_uses_bin_centers() {
  hilsim::SimErrorHistogram hist;
  for (int i = 0; i < 19; ++i) {
    hist.add(1.02f);
  }
  hist.add(5.01f);

  TEST_ASSERT_EQUAL(20, hist.samples());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.05f, hist.p95());
}

void test_error_histogram_fails_closed_for_bad_values() {
  hilsim::SimErrorHistogram hist;
  hist.add(-1.0f);
  hist.add(NAN);

  TEST_ASSERT_EQUAL(2, hist.samples());
  TEST_ASSERT_TRUE(hist.p95() > 63.9f);
}

void test_error_histogram_clear_resets_samples() {
  hilsim::SimErrorHistogram hist;
  hist.add(2.0f);
  hist.clear();

  TEST_ASSERT_EQUAL(0, hist.samples());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, hist.p95());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_error_histogram_reports_zero_without_samples);
  RUN_TEST(test_error_histogram_p95_uses_bin_centers);
  RUN_TEST(test_error_histogram_fails_closed_for_bad_values);
  RUN_TEST(test_error_histogram_clear_resets_samples);
  return UNITY_END();
}
