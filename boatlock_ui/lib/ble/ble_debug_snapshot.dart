class BleDebugSnapshot {
  const BleDebugSnapshot({
    required this.adapterState,
    required this.connectionState,
    required this.deviceId,
    required this.deviceName,
    required this.mtu,
    required this.rssi,
    required this.isScanning,
    required this.hasDataChar,
    required this.hasCommandChar,
    required this.hasLogChar,
    required this.hasOtaChar,
    required this.dataEvents,
    required this.deviceLogEvents,
    required this.appLogEvents,
    required this.connectedAt,
    required this.lastDataAt,
    required this.lastDeviceLogAt,
    required this.lastAppLogAt,
    required this.lastError,
    required this.appLogLines,
    required this.deviceLogLines,
  });

  factory BleDebugSnapshot.initial() => const BleDebugSnapshot(
    adapterState: 'unknown',
    connectionState: 'disconnected',
    deviceId: '',
    deviceName: '',
    mtu: 0,
    rssi: 0,
    isScanning: false,
    hasDataChar: false,
    hasCommandChar: false,
    hasLogChar: false,
    hasOtaChar: false,
    dataEvents: 0,
    deviceLogEvents: 0,
    appLogEvents: 0,
    connectedAt: null,
    lastDataAt: null,
    lastDeviceLogAt: null,
    lastAppLogAt: null,
    lastError: '',
    appLogLines: <String>[],
    deviceLogLines: <String>[],
  );

  final String adapterState;
  final String connectionState;
  final String deviceId;
  final String deviceName;
  final int mtu;
  final int rssi;
  final bool isScanning;
  final bool hasDataChar;
  final bool hasCommandChar;
  final bool hasLogChar;
  final bool hasOtaChar;
  final int dataEvents;
  final int deviceLogEvents;
  final int appLogEvents;
  final DateTime? connectedAt;
  final DateTime? lastDataAt;
  final DateTime? lastDeviceLogAt;
  final DateTime? lastAppLogAt;
  final String lastError;
  final List<String> appLogLines;
  final List<String> deviceLogLines;

  bool get connected => connectionState == 'connected';
  bool get coreReady => hasDataChar && hasCommandChar && hasLogChar;
  bool get otaReady => hasOtaChar;

  BleDebugSnapshot copyWith({
    String? adapterState,
    String? connectionState,
    String? deviceId,
    String? deviceName,
    int? mtu,
    int? rssi,
    bool? isScanning,
    bool? hasDataChar,
    bool? hasCommandChar,
    bool? hasLogChar,
    bool? hasOtaChar,
    int? dataEvents,
    int? deviceLogEvents,
    int? appLogEvents,
    DateTime? connectedAt,
    bool clearConnectedAt = false,
    DateTime? lastDataAt,
    bool clearLastDataAt = false,
    DateTime? lastDeviceLogAt,
    bool clearLastDeviceLogAt = false,
    DateTime? lastAppLogAt,
    bool clearLastAppLogAt = false,
    String? lastError,
    List<String>? appLogLines,
    List<String>? deviceLogLines,
  }) {
    return BleDebugSnapshot(
      adapterState: adapterState ?? this.adapterState,
      connectionState: connectionState ?? this.connectionState,
      deviceId: deviceId ?? this.deviceId,
      deviceName: deviceName ?? this.deviceName,
      mtu: mtu ?? this.mtu,
      rssi: rssi ?? this.rssi,
      isScanning: isScanning ?? this.isScanning,
      hasDataChar: hasDataChar ?? this.hasDataChar,
      hasCommandChar: hasCommandChar ?? this.hasCommandChar,
      hasLogChar: hasLogChar ?? this.hasLogChar,
      hasOtaChar: hasOtaChar ?? this.hasOtaChar,
      dataEvents: dataEvents ?? this.dataEvents,
      deviceLogEvents: deviceLogEvents ?? this.deviceLogEvents,
      appLogEvents: appLogEvents ?? this.appLogEvents,
      connectedAt: clearConnectedAt ? null : (connectedAt ?? this.connectedAt),
      lastDataAt: clearLastDataAt ? null : (lastDataAt ?? this.lastDataAt),
      lastDeviceLogAt: clearLastDeviceLogAt
          ? null
          : (lastDeviceLogAt ?? this.lastDeviceLogAt),
      lastAppLogAt: clearLastAppLogAt
          ? null
          : (lastAppLogAt ?? this.lastAppLogAt),
      lastError: lastError ?? this.lastError,
      appLogLines: appLogLines ?? this.appLogLines,
      deviceLogLines: deviceLogLines ?? this.deviceLogLines,
    );
  }
}
