#include "RuntimeSimExecution.h"

#include <unity.h>

#include <string>

namespace {

struct FakeSimManager {
  std::string list = "S0,S1";
  std::string status = "{\"state\":\"IDLE\"}";
  std::string report = "";
  bool startOk = true;
  int startCalls = 0;
  int abortCalls = 0;
  std::string lastScenarioId;
  int lastSpeedup = -1;

  std::string listCsv() const { return list; }
  std::string statusJson() const { return status; }
  bool startRun(const std::string& scenarioId, int speedup, std::string* err) {
    ++startCalls;
    lastScenarioId = scenarioId;
    lastSpeedup = speedup;
    if (!startOk) {
      if (err) {
        *err = "start failed";
      }
      return false;
    }
    return true;
  }
  void abortRun() { ++abortCalls; }
  std::string reportJson() const { return report; }
};

}  // namespace

void setUp() {}
void tearDown() {}

void test_execute_runtime_sim_command_returns_list_and_status_payloads() {
  FakeSimManager sim;

  RuntimeSimExecutionOutcome list =
      executeRuntimeSimCommand(parseRuntimeSimCommand("SIM_LIST"), "SIM_LIST", sim);
  RuntimeSimExecutionOutcome status =
      executeRuntimeSimCommand(parseRuntimeSimCommand("SIM_STATUS"), "SIM_STATUS", sim);

  TEST_ASSERT_TRUE(list.handled);
  TEST_ASSERT_EQUAL((int)RuntimeSimExecutionKind::LIST, (int)list.kind);
  TEST_ASSERT_EQUAL_STRING("S0,S1", list.text.c_str());
  TEST_ASSERT_TRUE(status.handled);
  TEST_ASSERT_EQUAL((int)RuntimeSimExecutionKind::STATUS, (int)status.kind);
  TEST_ASSERT_EQUAL_STRING("{\"state\":\"IDLE\"}", status.text.c_str());
}

void test_execute_runtime_sim_command_marks_run_side_effects_before_start() {
  FakeSimManager sim;

  const RuntimeSimExecutionOutcome out =
      executeRuntimeSimCommand(parseRuntimeSimCommand("SIM_RUN:S7,1"), "SIM_RUN:S7,1", sim);

  TEST_ASSERT_TRUE(out.handled);
  TEST_ASSERT_EQUAL((int)RuntimeSimExecutionKind::RUN_STARTED, (int)out.kind);
  TEST_ASSERT_TRUE(out.shouldStopMotion);
  TEST_ASSERT_TRUE(out.shouldClearFailsafe);
  TEST_ASSERT_TRUE(out.shouldClearAnchorDenied);
  TEST_ASSERT_TRUE(out.shouldResetSimWallClock);
  TEST_ASSERT_EQUAL(1, sim.startCalls);
  TEST_ASSERT_EQUAL_STRING("S7", sim.lastScenarioId.c_str());
  TEST_ASSERT_EQUAL(1, sim.lastSpeedup);
}

void test_execute_runtime_sim_command_rejects_invalid_run_payload() {
  FakeSimManager sim;

  const RuntimeSimExecutionOutcome out =
      executeRuntimeSimCommand(parseRuntimeSimCommand("SIM_RUN:"), "SIM_RUN:", sim);

  TEST_ASSERT_TRUE(out.handled);
  TEST_ASSERT_EQUAL((int)RuntimeSimExecutionKind::RUN_REJECTED, (int)out.kind);
  TEST_ASSERT_EQUAL_STRING("invalid payload", out.text.c_str());
  TEST_ASSERT_FALSE(out.shouldStopMotion);
  TEST_ASSERT_FALSE(out.shouldClearFailsafe);
  TEST_ASSERT_FALSE(out.shouldClearAnchorDenied);
  TEST_ASSERT_FALSE(out.shouldResetSimWallClock);
  TEST_ASSERT_EQUAL(0, sim.startCalls);
}

