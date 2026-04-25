import 'dart:async';
import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:geolocator/geolocator.dart';
import '../ble/ble_boatlock.dart';
import '../e2e/app_e2e_probe.dart';
import '../models/boat_data.dart';
import '../widgets/manual_control_sheet.dart';
import '../widgets/status_panel.dart';
import 'settings_page.dart';

class MapPage extends StatefulWidget {
  const MapPage({super.key});
  @override
  State<MapPage> createState() => _MapPageState();
}

class _MapPageState extends State<MapPage> {
  BoatData? boatData;
  late BleBoatLock ble;
  BoatLockAppE2eProbe? _e2eProbe;
  LatLng? selectedAnchorPos;
  final MapController _mapController = MapController();
  double _zoom = 16;
  bool _satellite = false;
  LatLng? phonePos;
  final List<LatLng> _history = [];
  StreamSubscription<Position>? _posSub;
  bool _autoCenter = true;

  Future<void> _confirmAndStopAll() async {
    if (boatData == null) return;
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Аварийный STOP'),
        content: const Text('Остановить якорь и моторы прямо сейчас?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Отмена'),
          ),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: Colors.red),
            onPressed: () => Navigator.of(ctx).pop(true),
            child: const Text('STOP'),
          ),
        ],
      ),
    );

    if (ok == true) {
      await ble.stopAll();
      if (!mounted) return;
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(const SnackBar(content: Text('STOP отправлен')));
    }
  }

  void _centerOnCurrent() {
    final pos = (boatData != null && boatData!.lat != 0 && boatData!.lon != 0)
        ? LatLng(boatData!.lat, boatData!.lon)
        : phonePos;
    if (pos != null) {
      _mapController.move(pos, _zoom);
    }
  }

  void _openManualControl() {
    showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      builder: (_) => ManualControlSheet(
        enabled: boatData != null,
        mode: boatData?.mode ?? 'IDLE',
        onManualControl:
            ({required int steer, required int throttlePct, int ttlMs = 500}) {
              return ble.sendManualControl(
                steer: steer,
                throttlePct: throttlePct,
                ttlMs: ttlMs,
              );
            },
        onManualOff: ble.manualOff,
      ),
    );
  }

  @override
  void initState() {
    super.initState();
    ble = BleBoatLock(
      onData: (data) {
        _e2eProbe?.onData(data);
        setState(() {
          boatData = data;
          if (data != null && data.lat != 0 && data.lon != 0) {
            final p = LatLng(data.lat, data.lon);
            _addToHistory(p);
            if (_autoCenter) {
              _mapController.move(p, _zoom);
            }
          }
        });
      },
      onLog: (line) {
        _e2eProbe?.onDeviceLog(line);
      },
    );
    _e2eProbe = BoatLockAppE2eProbe.maybeCreate(ble);
    _bootstrap();
  }

  Future<void> _bootstrap() async {
    await ble.connectAndListen();
    await _initLocation();
  }

  @override
  void dispose() {
    _e2eProbe?.dispose();
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
    const locationSettings = LocationSettings(
      accuracy: LocationAccuracy.best,
      distanceFilter: 1,
    );

    _posSub = Geolocator.getPositionStream(locationSettings: locationSettings)
        .listen((pos) {
          final p = LatLng(pos.latitude, pos.longitude);
          setState(() {
            phonePos = p;
            _addToHistory(p);
            if (boatData == null && _autoCenter) {
              _mapController.move(p, _zoom);
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
        (boatData != null &&
            boatData!.anchorLat != 0 &&
            boatData!.anchorLon != 0)
        ? LatLng(boatData!.anchorLat, boatData!.anchorLon)
        : null;
    final center = boatPos ?? phonePos ?? anchorPos ?? const LatLng(0, 0);
    final boatDirectionDeg = anchorPos != null
        ? boatData?.anchorHeading ?? 0
        : boatData?.heading ?? 0;

    return Scaffold(
      appBar: AppBar(
        title: const Text('BoatLock OSM Map'),
        actions: [
          IconButton(
            icon: const Icon(Icons.settings),
            tooltip: 'Настройки',
            onPressed: () => Navigator.of(context).push(
              MaterialPageRoute(
                builder: (_) => SettingsPage(
                  ble: ble,
                  holdHeading: boatData?.holdHeading ?? false,
                  stepMaxSpd: boatData?.stepMaxSpd ?? 1000,
                  stepAccel: boatData?.stepAccel ?? 500,
                  compassOffset: boatData?.compassOffset ?? 0.0,
                  compassQ: boatData?.compassQ ?? 0,
                  magQ: boatData?.magQ ?? 0,
                  gyroQ: boatData?.gyroQ ?? 0,
                  rvAcc: boatData?.rvAcc ?? 0.0,
                  magNorm: boatData?.magNorm ?? 0.0,
                  gyroNorm: boatData?.gyroNorm ?? 0.0,
                  pitch: boatData?.pitch ?? 0.0,
                  roll: boatData?.roll ?? 0.0,
                  isConnected: boatData != null,
                  secPaired: boatData?.secPaired ?? false,
                  secAuth: boatData?.secAuth ?? false,
                  secPairWindowOpen: boatData?.secPairWindowOpen ?? false,
                  secReject: boatData?.secReject ?? 'NONE',
                ),
              ),
            ),
          ),
          IconButton(
            icon: const Icon(Icons.sports_esports),
            tooltip: 'Ручное управление',
            onPressed: _openManualControl,
          ),
          IconButton(
            icon: Icon(_satellite ? Icons.map : Icons.satellite),
            tooltip: 'Переключить карту',
            onPressed: () => setState(() {
              _satellite = !_satellite;
            }),
          ),
        ],
      ),
      body: Stack(
        children: [
          FlutterMap(
            mapController: _mapController,
            options: MapOptions(
              initialCenter: center,
              initialZoom: _zoom,
              onPositionChanged: (pos, hasGesture) {
                if (pos.zoom != null) _zoom = pos.zoom!;
                if (hasGesture) _autoCenter = false;
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
              TileLayer(
                urlTemplate: _satellite
                    ? 'https://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}'
                    : 'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
                userAgentPackageName: 'com.example.boatlock_ui',
              ),
              if (_history.length > 1)
                PolylineLayer(
                  polylines: [
                    Polyline(
                      points: _history,
                      color: Colors.blueAccent,
                      strokeWidth: 3,
                    ),
                  ],
                ),
              MarkerLayer(
                markers: [
                  if (boatPos != null)
                    Marker(
                      point: boatPos,
                      width: 40,
                      height: 40,
                      child: Transform.rotate(
                        angle: boatDirectionDeg * math.pi / 180,
                        child: Icon(
                          Icons.directions_boat,
                          color: Colors.blue,
                          size: 36,
                        ),
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
                ],
              ),
            ],
          ),
          if (boatData == null)
            Positioned(
              bottom: 16,
              left: 0,
              right: 0,
              child: Center(
                child: Container(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 12,
                    vertical: 8,
                  ),
                  decoration: BoxDecoration(
                    color: Colors.white.withValues(alpha: 0.9),
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
            top: 0,
            left: 0,
            right: 0,
            child: StatusPanel(data: boatData),
          ),
          Positioned(
            top: 80,
            right: 8,
            child: Column(
              children: [
                FloatingActionButton(
                  heroTag: 'zoom_in_btn',
                  mini: true,
                  onPressed: () {
                    setState(() => _zoom += 1);
                    _mapController.move(_mapController.camera.center, _zoom);
                  },
                  child: const Icon(Icons.add),
                ),
                const SizedBox(height: 8),
                FloatingActionButton(
                  heroTag: 'zoom_out_btn',
                  mini: true,
                  onPressed: () {
                    setState(() => _zoom -= 1);
                    _mapController.move(_mapController.camera.center, _zoom);
                  },
                  child: const Icon(Icons.remove),
                ),
                const SizedBox(height: 8),
                FloatingActionButton(
                  heroTag: 'center_btn',
                  mini: true,
                  onPressed: _centerOnCurrent,
                  child: const Icon(Icons.my_location),
                ),
                const SizedBox(height: 8),
                FloatingActionButton(
                  heroTag: 'north_btn',
                  mini: true,
                  onPressed: () => _mapController.rotate(0),
                  child: const Icon(Icons.north),
                ),
              ],
            ),
          ),
          // Панель с дистанцией — только если есть данные
          if (boatData != null && boatData!.distance > 0)
            Positioned(
              bottom: 24,
              left: 0,
              right: 0,
              child: Center(
                child: Container(
                  padding: EdgeInsets.symmetric(vertical: 12, horizontal: 24),
                  decoration: BoxDecoration(
                    color: Colors.white.withValues(alpha: 0.85),
                    borderRadius: BorderRadius.circular(16),
                    boxShadow: [
                      BoxShadow(blurRadius: 8, color: Colors.black12),
                    ],
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
                      : () async {
                          await ble.setAnchorAt(
                            selectedAnchorPos!.latitude,
                            selectedAnchorPos!.longitude,
                          );
                          setState(() {
                            selectedAnchorPos = null;
                          });
                        },
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.red,
                    foregroundColor: Colors.white,
                    padding: const EdgeInsets.symmetric(
                      horizontal: 16,
                      vertical: 10,
                    ),
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
            heroTag: 'stop_all',
            tooltip: "Аварийный STOP",
            onPressed: boatData == null ? null : _confirmAndStopAll,
            backgroundColor: boatData == null ? Colors.grey : Colors.red,
            foregroundColor: Colors.white,
            child: const Icon(Icons.stop),
          ),
          const SizedBox(width: 12),
          FloatingActionButton(
            heroTag: 'refresh',
            tooltip: "Обновить данные",
            onPressed: () => ble.requestSnapshot(),
            child: const Icon(Icons.refresh),
          ),
          const SizedBox(width: 12),
          FloatingActionButton(
            heroTag: 'set_anchor',
            tooltip: "Установить якорь",
            onPressed: boatData == null ? null : () => ble.setAnchor(),
            backgroundColor: boatData == null ? Colors.grey : Colors.red[400],
            child: const Icon(Icons.anchor),
          ),
        ],
      ),
    );
  }
}
