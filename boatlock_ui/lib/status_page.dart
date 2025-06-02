import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'dart:async';
import 'dart:math';

class StatusPage extends StatefulWidget {
  const StatusPage({super.key});

  @override
  State<StatusPage> createState() => _StatusPageState();
}

class _StatusPageState extends State<StatusPage> {
  final LatLng anchorPoint = LatLng(54.9062, 37.6065); // Ока, Тульская обл.
  late Timer _timer;
  double heading = 0.0;
  double distanceToAnchor = 0.0;
  LatLng boatPosition = LatLng(54.9050, 37.6045);

  @override
  void initState() {
    super.initState();
    _startEmulation();
  }

  void _startEmulation() {
    _timer = Timer.periodic(const Duration(seconds: 1), (_) {
      setState(() {
        // Псевдослучайное движение
        double dx = 0.00005 * (Random().nextDouble() - 0.5);
        double dy = 0.00005 * (Random().nextDouble() - 0.5);
        boatPosition = LatLng(boatPosition.latitude + dx, boatPosition.longitude + dy);

        final distance = Distance();
        distanceToAnchor = distance(boatPosition, anchorPoint);

        heading = atan2(
          anchorPoint.longitude - boatPosition.longitude,
          anchorPoint.latitude - boatPosition.latitude,
        ) * (180 / pi);
        heading = (heading + 360) % 360;
      });
    });
  }

  @override
  void dispose() {
    _timer.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Состояние (эмуляция)")),
      body: Column(
        children: [
          Expanded(
            child: FlutterMap(
              options: MapOptions(
                center: boatPosition,
                zoom: 16.0,
              ),
              children: [
                TileLayer(
                  urlTemplate:
                      "https://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
                  userAgentPackageName: 'com.example.app',
                ),
                MarkerLayer(
                  markers: [
                    Marker(
                      point: boatPosition,
                      width: 80,
                      height: 80,
                      child: const Icon(Icons.directions_boat, color: Colors.blue, size: 40),
                    ),
                    Marker(
                      point: anchorPoint,
                      width: 80,
                      height: 80,
                      child: const Icon(Icons.anchor, color: Colors.red, size: 40),
                    ),
                  ],
                ),
                PolylineLayer(
                  polylines: [
                    Polyline(
                      points: [boatPosition, anchorPoint],
                      strokeWidth: 3,
                      color: Colors.yellow,
                    ),
                  ],
                ),
              ],
            ),
          ),
          Padding(
            padding: const EdgeInsets.all(12.0),
            child: Column(
              children: [
                Text("Курс на якорь: ${heading.toStringAsFixed(1)}°", style: const TextStyle(fontSize: 16)),
                const SizedBox(height: 8),
                Text("Расстояние до якоря: ${distanceToAnchor.toStringAsFixed(1)} м", style: const TextStyle(fontSize: 16)),
              ],
            ),
          )
        ],
      ),
    );
  }
}
