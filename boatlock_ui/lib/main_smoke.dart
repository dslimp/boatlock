import 'package:flutter/material.dart';

import 'smoke/ble_smoke_page.dart';

void main() {
  const smokeMode = String.fromEnvironment(
    'BOATLOCK_SMOKE_MODE',
    defaultValue: 'basic',
  );
  runApp(
    MaterialApp(
      home: BleSmokePage(mode: _smokeModeFromString(smokeMode)),
      debugShowCheckedModeBanner: false,
    ),
  );
}

BleSmokeMode _smokeModeFromString(String value) {
  switch (value) {
    case 'reconnect':
      return BleSmokeMode.reconnect;
    case 'manual':
      return BleSmokeMode.manual;
    case 'basic':
    default:
      return BleSmokeMode.basic;
  }
}
