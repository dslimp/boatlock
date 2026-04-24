#pragma once

#include <string>
#include <vector>

#include "RuntimeSimExecution.h"

inline std::vector<std::string> buildRuntimeSimLogLines(const RuntimeSimExecutionOutcome& outcome) {
  std::vector<std::string> lines;
  switch (outcome.kind) {
    case RuntimeSimExecutionKind::LIST:
      lines.push_back("[SIM] LIST " + outcome.text);
      break;
    case RuntimeSimExecutionKind::STATUS:
      lines.push_back("[SIM] STATUS " + outcome.text);
      break;
    case RuntimeSimExecutionKind::RUN_STARTED:
      lines.push_back("[SIM] RUN started id=" + outcome.scenarioId +
                      " speedup=" + std::to_string(outcome.speedup));
      break;
    case RuntimeSimExecutionKind::RUN_REJECTED:
      if (!outcome.scenarioId.empty()) {
        lines.push_back("[SIM] RUN failed id=" + outcome.scenarioId + " err=" + outcome.text);
      } else {
        lines.push_back("[SIM] RUN rejected: " + outcome.text);
      }
      break;
    case RuntimeSimExecutionKind::ABORTED:
      lines.push_back("[SIM] ABORTED");
      break;
    case RuntimeSimExecutionKind::REPORT_UNAVAILABLE:
      lines.push_back("[SIM] REPORT unavailable");
      break;
    case RuntimeSimExecutionKind::REPORT_CHUNKS:
      for (const std::string& chunk : outcome.reportChunks) {
        lines.push_back("[SIM] REPORT " + chunk);
      }
      break;
    case RuntimeSimExecutionKind::UNKNOWN:
      lines.push_back("[SIM] unknown command: " + outcome.text);
      break;
    case RuntimeSimExecutionKind::NONE:
    default:
      break;
  }
  return lines;
}
