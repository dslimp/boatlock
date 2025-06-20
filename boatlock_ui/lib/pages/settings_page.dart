import 'package:flutter/material.dart';
import '../ble/ble_boatlock.dart';

class SettingsPage extends StatefulWidget {
  final BleBoatLock ble;
  final bool holdHeading;
  final bool emuCompass;
  final int stepSpr;
  final bool isConnected;
  const SettingsPage({Key? key, required this.ble, required this.holdHeading, required this.emuCompass, required this.stepSpr, required this.isConnected}) : super(key: key);

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  late bool holdHeading;
  late bool emuCompass;
  late int stepSpr;
  late bool isConnected;

  @override
  void initState() {
    super.initState();
    holdHeading = widget.holdHeading;
    emuCompass = widget.emuCompass;
    stepSpr = widget.stepSpr;
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

  void _toggleEmu(bool v) {
    setState(() => emuCompass = v);
    widget.ble
        .sendCustomCommand('EMU_COMPASS:${v ? 1 : 0}');
  }

  Future<void> _editStepSpr() async {
    if (!isConnected) return;
    const options = [200, 400, 800, 1600, 3200];
    final selected = await showDialog<int>(
      context: context,
      builder: (_) => SimpleDialog(
        title: const Text('Шагов за оборот'),
        children: options
            .map((v) => SimpleDialogOption(
                  child: Text(v.toString()),
                  onPressed: () => Navigator.pop(context, v),
                ))
            .toList(),
      ),
    );
    if (selected != null && selected != stepSpr) {
      setState(() => stepSpr = selected);
      widget.ble.sendCustomCommand('SET_STEP_SPR:$selected');
    }
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
          SwitchListTile(
            title: const Text('Эмулировать компас'),
            value: emuCompass,
            onChanged: _toggleEmu,
          ),
          ListTile(
            title: const Text('Шагов за оборот'),
            subtitle: Text(stepSpr.toString()),
            trailing: const Icon(Icons.chevron_right),
            enabled: isConnected,
            onTap: isConnected ? _editStepSpr : null,
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
