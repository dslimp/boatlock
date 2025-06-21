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
        color: Colors.white.withOpacity(0.93),
        borderRadius: BorderRadius.circular(16),
        boxShadow: [BoxShadow(blurRadius: 8, color: Colors.black12)],
      ),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceEvenly,
        children: [
          _iconText(Icons.anchor, "${data!.distance.toStringAsFixed(1)} м"),
          _iconText(Icons.navigation, "${data!.heading.toStringAsFixed(0)}°"),
          _iconText(Icons.battery_full, "${data!.battery}%"),
          _iconText(Icons.info_outline, data!.status),
        ],
      ),
    );
  }

  Widget _iconText(IconData icon, String text) => Row(
    children: [
      Icon(icon, size: 20, color: Colors.blueGrey),
      SizedBox(width: 4),
      Text(text, style: TextStyle(fontWeight: FontWeight.w600)),
    ],
  );
}
