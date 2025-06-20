import 'dart:async';
import 'package:flutter/material.dart';
import 'package:sensors_plus/sensors_plus.dart';
import 'dart:math' as math;
import '../ble/ble_boatlock.dart';

class CompassPage extends StatefulWidget {
  final BleBoatLock ble;
  final bool emuCompass;
  const CompassPage({Key? key, required this.ble, required this.emuCompass}) : super(key: key);

  @override
  State<CompassPage> createState() => _CompassPageState();
}

class _CompassPageState extends State<CompassPage> {
  double _heading = 0;
  bool _usePhone = false;
  StreamSubscription<MagnetometerEvent>? _sub;

  @override
  void initState() {
    super.initState();
    if (widget.emuCompass) {
      _sendHeading();
    }
  }

  @override
  void dispose() {
    _sub?.cancel();
    super.dispose();
  }

  void _sendHeading() {
    widget.ble
        .sendCustomCommand('SET_HEADING:${_heading.toStringAsFixed(1)}');
  }

  void _togglePhone(bool v) {
    setState(() => _usePhone = v);
    _sub?.cancel();
    if (_usePhone) {
      _sub = magnetometerEventStream().listen((event) {
        final h =
            (math.atan2(event.y, event.x) * 180 / math.pi + 360) % 360;
        setState(() => _heading = h);
        if (widget.emuCompass) _sendHeading();
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Компас')),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Text('${_heading.toStringAsFixed(1)}°',
                style: const TextStyle(fontSize: 32)),
            Slider(
              min: 0,
              max: 360,
              value: _heading,
              onChanged: !_usePhone && widget.emuCompass
                  ? (v) {
                      setState(() => _heading = v);
                      _sendHeading();
                    }
                  : null,
            ),
            const SizedBox(height: 16),
            SwitchListTile(
              title: const Text('Использовать компас телефона'),
              value: _usePhone,
              onChanged: (v) => _togglePhone(v),
            ),
          ],
        ),
      ),
    );
  }
}
