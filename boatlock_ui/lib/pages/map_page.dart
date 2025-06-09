import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import '../ble/ble_boatlock.dart';
import '../models/boat_data.dart';
import '../widgets/status_panel.dart';

class MapPage extends StatefulWidget {
  const MapPage({super.key});
  @override
  State<MapPage> createState() => _MapPageState();
}

class _MapPageState extends State<MapPage> {
  BoatData? boatData;
  late BleBoatLock ble;

  @override
  void initState() {
    super.initState();
    ble = BleBoatLock(onData: (data) {
      setState(() => boatData = data);
    });
    ble.connectAndListen();
  }

  @override
  void dispose() {
    ble.dispose();
    super.dispose();
  }

@override
Widget build(BuildContext context) {
  final boatPos = boatData == null ? null : LatLng(boatData!.lat, boatData!.lon);
  final anchorPos = boatData == null ? null : LatLng(boatData!.anchorLat, boatData!.anchorLon);

  return Scaffold(
    appBar: AppBar(title: Text('BoatLock OSM Map')),
    body: Stack(
      children: [
        // Карта только если boatPos валиден
        if (boatPos != null && boatPos.latitude != 0 && boatPos.longitude != 0)
          FlutterMap(
            options: MapOptions(center: boatPos, zoom: 16),
            children: [
              TileLayer(urlTemplate: "https://tile.openstreetmap.org/{z}/{x}/{y}.png"),
              MarkerLayer(markers: [
                Marker(
                  point: boatPos,
                  width: 40,
                  height: 40,
                  child: Icon(Icons.directions_boat, color: Colors.blue, size: 36),
                ),
                if (anchorPos != null && anchorPos.latitude != 0 && anchorPos.longitude != 0)
                  Marker(
                    point: anchorPos,
                    width: 40,
                    height: 40,
                    child: Icon(Icons.anchor, color: Colors.red, size: 36),
                  ),
              ]),
            ],
          ),
        // Если нет координат — показывай заглушку/прогресс
        if (boatPos == null || boatPos.latitude == 0)
          Center(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                CircularProgressIndicator(),
                SizedBox(height: 16),
                Text(
                  'Поиск устройства BoatLock…',
                  style: TextStyle(fontSize: 18),
                ),
                SizedBox(height: 16),
                Text(
                  boatData == null
                      ? "Нет данных с BLE"
                      : "Ждём координаты от устройства…",
                  style: TextStyle(fontSize: 14, color: Colors.grey),
                ),
              ],
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
          onPressed: () => ble.setAnchor(),
          backgroundColor: Colors.red[400],
        ),
      ],
    ),
  );
}
}