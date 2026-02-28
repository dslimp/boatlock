import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';

import '../ble/ble_boatlock.dart';
import '../services/log_service.dart';

class RoutePage extends StatefulWidget {
  final BleBoatLock ble;
  const RoutePage({super.key, required this.ble});

  @override
  State<RoutePage> createState() => _RoutePageState();
}

class _RoutePageState extends State<RoutePage> {
  final MapController _mapController = MapController();
  final List<LatLng> _points = [];
  int _lastLogIndex = 0;

  @override
  void initState() {
    super.initState();
    logService.addListener(_processLogs);
    widget.ble.exportLog();
  }

  void _processLogs() {
    while (_lastLogIndex < logService.logs.length) {
      final line = logService.logs[_lastLogIndex++];
      if (line.startsWith('ROUTE:')) {
        final parts = line.substring(6).split(',');
        if (parts.length >= 3) {
          final lat = double.tryParse(parts[1]);
          final lon = double.tryParse(parts[2]);
          if (lat != null && lon != null) {
            setState(() {
              _points.add(LatLng(lat, lon));
            });
          }
        }
      }
    }
  }

  @override
  void dispose() {
    logService.removeListener(_processLogs);
    super.dispose();
  }

  void _clear() {
    widget.ble.clearLog();
    setState(() {
      _points.clear();
    });
  }

  @override
  Widget build(BuildContext context) {
    final center = _points.isNotEmpty ? _points.last : const LatLng(0, 0);
    return Scaffold(
      appBar: AppBar(
        title: const Text('Записанный маршрут'),
        actions: [
          IconButton(onPressed: _clear, icon: const Icon(Icons.delete)),
        ],
      ),
      body: FlutterMap(
        mapController: _mapController,
        options: MapOptions(initialCenter: center, initialZoom: 15),
        children: [
          TileLayer(
            urlTemplate: 'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
          ),
          if (_points.length > 1)
            PolylineLayer(
              polylines: [
                Polyline(points: _points, color: Colors.red, strokeWidth: 4),
              ],
            ),
          if (_points.isNotEmpty)
            MarkerLayer(
              markers: [
                Marker(
                  point: _points.first,
                  width: 40,
                  height: 40,
                  child: const Icon(Icons.play_arrow, color: Colors.green),
                ),
                Marker(
                  point: _points.last,
                  width: 40,
                  height: 40,
                  child: const Icon(Icons.flag, color: Colors.blue),
                ),
              ],
            ),
        ],
      ),
    );
  }
}
