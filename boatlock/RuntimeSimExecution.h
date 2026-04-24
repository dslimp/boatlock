#pragma once

#include <stddef.h>

#include <string>
#include <vector>

#include "RuntimeSimCommand.h"

enum class RuntimeSimExecutionKind {
  NONE,
  LIST,
  STATUS,
  RUN_STARTED,
  RUN_REJECTED,
  ABORTED,
  REPORT_UNAVAILABLE,
  REPORT_CHUNKS,
  UNKNOWN,
};

struct RuntimeSimExecutionOutcome {
  bool handled = false;
  bool shouldStopMotion = false;
  bool shouldClearFailsafe = false;
  bool shouldClearAnchorDenied = false;
  bool shouldResetSimWallClock = false;
  RuntimeSimExecutionKind kind = RuntimeSimExecutionKind::NONE;
  std::string text;
  std::string scenarioId;
  int speedup = 0;
  std::vector<std::string> reportChunks;
};

inline void buildRuntimeSimReportChunks(const std::string& json,
                                        size_t chunkSize,
                                        std::vector<std::string>* out) {
  if (!out || chunkSize == 0) {
    return;
  }
  out->clear();
  for (size_t i = 0; i < json.size(); i += chunkSize) {
    out->push_back(json.substr(i, chunkSize));
  }
}

template <typename SimManager>
inline RuntimeSimExecutionOutcome executeRuntimeSimCommand(const RuntimeSimCommand& parsed,
                                                           const std::string& rawCommand,
                                                           SimManager& hilSim,
                                                           size_t reportChunkSize = 220) {
  RuntimeSimExecutionOutcome out;
  if (parsed.type == RuntimeSimCommandType::NONE) {
    return out;
  }

  out.handled = true;
  switch (parsed.type) {
    case RuntimeSimCommandType::LIST:
      out.kind = RuntimeSimExecutionKind::LIST;
      out.text = hilSim.listCsv();
      return out;
    case RuntimeSimCommandType::STATUS:
      out.kind = RuntimeSimExecutionKind::STATUS;
      out.text = hilSim.statusJson();
      return out;
    case RuntimeSimCommandType::RUN: {
      out.shouldStopMotion = true;
      out.shouldClearFailsafe = true;
      out.shouldClearAnchorDenied = true;
      if (!parsed.valid) {
        out.kind = RuntimeSimExecutionKind::RUN_REJECTED;
        out.text = "invalid payload";
        return out;
      }
      std::string err;
      if (!hilSim.startRun(parsed.scenarioId, parsed.speedup, &err)) {
        out.kind = RuntimeSimExecutionKind::RUN_REJECTED;
        out.scenarioId = parsed.scenarioId;
        out.text = err;
        return out;
      }
      out.kind = RuntimeSimExecutionKind::RUN_STARTED;
      out.shouldResetSimWallClock = true;
      out.scenarioId = parsed.scenarioId;
      out.speedup = parsed.speedup;
      return out;
    }
    case RuntimeSimCommandType::ABORT:
      hilSim.abortRun();
      out.kind = RuntimeSimExecutionKind::ABORTED;
      return out;
    case RuntimeSimCommandType::REPORT: {
      const std::string json = hilSim.reportJson();
      if (json.empty()) {
        out.kind = RuntimeSimExecutionKind::REPORT_UNAVAILABLE;
        return out;
      }
      out.kind = RuntimeSimExecutionKind::REPORT_CHUNKS;
      buildRuntimeSimReportChunks(json, reportChunkSize, &out.reportChunks);
      return out;
    }
    case RuntimeSimCommandType::UNKNOWN:
      out.kind = RuntimeSimExecutionKind::UNKNOWN;
      out.text = rawCommand;
      return out;
    case RuntimeSimCommandType::NONE:
    default:
      out.handled = false;
      out.kind = RuntimeSimExecutionKind::NONE;
      return out;
  }
}
