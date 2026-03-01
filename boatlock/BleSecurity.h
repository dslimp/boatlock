#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include "Settings.h"

class BleSecurity {
public:
  enum class RejectReason : uint8_t {
    NONE = 0,
    NOT_PAIRED,
    PAIRING_WINDOW_CLOSED,
    INVALID_PAIR_CODE,
    AUTH_REQUIRED,
    AUTH_FAILED,
    BAD_FORMAT,
    BAD_SIGNATURE,
    REPLAY_DETECTED,
    RATE_LIMIT,
  };

  static constexpr uint8_t kStoreVersion = 1;
  static constexpr unsigned long kPairingWindowMs = 120000;
  static constexpr unsigned long kRateWindowMs = 1000;
  static constexpr uint16_t kMaxCommandsPerWindow = 25;
  static constexpr int EEPROM_ADDR = Settings::CRC_ADDR + sizeof(uint32_t) + 64;
  static constexpr int STORED_BYTES = 12;

  void load() {
    Stored s{};
    EEPROM.get(EEPROM_ADDR, s);
    if (s.version != kStoreVersion || s.crc != calcCrc(s)) {
      paired_ = false;
      ownerCode_ = 0;
      save();
      return;
    }
    paired_ = s.paired != 0;
    ownerCode_ = s.ownerCode;
    if (!validPairCode(ownerCode_)) {
      paired_ = false;
      ownerCode_ = 0;
      save();
    }
  }

  void save() {
    Stored s{};
    s.version = kStoreVersion;
    s.paired = paired_ ? 1 : 0;
    s.ownerCode = ownerCode_;
    s.crc = calcCrc(s);
    EEPROM.put(EEPROM_ADDR, s);
    EEPROM.commit();
  }

  bool isPaired() const { return paired_; }
  bool sessionActive() const { return sessionActive_; }
  uint32_t sessionNonce() const { return sessionNonce_; }
  uint32_t lastRejectCode() const { return (uint32_t)lastReject_; }
  const char* lastRejectString() const { return rejectReasonString(lastReject_); }

  void openPairingWindow(unsigned long nowMs, unsigned long windowMs = kPairingWindowMs) {
    pairingWindowOpen_ = true;
    pairingWindowUntilMs_ = nowMs + windowMs;
    setReject(RejectReason::NONE);
  }

  bool pairingWindowOpen(unsigned long nowMs) {
    if (pairingWindowOpen_ && nowMs >= pairingWindowUntilMs_) {
      pairingWindowOpen_ = false;
    }
    return pairingWindowOpen_;
  }

  bool setPairCode(uint32_t code, unsigned long nowMs) {
    if (!pairingWindowOpen(nowMs)) {
      setReject(RejectReason::PAIRING_WINDOW_CLOSED);
      return false;
    }
    if (!validPairCode(code)) {
      setReject(RejectReason::INVALID_PAIR_CODE);
      return false;
    }
    ownerCode_ = code;
    paired_ = true;
    sessionActive_ = false;
    sessionNonce_ = 0;
    sessionKey_ = 0;
    lastCounter_ = 0;
    save();
    setReject(RejectReason::NONE);
    return true;
  }

  void clearPairing() {
    paired_ = false;
    ownerCode_ = 0;
    sessionActive_ = false;
    sessionNonce_ = 0;
    sessionKey_ = 0;
    lastCounter_ = 0;
    save();
    setReject(RejectReason::NONE);
  }

  uint32_t startAuth(unsigned long nowMs) {
    if (!paired_) {
      setReject(RejectReason::NOT_PAIRED);
      return 0;
    }
    const uint32_t seed = (uint32_t)(nowMs ^ (ownerCode_ << 7) ^ 0xA5A5A5A5u);
    sessionNonce_ = fnv1a32((const uint8_t*)&seed, sizeof(seed));
    if (sessionNonce_ == 0) {
      sessionNonce_ = 1;
    }
    sessionActive_ = false;
    sessionKey_ = 0;
    lastCounter_ = 0;
    setReject(RejectReason::NONE);
    return sessionNonce_;
  }

  bool proveAuthHex(const char* proofHex) {
    if (!paired_ || sessionNonce_ == 0) {
      setReject(RejectReason::AUTH_REQUIRED);
      return false;
    }
    uint32_t provided = 0;
    if (!parseHexU32(proofHex, &provided)) {
      setReject(RejectReason::BAD_FORMAT);
      return false;
    }
    const uint32_t expected = authProof();
    if (provided != expected) {
      setReject(RejectReason::AUTH_FAILED);
      sessionActive_ = false;
      sessionKey_ = 0;
      return false;
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "SK:%08lX", (unsigned long)expected);
    sessionKey_ = fnv1a32((const uint8_t*)buf, strlen(buf));
    sessionActive_ = true;
    lastCounter_ = 0;
    rateWindowStartMs_ = 0;
    rateCount_ = 0;
    setReject(RejectReason::NONE);
    return true;
  }

