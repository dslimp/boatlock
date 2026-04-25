import 'package:flutter/material.dart';

import 'smoke/ble_smoke_mode.dart';
import 'smoke/ble_smoke_page.dart';

void main() {
  const smokeMode = String.fromEnvironment(
    kBoatLockSmokeModeDefine,
    defaultValue: kBoatLockDefaultSmokeModeName,
  );
  runApp(
    MaterialApp(
      home: BleSmokePage(mode: boatLockSmokeModeFromString(smokeMode)),
      debugShowCheckedModeBanner: false,
    ),
  );
}
