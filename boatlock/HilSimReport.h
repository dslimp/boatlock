#pragma once

#include <stddef.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "HilSimEvents.h"
#include "HilSimExpect.h"
#include "HilSimJson.h"

namespace hilsim {

inline std::string buildSimReportJson(const std::string& id,
                                      const char* state,
                                      bool pass,
                                      const std::string& reason,
                                      const SimMetrics& metrics,
                                      const std::vector<SimEvent>& events,
                                      size_t maxEventsInReport) {
  std::string out;
  char line[256];
  out += "{\"id\":";
  appendSimJsonString(out, id);
  out += ",\"state\":\"";
  out += state ? state : "";
  out += "\",\"pass\":";
  out += pass ? "true" : "false";
  out += ",\"reason\":";
  appendSimJsonString(out, reason);
  out += ",";

  snprintf(line,
           sizeof(line),
           "\"metrics\":{\"p95_error_m\":%.3f,\"max_error_m\":%.3f,"
           "\"time_in_deadband_pct\":%.2f,\"time_saturated_pct\":%.2f,"
           "\"dir_changes_per_min\":%.2f,\"ramp_violations\":%lu,"
           "\"max_bad_gnss_in_anchor_s\":%.3f,\"nan_detected\":%s,"
           "\"out_of_range_command\":%s}",
           metrics.p95ErrorM,
           metrics.maxErrorM,
           metrics.timeInDeadbandPct,
           metrics.timeSaturatedPct,
           metrics.dirChangesPerMin,
           (unsigned long)metrics.rampViolations,
           metrics.maxBadGnssInAnchorS,
           metrics.nanDetected ? "true" : "false",
           metrics.outOfRangeCommand ? "true" : "false");
  out += line;

  const size_t totalEvents = events.size();
  const size_t beginEventIdx =
      (totalEvents > maxEventsInReport) ? (totalEvents - maxEventsInReport) : 0;
  const size_t keptEvents = totalEvents - beginEventIdx;
  if (totalEvents > keptEvents) {
    char evCountBuf[96];
    snprintf(evCountBuf,
             sizeof(evCountBuf),
             ",\"events_total\":%lu,\"events_kept\":%lu",
             (unsigned long)totalEvents,
             (unsigned long)keptEvents);
    out += evCountBuf;
  }

  out += ",\"events\":[";
  for (size_t i = beginEventIdx; i < totalEvents; ++i) {
    const SimEvent& ev = events[i];
    if (i > beginEventIdx) {
      out += ",";
    }
    out += "{\"at_ms\":";
    snprintf(line, sizeof(line), "%lu", (unsigned long)ev.atMs);
    out += line;
    out += ",\"code\":";
    appendSimJsonString(out, ev.code);
    out += ",\"details\":";
    appendSimJsonString(out, ev.details);
    out += "}";
  }
  out += "]}";
  return out;
}

} // namespace hilsim
