#pragma once

#include <stdio.h>

#include <string>

#include "HilSimJson.h"

namespace hilsim {

inline std::string buildSimStatusJson(const std::string& id,
                                      const char* state,
                                      float progressPct,
                                      unsigned long simMs,
                                      unsigned long durationMs,
                                      unsigned long dtMs) {
  std::string out;
  out += "{\"id\":";
  appendSimJsonString(out, id);
  char tail[160];
  snprintf(tail,
           sizeof(tail),
           ",\"state\":\"%s\",\"progress_pct\":%.2f,"
           "\"sim_ms\":%lu,\"duration_ms\":%lu,\"dt_ms\":%lu}",
           state ? state : "",
           progressPct,
           (unsigned long)simMs,
           (unsigned long)durationMs,
           (unsigned long)dtMs);
  out += tail;
  return out;
}

} // namespace hilsim
