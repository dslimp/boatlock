#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include "Settings.h"

class BleSecurity {
private:
  struct Stored {
    uint8_t version = 0;
    uint8_t paired = 0;
    uint8_t ownerSecret[16] = {0};
    uint32_t crc = 0;
  };

public:
  enum class RejectReason : uint8_t {
    NONE = 0,
    NOT_PAIRED,
    PAIRING_WINDOW_CLOSED,
    INVALID_OWNER_SECRET,
    AUTH_REQUIRED,
    AUTH_FAILED,
    BAD_FORMAT,
    BAD_SIGNATURE,
    REPLAY_DETECTED,
    RATE_LIMIT,
  };

  static constexpr uint8_t kStoreVersion = 2;
  static constexpr size_t kOwnerSecretBytes = 16;
  static constexpr size_t kOwnerSecretHexChars = kOwnerSecretBytes * 2;
  static constexpr unsigned long kPairingWindowMs = 120000;
  static constexpr unsigned long kRateWindowMs = 1000;
  static constexpr uint16_t kMaxCommandsPerWindow = 25;
  static constexpr int EEPROM_ADDR = Settings::CRC_ADDR + sizeof(uint32_t) + 64;
  static constexpr int STORED_BYTES = sizeof(Stored);

  void load() {
    Stored s{};
    EEPROM.get(EEPROM_ADDR, s);
    if (s.version != kStoreVersion || s.crc != calcCrc(s)) {
      clearRuntimeState();
      paired_ = false;
      memset(ownerSecret_, 0, sizeof(ownerSecret_));
      save();
      return;
    }
    paired_ = s.paired != 0;
    memcpy(ownerSecret_, s.ownerSecret, sizeof(ownerSecret_));
    clearRuntimeState();
    if (paired_ && !ownerSecretValid()) {
      paired_ = false;
      memset(ownerSecret_, 0, sizeof(ownerSecret_));
      save();
    }
  }

  void save() {
    Stored s{};
    s.version = kStoreVersion;
    s.paired = paired_ ? 1 : 0;
    memcpy(s.ownerSecret, ownerSecret_, sizeof(ownerSecret_));
    s.crc = calcCrc(s);
    EEPROM.put(EEPROM_ADDR, s);
    EEPROM.commit();
  }

  bool isPaired() const { return paired_; }
  bool sessionActive() const { return sessionActive_; }
  uint64_t sessionNonce() const { return sessionNonce_; }
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

  bool setOwnerSecretHex(const char* secretHex, unsigned long nowMs) {
    if (!pairingWindowOpen(nowMs)) {
      setReject(RejectReason::PAIRING_WINDOW_CLOSED);
      return false;
    }
    uint8_t parsed[kOwnerSecretBytes] = {0};
    if (!parseHexBytes(secretHex, parsed, sizeof(parsed))) {
      setReject(RejectReason::INVALID_OWNER_SECRET);
      return false;
    }
    memcpy(ownerSecret_, parsed, sizeof(ownerSecret_));
    paired_ = true;
    clearRuntimeState();
    save();
    setReject(RejectReason::NONE);
    return true;
  }

  void clearPairing() {
    paired_ = false;
    memset(ownerSecret_, 0, sizeof(ownerSecret_));
    clearRuntimeState();
    save();
    setReject(RejectReason::NONE);
  }

  uint64_t startAuth(unsigned long nowMs) {
    if (!paired_) {
      setReject(RejectReason::NOT_PAIRED);
      return 0;
    }
    ++authSequence_;
    const uint64_t k0 = read64le(ownerSecret_);
    const uint64_t k1 = read64le(ownerSecret_ + 8);
    uint64_t seed = (static_cast<uint64_t>(nowMs) << 32) ^ authSequence_ ^ k0 ^ rotl64(k1, 17);
    sessionNonce_ = mix64(seed);
    if (sessionNonce_ == 0) {
      sessionNonce_ = 1;
    }
    sessionActive_ = false;
    lastCounter_ = 0;
    rateWindowStartMs_ = 0;
    rateCount_ = 0;
    setReject(RejectReason::NONE);
    return sessionNonce_;
  }

