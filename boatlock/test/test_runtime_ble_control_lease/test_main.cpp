#include "RuntimeBleControlLease.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

RuntimeBleControlClient appClient(uint16_t handle, uint32_t generation) {
  RuntimeBleControlClient client;
  client.connHandle = handle;
  client.generation = generation;
  client.role = RuntimeBleControlRole::APP;
  client.valid = true;
  return client;
}

RuntimeBleControlClient remoteClient(uint16_t handle, uint32_t generation) {
  RuntimeBleControlClient client = appClient(handle, generation);
  client.role = RuntimeBleControlRole::REMOTE;
  return client;
}

void assertDecision(RuntimeBleControlLeaseDecision expected,
                    const RuntimeBleControlLeaseResult& actual) {
  TEST_ASSERT_EQUAL(static_cast<int>(expected), static_cast<int>(actual.decision));
}

void assertKind(RuntimeBleControlLeaseCommandKind expected, const char* command) {
  TEST_ASSERT_EQUAL(static_cast<int>(expected),
                    static_cast<int>(runtimeBleControlLeaseCommandKind(command)));
}

void test_command_kinds_keep_read_only_out_of_owner_lease() {
  assertKind(RuntimeBleControlLeaseCommandKind::READ_ONLY, "STREAM_START");
  assertKind(RuntimeBleControlLeaseCommandKind::READ_ONLY, "STREAM_STOP");
  assertKind(RuntimeBleControlLeaseCommandKind::READ_ONLY, "SNAPSHOT");
}

void test_command_kinds_mark_control_stop_and_rejected_commands() {
  assertKind(RuntimeBleControlLeaseCommandKind::CONTROL, "ANCHOR_ON");
  assertKind(RuntimeBleControlLeaseCommandKind::CONTROL, "HEARTBEAT");
  assertKind(RuntimeBleControlLeaseCommandKind::CONTROL, "MANUAL_TARGET:0,25,500");
  assertKind(RuntimeBleControlLeaseCommandKind::CONTROL, "OTA_BEGIN:bad");
  assertKind(RuntimeBleControlLeaseCommandKind::CONTROL, "SIM_STATUS");
  assertKind(RuntimeBleControlLeaseCommandKind::STOP_PREEMPT, "STOP");
  assertKind(RuntimeBleControlLeaseCommandKind::REJECT, "MANUAL_DIR:1");
}

void test_read_only_commands_do_not_acquire_owner() {
  RuntimeBleControlLease lease;
  RuntimeBleControlClient app = appClient(4, 10);

  RuntimeBleControlLeaseResult stream =
      lease.authorize(app, "STREAM_START", 1000);
  assertDecision(RuntimeBleControlLeaseDecision::ALLOW_READ_ONLY, stream);
  TEST_ASSERT_TRUE(stream.allowed());
  TEST_ASSERT_FALSE(stream.ownerCommand());
  TEST_ASSERT_FALSE(lease.ownerActive(1000));
}

void test_first_app_control_command_acquires_and_same_owner_refreshes() {
  RuntimeBleControlLease lease;
  RuntimeBleControlClient app = appClient(1, 7);

  RuntimeBleControlLeaseResult first =
      lease.authorize(app, "ANCHOR_ON", 1000, 3000);
  assertDecision(RuntimeBleControlLeaseDecision::ACQUIRE, first);
  TEST_ASSERT_TRUE(first.allowed());
  TEST_ASSERT_TRUE(first.ownerCommand());
  TEST_ASSERT_TRUE(lease.ownerIs(app, 1000));
  TEST_ASSERT_EQUAL_UINT32(1000, lease.acquiredMs());
  TEST_ASSERT_EQUAL_UINT32(1000, lease.refreshedMs());

  RuntimeBleControlLeaseResult refresh =
      lease.authorize(app, "HEARTBEAT", 2500, 3000);
  assertDecision(RuntimeBleControlLeaseDecision::REFRESH, refresh);
  TEST_ASSERT_TRUE(lease.ownerIs(app, 5499));
  TEST_ASSERT_FALSE(lease.ownerActive(5500));
  TEST_ASSERT_EQUAL_UINT32(1000, lease.acquiredMs());
  TEST_ASSERT_EQUAL_UINT32(2500, lease.refreshedMs());
}

void test_non_owner_control_is_busy_until_lease_expires() {
  RuntimeBleControlLease lease;
  RuntimeBleControlClient owner = appClient(1, 1);
  RuntimeBleControlClient second = appClient(2, 1);

  assertDecision(RuntimeBleControlLeaseDecision::ACQUIRE,
                 lease.authorize(owner, "MANUAL_TARGET:0,10,500", 1000, 3000));

  RuntimeBleControlLeaseResult busy =
      lease.authorize(second, "ANCHOR_OFF", 3999, 3000);
  assertDecision(RuntimeBleControlLeaseDecision::REJECT_BUSY, busy);
  TEST_ASSERT_FALSE(busy.allowed());
  TEST_ASSERT_TRUE(lease.ownerIs(owner, 3999));

  RuntimeBleControlLeaseResult takeover =
      lease.authorize(second, "ANCHOR_OFF", 4000, 3000);
  assertDecision(RuntimeBleControlLeaseDecision::ACQUIRE, takeover);
  TEST_ASSERT_TRUE(lease.ownerIs(second, 4000));
}

