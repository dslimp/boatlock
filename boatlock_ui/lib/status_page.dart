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
  LatLng anchorPoint = LatLng(54.9062, 37.6065); // Ока, Тульская обл.
  late Timer _timer;
  double heading = 0.0;
  double distanceToAnchor = 0.0;
  LatLng boatPosition = LatLng(54.9050, 37.6045);

  double distanceSlider = 3.0;

  late MapController _mapController;
  bool _fitBoundsCalled = false;

  @override
  void initState() {
    super.initState();
    _mapController = MapController();
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

        // После появления boatPosition и anchorPoint выставляем fitBounds один раз
        if (!_fitBoundsCalled) {
          LatLngBounds bounds = LatLngBounds.fromPoints([boatPosition, anchorPoint]);
          _mapController.fitBounds(
            bounds,
            options: const FitBoundsOptions(padding: EdgeInsets.all(60)),
          );
          _fitBoundsCalled = true;
        }
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
      appBar: AppBar(
        title: const Text("Состояние (эмуляция)"),
        actions: [
          IconButton(
            tooltip: "Сделать якорь по текущей позиции",
            icon: const Icon(Icons.anchor_outlined),
            onPressed: () {
              setState(() {
                anchorPoint = boatPosition;
                _fitBoundsCalled = false; // пересчитать fitBounds
              });
            },
          )
        ],
      ),
      body: Column(
        children: [
          // Компасная стрелка и слайдер
          Padding(
            padding: const EdgeInsets.all(12.0),
            child: Column(
              children: [
                Text("Курс на якорь: ${heading.toStringAsFixed(1)}°", style: const TextStyle(fontSize: 16)),
                const SizedBox(height: 4),
                Stack(
                  alignment: Alignment.center,
                  children: [
                    Container(
                      width: 70,
                      height: 70,
                      decoration: BoxDecoration(
                        shape: BoxShape.circle,
                        color: Colors.blue.withOpacity(0.1),
                        boxShadow: [
                          BoxShadow(
                            color: Colors.blue.withOpacity(0.15),
                            blurRadius: 12,
                            spreadRadius: 5,
                          ),
                        ],
                      ),
                    ),
                    Transform.rotate(
                      angle: heading * pi / 180,
                      child: const Icon(Icons.navigation, size: 56, color: Colors.blue),
                    ),
                  ],
                ),
                const SizedBox(height: 6),
                Text("Расстояние до якоря: ${distanceToAnchor.toStringAsFixed(1)} м", style: const TextStyle(fontSize: 16)),
                Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    const Text("Радиус: "),
                    SizedBox(
                      width: 160,
                      child: Slider(
                        value: distanceSlider,
                        min: 0.5,
                        max: 10.0,
                        divisions: 19,
                        label: "${distanceSlider.toStringAsFixed(1)} м",
                        onChanged: (value) {
                          setState(() {
                            distanceSlider = value;
                          });
                        },
                      ),
                    ),
                    Text("${distanceSlider.toStringAsFixed(1)} м"),
                  ],
                ),
              ],
            ),
          ),
          Expanded(
            child: Stack(
              children: [
                FlutterMap(
                  mapController: _mapController,
                  options: MapOptions(
                    center: boatPosition,
                    zoom: 16.0,
                    onPositionChanged: (position, hasGesture) {
                      if (hasGesture) _fitBoundsCalled = true;
                    },
                  ),
                  children: [
                    TileLayer(
                      urlTemplate: "https://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
                      userAgentPackageName: 'com.example.app',
                    ),
                    MarkerLayer(
                      markers: [
                        Marker(
                          point: boatPosition,
                          width: 70,
                          height: 70,
                          child: Stack(
                            alignment: Alignment.center,
                            children: [
                              Container(
                                width: 44,
                                height: 44,
                                decoration: BoxDecoration(
                                  shape: BoxShape.circle,
                                  color: Colors.blue.withOpacity(0.22),
                                  boxShadow: [
                                    BoxShadow(
                                      color: Colors.blue.withOpacity(0.4),
                                      blurRadius: 12,
                                      spreadRadius: 6,
                                    ),
                                  ],
                                ),
                              ),
                              const Icon(Icons.directions_boat, color: Colors.blue, size: 36),
                            ],
                          ),
                        ),
                        Marker(
                          point: anchorPoint,
                          width: 70,
                          height: 70,
                          child: Stack(
                            alignment: Alignment.center,
                            children: [
                              Container(
                                width: 44,
                                height: 44,
                                decoration: BoxDecoration(
                                  shape: BoxShape.circle,
                                  color: Colors.red.withOpacity(0.17),
                                  boxShadow: [
                                    BoxShadow(
                                      color: Colors.red.withOpacity(0.35),
                                      blurRadius: 10,
                                      spreadRadius: 7,
                                    ),
                                  ],
                                ),
                              ),
                              const Icon(Icons.anchor, color: Colors.red, size: 36),
                            ],
                          ),
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
                    CircleLayer(
                      circles: [
                        CircleMarker(
                          point: anchorPoint,
                          color: Colors.yellow.withOpacity(0.12),
                          borderColor: Colors.yellow.withOpacity(0.4),
                          borderStrokeWidth: 2,
                          useRadiusInMeter: true,
                          radius: distanceSlider,
                        ),
                      ],
                    ),
                  ],
                ),
                // Кнопки +/-
                Positioned(
                  top: 16,
                  right: 16,
                  child: Column(
                    children: [
                      FloatingActionButton(
                        heroTag: "zoom_in",
                        mini: true,
                        onPressed: () {
                          _mapController.move(
                            _mapController.center,
                            _mapController.zoom + 1,
                          );
                        },
                        child: const Icon(Icons.add),
                      ),
                      const SizedBox(height: 8),
                      FloatingActionButton(
                        heroTag: "zoom_out",
                        mini: true,
                        onPressed: () {
                          _mapController.move(
                            _mapController.center,
                            _mapController.zoom - 1,
                          );
                        },
                        child: const Icon(Icons.remove),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
