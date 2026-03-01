import 'package:flutter/material.dart';
import '../ble/ble_boatlock.dart';

class SettingsPage extends StatefulWidget {
  final BleBoatLock ble;
  final bool holdHeading;
  final int stepSpr;
  final double stepMaxSpd;
  final double stepAccel;
  final double compassOffset;
  final int compassQ;
  final int magQ;
  final int gyroQ;
  final double rvAcc;
  final double magNorm;
  final double gyroNorm;
  final double pitch;
  final double roll;
  final bool isConnected;
  const SettingsPage({
    super.key,
    required this.ble,
    required this.holdHeading,
    required this.stepSpr,
    required this.stepMaxSpd,
    required this.stepAccel,
    required this.compassOffset,
    required this.compassQ,
    required this.magQ,
    required this.gyroQ,
    required this.rvAcc,
    required this.magNorm,
    required this.gyroNorm,
    required this.pitch,
    required this.roll,
    required this.isConnected,
  });

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  late bool holdHeading;
  late int stepSpr;
  late double stepMaxSpd;
  late double stepAccel;
  late double compassOffset;
  late int compassQ;
  late int magQ;
  late int gyroQ;
  late double rvAcc;
  late double magNorm;
  late double gyroNorm;
  late double pitch;
  late double roll;
  late bool isConnected;

  @override
  void initState() {
    super.initState();
    holdHeading = widget.holdHeading;
    stepSpr = widget.stepSpr;
    stepMaxSpd = widget.stepMaxSpd;
    stepAccel = widget.stepAccel;
    compassOffset = widget.compassOffset;
    compassQ = widget.compassQ;
    magQ = widget.magQ;
    gyroQ = widget.gyroQ;
    rvAcc = widget.rvAcc;
    magNorm = widget.magNorm;
    gyroNorm = widget.gyroNorm;
    pitch = widget.pitch;
    roll = widget.roll;
    isConnected = widget.isConnected;
  }

  void _toggleHoldHeading(bool v) {
    if (!isConnected) return;
    setState(() => holdHeading = v);
    widget.ble.sendCustomCommand('SET_HOLD_HEADING:${v ? 1 : 0}');
  }

  Future<void> _editCompassOffset() async {
    if (!isConnected) return;
    final ctrl = TextEditingController(text: compassOffset.toStringAsFixed(1));
    final val = await showDialog<double>(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Офсет компаса (°)'),
        content: TextField(
          controller: ctrl,
          keyboardType: const TextInputType.numberWithOptions(signed: true),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Отмена'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, double.tryParse(ctrl.text)),
            child: const Text('OK'),
          ),
        ],
      ),
    );
    if (val == null) return;
    setState(() => compassOffset = val);
    widget.ble.sendCustomCommand(
      'SET_COMPASS_OFFSET:${val.toStringAsFixed(1)}',
    );
  }

  void _resetCompassOffset() {
    if (!isConnected) return;
    setState(() => compassOffset = 0.0);
    widget.ble.sendCustomCommand('RESET_COMPASS_OFFSET');
  }

  Future<void> _editStepSpr() async {
    if (!isConnected) return;
    setState(() => stepSpr = 4096);
    widget.ble.sendCustomCommand('SET_STEP_SPR:4096');
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Для 28BYJ шагов/оборот фиксировано: 4096')),
    );
  }

  Future<void> _editStepMaxSpd() async {
    if (!isConnected) return;
    final ctrl = TextEditingController(text: stepMaxSpd.round().toString());
    final val = await showDialog<double>(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Макс. скорость'),
        content: TextField(
          controller: ctrl,
          keyboardType: TextInputType.number,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Отмена'),
          ),
          TextButton(
            onPressed: () {
              Navigator.pop(context, double.tryParse(ctrl.text));
            },
            child: const Text('OK'),
          ),
        ],
      ),
    );
    if (val != null && val != stepMaxSpd) {
      setState(() => stepMaxSpd = val);
      widget.ble.sendCustomCommand('SET_STEP_MAXSPD:${val.round()}');
    }
  }

  Future<void> _editStepAccel() async {
    if (!isConnected) return;
    final ctrl = TextEditingController(text: stepAccel.round().toString());
    final val = await showDialog<double>(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Ускорение'),
        content: TextField(
          controller: ctrl,
          keyboardType: TextInputType.number,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Отмена'),
          ),
          TextButton(
            onPressed: () {
              Navigator.pop(context, double.tryParse(ctrl.text));
            },
            child: const Text('OK'),
          ),
        ],
      ),
    );
    if (val != null && val != stepAccel) {
      setState(() => stepAccel = val);
      widget.ble.sendCustomCommand('SET_STEP_ACCEL:${val.round()}');
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
          ListTile(
            title: const Text('Шагов за оборот'),
            subtitle: Text('$stepSpr (фиксировано для 28BYJ)'),
            trailing: const Icon(Icons.lock),
            enabled: isConnected,
            onTap: isConnected ? _editStepSpr : null,
          ),
          ListTile(
            title: const Text('Макс. скорость'),
            subtitle: Text(stepMaxSpd.round().toString()),
            trailing: const Icon(Icons.chevron_right),
            enabled: isConnected,
            onTap: isConnected ? _editStepMaxSpd : null,
          ),
          ListTile(
            title: const Text('Ускорение'),
            subtitle: Text(stepAccel.round().toString()),
            trailing: const Icon(Icons.chevron_right),
            enabled: isConnected,
            onTap: isConnected ? _editStepAccel : null,
          ),
          const Divider(),
          ListTile(
            title: const Text('Офсет компаса'),
            subtitle: Text('${compassOffset.toStringAsFixed(1)}°'),
            trailing: const Icon(Icons.chevron_right),
            enabled: isConnected,
            onTap: isConnected ? _editCompassOffset : null,
          ),
          ListTile(
            title: const Text('Сбросить офсет'),
            enabled: isConnected,
            onTap: isConnected ? _resetCompassOffset : null,
          ),
          const Divider(),
          ListTile(
            title: const Text('BNO08x quality'),
            subtitle: Text('RV=$compassQ MAG=$magQ GYR=$gyroQ'),
          ),
          ListTile(
            title: const Text('RV accuracy'),
            subtitle: Text('${rvAcc.toStringAsFixed(2)}°'),
          ),
          ListTile(
            title: const Text('Mag / Gyro'),
            subtitle: Text(
              '|B|=${magNorm.toStringAsFixed(1)} uT, |w|=${gyroNorm.toStringAsFixed(1)} dps',
            ),
          ),
          ListTile(
            title: const Text('Pitch / Roll'),
            subtitle: Text(
              '${pitch.toStringAsFixed(1)}° / ${roll.toStringAsFixed(1)}°',
            ),
          ),
        ],
      ),
    );
  }
}
