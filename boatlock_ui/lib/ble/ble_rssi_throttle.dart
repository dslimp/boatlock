class BleRssiThrottle {
  BleRssiThrottle({this.interval = const Duration(seconds: 5)});

  final Duration interval;
  DateTime? _lastReadAt;

  bool shouldRead(DateTime now) {
    final last = _lastReadAt;
    if (last != null && now.difference(last) < interval) {
      return false;
    }
    _lastReadAt = now;
    return true;
  }

  void reset() {
    _lastReadAt = null;
  }
}
