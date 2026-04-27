#include "BleSecurity.h"
#include <unity.h>

void logMessage(const char*, ...) {}

static const char* kOwnerSecret = "00112233445566778899AABBCCDDEEFF";

static void formatHex64(uint64_t value, char* out, size_t outSize) {
  snprintf(out, outSize, "%016llX", static_cast<unsigned long long>(value));
}

void setUp() {
  mockSetMillis(0);
}
void tearDown() {}

void test_pairing_requires_open_window() {
  BleSecurity s;
  s.load();
  TEST_ASSERT_FALSE(s.isPaired());
  TEST_ASSERT_FALSE(s.setOwnerSecretHex(kOwnerSecret, 0));
  TEST_ASSERT_EQUAL_STRING("PAIRING_WINDOW_CLOSED", s.lastRejectString());
}

void test_pair_and_auth_flow() {
  BleSecurity s;
  s.clearPairing();
  s.openPairingWindow(1000);
  TEST_ASSERT_TRUE(s.setOwnerSecretHex(kOwnerSecret, 1100));
  TEST_ASSERT_TRUE(s.isPaired());

  const uint64_t nonce = s.startAuth(2000);
  TEST_ASSERT_TRUE(nonce != 0);
  const uint64_t proof = s.authProof();
  char proofHex[24];
  formatHex64(proof, proofHex, sizeof(proofHex));
  TEST_ASSERT_TRUE(s.proveAuthHex(proofHex));
  TEST_ASSERT_TRUE(s.sessionActive());
}

void test_secure_command_replay_and_signature_checks() {
  BleSecurity s;
  s.clearPairing();
  s.openPairingWindow(0);
  TEST_ASSERT_TRUE(s.setOwnerSecretHex(kOwnerSecret, 10));
  s.startAuth(200);
  const uint64_t proof = s.authProof();
  char proofHex[24];
  formatHex64(proof, proofHex, sizeof(proofHex));
  TEST_ASSERT_TRUE(s.proveAuthHex(proofHex));

  const uint32_t counter = 1;
  const char* payload = "ANCHOR_ON";
  const uint64_t sig = s.commandSig(counter, payload);
  char secureCmd[160];
  snprintf(secureCmd,
           sizeof(secureCmd),
           "SEC_CMD:%08lX:%016llX:%s",
           static_cast<unsigned long>(counter),
           static_cast<unsigned long long>(sig),
           payload);

  std::string out;
  TEST_ASSERT_TRUE(s.unwrapSecureCommand(secureCmd, &out, 300));
  TEST_ASSERT_EQUAL_STRING("ANCHOR_ON", out.c_str());
  TEST_ASSERT_FALSE(s.unwrapSecureCommand(secureCmd, &out, 350));
  TEST_ASSERT_EQUAL_STRING("REPLAY_DETECTED", s.lastRejectString());

  snprintf(secureCmd, sizeof(secureCmd), "SEC_CMD:%08X:%016llX:%s", 2, 0xDEADBEEFull, payload);
  TEST_ASSERT_FALSE(s.unwrapSecureCommand(secureCmd, &out, 400));
  TEST_ASSERT_EQUAL_STRING("BAD_SIGNATURE", s.lastRejectString());
}

void test_rate_limit_blocks_spam() {
  BleSecurity s;
  s.clearPairing();
  s.openPairingWindow(0);
  TEST_ASSERT_TRUE(s.setOwnerSecretHex(kOwnerSecret, 1));
  s.startAuth(100);
  const uint64_t proof = s.authProof();
  char proofHex[24];
  formatHex64(proof, proofHex, sizeof(proofHex));
  TEST_ASSERT_TRUE(s.proveAuthHex(proofHex));

  std::string out;
  for (uint32_t i = 1; i <= BleSecurity::kMaxCommandsPerWindow; ++i) {
    const char* payload = "HEARTBEAT";
    const uint64_t sig = s.commandSig(i, payload);
    char secureCmd[160];
    snprintf(secureCmd,
             sizeof(secureCmd),
             "SEC_CMD:%08lX:%016llX:%s",
             static_cast<unsigned long>(i),
             static_cast<unsigned long long>(sig),
             payload);
    TEST_ASSERT_TRUE(s.unwrapSecureCommand(secureCmd, &out, 200));
  }

  const uint32_t counter = BleSecurity::kMaxCommandsPerWindow + 1;
  const uint64_t sig = s.commandSig(counter, "HEARTBEAT");
  char tooMany[160];
  snprintf(tooMany,
           sizeof(tooMany),
           "SEC_CMD:%08lX:%016llX:%s",
           static_cast<unsigned long>(counter),
           static_cast<unsigned long long>(sig),
           "HEARTBEAT");
  TEST_ASSERT_FALSE(s.unwrapSecureCommand(tooMany, &out, 200));
  TEST_ASSERT_EQUAL_STRING("RATE_LIMIT", s.lastRejectString());
}

void test_invalid_owner_secret_is_rejected() {
  BleSecurity s;
  s.clearPairing();
  s.openPairingWindow(0);
  TEST_ASSERT_FALSE(s.setOwnerSecretHex("123456", 1));
  TEST_ASSERT_EQUAL_STRING("INVALID_OWNER_SECRET", s.lastRejectString());
  TEST_ASSERT_FALSE(s.isPaired());
}

void test_paired_mode_requires_secure_control_and_safety_commands() {
  BleSecurity s;
  s.clearPairing();
  s.openPairingWindow(0);
  TEST_ASSERT_TRUE(s.setOwnerSecretHex(kOwnerSecret, 1));

  TEST_ASSERT_FALSE(s.commandNeedsAuth("STREAM_START"));
  TEST_ASSERT_FALSE(s.commandNeedsAuth("STREAM_STOP"));
  TEST_ASSERT_FALSE(s.commandNeedsAuth("SNAPSHOT"));
  TEST_ASSERT_FALSE(s.commandNeedsAuth("AUTH_HELLO"));
  TEST_ASSERT_FALSE(s.commandNeedsAuth("AUTH_PROVE:abcd"));
  TEST_ASSERT_FALSE(s.commandNeedsAuth("PAIR_SET:00112233445566778899AABBCCDDEEFF"));
  TEST_ASSERT_FALSE(s.commandNeedsAuth("SEC_CMD:00000001:0000000000000000:HEARTBEAT"));

  TEST_ASSERT_TRUE(s.commandNeedsAuth("AUTH_DEBUG"));
  TEST_ASSERT_TRUE(s.commandNeedsAuth("AUTH_PROVE"));
  TEST_ASSERT_TRUE(s.commandNeedsAuth("HEARTBEAT"));
  TEST_ASSERT_TRUE(s.commandNeedsAuth("STOP"));
  TEST_ASSERT_TRUE(s.commandNeedsAuth("ANCHOR_OFF"));
  TEST_ASSERT_TRUE(s.commandNeedsAuth("ANCHOR_ON"));
  TEST_ASSERT_TRUE(s.commandNeedsAuth("MANUAL_TARGET:0,0,500"));
  TEST_ASSERT_TRUE(s.commandNeedsAuth("SIM_RUN:S0,1"));
  TEST_ASSERT_TRUE(s.commandNeedsAuth("SIM_ABORT"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pairing_requires_open_window);
  RUN_TEST(test_pair_and_auth_flow);
  RUN_TEST(test_secure_command_replay_and_signature_checks);
  RUN_TEST(test_rate_limit_blocks_spam);
  RUN_TEST(test_invalid_owner_secret_is_rejected);
  RUN_TEST(test_paired_mode_requires_secure_control_and_safety_commands);
  return UNITY_END();
}
