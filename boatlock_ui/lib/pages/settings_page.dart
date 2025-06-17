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
        ],
      ),
    );
  }
}