void test_execute_runtime_sim_command_returns_run_error_when_start_fails() {
  FakeSimManager sim;
  sim.startOk = false;

  const RuntimeSimExecutionOutcome out =
      executeRuntimeSimCommand(parseRuntimeSimCommand("SIM_RUN:S9,0"), "SIM_RUN:S9,0", sim);

  TEST_ASSERT_TRUE(out.handled);
  TEST_ASSERT_EQUAL((int)RuntimeSimExecutionKind::RUN_REJECTED, (int)out.kind);
  TEST_ASSERT_EQUAL_STRING("S9", out.scenarioId.c_str());
  TEST_ASSERT_EQUAL_STRING("start failed", out.text.c_str());
  TEST_ASSERT_FALSE(out.shouldStopMotion);
  TEST_ASSERT_FALSE(out.shouldClearFailsafe);
  TEST_ASSERT_FALSE(out.shouldClearAnchorDenied);
  TEST_ASSERT_FALSE(out.shouldResetSimWallClock);
  TEST_ASSERT_EQUAL(1, sim.startCalls);
}

void test_execute_runtime_sim_command_aborts_and_reports_unavailable() {
  FakeSimManager sim;

  const RuntimeSimExecutionOutcome abort =
      executeRuntimeSimCommand(parseRuntimeSimCommand("SIM_ABORT"), "SIM_ABORT", sim);
  const RuntimeSimExecutionOutcome report =
      executeRuntimeSimCommand(parseRuntimeSimCommand("SIM_REPORT"), "SIM_REPORT", sim);

  TEST_ASSERT_EQUAL((int)RuntimeSimExecutionKind::ABORTED, (int)abort.kind);
  TEST_ASSERT_EQUAL(1, sim.abortCalls);
  TEST_ASSERT_EQUAL((int)RuntimeSimExecutionKind::REPORT_UNAVAILABLE, (int)report.kind);
  TEST_ASSERT_EQUAL(0, (int)report.reportChunks.size());
}

void test_execute_runtime_sim_command_chunks_report_and_keeps_unknown_command_text() {
  FakeSimManager sim;
  sim.report = "abcdefghijklmnopqrstuvwxyz";

  const RuntimeSimExecutionOutcome report = executeRuntimeSimCommand(
      parseRuntimeSimCommand("SIM_REPORT"), "SIM_REPORT", sim, 10);
  const RuntimeSimExecutionOutcome unknown = executeRuntimeSimCommand(
      parseRuntimeSimCommand("SIM_WAT"), "SIM_WAT", sim);

  TEST_ASSERT_EQUAL((int)RuntimeSimExecutionKind::REPORT_CHUNKS, (int)report.kind);
  TEST_ASSERT_EQUAL(3, (int)report.reportChunks.size());
  TEST_ASSERT_EQUAL_STRING("abcdefghij", report.reportChunks[0].c_str());
  TEST_ASSERT_EQUAL_STRING("klmnopqrst", report.reportChunks[1].c_str());
  TEST_ASSERT_EQUAL_STRING("uvwxyz", report.reportChunks[2].c_str());
  TEST_ASSERT_EQUAL((int)RuntimeSimExecutionKind::UNKNOWN, (int)unknown.kind);
  TEST_ASSERT_EQUAL_STRING("SIM_WAT", unknown.text.c_str());
}

void test_report_chunk_zero_size_is_unavailable_and_clears_output() {
  FakeSimManager sim;
  sim.report = "abcdefghijklmnopqrstuvwxyz";

  std::vector<std::string> chunks;
  chunks.push_back("stale");
  buildRuntimeSimReportChunks(sim.report, 0, &chunks);

  const RuntimeSimExecutionOutcome report = executeRuntimeSimCommand(
      parseRuntimeSimCommand("SIM_REPORT"), "SIM_REPORT", sim, 0);

  TEST_ASSERT_EQUAL(0, (int)chunks.size());
  TEST_ASSERT_EQUAL((int)RuntimeSimExecutionKind::REPORT_UNAVAILABLE, (int)report.kind);
  TEST_ASSERT_EQUAL(0, (int)report.reportChunks.size());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_execute_runtime_sim_command_returns_list_and_status_payloads);
  RUN_TEST(test_execute_runtime_sim_command_marks_run_side_effects_before_start);
  RUN_TEST(test_execute_runtime_sim_command_rejects_invalid_run_payload);
  RUN_TEST(test_execute_runtime_sim_command_returns_run_error_when_start_fails);
  RUN_TEST(test_execute_runtime_sim_command_aborts_and_reports_unavailable);
  RUN_TEST(test_execute_runtime_sim_command_chunks_report_and_keeps_unknown_command_text);
  RUN_TEST(test_report_chunk_zero_size_is_unavailable_and_clears_output);
  return UNITY_END();
}
