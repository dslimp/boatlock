enum BleSmokeMode {
  basic,
  reconnect,
  manual,
  status,
  sim,
  anchor,
  compass,
  gps,
  ota,
}

const kBoatLockSmokeModeDefine = 'BOATLOCK_SMOKE_MODE';
const kBoatLockDefaultSmokeModeName = 'basic';

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
