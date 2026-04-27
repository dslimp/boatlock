import 'boat_data.dart';

class AnchorPreflightItem {
  const AnchorPreflightItem({
    required this.label,
    required this.detail,
    required this.passed,
  });

  final String label;
  final String detail;
  final bool passed;
}

class AnchorPreflightResult {
  const AnchorPreflightResult({required this.items});

  final List<AnchorPreflightItem> items;

  bool get canEnable => items.every((item) => item.passed);

  String get blockedSummary {
    final failed = items
        .where((item) => !item.passed)
        .map((item) => item.label)
        .toList();
    return failed.join(', ');
  }
}

AnchorPreflightResult buildAnchorPreflight(BoatData? data) {
  final reasons = _reasonSet(data?.statusReasons ?? '');
  final linkReady = data != null;
  final authReady = data != null && (!data.secPaired || data.secAuth);
  final anchorReady =
      data != null && data.anchorLat != 0.0 && data.anchorLon != 0.0;
  final gnssReady =
      data != null &&
      data.lat != 0.0 &&
      data.lon != 0.0 &&
      data.gnssQ >= 2 &&
      !_hasAny(reasons, const {
        'NO_GPS',
        'GPS_WEAK',
        'GPS_NO_FIX',
        'GPS_DATA_STALE',
        'GPS_HDOP_MISSING',
        'GPS_HDOP_TOO_HIGH',
        'GPS_SATS_TOO_LOW',
        'GPS_POSITION_JUMP',
      });
  final headingReady =
      data != null &&
      data.compassQ > 0 &&
      !_hasAny(reasons, const {'NO_COMPASS', 'NO_HEADING'});
  final safetyReady =
      data != null &&
      data.status != 'ALERT' &&
      !_hasAny(reasons, const {
        'DRIFT_FAIL',
        'CONTAINMENT_BREACH',
        'COMM_TIMEOUT',
        'CONTROL_LOOP_TIMEOUT',
        'SENSOR_TIMEOUT',
        'INTERNAL_ERROR_NAN',
        'COMMAND_OUT_OF_RANGE',
        'STOP_CMD',
      });
  final motorReady =
      data != null &&
      data.stepSpr > 0 &&
      data.stepGear > 0.0 &&
      data.stepMaxSpd > 0.0 &&
      data.stepAccel > 0.0;

  return AnchorPreflightResult(
    items: [
      AnchorPreflightItem(
        label: 'BLE',
        detail: linkReady ? 'связь есть' : 'нет данных от устройства',
        passed: linkReady,
      ),
      AnchorPreflightItem(
        label: 'Auth',
        detail: authReady
            ? (data.secPaired ? 'owner session' : 'pairing off')
            : 'нужна авторизация',
        passed: authReady,
      ),
      AnchorPreflightItem(
        label: 'Anchor',
        detail: anchorReady ? 'точка сохранена' : 'точка не сохранена',
        passed: anchorReady,
      ),
      AnchorPreflightItem(
        label: 'GNSS',
        detail: gnssReady ? 'gate OK' : 'нет качественного hardware fix',
        passed: gnssReady,
      ),
      AnchorPreflightItem(
        label: 'Heading',
        detail: headingReady ? 'BNO08x свежий' : 'нет свежего курса',
        passed: headingReady,
      ),
      AnchorPreflightItem(
        label: 'Safety',
        detail: safetyReady ? 'нет latch/failsafe' : 'есть блокирующий статус',
        passed: safetyReady,
      ),
      AnchorPreflightItem(
        label: 'Motor',
        detail: motorReady ? 'stepper params OK' : 'нет motor/stepper config',
        passed: motorReady,
      ),
      AnchorPreflightItem(
        label: 'STOP',
        detail: linkReady ? 'доступен по BLE' : 'недоступен без связи',
        passed: linkReady,
      ),
    ],
  );
}

Set<String> _reasonSet(String raw) {
  return raw
      .split(',')
      .map((reason) => reason.trim())
      .where((reason) => reason.isNotEmpty)
      .toSet();
}

bool _hasAny(Set<String> reasons, Set<String> blocked) {
  return reasons.any(blocked.contains);
}
