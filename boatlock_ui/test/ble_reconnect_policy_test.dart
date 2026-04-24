import 'package:boatlock_ui/ble/ble_reconnect_policy.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('start scans immediately when adapter is ready', () {
    final policy = BleReconnectPolicy();

    final decision = policy.start(adapterReady: true);

    expect(decision.scanNow, isTrue);
    expect(decision.scheduleRetry, isFalse);
    expect(policy.canAttempt, isTrue);
  });

  test('start waits quietly when adapter is off', () {
    final policy = BleReconnectPolicy();

    final decision = policy.start(adapterReady: false);

    expect(decision.scanNow, isFalse);
    expect(decision.stopScan, isTrue);
    expect(decision.clearLink, isTrue);
    expect(policy.canAttempt, isFalse);
  });

  test('adapter on after outage triggers a fresh scan', () {
    final policy = BleReconnectPolicy();
    policy.start(adapterReady: false);

    final decision = policy.adapterChanged(adapterReady: true);

    expect(decision.reason, 'adapter_ready');
    expect(decision.scanNow, isTrue);
    expect(policy.canAttempt, isTrue);
  });

  test('disconnect schedules retry and clears stale link', () {
    final policy = BleReconnectPolicy(retryDelay: const Duration(seconds: 9));
    policy.start(adapterReady: true);

    final decision = policy.disconnected();

    expect(decision.scheduleRetry, isTrue);
    expect(decision.clearLink, isTrue);
    expect(decision.retryDelay, const Duration(seconds: 9));
  });

  test('scan miss keeps retrying while app wants connection', () {
    final policy = BleReconnectPolicy();
    policy.start(adapterReady: true);

    final decision = policy.scanMiss();

    expect(decision.scheduleRetry, isTrue);
    expect(decision.clearLink, isFalse);
  });

  test('dispose stops all future reconnect attempts', () {
    final policy = BleReconnectPolicy();
    policy.start(adapterReady: true);

    final disposeDecision = policy.dispose();
    final resumeDecision = policy.appResumed();
    final onDecision = policy.adapterChanged(adapterReady: true);

    expect(disposeDecision.stopScan, isTrue);
    expect(disposeDecision.clearLink, isTrue);
    expect(resumeDecision.isIdle, isTrue);
    expect(onDecision.isIdle, isTrue);
    expect(policy.canAttempt, isFalse);
  });
}
