import 'dart:async';
import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:geolocator/geolocator.dart';
import 'package:sensors_plus/sensors_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../ble/ble_boatlock.dart';
import '../models/boat_data.dart';
import '../widgets/status_panel.dart';
import '../services/log_service.dart';
import 'logs_page.dart';
import 'settings_page.dart';
import 'route_page.dart';
import 'compass_page.dart';

class MapPage extends StatefulWidget {
  const MapPage({super.key});
  @override
  State<MapPage> createState() => _MapPageState();
}

class _MapPageState extends State<MapPage> {
  static const String _kPhoneHeadingOffsetKey = 'phone_heading_offset_deg';
  BoatData? boatData;
  late BleBoatLock ble;
  LatLng? selectedAnchorPos;
  final MapController _mapController = MapController();
  double _zoom = 16;
  bool _satellite = false;
  LatLng? phonePos;
  final List<LatLng> _history = [];
  StreamSubscription<Position>? _posSub;
  StreamSubscription<MagnetometerEvent>? _magSub;
  bool _autoCenter = true;
  bool _manualMode = false;
  double _manualSpeed = 0;
  DateTime? _lastPhoneGpsSentAt;
  DateTime? _lastPhoneHeadingSentAt;
  double? _lastPhoneHeadingSent;
  double? _lastRawPhoneHeading;
  double? _lastGpsCourseHeadingDeg;
  DateTime? _lastGpsCourseUpdatedAt;
  double _phoneHeadingOffsetDeg = 0.0;
  bool _pausePhoneHeadingForwarding = false;

  void _centerOnCurrent() {
    final pos = (boatData != null && boatData!.lat != 0 && boatData!.lon != 0)
        ? LatLng(boatData!.lat, boatData!.lon)
        : phonePos;
    if (pos != null) {
      _mapController.move(pos, _zoom);
    }
  }

  void _toggleManual(bool v) {
    setState(() => _manualMode = v);
    ble.setManualMode(v);
  }

  void _sendDirection(int dir) {
    ble.sendManualDirection(dir);
  }

  void _setSpeed(double v) {
    setState(() => _manualSpeed = v);
    ble.sendManualSpeed(v.round());
  }

  Widget _holdBtn(IconData icon, int dir) => Padding(
    padding: const EdgeInsets.all(4),
    child: GestureDetector(
      onTapDown: (_) => _sendDirection(dir),
      onTapUp: (_) => _sendDirection(-1),
      onTapCancel: () => _sendDirection(-1),
      child: Container(
        width: 48,
        height: 48,
        decoration: BoxDecoration(
          color: Theme.of(context).colorScheme.primary,
          borderRadius: BorderRadius.circular(4),
        ),
        child: Icon(icon, color: Colors.white),
      ),
    ),
  );

  double _normalizeDeg(double value) {
    var normalized = value % 360.0;
    if (normalized < 0) normalized += 360.0;
    return normalized;
  }

  double _normalizeSignedDeg(double value) {
    var normalized = _normalizeDeg(value);
    if (normalized > 180.0) normalized -= 360.0;
    return normalized;
  }

  double _angularDistanceDeg(double a, double b) {
    final diff = (a - b).abs();
    return diff > 180.0 ? 360.0 - diff : diff;
  }

  Future<void> _loadPhoneHeadingOffset() async {
    final prefs = await SharedPreferences.getInstance();
    _phoneHeadingOffsetDeg = _normalizeSignedDeg(
      prefs.getDouble(_kPhoneHeadingOffsetKey) ?? 0.0,
    );
  }

