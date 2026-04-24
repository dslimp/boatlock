#pragma once

#include <string>
#include <vector>

#include "RuntimeSimExecution.h"

inline std::string sanitizeRuntimeSimLogField(const std::string& input, size_t maxLen = 160) {
  std::string out;
  out.reserve(input.size() < maxLen ? input.size() : maxLen);
  for (char ch : input) {
    if (out.size() >= maxLen) {
      break;
    }
    const unsigned char c = static_cast<unsigned char>(ch);
    if (c == '\r' || c == '\n' || c == '\t' || c == '\0') {
      out.push_back(' ');
    } else if (c < 0x20 || c == 0x7f) {
      out.push_back('?');
    } else {
      out.push_back(ch);
    }
  }
  return out;
}

inline std::vector<std::string> buildRuntimeSimLogLines(const RuntimeSimExecutionOutcome& outcome) {
  std::vector<std::string> lines;
  switch (outcome.kind) {
    case RuntimeSimExecutionKind::LIST:
      lines.push_back("[SIM] LIST " + sanitizeRuntimeSimLogField(outcome.text, 220));
      break;
    case RuntimeSimExecutionKind::STATUS:
      lines.push_back("[SIM] STATUS " + sanitizeRuntimeSimLogField(outcome.text, 220));
      break;
    case RuntimeSimExecutionKind::RUN_STARTED:
      lines.push_back("[SIM] RUN started id=" + sanitizeRuntimeSimLogField(outcome.scenarioId, 80) +
                      " speedup=" + std::to_string(outcome.speedup));
      break;
    case RuntimeSimExecutionKind::RUN_REJECTED:
      if (!outcome.scenarioId.empty()) {
        lines.push_back("[SIM] RUN failed id=" + sanitizeRuntimeSimLogField(outcome.scenarioId, 80) +
                        " err=" + sanitizeRuntimeSimLogField(outcome.text, 160));
      } else {
        lines.push_back("[SIM] RUN rejected: " + sanitizeRuntimeSimLogField(outcome.text, 160));
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
        lines.push_back("[SIM] REPORT " + sanitizeRuntimeSimLogField(chunk, 220));
      }
      break;
    case RuntimeSimExecutionKind::UNKNOWN:
      lines.push_back("[SIM] unknown command: " + sanitizeRuntimeSimLogField(outcome.text, 120));
      break;
    case RuntimeSimExecutionKind::NONE:
    default:
      break;
  }
  return lines;
}