void test_reconnect_generation_does_not_match_owner() {
  RuntimeBleControlLease lease;
  RuntimeBleControlClient owner = appClient(1, 1);
  RuntimeBleControlClient reconnect = appClient(1, 2);

  assertDecision(RuntimeBleControlLeaseDecision::ACQUIRE,
                 lease.authorize(owner, "SET_HOLD_HEADING:1", 1000, 3000));
  assertDecision(RuntimeBleControlLeaseDecision::REJECT_BUSY,
                 lease.authorize(reconnect, "HEARTBEAT", 1200, 3000));
}

void test_owner_disconnect_clears_lease_but_peer_disconnect_does_not() {
  RuntimeBleControlLease lease;
  RuntimeBleControlClient owner = appClient(1, 1);
  RuntimeBleControlClient peer = appClient(2, 1);

  assertDecision(RuntimeBleControlLeaseDecision::ACQUIRE,
                 lease.authorize(owner, "ANCHOR_OFF", 1000, 3000));
  TEST_ASSERT_FALSE(lease.releaseOnDisconnect(peer));
  TEST_ASSERT_TRUE(lease.ownerIs(owner, 1200));

  TEST_ASSERT_TRUE(lease.releaseOnDisconnect(owner));
  TEST_ASSERT_FALSE(lease.ownerActive(1200));
  assertDecision(RuntimeBleControlLeaseDecision::ACQUIRE,
                 lease.authorize(peer, "ANCHOR_OFF", 1300, 3000));
}

void test_stop_from_non_owner_preempts_and_clears_lease() {
  RuntimeBleControlLease lease;
  RuntimeBleControlClient owner = appClient(1, 1);
  RuntimeBleControlClient peer = appClient(2, 1);

  assertDecision(RuntimeBleControlLeaseDecision::ACQUIRE,
                 lease.authorize(owner, "ANCHOR_ON", 1000, 3000));
  RuntimeBleControlLeaseResult stop = lease.authorize(peer, "STOP", 1100, 3000);
  assertDecision(RuntimeBleControlLeaseDecision::STOP_PREEMPT, stop);
  TEST_ASSERT_TRUE(stop.allowed());
  TEST_ASSERT_FALSE(lease.ownerActive(1100));
}

void test_remote_role_cannot_acquire_control_in_this_slice() {
  RuntimeBleControlLease lease;
  RuntimeBleControlClient remote = remoteClient(3, 1);

  RuntimeBleControlLeaseResult rejected =
      lease.authorize(remote, "MANUAL_TARGET:0,20,500", 1000, 3000);
  assertDecision(RuntimeBleControlLeaseDecision::REJECT_ROLE, rejected);
  TEST_ASSERT_FALSE(rejected.allowed());
  TEST_ASSERT_FALSE(lease.ownerActive(1000));

  RuntimeBleControlLeaseResult stop = lease.authorize(remote, "STOP", 1100, 3000);
  assertDecision(RuntimeBleControlLeaseDecision::STOP_PREEMPT, stop);
  TEST_ASSERT_TRUE(stop.allowed());
}

void test_invalid_client_or_unknown_command_cannot_acquire() {
  RuntimeBleControlLease lease;
  RuntimeBleControlClient invalid;
  RuntimeBleControlClient app = appClient(1, 1);

  assertDecision(RuntimeBleControlLeaseDecision::REJECT_INVALID,
                 lease.authorize(invalid, "ANCHOR_ON", 1000, 3000));
  assertDecision(RuntimeBleControlLeaseDecision::REJECT_INVALID,
                 lease.authorize(app, "SET_ROUTE:1,2", 1000, 3000));
  assertDecision(RuntimeBleControlLeaseDecision::REJECT_INVALID,
                 lease.authorize(app, "ANCHOR_ON", 1000, 0));
  TEST_ASSERT_FALSE(lease.ownerActive(1000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_command_kinds_keep_read_only_out_of_owner_lease);
  RUN_TEST(test_command_kinds_mark_control_stop_and_rejected_commands);
  RUN_TEST(test_read_only_commands_do_not_acquire_owner);
  RUN_TEST(test_first_app_control_command_acquires_and_same_owner_refreshes);
  RUN_TEST(test_non_owner_control_is_busy_until_lease_expires);
  RUN_TEST(test_reconnect_generation_does_not_match_owner);
  RUN_TEST(test_owner_disconnect_clears_lease_but_peer_disconnect_does_not);
  RUN_TEST(test_stop_from_non_owner_preempts_and_clears_lease);
  RUN_TEST(test_remote_role_cannot_acquire_control_in_this_slice);
  RUN_TEST(test_invalid_client_or_unknown_command_cannot_acquire);
  return UNITY_END();
}