  bool proveAuthHex(const char* proofHex) {
    if (!paired_ || sessionNonce_ == 0) {
      setReject(RejectReason::AUTH_REQUIRED);
      return false;
    }
    uint64_t provided = 0;
    if (!parseHexU64(proofHex, &provided)) {
      setReject(RejectReason::BAD_FORMAT);
      return false;
    }
    const uint64_t expected = authProof();
    if (provided != expected) {
      setReject(RejectReason::AUTH_FAILED);
      sessionActive_ = false;
      lastCounter_ = 0;
      return false;
    }
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
    uint64_t sig = 0;
    const std::string counterHex = in.substr(8, p1 - 8);
    const std::string sigHex = in.substr(p1 + 1, p2 - (p1 + 1));
    if (!parseHexU32(counterHex.c_str(), &counter) ||
        !parseHexU64(sigHex.c_str(), &sig)) {
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
    const uint64_t expected = commandSig(counter, payload.c_str());
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
    if (cmd == "STOP" || cmd == "ANCHOR_OFF" || cmd == "HEARTBEAT" ||
        cmd == "STREAM_START" || cmd == "STREAM_STOP" || cmd == "SNAPSHOT") {
      return false;
    }
    if (cmd.rfind("AUTH_", 0) == 0 || cmd.rfind("PAIR_SET:", 0) == 0 ||
        cmd.rfind("SEC_CMD:", 0) == 0) {
      return false;
    }
    if (cmd == "PAIR_CLEAR") {
      return true;
    }
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
      case RejectReason::INVALID_OWNER_SECRET:
        return "INVALID_OWNER_SECRET";
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

  uint64_t authProof() const {
    return computeMac("AUTH", 0, nullptr);
  }

  uint64_t commandSig(uint32_t counter, const char* payload) const {
    return computeMac("CMD", counter, payload);
  }

  std::string sessionNonceHex() const {
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llX", static_cast<unsigned long long>(sessionNonce_));
    return std::string(buf);
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
    *out = static_cast<uint32_t>(v);
    return true;
  }

  static bool parseHexU64(const char* text, uint64_t* out) {
    if (!text || !out || !*text) {
      return false;
    }
    char* end = nullptr;
    unsigned long long v = strtoull(text, &end, 16);
    if (!end || *end != '\0') {
      return false;
    }
    *out = static_cast<uint64_t>(v);
    return true;
  }

  bool paired_ = false;
  uint8_t ownerSecret_[kOwnerSecretBytes] = {0};
  bool pairingWindowOpen_ = false;
  unsigned long pairingWindowUntilMs_ = 0;
  bool sessionActive_ = false;
  uint64_t sessionNonce_ = 0;
  uint32_t lastCounter_ = 0;
  uint64_t authSequence_ = 0;
  unsigned long rateWindowStartMs_ = 0;
  uint16_t rateCount_ = 0;
  RejectReason lastReject_ = RejectReason::NONE;

  bool ownerSecretValid() const {
    for (size_t i = 0; i < sizeof(ownerSecret_); ++i) {
      if (ownerSecret_[i] != 0) {
        return true;
      }
    }
    return false;
  }

  void clearRuntimeState() {
    sessionActive_ = false;
    sessionNonce_ = 0;
    lastCounter_ = 0;
    authSequence_ = 0;
    rateWindowStartMs_ = 0;
    rateCount_ = 0;
  }

  bool checkRateLimit(unsigned long nowMs) {
    if (rateWindowStartMs_ == 0 || nowMs < rateWindowStartMs_ ||
        nowMs - rateWindowStartMs_ > kRateWindowMs) {
      rateWindowStartMs_ = nowMs;
      rateCount_ = 0;
    }
    ++rateCount_;
    return rateCount_ <= kMaxCommandsPerWindow;
  }

  void setReject(RejectReason reason) { lastReject_ = reason; }

  uint64_t computeMac(const char* scope, uint32_t counter, const char* payload) const {
    char msg[160];
    if (payload && *payload) {
      snprintf(msg,
               sizeof(msg),
               "%s:%016llX:%08lX:%s",
               scope,
               static_cast<unsigned long long>(sessionNonce_),
               static_cast<unsigned long>(counter),
               payload);
    } else {
      snprintf(msg,
               sizeof(msg),
               "%s:%016llX",
               scope,
               static_cast<unsigned long long>(sessionNonce_));
    }
    return sipHash24(ownerSecret_, reinterpret_cast<const uint8_t*>(msg), strlen(msg));
  }

  static uint32_t calcCrc(const Stored& s) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&s);
    const size_t len = sizeof(Stored) - sizeof(uint32_t);
    return fnv1a32(data, len);
  }

