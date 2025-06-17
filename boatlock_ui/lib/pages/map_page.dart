import 'dart:async';
import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:geolocator/geolocator.dart';
import '../ble/ble_boatlock.dart';
import '../models/boat_data.dart';
import '../widgets/status_panel.dart';
import 'logs_page.dart';
import 'settings_page.dart';
import 'route_page.dart';


class MapPage extends StatefulWidget {
  const MapPage({super.key});
  @override
  State<MapPage> createState() => _MapPageState();
}

class _MapPageState extends State<MapPage> {
  BoatData? boatData;
  late BleBoatLock ble;
  LatLng? selectedAnchorPos;
  final MapController _mapController = MapController();
  double _zoom = 16;
  LatLng? phonePos;
  final List<LatLng> _history = [];
  StreamSubscription<Position>? _posSub;

  @override
  void initState() {
    super.initState();
    ble = BleBoatLock(onData: (data) {
      setState(() {
        boatData = data;
        if (data != null && data.lat != 0 && data.lon != 0) {
          final p = LatLng(data.lat, data.lon);
          _addToHistory(p);
          _mapController.move(p, _zoom);
        }
      });
    });
    ble.connectAndListen();
    _initLocation();
  }

  @override
  void dispose() {
    ble.dispose();
    _posSub?.cancel();
    super.dispose();
  }

  Future<void> _initLocation() async {
    final serviceEnabled = await Geolocator.isLocationServiceEnabled();
    if (!serviceEnabled) return;
    var permission = await Geolocator.checkPermission();
    if (permission == LocationPermission.denied) {
      permission = await Geolocator.requestPermission();
      if (permission == LocationPermission.denied ||
          permission == LocationPermission.deniedForever) {
        return;
      }
    }
    _posSub = Geolocator.getPositionStream().listen((pos) {
      setState(() {
        phonePos = LatLng(pos.latitude, pos.longitude);
        _addToHistory(phonePos!);
        if (boatData == null) {
          _mapController.move(phonePos!, _zoom);
        }
      });
    });
  }

