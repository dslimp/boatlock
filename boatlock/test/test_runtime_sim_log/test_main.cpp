#include "RuntimeSimLog.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_build_runtime_sim_log_lines_formats_simple_outcomes() {
  RuntimeSimExecutionOutcome list;
  list.kind = RuntimeSimExecutionKind::LIST;
  list.text = "S0,S1";
  RuntimeSimExecutionOutcome status;
  status.kind = RuntimeSimExecutionKind::STATUS;
  status.text = "{\"state\":\"IDLE\"}";
  RuntimeSimExecutionOutcome aborted;
  aborted.kind = RuntimeSimExecutionKind::ABORTED;

  const std::vector<std::string> listLines = buildRuntimeSimLogLines(list);
  const std::vector<std::string> statusLines = buildRuntimeSimLogLines(status);
  const std::vector<std::string> abortLines = buildRuntimeSimLogLines(aborted);

  TEST_ASSERT_EQUAL(1, (int)listLines.size());
  TEST_ASSERT_EQUAL_STRING("[SIM] LIST S0,S1", listLines[0].c_str());
  TEST_ASSERT_EQUAL(1, (int)statusLines.size());
  TEST_ASSERT_EQUAL_STRING("[SIM] STATUS {\"state\":\"IDLE\"}", statusLines[0].c_str());
  TEST_ASSERT_EQUAL(1, (int)abortLines.size());
  TEST_ASSERT_EQUAL_STRING("[SIM] ABORTED", abortLines[0].c_str());
}

void test_build_runtime_sim_log_lines_formats_run_variants() {
  RuntimeSimExecutionOutcome started;
  started.kind = RuntimeSimExecutionKind::RUN_STARTED;
  started.scenarioId = "S7";
  started.speedup = 1;
  RuntimeSimExecutionOutcome rejected;
  rejected.kind = RuntimeSimExecutionKind::RUN_REJECTED;
  rejected.text = "invalid payload";
  RuntimeSimExecutionOutcome failed;
  failed.kind = RuntimeSimExecutionKind::RUN_REJECTED;
  failed.scenarioId = "S9";
  failed.text = "start failed";

  const std::vector<std::string> startedLines = buildRuntimeSimLogLines(started);
  const std::vector<std::string> rejectedLines = buildRuntimeSimLogLines(rejected);
  const std::vector<std::string> failedLines = buildRuntimeSimLogLines(failed);

  TEST_ASSERT_EQUAL_STRING("[SIM] RUN started id=S7 speedup=1", startedLines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("[SIM] RUN rejected: invalid payload", rejectedLines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("[SIM] RUN failed id=S9 err=start failed", failedLines[0].c_str());
}

void test_build_runtime_sim_log_lines_formats_report_and_unknown_command() {
  RuntimeSimExecutionOutcome report;
  report.kind = RuntimeSimExecutionKind::REPORT_CHUNKS;
  report.reportChunks = {"abc", "def"};
  RuntimeSimExecutionOutcome unknown;
  unknown.kind = RuntimeSimExecutionKind::UNKNOWN;
  unknown.text = "SIM_WAT";

  const std::vector<std::string> reportLines = buildRuntimeSimLogLines(report);
  const std::vector<std::string> unknownLines = buildRuntimeSimLogLines(unknown);

  TEST_ASSERT_EQUAL(2, (int)reportLines.size());
  TEST_ASSERT_EQUAL_STRING("[SIM] REPORT abc", reportLines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("[SIM] REPORT def", reportLines[1].c_str());
  TEST_ASSERT_EQUAL_STRING("[SIM] unknown command: SIM_WAT", unknownLines[0].c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_build_runtime_sim_log_lines_formats_simple_outcomes);
  RUN_TEST(test_build_runtime_sim_log_lines_formats_run_variants);
  RUN_TEST(test_build_runtime_sim_log_lines_formats_report_and_unknown_command);
  return UNITY_END();
}
