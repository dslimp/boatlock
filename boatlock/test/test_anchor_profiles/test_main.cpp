#include "AnchorProfiles.h"
#include <unity.h>

void logMessage(const char*, ...) {}

void setUp() {}
void tearDown() {}

void test_parse_anchor_profile_accepts_names_and_ids() {
  AnchorProfileId id = AnchorProfileId::NORMAL;
  TEST_ASSERT_TRUE(parseAnchorProfile("quiet", &id));
  TEST_ASSERT_EQUAL((int)AnchorProfileId::QUIET, (int)id);
  TEST_ASSERT_TRUE(parseAnchorProfile("CURRENT", &id));
  TEST_ASSERT_EQUAL((int)AnchorProfileId::CURRENT, (int)id);
  TEST_ASSERT_TRUE(parseAnchorProfile("1", &id));
  TEST_ASSERT_EQUAL((int)AnchorProfileId::NORMAL, (int)id);
  TEST_ASSERT_FALSE(parseAnchorProfile("storm", &id));
}

void test_apply_anchor_profile_sets_expected_values() {
  Settings s;
  s.reset();
  TEST_ASSERT_TRUE(applyAnchorProfile(s, AnchorProfileId::QUIET));
  TEST_ASSERT_EQUAL_FLOAT(3.0f, s.get("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(2.2f, s.get("DeadbandM"));
  TEST_ASSERT_EQUAL_FLOAT(45.0f, s.get("MaxThrustA"));
  TEST_ASSERT_EQUAL_FLOAT(20.0f, s.get("ThrRampA"));

  TEST_ASSERT_TRUE(applyAnchorProfile(s, AnchorProfileId::CURRENT));
  TEST_ASSERT_EQUAL_FLOAT(1.5f, s.get("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, s.get("DeadbandM"));
  TEST_ASSERT_EQUAL_FLOAT(90.0f, s.get("MaxThrustA"));
  TEST_ASSERT_EQUAL_FLOAT(55.0f, s.get("ThrRampA"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_anchor_profile_accepts_names_and_ids);
  RUN_TEST(test_apply_anchor_profile_sets_expected_values);
  return UNITY_END();
}
