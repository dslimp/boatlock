import 'package:flutter/material.dart';
import '../ble/ble_boatlock.dart';

class SettingsPage extends StatefulWidget {
  final BleBoatLock ble;
  final bool holdHeading;
  final bool emuCompass;
  final bool isConnected;
  final double stepMaxSpd;
  final double stepAccel;
  final int stepSpr;
  const SettingsPage({Key? key, required this.ble, required this.holdHeading, required this.emuCompass, required this.isConnected, required this.stepMaxSpd, required this.stepAccel, required this.stepSpr}) : super(key: key);

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  late bool holdHeading;
  late bool emuCompass;
  late bool isConnected;
  late double stepMaxSpd;
  late double stepAccel;
  late int stepSpr;

  @override
  void initState() {
    super.initState();
    holdHeading = widget.holdHeading;
    emuCompass = widget.emuCompass;
    isConnected = widget.isConnected;
    stepMaxSpd = widget.stepMaxSpd;
    stepAccel = widget.stepAccel;
    stepSpr = widget.stepSpr;
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

  void _editStepMaxSpd() async {
    await _editNumber('Макс. скорость шагов', stepMaxSpd, (v) {
      setState(() => stepMaxSpd = v);
      widget.ble.sendCustomCommand('SET_STEP_MAXSPD:${v.toStringAsFixed(0)}');
    });
  }

  void _editStepAccel() async {
    await _editNumber('Ускорение шагов', stepAccel, (v) {
      setState(() => stepAccel = v);
      widget.ble.sendCustomCommand('SET_STEP_ACCEL:${v.toStringAsFixed(0)}');
    });
  }

  Future<void> _editNumber(String title, double current, Function(double) onSave) async {
    final ctrl = TextEditingController(text: current.toStringAsFixed(0));
    await showDialog(
      context: context,
      builder: (_) => AlertDialog(
        title: Text(title),
        content: TextField(
          controller: ctrl,
          keyboardType: TextInputType.number,
        ),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(context), child: const Text('Отмена')),
          TextButton(
            onPressed: () {
              final v = double.tryParse(ctrl.text) ?? current;
              onSave(v);
              Navigator.pop(context);
            },
            child: const Text('OK'),
          )
        ],
      ),
    );
  }

  void _editStepSpr() async {
    final ctrl = TextEditingController(text: stepSpr.toString());
    await showDialog(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Шагов за оборот'),
        content: TextField(
          controller: ctrl,
          keyboardType: TextInputType.number,
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context), child: const Text('Отмена')),
          TextButton(
            onPressed: () {
              final v = int.tryParse(ctrl.text) ?? stepSpr;
              setState(() => stepSpr = v);
              widget.ble.sendCustomCommand('SET_STEP_SPR:$v');
              Navigator.pop(context);
            },
            child: const Text('OK'),
          )
        ],
      ),
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
          SwitchListTile(
            title: const Text('Эмулировать компас'),
            value: emuCompass,
            onChanged: _toggleEmu,
          ),
          ListTile(
            title: const Text('Калибровка компаса'),
            trailing: const Icon(Icons.compass_calibration),
            enabled: isConnected,
            onTap: isConnected ? _startCalib : null,
          ),
          ListTile(
            title: const Text('Макс. скорость шагов'),
            subtitle: Text(stepMaxSpd.toStringAsFixed(0)),
            onTap: isConnected ? _editStepMaxSpd : null,
          ),
          ListTile(
            title: const Text('Ускорение шагов'),
            subtitle: Text(stepAccel.toStringAsFixed(0)),
            onTap: isConnected ? _editStepAccel : null,
          ),
          ListTile(
            title: const Text('Шагов за оборот'),
            subtitle: Text(stepSpr.toString()),
            onTap: isConnected ? _editStepSpr : null,
          ),
        ],
      ),
    );
  }
}
