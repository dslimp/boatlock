import 'package:flutter/material.dart';
import '../ble/ble_boatlock.dart';

class SettingsPage extends StatefulWidget {
  final BleBoatLock ble;
  final bool holdHeading;
  final bool isConnected;
  const SettingsPage({Key? key, required this.ble, required this.holdHeading, required this.isConnected}) : super(key: key);

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  late bool holdHeading;
  late bool isConnected;

  @override
  void initState() {
    super.initState();
    holdHeading = widget.holdHeading;
    isConnected = widget.isConnected;
  }

  void _toggleHoldHeading(bool v) {
    if (!isConnected) return;
    setState(() => holdHeading = v);
    widget.ble.sendCustomCommand('SET_HOLD_HEADING:${v ? 1 : 0}');
  }

  void _startCalib() {
    if (!isConnected) return;
    widget.ble.calibrateCompass();
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Калибровка компаса запущена')),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Настройки')),
      body: ListView(
        children: [
          SwitchListTile(
            title: const Text('Поддерживать курс носа'),
            value: holdHeading,
            onChanged: isConnected ? _toggleHoldHeading : null,
          ),
          ListTile(
            title: const Text('Калибровка компаса'),
            trailing: const Icon(Icons.compass_calibration),
            enabled: isConnected,
            onTap: isConnected ? _startCalib : null,
          ),
        ],
      ),
    );
  }
}