  void _addToHistory(LatLng p) {
    _history.add(p);
    if (_history.length > 500) _history.removeAt(0);
  }

@override
Widget build(BuildContext context) {
  final boatPos =
      (boatData != null && boatData!.lat != 0 && boatData!.lon != 0)
          ? LatLng(boatData!.lat, boatData!.lon)
          : null;
  final anchorPos =
      (boatData != null && boatData!.anchorLat != 0 && boatData!.anchorLon != 0)
          ? LatLng(boatData!.anchorLat, boatData!.anchorLon)
          : null;
  final center = boatPos ?? phonePos ?? anchorPos ?? const LatLng(0, 0);


  return Scaffold(
    appBar: AppBar(
      title: const Text('BoatLock OSM Map'),
      actions: [
        IconButton(
          icon: const Icon(Icons.list),
          tooltip: 'Логи',
          onPressed: () => Navigator.of(context).push(
            MaterialPageRoute(builder: (_) => const LogsPage()),
          ),
        ),
        IconButton(
          icon: const Icon(Icons.route),
          tooltip: 'Маршруты',
          onPressed: () => Navigator.of(context).push(
            MaterialPageRoute(builder: (_) => RoutePage(ble: ble)),
          ),
        ),
        IconButton(
          icon: const Icon(Icons.settings),
          tooltip: 'Настройки',
          onPressed: () => Navigator.of(context).push(
            MaterialPageRoute(
              builder: (_) => SettingsPage(
                ble: ble,
                holdHeading: boatData?.holdHeading ?? false,
                isConnected: boatData != null,
              ),
            ),
          ),
        ),
      ],
    ),
    body: Stack(
      children: [
        FlutterMap(
          mapController: _mapController,
          options: MapOptions(
            center: center,
            zoom: _zoom,
            onPositionChanged: (pos, _) {
              if (pos.zoom != null) _zoom = pos.zoom!;
            },
            onLongPress: (_, point) {
              if (boatData != null) {
                setState(() {
                  selectedAnchorPos = point;
                });
              }
            },
          ),
          children: [
            TileLayer(urlTemplate: "https://tile.openstreetmap.org/{z}/{x}/{y}.png"),
            if (_history.length > 1)
              PolylineLayer(
                polylines: [
                  Polyline(points: _history, color: Colors.blueAccent, strokeWidth: 3),
                ],
              ),
            MarkerLayer(markers: [
              if (boatPos != null)
                Marker(
                  point: boatPos,
                  width: 40,
                  height: 40,
                  child: Transform.rotate(
                    angle: (boatData?.heading ?? 0) * math.pi / 180,
                    child: Icon(Icons.directions_boat, color: Colors.blue, size: 36),
                  ),
                ),
              if (anchorPos != null)
                Marker(
                  point: anchorPos,
                  width: 40,
                  height: 40,
                  child: Icon(Icons.anchor, color: Colors.red, size: 36),
                ),
              if (selectedAnchorPos != null)
                Marker(
                  point: selectedAnchorPos!,
                  width: 40,
                  height: 40,
                  child: Icon(Icons.place, color: Colors.orange, size: 36),
                ),
            ]),
          ],
        ),
        if (boatData == null)
          Positioned(
            bottom: 16,
            left: 0,
            right: 0,
            child: Center(
              child: Container(
                padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                decoration: BoxDecoration(
                  color: Colors.white.withOpacity(0.9),
                  borderRadius: BorderRadius.circular(12),
                ),
                child: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: const [
                    SizedBox(
                      width: 16,
                      height: 16,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    ),
                    SizedBox(width: 8),
                    Text('Поиск устройства BoatLock…'),
                  ],
                ),
              ),
            ),
          ),
        // Панель статуса — всегда вверху
        Positioned(
          top: 0, left: 0, right: 0,
          child: StatusPanel(data: boatData),
        ),
        // Панель с дистанцией — только если есть данные
        if (boatData != null && boatData!.distance > 0)
          Positioned(
            bottom: 24,
            left: 0, right: 0,
            child: Center(
              child: Container(
                padding: EdgeInsets.symmetric(vertical: 12, horizontal: 24),
                decoration: BoxDecoration(
                  color: Colors.white.withOpacity(0.85),
                  borderRadius: BorderRadius.circular(16),
                  boxShadow: [BoxShadow(blurRadius: 8, color: Colors.black12)],
                ),
                child: Text(
                  'До якоря: ${boatData!.distance.toStringAsFixed(1)} м',
                  style: TextStyle(fontSize: 22, fontWeight: FontWeight.bold),
                ),
              ),
            ),
          ),
        if (selectedAnchorPos != null && boatData != null)
          Positioned(
            bottom: 90,
            left: 0,
            right: 0,
            child: Center(
              child: ElevatedButton.icon(
                icon: const Icon(Icons.check),
                label: const Text('Установить якорь здесь'),
                onPressed: boatData == null
                    ? null
                    : () {
                        final cmd =
                            'SET_ANCHOR:${selectedAnchorPos!.latitude},${selectedAnchorPos!.longitude}';
                        ble.sendCustomCommand(cmd);
                        setState(() {
                          selectedAnchorPos = null;
                        });
                      },
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.red,
                  foregroundColor: Colors.white,
                  padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
                ),
              ),
            ),
          ),
      ],
    ),
    floatingActionButton: Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        FloatingActionButton(
          heroTag: 'refresh',
          child: Icon(Icons.refresh),
          tooltip: "Обновить данные",
          onPressed: () => ble.requestAllParams(),
        ),
        SizedBox(width: 12),
        FloatingActionButton(
          heroTag: 'set_anchor',
          child: Icon(Icons.anchor),
          tooltip: "Установить якорь",
          onPressed: boatData == null ? null : () => ble.setAnchor(),
          backgroundColor:
              boatData == null ? Colors.grey : Colors.red[400],
        ),
      ],
    ),
  );
}
}