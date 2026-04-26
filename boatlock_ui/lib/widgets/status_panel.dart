import 'package:flutter/material.dart';
import '../models/boat_data.dart';

class StatusPanel extends StatelessWidget {
  final BoatData? data;

  const StatusPanel({super.key, this.data});

  @override
  Widget build(BuildContext context) {
    if (data == null) return SizedBox.shrink();
    final statusReasonText = _statusReasonText(data!);
    return Container(
      margin: EdgeInsets.all(12),
      padding: EdgeInsets.symmetric(vertical: 10, horizontal: 16),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.93),
        borderRadius: BorderRadius.circular(16),
        boxShadow: [BoxShadow(blurRadius: 8, color: Colors.black12)],
      ),
      child: Wrap(
        alignment: WrapAlignment.spaceEvenly,
        runSpacing: 8,
        spacing: 14,
        children: [
          _iconText(Icons.info_outline, data!.status),
          if (statusReasonText.isNotEmpty)
            _iconText(Icons.warning_amber_rounded, statusReasonText),
          _iconText(
            Icons.anchor,
            (data!.anchorLat != 0 || data!.anchorLon != 0)
                ? "${data!.anchorLat.toStringAsFixed(3)}, ${data!.anchorLon.toStringAsFixed(3)}"
                : "нет",
          ),
          _iconText(Icons.directions_boat, data!.mode),
          _iconText(Icons.network_cell, "${data!.rssi} дБ"),
          _iconText(
            Icons.explore,
            "Q ${data!.compassQ}/3 M${data!.magQ} G${data!.gyroQ}",
          ),
          _iconText(Icons.speed, "rv ${data!.rvAcc.toStringAsFixed(1)}°"),
          _iconText(
            Icons.waves,
            "B ${data!.magNorm.toStringAsFixed(1)}uT w ${data!.gyroNorm.toStringAsFixed(1)}",
          ),
          _iconText(
            Icons.screen_rotation,
            "P/R ${data!.pitch.toStringAsFixed(1)}/${data!.roll.toStringAsFixed(1)}",
          ),
        ],
      ),
    );
  }

  String _statusReasonText(BoatData data) {
    final reasons = data.statusReasons.trim();
    if (reasons.isNotEmpty) {
      return reasons
          .split(',')
          .map((reason) => _statusReasonLabel(reason.trim()))
          .where((reason) => reason.isNotEmpty)
          .join(', ');
    }
    if (data.status == 'OK') {
      return '';
    }
    if (data.gnssQ == 0) {
      return 'GPS не готов';
    }
    return 'Причина не передана';
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

  Widget _iconText(IconData icon, String text) => ConstrainedBox(
    constraints: const BoxConstraints(maxWidth: 220),
    child: Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, size: 20, color: Colors.blueGrey),
        SizedBox(width: 4),
        Flexible(
          child: Text(
            text,
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
            style: TextStyle(fontWeight: FontWeight.w600),
          ),
        ),
      ],
    ),
  );
}
