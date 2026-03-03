import 'package:flutter/material.dart';
import '../models/boat_data.dart';

class StatusPanel extends StatelessWidget {
  final BoatData? data;

  const StatusPanel({super.key, this.data});

  @override
  Widget build(BuildContext context) {
    if (data == null) return SizedBox.shrink();
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
          _iconText(
            Icons.anchor,
            (data!.anchorLat != 0 || data!.anchorLon != 0)
                ? "${data!.anchorLat.toStringAsFixed(3)}, ${data!.anchorLon.toStringAsFixed(3)}"
                : "нет",
          ),
          _iconText(Icons.directions_boat, data!.mode),
          _iconText(Icons.network_cell, "${data!.rssi} дБ"),
          _iconText(Icons.route, "${data!.speedKmh.toStringAsFixed(1)} км/ч"),
          _iconText(Icons.settings_input_component, "PWM ${data!.motorPwm}%"),
          _iconText(
            Icons.explore,
            "Q ${data!.compassQ}/3 M${data!.magQ} G${data!.gyroQ}",
          ),
          _iconText(
            Icons.speed,
            "rv ${data!.rvAcc.toStringAsFixed(1)}°",
          ),
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

  Widget _iconText(IconData icon, String text) => Row(
    mainAxisSize: MainAxisSize.min,
    children: [
      Icon(icon, size: 20, color: Colors.blueGrey),
      SizedBox(width: 4),
      Text(text, style: TextStyle(fontWeight: FontWeight.w600)),
    ],
  );
}