  bool unwrapSecureCommand(const std::string& in,
                           std::string* outPayload,
                           unsigned long nowMs) {
    if (!outPayload) {
      setReject(RejectReason::BAD_FORMAT);
      return false;
    }
    outPayload->clear();
    if (!sessionActive_) {
      setReject(RejectReason::AUTH_REQUIRED);
      return false;
    }
    if (in.rfind("SEC_CMD:", 0) != 0) {
      setReject(RejectReason::BAD_FORMAT);
      return false;
    }

    size_t p1 = in.find(':', 8);
    if (p1 == std::string::npos) {
      setReject(RejectReason::BAD_FORMAT);
      return false;
    }
    size_t p2 = in.find(':', p1 + 1);
    if (p2 == std::string::npos || p2 + 1 >= in.size()) {
      setReject(RejectReason::BAD_FORMAT);
      return false;
    }

    uint32_t counter = 0;
    uint32_t sig = 0;
    const std::string counterHex = in.substr(8, p1 - 8);
    const std::string sigHex = in.substr(p1 + 1, p2 - (p1 + 1));
    if (!parseHexU32(counterHex.c_str(), &counter) ||
        !parseHexU32(sigHex.c_str(), &sig)) {
      setReject(RejectReason::BAD_FORMAT);
      return false;
    }

    if (!checkRateLimit(nowMs)) {
      setReject(RejectReason::RATE_LIMIT);
      return false;
    }

    if (counter <= lastCounter_) {
      setReject(RejectReason::REPLAY_DETECTED);
      return false;
    }

    const std::string payload = in.substr(p2 + 1);
    const uint32_t expected = commandSig(counter, payload.c_str());
    if (sig != expected) {
      setReject(RejectReason::BAD_SIGNATURE);
      return false;
    }

    lastCounter_ = counter;
    *outPayload = payload;
    setReject(RejectReason::NONE);
    return true;
  }

  bool commandNeedsAuth(const std::string& cmd) const {
    if (!paired_) {
      return false;
    }
    if (cmd == "STOP" || cmd == "ANCHOR_OFF" || cmd == "HEARTBEAT" || cmd == "all") {
      return false;
    }
    if (cmd.rfind("PAIR_", 0) == 0 || cmd.rfind("AUTH_", 0) == 0 || cmd.rfind("SEC_CMD:", 0) == 0) {
      return false;
    }
    // Any write/control command requires owner session once paired.
    return true;
  }

  static const char* rejectReasonString(RejectReason reason) {
    switch (reason) {
      case RejectReason::NONE:
        return "NONE";
      case RejectReason::NOT_PAIRED:
        return "NOT_PAIRED";
      case RejectReason::PAIRING_WINDOW_CLOSED:
        return "PAIRING_WINDOW_CLOSED";
      case RejectReason::INVALID_PAIR_CODE:
        return "INVALID_PAIR_CODE";
      case RejectReason::AUTH_REQUIRED:
        return "AUTH_REQUIRED";
      case RejectReason::AUTH_FAILED:
        return "AUTH_FAILED";
      case RejectReason::BAD_FORMAT:
        return "BAD_FORMAT";
      case RejectReason::BAD_SIGNATURE:
        return "BAD_SIGNATURE";
      case RejectReason::REPLAY_DETECTED:
        return "REPLAY_DETECTED";
      case RejectReason::RATE_LIMIT:
        return "RATE_LIMIT";
      default:
        return "UNKNOWN";
    }
  }

  uint32_t authProof() const {
    char buf[48];
    snprintf(buf, sizeof(buf), "%lu:%08lX",
             (unsigned long)ownerCode_,
             (unsigned long)sessionNonce_);
    return fnv1a32((const uint8_t*)buf, strlen(buf));
  }

  uint32_t commandSig(uint32_t counter, const char* payload) const {
    char prefix[48];
    snprintf(prefix, sizeof(prefix), "%08lX:%08lX:",
             (unsigned long)sessionKey_,
             (unsigned long)counter);
    uint32_t h = 2166136261u;
    h = fnv1a32Update(h, (const uint8_t*)prefix, strlen(prefix));
    h = fnv1a32Update(h, (const uint8_t*)payload, strlen(payload));
    return h;
  }

  static bool parseHexU32(const char* text, uint32_t* out) {
    if (!text || !out || !*text) {
      return false;
    }
    char* end = nullptr;
    unsigned long v = strtoul(text, &end, 16);
    if (!end || *end != '\0') {
      return false;
    }
    *out = (uint32_t)v;
    return true;
  }

private:
  struct Stored {
    uint8_t version = 0;
    uint8_t paired = 0;
    uint16_t reserved = 0;
    uint32_t ownerCode = 0;
    uint32_t crc = 0;
  };

  bool paired_ = false;
  uint32_t ownerCode_ = 0;
  bool pairingWindowOpen_ = false;
  unsigned long pairingWindowUntilMs_ = 0;
  bool sessionActive_ = false;
  uint32_t sessionNonce_ = 0;
  uint32_t sessionKey_ = 0;
  uint32_t lastCounter_ = 0;
  unsigned long rateWindowStartMs_ = 0;
  uint16_t rateCount_ = 0;
  RejectReason lastReject_ = RejectReason::NONE;

  static bool validPairCode(uint32_t code) {
    return code >= 100000u && code <= 999999u;
  }

  bool checkRateLimit(unsigned long nowMs) {
    if (rateWindowStartMs_ == 0 || nowMs < rateWindowStartMs_ || nowMs - rateWindowStartMs_ > kRateWindowMs) {
      rateWindowStartMs_ = nowMs;
      rateCount_ = 0;
    }
    ++rateCount_;
    return rateCount_ <= kMaxCommandsPerWindow;
  }

  void setReject(RejectReason reason) { lastReject_ = reason; }

  static uint32_t calcCrc(const Stored& s) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&s);
    const size_t len = sizeof(Stored) - sizeof(uint32_t);
    return fnv1a32(data, len);
  }

  static uint32_t fnv1a32Update(uint32_t seed, const uint8_t* data, size_t len) {
    uint32_t h = seed;
    for (size_t i = 0; i < len; ++i) {
      h ^= data[i];
      h *= 16777619u;
    }
    return h;
  }

  static uint32_t fnv1a32(const uint8_t* data, size_t len) {
    return fnv1a32Update(2166136261u, data, len);
  }
};
