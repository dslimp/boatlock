import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'ble_ids.dart';

const kBoatLockScanTimeout = Duration(seconds: 6);
const kBoatLockScanCompletionWait = Duration(seconds: 7);

List<Guid> boatLockScanServiceFilters() {
  return <Guid>[Guid(boatLockServiceUuid)];
}