  Future<void> _savePhoneHeadingOffset() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setDouble(_kPhoneHeadingOffsetKey, _phoneHeadingOffsetDeg);
  }

  Future<void> _calibratePhoneBow() async {
    final rawHeading = _lastRawPhoneHeading;
    final course = _lastGpsCourseHeadingDeg;
    final courseFresh =
        course != null &&
        _lastGpsCourseUpdatedAt != null &&
        DateTime.now().difference(_lastGpsCourseUpdatedAt!).inSeconds <= 6;

    if (rawHeading == null) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Нет данных компаса телефона')),
      );
      return;
    }

    if (!courseFresh) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Нужен курс GPS: двигайтесь прямо 5-10 секунд'),
        ),
      );
      return;
    }

    _phoneHeadingOffsetDeg = _normalizeSignedDeg(course - rawHeading);
    await _savePhoneHeadingOffset();
    if (!mounted) return;
    setState(() {});
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(
          'Калибровка носа сохранена: offset ${_phoneHeadingOffsetDeg.toStringAsFixed(1)}°',
        ),
      ),
    );
  }

  Future<void> _calibratePhoneBowStatic() async {
    final mountedRaw = _lastRawPhoneHeading;
    if (mountedRaw == null) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Нет данных компаса телефона')),
      );
      return;
    }

    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Калибровка носа без движения'),
        content: const Text(
          '1) Текущее положение телефона будет сохранено как монтажное.\n'
          '2) Поверните телефон так, чтобы его верх смотрел точно в нос лодки.\n'
          '3) Нажмите "Готово".',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Отмена'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(ctx).pop(true),
            child: const Text('Готово'),
          ),
        ],
      ),
    );
    if (confirmed != true) {
      return;
    }

    final bowRaw = _lastRawPhoneHeading;
    if (bowRaw == null) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Не удалось прочитать компас телефона')),
      );
      return;
    }

    _phoneHeadingOffsetDeg = _normalizeSignedDeg(bowRaw - mountedRaw);
    await _savePhoneHeadingOffset();
    if (!mounted) return;
    setState(() {});
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(
          'Калибровка без движения сохранена: offset ${_phoneHeadingOffsetDeg.toStringAsFixed(1)}°',
        ),
      ),
    );
  }

  Future<void> _resetPhoneBowCalibration() async {
    _phoneHeadingOffsetDeg = 0.0;
    await _savePhoneHeadingOffset();
    if (!mounted) return;
    setState(() {});
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Калибровка носа сброшена')),
    );
  }

  @override
  void initState() {
    super.initState();
    ble = BleBoatLock(
      onData: (data) {
        setState(() {
          boatData = data;
          _manualMode = data?.mode == 'MANUAL';
          if (data != null && data.lat != 0 && data.lon != 0) {
            final p = LatLng(data.lat, data.lon);
            _addToHistory(p);
            if (_autoCenter) {
              _mapController.move(p, _zoom);
            }
          }
        });
      },
      onLog: (line) => logService.add(line),
    );
    _bootstrap();
  }

  Future<void> _bootstrap() async {
    await _loadPhoneHeadingOffset();
    _startPhoneCompassForwarding();
    await ble.connectAndListen();
    await _initLocation();
  }

  @override
  void dispose() {
    ble.dispose();
    _posSub?.cancel();
    _magSub?.cancel();
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

    _posSub = Geolocator.getPositionStream(
      locationSettings: locationSettings,
    ).listen((pos) {
      final speedMps = (pos.speed.isFinite && pos.speed > 0.0) ? pos.speed : 0.0;
      if (pos.heading.isFinite && pos.heading >= 0 && speedMps >= 1.5) {
        _lastGpsCourseHeadingDeg = _normalizeDeg(pos.heading);
        _lastGpsCourseUpdatedAt = DateTime.now();
      }
      final p = LatLng(pos.latitude, pos.longitude);
      setState(() {
        phonePos = p;
        _addToHistory(p);
        if (boatData == null && _autoCenter) {
          _mapController.move(p, _zoom);
        }
      });
      _forwardPhoneGps(pos);
    });
  }

  void _startPhoneCompassForwarding() {
    _magSub?.cancel();
    _magSub = magnetometerEventStream().listen((event) {
      if (_pausePhoneHeadingForwarding) {
        return;
      }

      final rawHeading =
          _normalizeDeg(math.atan2(event.y, event.x) * 180.0 / math.pi);
      _lastRawPhoneHeading = rawHeading;
      final heading = _normalizeDeg(rawHeading + _phoneHeadingOffsetDeg);
      final now = DateTime.now();
      final enoughTimePassed =
          _lastPhoneHeadingSentAt == null ||
          now.difference(_lastPhoneHeadingSentAt!).inMilliseconds >= 150;
      final changedEnough =
          _lastPhoneHeadingSent == null ||
          _angularDistanceDeg(heading, _lastPhoneHeadingSent!) >= 1.0;
      if (!enoughTimePassed || !changedEnough) {
        return;
      }

      _lastPhoneHeadingSentAt = now;
      _lastPhoneHeadingSent = heading;
      ble.sendHeading(heading);
    });
  }

  void _forwardPhoneGps(Position pos) {
    final now = DateTime.now();
    if (_lastPhoneGpsSentAt != null &&
        now.difference(_lastPhoneGpsSentAt!).inMilliseconds < 900) {
      return;
    }

    final speedKmh = (pos.speed.isFinite && pos.speed > 0.0)
        ? pos.speed * 3.6
        : 0.0;
    final raw = pos.toJson();
    final satUsedInFix =
        (raw['gnss_satellites_used_in_fix'] as num?)?.round() ?? 0;
    final satCount = (raw['gnss_satellite_count'] as num?)?.round() ?? 0;
    final satellites = satUsedInFix > 0 ? satUsedInFix : satCount;
    _lastPhoneGpsSentAt = now;
    ble.sendPhoneGps(
      pos.latitude,
      pos.longitude,
      speedKmh: speedKmh,
      satellites: satellites,
    );
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
            icon: const Icon(Icons.list),
            tooltip: 'Логи',
            onPressed: () => Navigator.of(
              context,
            ).push(MaterialPageRoute(builder: (_) => const LogsPage())),
          ),
          IconButton(
            icon: const Icon(Icons.alt_route),
            tooltip: 'Трек',
            onPressed: () => Navigator.of(
              context,
            ).push(MaterialPageRoute(builder: (_) => RoutePage(ble: ble))),
          ),
          IconButton(
            icon: const Icon(Icons.explore),
            tooltip: 'Компас',
            onPressed: () async {
              setState(() => _pausePhoneHeadingForwarding = true);
              await Navigator.of(context).push(
                MaterialPageRoute(
                  builder: (_) => CompassPage(
                    ble: ble,
                    emuCompass: boatData?.emuCompass == 1,
                  ),
                ),
              );
              if (mounted) {
                setState(() => _pausePhoneHeadingForwarding = false);
              }
            },
          ),
          PopupMenuButton<String>(
            tooltip: 'Калибровка носа',
            icon: const Icon(Icons.compass_calibration),
            onSelected: (value) async {
              if (value == 'calibrate') {
                await _calibratePhoneBow();
              } else if (value == 'calibrate_static') {
                await _calibratePhoneBowStatic();
              } else if (value == 'reset') {
                await _resetPhoneBowCalibration();
              }
            },
            itemBuilder: (_) => [
              const PopupMenuItem<String>(
                value: 'calibrate_static',
                child: Text('Калибровать нос (без движения)'),
              ),
              const PopupMenuItem<String>(
                value: 'calibrate',
                child: Text('Калибровать нос (по GPS курсу)'),
              ),
              PopupMenuItem<String>(
                value: 'reset',
                child: Text(
                  'Сбросить калибровку (${_phoneHeadingOffsetDeg.toStringAsFixed(1)}°)',
                ),
              ),
            ],
          ),
          IconButton(
            icon: const Icon(Icons.settings),
            tooltip: 'Настройки',
            onPressed: () => Navigator.of(context).push(
              MaterialPageRoute(
                builder: (_) => SettingsPage(
                  ble: ble,
                  holdHeading: boatData?.holdHeading ?? false,
                  emuCompass: boatData?.emuCompass == 1,
                  stepSpr: boatData?.stepSpr ?? 200,
                  stepMaxSpd: boatData?.stepMaxSpd ?? 1000,
                  stepAccel: boatData?.stepAccel ?? 500,
                  isConnected: boatData != null,
                ),
              ),
            ),
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
                    padding: const EdgeInsets.symmetric(
                      horizontal: 16,
                      vertical: 10,
                    ),
                  ),
                ),
              ),
            ),
          Positioned(
            bottom: 150,
            left: 0,
            right: 0,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                ElevatedButton.icon(
                  icon: Icon(_manualMode ? Icons.toggle_on : Icons.toggle_off),
                  label: const Text('Ручной режим'),
                  onPressed: () => _toggleManual(!_manualMode),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: _manualMode ? Colors.green : Colors.blue,
                  ),
                ),
                if (_manualMode)
                  Column(
                    children: [
                      Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          _holdBtn(Icons.rotate_left, 0),
                          _holdBtn(Icons.rotate_right, 1),
                        ],
                      ),
                      Slider(
                        min: -255,
                        max: 255,
                        value: _manualSpeed,
                        onChanged: (v) => _setSpeed(v),
                      ),
                    ],
                  ),
              ],
            ),
          ),
        ],
      ),
      floatingActionButton: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          FloatingActionButton(
            heroTag: 'refresh',
            tooltip: "Обновить данные",
            onPressed: () => ble.requestAllParams(),
            child: Icon(Icons.refresh),
          ),
          SizedBox(width: 12),
          FloatingActionButton(
            heroTag: 'set_anchor',
            tooltip: "Установить якорь",
            onPressed: boatData == null ? null : () => ble.setAnchor(),
            backgroundColor: boatData == null ? Colors.grey : Colors.red[400],
            child: Icon(Icons.anchor),
          ),
        ],
      ),
    );
  }
}
