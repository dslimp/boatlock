import 'package:flutter/material.dart';
import '../ble/ble_debug_snapshot.dart';
import '../models/boat_data.dart';

class StatusPanel extends StatelessWidget {
  final BoatData? data;
  final BleDebugSnapshot? diagnostics;

  const StatusPanel({super.key, this.data, this.diagnostics});

  @override
  Widget build(BuildContext context) {
    final items = _readinessItems(data, diagnostics);
    final blocked = items
        .where((item) => item.level == _ReadinessLevel.blocked)
        .length;
    final warned = items
        .where((item) => item.level == _ReadinessLevel.warn)
        .length;
    final summary = data == null
        ? 'WAIT'
        : blocked > 0
        ? 'BLOCKED $blocked'
        : warned > 0
        ? 'WARN $warned'
        : 'OK';

    return Container(
      margin: const EdgeInsets.all(12),
      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 10),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.93),
        borderRadius: BorderRadius.circular(8),
        boxShadow: const [BoxShadow(blurRadius: 8, color: Colors.black12)],
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(
                _summaryIcon(summary),
                size: 18,
                color: _summaryColor(summary),
              ),
              const SizedBox(width: 6),
              Text(
                'Готовность: $summary',
                style: const TextStyle(
                  fontWeight: FontWeight.w700,
                  fontSize: 13,
                ),
              ),
            ],
          ),
          const SizedBox(height: 6),
          Wrap(
            runSpacing: 6,
            spacing: 6,
            children: [for (final item in items) _readinessTile(item)],
          ),
        ],
      ),
    );
  }

  List<_ReadinessItem> _readinessItems(
    BoatData? data,
    BleDebugSnapshot? diagnostics,
  ) {
    final reasons = _reasonSet(data?.statusReasons ?? '');
    return [
      _bleItem(data, diagnostics),
      _dataItem(data, diagnostics),
      _gnssItem(data, reasons),
      _headingItem(data, reasons),
      _anchorItem(data),
      _safetyItem(data, reasons),
      _modeItem(data),
      _motorItem(data),
      _distanceItem(data),
    ];
  }

  _ReadinessItem _bleItem(BoatData? data, BleDebugSnapshot? diagnostics) {
    if (diagnostics == null) {
      final live = data != null;
      return _ReadinessItem(
        icon: Icons.bluetooth,
        label: 'BLE',
        value: live ? 'live' : 'нет',
        detail: live ? 'telemetry frame received' : 'нет данных от устройства',
        level: live ? _ReadinessLevel.ok : _ReadinessLevel.blocked,
      );
    }
    if (diagnostics.connected && diagnostics.coreReady) {
      return _ReadinessItem(
        icon: Icons.bluetooth_connected,
        label: 'BLE',
        value: 'link',
        detail:
            '${diagnostics.deviceName} ${diagnostics.deviceId}; data/cmd/log ready',
        level: _ReadinessLevel.ok,
      );
    }
    if (diagnostics.connected) {
      final missing = <String>[
        if (!diagnostics.hasDataChar) 'data',
        if (!diagnostics.hasCommandChar) 'cmd',
        if (!diagnostics.hasLogChar) 'log',
      ].join('/');
      return _ReadinessItem(
        icon: Icons.bluetooth_searching,
        label: 'BLE',
        value: 'GATT',
        detail: 'connected, missing $missing characteristic',
        level: _ReadinessLevel.warn,
      );
    }
    return _ReadinessItem(
      icon: Icons.bluetooth_disabled,
      label: 'BLE',
      value: diagnostics.isScanning ? 'scan' : 'нет',
      detail: diagnostics.isScanning
          ? 'scan active'
          : 'connection ${diagnostics.connectionState}',
      level: diagnostics.isScanning
          ? _ReadinessLevel.warn
          : _ReadinessLevel.blocked,
    );
  }

  _ReadinessItem _dataItem(BoatData? data, BleDebugSnapshot? diagnostics) {
    final live = data != null;
    return _ReadinessItem(
      icon: Icons.sensors,
      label: 'DATA',
      value: live ? 'live' : 'нет',
      detail: live
          ? 'live frame ok, events ${diagnostics?.dataEvents ?? '-'}'
          : 'нет live frame',
      level: live ? _ReadinessLevel.ok : _ReadinessLevel.blocked,
    );
  }

  _ReadinessItem _gnssItem(BoatData? data, Set<String> reasons) {
    if (data == null) {
      return const _ReadinessItem(
        icon: Icons.gps_off,
        label: 'GNSS',
        value: 'нет',
        detail: 'нет telemetry для GNSS gate',
        level: _ReadinessLevel.blocked,
      );
    }
    final hasPosition = data.lat != 0.0 && data.lon != 0.0;
    final badReason = _hasAny(reasons, const {
      'NO_GPS',
      'GPS_WEAK',
      'GPS_NO_FIX',
      'GPS_DATA_STALE',
      'GPS_HDOP_MISSING',
      'GPS_HDOP_TOO_HIGH',
      'GPS_SATS_TOO_LOW',
      'GPS_POSITION_JUMP',
    });
    final ready = hasPosition && data.gnssQ >= 2 && !badReason;
    final weak = hasPosition && data.gnssQ > 0 && !ready;
    return _ReadinessItem(
      icon: ready ? Icons.gps_fixed : Icons.gps_not_fixed,
      label: 'GNSS',
      value: 'Q${data.gnssQ}',
      detail: ready
          ? '${data.lat.toStringAsFixed(6)}, ${data.lon.toStringAsFixed(6)}'
          : _reasonDetail(reasons, fallback: 'нет качественного hardware fix'),
      level: ready
          ? _ReadinessLevel.ok
          : weak
          ? _ReadinessLevel.warn
          : _ReadinessLevel.blocked,
    );
  }

  _ReadinessItem _headingItem(BoatData? data, Set<String> reasons) {
    if (data == null) {
      return const _ReadinessItem(
        icon: Icons.explore_off,
        label: 'HDG',
        value: 'нет',
        detail: 'нет telemetry для heading freshness proxy',
        level: _ReadinessLevel.blocked,
      );
    }
    final ready =
        data.compassQ > 0 &&
        !_hasAny(reasons, const {'NO_COMPASS', 'NO_HEADING'});
    return _ReadinessItem(
      icon: ready ? Icons.explore : Icons.explore_off,
      label: 'HDG',
      value: ready ? 'Q${data.compassQ}' : 'нет',
      detail:
          'heading ${data.heading.toStringAsFixed(1)}°, raw ${data.headingRaw.toStringAsFixed(1)}°, M${data.magQ} G${data.gyroQ}, rv ${data.rvAcc.toStringAsFixed(1)}°',
      level: ready ? _ReadinessLevel.ok : _ReadinessLevel.blocked,
    );
  }

  _ReadinessItem _anchorItem(BoatData? data) {
    if (data == null) {
      return const _ReadinessItem(
        icon: Icons.anchor,
        label: 'ANCH',
        value: 'нет',
        detail: 'anchor unknown until live telemetry arrives',
        level: _ReadinessLevel.blocked,
      );
    }
    final saved = data.anchorLat != 0.0 && data.anchorLon != 0.0;
    return _ReadinessItem(
      icon: Icons.anchor,
      label: 'ANCH',
      value: saved ? 'есть' : 'нет',
      detail: saved
          ? '${data.anchorLat.toStringAsFixed(6)}, ${data.anchorLon.toStringAsFixed(6)}, head ${data.anchorHeading.toStringAsFixed(1)}°'
          : 'точка не сохранена',
      level: saved ? _ReadinessLevel.ok : _ReadinessLevel.blocked,
    );
  }

  _ReadinessItem _safetyItem(BoatData? data, Set<String> reasons) {
    if (data == null) {
      return const _ReadinessItem(
        icon: Icons.health_and_safety_outlined,
        label: 'SAFE',
        value: 'нет',
        detail: 'safety unknown until live telemetry arrives',
        level: _ReadinessLevel.blocked,
      );
    }
    final blocking = _hasAny(reasons, const {
      'DRIFT_FAIL',
      'CONTAINMENT_BREACH',
      'COMM_TIMEOUT',
      'CONTROL_LOOP_TIMEOUT',
      'SENSOR_TIMEOUT',
      'INTERNAL_ERROR_NAN',
      'COMMAND_OUT_OF_RANGE',
      'STOP_CMD',
    });
    final level = data.status == 'ALERT' || blocking
        ? _ReadinessLevel.blocked
        : data.status == 'OK'
        ? _ReadinessLevel.ok
        : _ReadinessLevel.warn;
    return _ReadinessItem(
      icon: level == _ReadinessLevel.ok
          ? Icons.health_and_safety
          : Icons.warning_amber_rounded,
      label: 'SAFE',
      value: data.status,
      detail: _reasonDetail(reasons, fallback: 'нет latch/failsafe'),
      level: level,
    );
  }

  _ReadinessItem _modeItem(BoatData? data) {
    if (data == null) {
      return const _ReadinessItem(
        icon: Icons.directions_boat_outlined,
        label: 'MODE',
        value: 'нет',
        detail: 'mode unknown until live telemetry arrives',
        level: _ReadinessLevel.blocked,
      );
    }
    final manual = data.mode == 'MANUAL';
    return _ReadinessItem(
      icon: Icons.directions_boat,
      label: 'MODE',
      value: data.mode,
      detail: manual
          ? 'manual lease active; ttl/source not in telemetry'
          : 'manual lease inactive',
      level: manual ? _ReadinessLevel.warn : _ReadinessLevel.ok,
    );
  }

  _ReadinessItem _motorItem(BoatData? data) {
    if (data == null) {
      return const _ReadinessItem(
        icon: Icons.settings_input_component_outlined,
        label: 'DRV',
        value: 'нет',
        detail: 'motor/stepper config unknown until live telemetry arrives',
        level: _ReadinessLevel.blocked,
      );
    }
    final stepperReady =
        data.stepSpr > 0 &&
        data.stepGear > 0.0 &&
        data.stepMaxSpd > 0.0 &&
        data.stepAccel > 0.0;
    return _ReadinessItem(
      icon: Icons.settings_input_component,
      label: 'DRV',
      value: stepperReady ? 'stepper OK' : 'cfg',
      detail:
          'motorSpr=${data.stepSpr}, gear=${data.stepGear.toStringAsFixed(1)}, max=${data.stepMaxSpd.toStringAsFixed(0)}, accel=${data.stepAccel.toStringAsFixed(0)}; motor driver health not in telemetry',
      level: stepperReady ? _ReadinessLevel.ok : _ReadinessLevel.blocked,
    );
  }

  _ReadinessItem _distanceItem(BoatData? data) {
    if (data == null) {
      return const _ReadinessItem(
        icon: Icons.social_distance,
        label: 'DST/BRG',
        value: 'нет',
        detail: 'distance/bearing unknown until live telemetry arrives',
        level: _ReadinessLevel.neutral,
      );
    }
    final saved = data.anchorLat != 0.0 && data.anchorLon != 0.0;
    final hasPosition = data.lat != 0.0 && data.lon != 0.0;
    final hasRange = saved && hasPosition;
    final distance = hasRange ? '${data.distance.toStringAsFixed(1)} м' : 'нет';
    final bearing = hasRange
        ? '${data.anchorBearing.toStringAsFixed(0)}°'
        : '-';
    return _ReadinessItem(
      icon: Icons.social_distance,
      label: 'DST/BRG',
      value: '$distance / $bearing',
      detail: hasRange
          ? 'bearing to anchor ${data.anchorBearing.toStringAsFixed(1)}°, anchorHead ${data.anchorHeading.toStringAsFixed(1)}°'
          : saved
          ? 'нет текущей позиции для расчета'
          : 'нет anchor point',
      level: _ReadinessLevel.neutral,
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

  String _reasonDetail(Set<String> reasons, {required String fallback}) {
    if (reasons.isEmpty) {
      return fallback;
    }
    return reasons.map(_statusReasonLabel).join(', ');
  }

  String _statusReasonLabel(String reason) {
    return switch (reason) {
      'NO_GPS' => 'Нет GPS',
      'GPS_WEAK' => 'GPS слабый',
      'GPS_NO_FIX' => 'Нет GPS fix',
      'GPS_DATA_STALE' => 'GPS устарел',
      'GPS_HDOP_MISSING' => 'Нет HDOP GPS',
      'GPS_HDOP_TOO_HIGH' => 'HDOP GPS высокий',
      'GPS_SATS_TOO_LOW' => 'Мало спутников',
      'GPS_POSITION_JUMP' => 'Скачок GPS',
      'NO_COMPASS' => 'Нет компаса',
      'NO_HEADING' => 'Нет курса',
      'DRIFT_ALERT' => 'Дрейф',
      'DRIFT_FAIL' => 'Дрейф критичный',
      'CONTAINMENT_BREACH' => 'Выход за зону',
      'COMM_TIMEOUT' => 'Нет связи',
      'CONTROL_LOOP_TIMEOUT' => 'Контур завис',
      'SENSOR_TIMEOUT' => 'Сенсоры устарели',
      'INTERNAL_ERROR_NAN' => 'Ошибка расчета',
      'COMMAND_OUT_OF_RANGE' => 'Команда вне диапазона',
      'STOP_CMD' => 'STOP',
      'NO_ANCHOR_POINT' => 'Нет якорной точки',
      _ => reason,
    };
  }

  IconData _summaryIcon(String summary) {
    if (summary == 'OK') return Icons.check_circle;
    if (summary.startsWith('WARN')) return Icons.warning_amber_rounded;
    if (summary.startsWith('BLOCKED')) return Icons.error;
    return Icons.sync;
  }

  Color _summaryColor(String summary) {
    if (summary == 'OK') return Colors.green.shade700;
    if (summary.startsWith('WARN')) return Colors.orange.shade800;
    if (summary.startsWith('BLOCKED')) return Colors.red.shade700;
    return Colors.blueGrey.shade700;
  }

  Widget _readinessTile(_ReadinessItem item) {
    final colors = _ReadinessColors.forLevel(item.level);
    return Tooltip(
      message: item.detail,
      child: Container(
        constraints: const BoxConstraints(maxWidth: 180, minHeight: 30),
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 5),
        decoration: BoxDecoration(
          color: colors.background,
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: colors.border),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(item.icon, size: 16, color: colors.foreground),
            const SizedBox(width: 5),
            Flexible(
              child: Text(
                '${item.label}: ${item.value}',
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: TextStyle(
                  color: colors.foreground,
                  fontWeight: FontWeight.w700,
                  fontSize: 12,
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

enum _ReadinessLevel { ok, warn, blocked, neutral }

class _ReadinessItem {
  const _ReadinessItem({
    required this.icon,
    required this.label,
    required this.value,
    required this.detail,
    required this.level,
  });

  final IconData icon;
  final String label;
  final String value;
  final String detail;
  final _ReadinessLevel level;
}

class _ReadinessColors {
  const _ReadinessColors({
    required this.background,
    required this.border,
    required this.foreground,
  });

  final Color background;
  final Color border;
  final Color foreground;

  static _ReadinessColors forLevel(_ReadinessLevel level) {
    return switch (level) {
      _ReadinessLevel.ok => _ReadinessColors(
        background: Colors.green.shade50,
        border: Colors.green.shade200,
        foreground: Colors.green.shade800,
      ),
      _ReadinessLevel.warn => _ReadinessColors(
        background: Colors.amber.shade50,
        border: Colors.amber.shade300,
        foreground: Colors.orange.shade900,
      ),
      _ReadinessLevel.blocked => _ReadinessColors(
        background: Colors.red.shade50,
        border: Colors.red.shade200,
        foreground: Colors.red.shade800,
      ),
      _ReadinessLevel.neutral => _ReadinessColors(
        background: Colors.blueGrey.shade50,
        border: Colors.blueGrey.shade100,
        foreground: Colors.blueGrey.shade800,
      ),
    };
  }
}
