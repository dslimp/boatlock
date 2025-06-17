import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:geolocator/geolocator.dart';
import '../ble/ble_boatlock.dart';
import '../models/route_model.dart';
import '../services/route_storage.dart';

class RoutePage extends StatefulWidget {
  final BleBoatLock ble;
  const RoutePage({super.key, required this.ble});

  @override
  State<RoutePage> createState() => _RoutePageState();
}

class _RoutePageState extends State<RoutePage> {
  final MapController _mapController = MapController();
  final RouteStorage storage = RouteStorage();
  List<RouteModel> saved = [];
  List<LatLng> points = [];
  double _zoom = 16;

  @override
  void initState() {
    super.initState();
    _load();
    _setStartPoint();
  }

  Future<void> _setStartPoint() async {
    LatLng? start;
    final data = widget.ble.currentData;
    if (data != null && data.lat != 0 && data.lon != 0) {
      start = LatLng(data.lat, data.lon);
    } else {
      final serviceEnabled = await Geolocator.isLocationServiceEnabled();
      if (serviceEnabled) {
        var perm = await Geolocator.checkPermission();
        if (perm == LocationPermission.denied) {
          perm = await Geolocator.requestPermission();
        }
        if (perm != LocationPermission.denied &&
            perm != LocationPermission.deniedForever) {
          final pos = await Geolocator.getCurrentPosition();
          start = LatLng(pos.latitude, pos.longitude);
        }
      }
    }
    if (start != null && points.isEmpty) {
      setState(() {
        points.add(start!);
        _mapController.move(start!, _zoom);
      });
    }
  }

  Future<void> _load() async {
    saved = await storage.loadRoutes();
    setState(() {});
  }

  void _addPoint(LatLng p) {
    setState(() => points.add(p));
  }

  Future<void> _save() async {
    final ctrl = TextEditingController();
    await showDialog(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Имя маршрута'),
        content: TextField(controller: ctrl),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(context), child: const Text('Отмена')),
          TextButton(
            onPressed: () async {
              final name = ctrl.text.trim();
              if (name.isNotEmpty) {
                final routes = await storage.loadRoutes();
                routes.removeWhere((r) => r.name == name);
                routes.add(RouteModel(name: name, points: points));
                await storage.saveRoutes(routes);
              }
              if (mounted) Navigator.pop(context);
              _load();
            },
            child: const Text('Сохранить'),
          )
        ],
      ),
    );
  }

  void _loadRoute(RouteModel r) {
    setState(() => points = List.from(r.points));
  }

  void _chooseRoute() async {
    await showDialog(
      context: context,
      builder: (_) => SimpleDialog(
        title: const Text('Выбрать маршрут'),
        children: saved
            .map((r) => SimpleDialogOption(
                  child: Text(r.name),
                  onPressed: () {
                    Navigator.pop(context);
                    _loadRoute(r);
                  },
                ))
            .toList(),
      ),
    );
  }

  void _send() {
    if (points.isEmpty) return;
    final cmd = 'SET_ROUTE:' +
        points
            .map((p) =>
                '${p.latitude.toStringAsFixed(6)},${p.longitude.toStringAsFixed(6)}')
            .join(';');
    widget.ble.sendCustomCommand(cmd);
    widget.ble.startRoute();
  }

  @override
  Widget build(BuildContext context) {
    final center = points.isNotEmpty ? points.last : const LatLng(0, 0);
    return Scaffold(
      appBar: AppBar(
        title: const Text('Маршрут'),
        actions: [
          IconButton(icon: const Icon(Icons.folder_open), onPressed: _chooseRoute),
          IconButton(icon: const Icon(Icons.save), onPressed: _save),
        ],
      ),
      body: FlutterMap(
        mapController: _mapController,
        options: MapOptions(
          center: center,
          zoom: _zoom,
          onLongPress: (_, p) => _addPoint(p),
          onPositionChanged: (pos, _) {
            if (pos.zoom != null) _zoom = pos.zoom!;
          },
        ),
        children: [
          TileLayer(urlTemplate: 'https://tile.openstreetmap.org/{z}/{x}/{y}.png'),
          if (points.length > 1)
            PolylineLayer(polylines: [
              Polyline(points: points, color: Colors.green, strokeWidth: 4)
            ]),
          MarkerLayer(
            markers: points
                .map((p) => Marker(
                      point: p,
                      width: 40,
                      height: 40,
                      child:
                          const Icon(Icons.circle, color: Colors.green, size: 12),
                    ))
                .toList(),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: _send,
        icon: const Icon(Icons.play_arrow),
        label: const Text('Старт'),
      ),
    );
  }
}
