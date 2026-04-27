enum BleSmokeMode {
  basic,
  reconnect,
  manual,
  status,
  sim,
  simSuite,
  anchor,
  compass,
  gps,
  ota,
}

const kBoatLockDefaultSmokeModeName = 'basic';

extension BleSmokeModeWireName on BleSmokeMode {
  String get wireName {
    return switch (this) {
      BleSmokeMode.simSuite => 'sim_suite',
      _ => name,
    };
  }
}

BleSmokeMode boatLockSmokeModeFromString(String value) {
  switch (value) {
    case 'reconnect':
      return BleSmokeMode.reconnect;
    case 'manual':
      return BleSmokeMode.manual;
    case 'status':
      return BleSmokeMode.status;
    case 'sim':
      return BleSmokeMode.sim;
    case 'sim_suite':
      return BleSmokeMode.simSuite;
    case 'anchor':
      return BleSmokeMode.anchor;
    case 'compass':
      return BleSmokeMode.compass;
    case 'gps':
      return BleSmokeMode.gps;
    case 'ota':
      return BleSmokeMode.ota;
    case 'basic':
    default:
      return BleSmokeMode.basic;
  }
}
