#include "Settings.h"
#include <unity.h>

void logMessage(const char *, ...) {}

void setUp() {}
void tearDown() {}

void test_get_set() {
  Settings s;
  s.reset();
  TEST_ASSERT_FLOAT_WITHIN(0.001, 20.0, s.get("Kp"));
  TEST_ASSERT_TRUE(s.set("Kp", 30.0));
  TEST_ASSERT_EQUAL_FLOAT(30.0, s.get("Kp"));
  s.set("Kp", -1.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.01f, s.get("Kp"));
  s.set("Kp", 1000.0f);
  TEST_ASSERT_EQUAL_FLOAT(200.0f, s.get("Kp"));
}

void test_save_load() {
  Settings s;
  s.reset();
  s.set("Kp", 40.0f);
  s.save();
  s.set("Kp", 10.0f);
  s.load();
  TEST_ASSERT_EQUAL_FLOAT(40.0f, s.get("Kp"));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_get_set);
  RUN_TEST(test_save_load);
  return UNITY_END();
}
