import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

void main() {
  runApp(const BoatLockApp());
}

class BoatLockApp extends StatelessWidget {
  const BoatLockApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'BoatLock BLE Control',
      theme: ThemeData(
        primarySwatch: Colors.blue,
      ),
      home: const ControlPage(),
    );
  }
}

class ControlPage extends StatefulWidget {
  const ControlPage({super.key});

  @override
  State<ControlPage> createState() => _ControlPageState();
}

class _ControlPageState extends State<ControlPage> {
  final flutterReactiveBle = FlutterReactiveBle();
  StreamSubscription<DiscoveredDevice>? _scanStream;
  late DiscoveredDevice connectedDevice;
  late QualifiedCharacteristic txChar;
  late QualifiedCharacteristic rxChar;

  final kpController = TextEditingController();
  final kiController = TextEditingController();
  final kdController = TextEditingController();
  final distController = TextEditingController();
  final angleController = TextEditingController();
  double? currentDistance;

  final serviceUuid = Uuid.parse("0000ffe0-0000-1000-8000-00805f9b34fb");
  final txCharUuid = Uuid.parse("0000ffe1-0000-1000-8000-00805f9b34fb");
  final rxCharUuid = Uuid.parse("0000ffe1-0000-1000-8000-00805f9b34fb");

  @override
  void initState() {
    super.initState();
    scanAndConnect();
  }

  void scanAndConnect() async {
    _scanStream = flutterReactiveBle.scanForDevices(withServices: [serviceUuid]).listen((device) async {
      if (device.name == "VirtualAnchor") {
        await _scanStream?.cancel();
        connectedDevice = device;

        await flutterReactiveBle.connectToDevice(id: device.id).listen((_) {}).asFuture();

        txChar = QualifiedCharacteristic(
          serviceId: serviceUuid,
          characteristicId: txCharUuid,
          deviceId: device.id,
        );
        rxChar = QualifiedCharacteristic(
          serviceId: serviceUuid,
          characteristicId: rxCharUuid,
          deviceId: device.id,
        );

        flutterReactiveBle.subscribeToCharacteristic(txChar).listen((data) {
          final message = utf8.decode(data);
          handleIncomingData(message);
        });
      }
    });
  }

  void handleIncomingData(String data) {
    for (var line in data.split('\n')) {
      if (line.startsWith("Kp:")) kpController.text = line.substring(3);
      else if (line.startsWith("Ki:")) kiController.text = line.substring(3);
      else if (line.startsWith("Kd:")) kdController.text = line.substring(3);
      else if (line.startsWith("dist:")) distController.text = line.substring(5);
      else if (line.startsWith("angle:")) angleController.text = line.substring(6);
      else if (line.startsWith("curdist:")) currentDistance = double.tryParse(line.substring(8));
    }
    setState(() {});
  }

  void sendParam(String name, String value) async {
    final data = utf8.encode("$name:$value\n");
    await flutterReactiveBle.writeCharacteristicWithResponse(rxChar, value: data);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("BoatLock Control")),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: SingleChildScrollView(
          child: Column(
            children: [
              TextField(
                controller: kpController,
                decoration: const InputDecoration(labelText: "Kp"),
                onSubmitted: (val) => sendParam("Kp", val),
              ),
              TextField(
                controller: kiController,
                decoration: const InputDecoration(labelText: "Ki"),
                onSubmitted: (val) => sendParam("Ki", val),
              ),
              TextField(
                controller: kdController,
                decoration: const InputDecoration(labelText: "Kd"),
                onSubmitted: (val) => sendParam("Kd", val),
              ),
              TextField(
                controller: distController,
                decoration: const InputDecoration(labelText: "Distance Threshold (m)"),
                onSubmitted: (val) => sendParam("dist", val),
              ),
              TextField(
                controller: angleController,
                decoration: const InputDecoration(labelText: "Angle Tolerance (deg)"),
                onSubmitted: (val) => sendParam("angle", val),
              ),
              const SizedBox(height: 16),
              ElevatedButton(
                onPressed: () => sendParam("get", "1"),
                child: const Text("Обновить параметры"),
              ),
              const SizedBox(height: 16),
              if (currentDistance != null)
                Text("Текущая дистанция до якоря: ${currentDistance!.toStringAsFixed(2)} м"),
            ],
          ),
        ),
      ),
    );
  }

  @override
  void dispose() {
    kpController.dispose();
    kiController.dispose();
    kdController.dispose();
    distController.dispose();
    angleController.dispose();
    _scanStream?.cancel();
    super.dispose();
  }
}
