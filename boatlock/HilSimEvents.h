#pragma once

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "HilSimTime.h"

namespace hilsim {

struct SimEvent {
  unsigned long atMs = 0;
  std::string code;
  std::string details;
};

class SimEventLog {
public:
  SimEventLog(size_t maxEvents = 200,
              size_t maxSeenTokens = 64,
              unsigned long duplicateSuppressMs = 1500)
      : maxEvents_(maxEvents),
        maxSeenTokens_(maxSeenTokens),
        duplicateSuppressMs_(duplicateSuppressMs) {}

  void clear() {
    events_.clear();
    seenTokens_.clear();
    lastCode_.clear();
    lastDetails_.clear();
    lastAtMs_ = 0;
  }

  bool record(unsigned long atMs, const char* code, const char* details) {
    std::string codeStr = code ? code : "";
    std::string detailsStr = details ? details : "";
    markSeen(codeStr);
    markSeen(detailsStr);

    if (codeStr == lastCode_ && detailsStr == lastDetails_ &&
        simTimeWindowContains(atMs, lastAtMs_, duplicateSuppressMs_)) {
      return false;
    }
    lastCode_ = codeStr;
    lastDetails_ = detailsStr;
    lastAtMs_ = atMs;

    if (maxEvents_ == 0) {
      return true;
    }
    if (events_.size() >= maxEvents_) {
      events_.erase(events_.begin());
    }
    SimEvent ev;
    ev.atMs = atMs;
    ev.code = std::move(codeStr);
    ev.details = std::move(detailsStr);
    events_.push_back(std::move(ev));
    return true;
  }

  const std::vector<SimEvent>& events() const { return events_; }

  std::vector<SimEvent> takeEvents() { return std::move(events_); }

  bool wasSeen(const std::string& token) const {
    if (token.empty()) {
      return false;
    }
    for (const std::string& seen : seenTokens_) {
      if (seen == token) {
        return true;
      }
    }
    return false;
  }

private:
  void markSeen(const std::string& token) {
    if (token.empty()) {
      return;
    }
    for (const std::string& seen : seenTokens_) {
      if (seen == token) {
        return;
      }
    }
    if (seenTokens_.size() < maxSeenTokens_) {
      seenTokens_.push_back(token);
    }
  }

  size_t maxEvents_ = 200;
  size_t maxSeenTokens_ = 64;
  unsigned long duplicateSuppressMs_ = 1500;
  std::vector<SimEvent> events_;
  std::vector<std::string> seenTokens_;
  std::string lastCode_;
  std::string lastDetails_;
  unsigned long lastAtMs_ = 0;
};

} // namespace hilsim
