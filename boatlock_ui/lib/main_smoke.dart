import 'package:flutter/material.dart';

import 'smoke/ble_smoke_page.dart';

void main() {
  runApp(
    const MaterialApp(
      home: BleSmokePage(),
      debugShowCheckedModeBanner: false,
    ),
  );
}
