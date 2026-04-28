#pragma once

#include <stdint.h>
#include <string>

#include "AnchorSupervisor.h"
#include "RuntimeBleCommandScope.h"

enum class RuntimeBleControlRole : uint8_t {
  APP = 0,
  REMOTE = 1,
};

enum class RuntimeBleControlLeaseCommandKind : uint8_t {
  REJECT = 0,
  READ_ONLY,
  CONTROL,
  STOP_PREEMPT,
};

enum class RuntimeBleControlLeaseDecision : uint8_t {
  REJECT_INVALID = 0,
  REJECT_BUSY,
  REJECT_ROLE,
  ALLOW_READ_ONLY,
  ACQUIRE,
  REFRESH,
  STOP_PREEMPT,
};

struct RuntimeBleControlClient {
  uint16_t connHandle = 0;
  uint32_t generation = 0;
  RuntimeBleControlRole role = RuntimeBleControlRole::APP;
  bool valid = false;
};

struct RuntimeBleControlLeaseResult {
  RuntimeBleControlLeaseDecision decision =
      RuntimeBleControlLeaseDecision::REJECT_INVALID;
  RuntimeBleControlLeaseCommandKind commandKind =
      RuntimeBleControlLeaseCommandKind::REJECT;

  bool allowed() const {
    return decision == RuntimeBleControlLeaseDecision::ALLOW_READ_ONLY ||
           decision == RuntimeBleControlLeaseDecision::ACQUIRE ||
           decision == RuntimeBleControlLeaseDecision::REFRESH ||
           decision == RuntimeBleControlLeaseDecision::STOP_PREEMPT;
  }

  bool ownerCommand() const {
    return decision == RuntimeBleControlLeaseDecision::ACQUIRE ||
           decision == RuntimeBleControlLeaseDecision::REFRESH;
  }
};

inline bool runtimeBleControlClientSame(const RuntimeBleControlClient& a,
                                        const RuntimeBleControlClient& b) {
  return a.valid && b.valid && a.connHandle == b.connHandle &&
         a.generation == b.generation && a.role == b.role;
}

inline RuntimeBleControlLeaseCommandKind runtimeBleControlLeaseCommandKind(
    const std::string& command) {
  if (command == "STREAM_START" || command == "STREAM_STOP" ||
      command == "SNAPSHOT") {
    return RuntimeBleControlLeaseCommandKind::READ_ONLY;
  }

  if (command == "STOP") {
    return RuntimeBleControlLeaseCommandKind::STOP_PREEMPT;
  }

  if (runtimeBleClassifyCommand(command) != RuntimeBleCommandScope::UNKNOWN) {
    return RuntimeBleControlLeaseCommandKind::CONTROL;
  }

  return RuntimeBleControlLeaseCommandKind::REJECT;
}

inline bool runtimeBleControlRoleCanAcquire(RuntimeBleControlRole role) {
  return role == RuntimeBleControlRole::APP;
}

class RuntimeBleControlLease {
public:
  static constexpr unsigned long kDefaultTtlMs =
      AnchorSupervisor::kMinCommTimeoutMs;

  RuntimeBleControlLeaseResult authorize(const RuntimeBleControlClient& client,
                                         const std::string& command,
                                         unsigned long nowMs,
                                         unsigned long ttlMs = kDefaultTtlMs) {
    const RuntimeBleControlLeaseCommandKind kind =
        runtimeBleControlLeaseCommandKind(command);
    RuntimeBleControlLeaseResult result;
    result.commandKind = kind;

    if (kind == RuntimeBleControlLeaseCommandKind::READ_ONLY) {
      result.decision = RuntimeBleControlLeaseDecision::ALLOW_READ_ONLY;
      return result;
    }

    if (kind == RuntimeBleControlLeaseCommandKind::STOP_PREEMPT) {
      clear();
      result.decision = RuntimeBleControlLeaseDecision::STOP_PREEMPT;
      return result;
    }

    if (kind == RuntimeBleControlLeaseCommandKind::REJECT || !client.valid ||
        ttlMs == 0) {
      result.decision = RuntimeBleControlLeaseDecision::REJECT_INVALID;
      return result;
    }

    if (!runtimeBleControlRoleCanAcquire(client.role)) {
      result.decision = RuntimeBleControlLeaseDecision::REJECT_ROLE;
      return result;
    }

    if (!ownerActive(nowMs) || runtimeBleControlClientSame(owner_, client)) {
      const bool wasOwner = active_ && runtimeBleControlClientSame(owner_, client);
      owner_ = client;
      active_ = true;
      acquiredMs_ = wasOwner ? acquiredMs_ : nowMs;
      refreshedMs_ = nowMs;
      ttlMs_ = ttlMs;
      result.decision = wasOwner ? RuntimeBleControlLeaseDecision::REFRESH
                                 : RuntimeBleControlLeaseDecision::ACQUIRE;
      return result;
    }

    result.decision = RuntimeBleControlLeaseDecision::REJECT_BUSY;
    return result;
  }

  bool ownerActive(unsigned long nowMs) const {
    return active_ && elapsedMs(nowMs, refreshedMs_) < ttlMs_;
  }

  bool ownerIs(const RuntimeBleControlClient& client, unsigned long nowMs) const {
    return ownerActive(nowMs) && runtimeBleControlClientSame(owner_, client);
  }

  bool releaseOnDisconnect(const RuntimeBleControlClient& client) {
    if (!runtimeBleControlClientSame(owner_, client)) {
      return false;
    }
    clear();
    return true;
  }

  void clear() {
    active_ = false;
    owner_ = {};
    acquiredMs_ = 0;
    refreshedMs_ = 0;
    ttlMs_ = 0;
  }

  const RuntimeBleControlClient& owner() const { return owner_; }
  unsigned long acquiredMs() const { return acquiredMs_; }
  unsigned long refreshedMs() const { return refreshedMs_; }
  unsigned long ttlMs() const { return ttlMs_; }

private:
  static unsigned long elapsedMs(unsigned long nowMs, unsigned long thenMs) {
    return nowMs - thenMs;
  }

  RuntimeBleControlClient owner_;
  bool active_ = false;
  unsigned long acquiredMs_ = 0;
  unsigned long refreshedMs_ = 0;
  unsigned long ttlMs_ = 0;
};
