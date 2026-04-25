import 'package:boatlock_ui/smoke/ble_smoke_mode.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('boatLockSmokeModeFromString maps supported smoke modes', () {
    expect(boatLockSmokeModeFromString('basic'), BleSmokeMode.basic);
    expect(boatLockSmokeModeFromString('reconnect'), BleSmokeMode.reconnect);
    expect(boatLockSmokeModeFromString('manual'), BleSmokeMode.manual);
    expect(boatLockSmokeModeFromString('status'), BleSmokeMode.status);
    expect(boatLockSmokeModeFromString('sim'), BleSmokeMode.sim);
    expect(boatLockSmokeModeFromString('anchor'), BleSmokeMode.anchor);
    expect(boatLockSmokeModeFromString('compass'), BleSmokeMode.compass);
    expect(boatLockSmokeModeFromString('gps'), BleSmokeMode.gps);
  });

  test('boatLockSmokeModeFromString defaults unknown values to basic', () {
    expect(boatLockSmokeModeFromString(''), BleSmokeMode.basic);
    expect(boatLockSmokeModeFromString('unknown'), BleSmokeMode.basic);
    expect(boatLockSmokeModeFromString(' manual '), BleSmokeMode.basic);
  });

  test('smoke mode define name is stable', () {
    expect(kBoatLockSmokeModeDefine, 'BOATLOCK_SMOKE_MODE');
    expect(kBoatLockDefaultSmokeModeName, 'basic');
  });
}