  static bool parseHexBytes(const char* text, uint8_t* out, size_t outLen) {
    if (!text || !out) {
      return false;
    }
    const size_t len = strlen(text);
    if (len != outLen * 2) {
      return false;
    }
    for (size_t i = 0; i < outLen; ++i) {
      const int hi = hexNibble(text[i * 2]);
      const int lo = hexNibble(text[i * 2 + 1]);
      if (hi < 0 || lo < 0) {
        return false;
      }
      out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
  }

  static int hexNibble(char c) {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
      return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
      return 10 + (c - 'A');
    }
    return -1;
  }

  static uint32_t fnv1a32(const uint8_t* data, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
      h ^= data[i];
      h *= 16777619u;
    }
    return h;
  }

  static uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
  }

  static uint64_t read64le(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
      v |= static_cast<uint64_t>(p[i]) << (i * 8);
    }
    return v;
  }

  static uint64_t rotl64(uint64_t v, uint8_t shift) {
    return (v << shift) | (v >> (64 - shift));
  }

  static void sipRound(uint64_t& v0, uint64_t& v1, uint64_t& v2, uint64_t& v3) {
    v0 += v1;
    v1 = rotl64(v1, 13);
    v1 ^= v0;
    v0 = rotl64(v0, 32);
    v2 += v3;
    v3 = rotl64(v3, 16);
    v3 ^= v2;
    v0 += v3;
    v3 = rotl64(v3, 21);
    v3 ^= v0;
    v2 += v1;
    v1 = rotl64(v1, 17);
    v1 ^= v2;
    v2 = rotl64(v2, 32);
  }

  static uint64_t sipHash24(const uint8_t key[16], const uint8_t* data, size_t len) {
    const uint64_t k0 = read64le(key);
    const uint64_t k1 = read64le(key + 8);
    uint64_t v0 = 0x736f6d6570736575ull ^ k0;
    uint64_t v1 = 0x646f72616e646f6dull ^ k1;
    uint64_t v2 = 0x6c7967656e657261ull ^ k0;
    uint64_t v3 = 0x7465646279746573ull ^ k1;

    const size_t blocks = len / 8;
    for (size_t i = 0; i < blocks; ++i) {
      const uint64_t m = read64le(data + (i * 8));
      v3 ^= m;
      sipRound(v0, v1, v2, v3);
      sipRound(v0, v1, v2, v3);
      v0 ^= m;
    }

    uint64_t b = static_cast<uint64_t>(len) << 56;
    const size_t tailOffset = blocks * 8;
    for (size_t i = len - tailOffset; i > 0; --i) {
      b |= static_cast<uint64_t>(data[tailOffset + i - 1]) << ((i - 1) * 8);
    }

    v3 ^= b;
    sipRound(v0, v1, v2, v3);
    sipRound(v0, v1, v2, v3);
    v0 ^= b;
    v2 ^= 0xff;
    sipRound(v0, v1, v2, v3);
    sipRound(v0, v1, v2, v3);
    sipRound(v0, v1, v2, v3);
    sipRound(v0, v1, v2, v3);
    return v0 ^ v1 ^ v2 ^ v3;
  }
};
