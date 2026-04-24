class BleReconnectDecision {
  const BleReconnectDecision({
    required this.reason,
    this.scanNow = false,
    this.scheduleRetry = false,
    this.stopScan = false,
    this.clearLink = false,
    this.retryDelay = const Duration(seconds: 3),
  });

  final String reason;
  final bool scanNow;
  final bool scheduleRetry;
  final bool stopScan;
  final bool clearLink;
  final Duration retryDelay;

  bool get isIdle => !scanNow && !scheduleRetry && !stopScan && !clearLink;

  static const idle = BleReconnectDecision(reason: 'idle');
}

class BleReconnectPolicy {
  BleReconnectPolicy({this.retryDelay = const Duration(seconds: 3)});

  final Duration retryDelay;
  bool _wanted = false;
  bool _adapterReady = false;
  bool _disposed = false;

  bool get canAttempt => _wanted && _adapterReady && !_disposed;

  BleReconnectDecision start({required bool adapterReady}) {
    _wanted = true;
    _disposed = false;
    _adapterReady = adapterReady;
    if (!_adapterReady) {
      return const BleReconnectDecision(
        reason: 'adapter_not_ready',
        stopScan: true,
        clearLink: true,
      );
    }
    return const BleReconnectDecision(reason: 'start', scanNow: true);
  }

  BleReconnectDecision adapterChanged({required bool adapterReady}) {
    _adapterReady = adapterReady;
    if (_disposed || !_wanted) return BleReconnectDecision.idle;
    if (!_adapterReady) {
      return const BleReconnectDecision(
        reason: 'adapter_not_ready',
        stopScan: true,
        clearLink: true,
      );
    }
    return const BleReconnectDecision(reason: 'adapter_ready', scanNow: true);
  }

  BleReconnectDecision appResumed() {
    if (!canAttempt) return BleReconnectDecision.idle;
    return const BleReconnectDecision(reason: 'app_resumed', scanNow: true);
  }

  BleReconnectDecision disconnected() {
    if (_disposed || !_wanted) return BleReconnectDecision.idle;
    if (!_adapterReady) {
      return const BleReconnectDecision(
        reason: 'disconnected_adapter_not_ready',
        stopScan: true,
        clearLink: true,
      );
    }
    return BleReconnectDecision(
      reason: 'disconnected',
      scheduleRetry: true,
      clearLink: true,
      retryDelay: retryDelay,
    );
  }

  BleReconnectDecision scanMiss() {
    if (!canAttempt) return BleReconnectDecision.idle;
    return BleReconnectDecision(
      reason: 'scan_miss',
      scheduleRetry: true,
      retryDelay: retryDelay,
    );
  }

  BleReconnectDecision connectFailed() {
    if (!canAttempt) return BleReconnectDecision.idle;
    return BleReconnectDecision(
      reason: 'connect_failed',
      scheduleRetry: true,
      clearLink: true,
      retryDelay: retryDelay,
    );
  }

  BleReconnectDecision dispose() {
    _wanted = false;
    _disposed = true;
    return const BleReconnectDecision(
      reason: 'disposed',
      stopScan: true,
      clearLink: true,
    );
  }
}
