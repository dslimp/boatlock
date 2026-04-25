import 'package:flutter/material.dart';
import '../ble/ble_boatlock.dart';
import '../ble/ble_security_codec.dart';

class SettingsPage extends StatefulWidget {
  final BleBoatLock ble;
  final bool holdHeading;
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
  final bool secPaired;
  final bool secAuth;
  final bool secPairWindowOpen;
  final String secReject;

  const SettingsPage({
    super.key,
    required this.ble,
    required this.holdHeading,
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
    required this.secPaired,
    required this.secAuth,
    required this.secPairWindowOpen,
    required this.secReject,
  });

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  late bool holdHeading;
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
  late bool secPaired;
  late bool secAuth;
  late bool secPairWindowOpen;
  late String secReject;
  late final TextEditingController _ownerSecretCtrl;

  @override
  void initState() {
    super.initState();
    holdHeading = widget.holdHeading;
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
    secPaired = widget.secPaired;
    secAuth = widget.secAuth;
    secPairWindowOpen = widget.secPairWindowOpen;
    secReject = widget.secReject;
    _ownerSecretCtrl = TextEditingController(
      text: widget.ble.ownerSecret ?? '',
    );
    widget.ble.setOwnerSecret(_ownerSecretCtrl.text);
  }

  @override
  void dispose() {
    _ownerSecretCtrl.dispose();
    super.dispose();
  }

  void _showMessage(String message) {
    if (!mounted) return;
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text(message)));
  }

  void _syncOwnerSecret() {
    widget.ble.setOwnerSecret(_ownerSecretCtrl.text);
  }

  Future<void> _toggleHoldHeading(bool value) async {
    if (!isConnected) return;
    final previous = holdHeading;
    setState(() => holdHeading = value);
    final ok = await widget.ble.setHoldHeading(value);
    if (!ok) {
      setState(() => holdHeading = previous);
      _showMessage('Изменение отклонено: ${widget.ble.secReject}');
    }
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
    final previous = compassOffset;
    setState(() => compassOffset = val);
    final ok = await widget.ble.setCompassOffset(val);
    if (!ok) {
      setState(() => compassOffset = previous);
      _showMessage('Команда отклонена: ${widget.ble.secReject}');
    }
  }

  Future<void> _resetCompassOffset() async {
    if (!isConnected) return;
    final previous = compassOffset;
    setState(() => compassOffset = 0.0);
    final ok = await widget.ble.resetCompassOffset();
    if (!ok) {
      setState(() => compassOffset = previous);
      _showMessage('Команда отклонена: ${widget.ble.secReject}');
    }
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
            onPressed: () => Navigator.pop(context, double.tryParse(ctrl.text)),
            child: const Text('OK'),
          ),
        ],
      ),
    );
    if (val == null || val == stepMaxSpd) return;
    final previous = stepMaxSpd;
    setState(() => stepMaxSpd = val);
    final ok = await widget.ble.setStepMaxSpeed(val);
    if (!ok) {
      setState(() => stepMaxSpd = previous);
      _showMessage('Команда отклонена: ${widget.ble.secReject}');
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
            onPressed: () => Navigator.pop(context, double.tryParse(ctrl.text)),
            child: const Text('OK'),
          ),
        ],
      ),
    );
    if (val == null || val == stepAccel) return;
    final previous = stepAccel;
    setState(() => stepAccel = val);
    final ok = await widget.ble.setStepAccel(val);
    if (!ok) {
      setState(() => stepAccel = previous);
      _showMessage('Команда отклонена: ${widget.ble.secReject}');
    }
  }

  void _generateOwnerSecret() {
    final secret = widget.ble.generateOwnerSecret();
    _ownerSecretCtrl.text = secret;
    _syncOwnerSecret();
    _showMessage('Новый owner secret сгенерирован');
  }

  Future<void> _pairDevice() async {
    if (!isConnected) return;
    _syncOwnerSecret();
    if (normalizeOwnerSecret(_ownerSecretCtrl.text) == null) {
      _showMessage('Нужен owner secret из 32 hex-символов');
      return;
    }
    final ok = await widget.ble.pairWithOwnerSecret(_ownerSecretCtrl.text);
    setState(() {
      secPaired = ok;
      secAuth = false;
      secReject = ok ? 'NONE' : widget.ble.secReject;
      secPairWindowOpen = widget.ble.secPairWindowOpen;
    });
    _showMessage(
      ok
          ? 'Пара привязана. Owner secret нужен для последующей авторизации.'
          : 'Привязка не прошла: ${widget.ble.secReject}',
    );
  }

  Future<void> _authenticateOwner() async {
    if (!isConnected) return;
    _syncOwnerSecret();
    final ok = await widget.ble.authenticateOwner(_ownerSecretCtrl.text);
    setState(() {
      secAuth = ok;
      secPaired = widget.ble.secPaired;
      secReject = ok ? 'NONE' : widget.ble.secReject;
    });
    _showMessage(
      ok ? 'Owner auth выполнен' : 'Auth не прошёл: ${widget.ble.secReject}',
    );
  }

  Future<void> _clearPairing() async {
    if (!isConnected) return;
    final ok = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Сбросить pairing'),
        content: const Text(
          'Сброс будет принят только из owner-session или пока открыт pairing window.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Отмена'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Сбросить'),
          ),
        ],
      ),
    );
    if (ok != true) return;

    _syncOwnerSecret();
    final cleared = await widget.ble.clearPairing();
    setState(() {
      secPaired = !cleared;
      secAuth = false;
      secReject = cleared ? 'NONE' : widget.ble.secReject;
    });
    _showMessage(
      cleared ? 'Pairing сброшен' : 'Сброс отклонён: ${widget.ble.secReject}',
    );
  }

  Widget _securityTile(String label, String value) {
    return ListTile(dense: true, title: Text(label), trailing: Text(value));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Настройки')),
      body: ListView(
        padding: const EdgeInsets.only(bottom: 24),
        children: [
          SwitchListTile(
            title: const Text('Поддерживать курс носа'),
            value: holdHeading,
            onChanged: isConnected ? _toggleHoldHeading : null,
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
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 8, 16, 4),
            child: Text(
              'Security',
              style: Theme.of(context).textTheme.titleMedium,
            ),
          ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16),
            child: TextField(
              controller: _ownerSecretCtrl,
              enabled: isConnected,
              autocorrect: false,
              enableSuggestions: false,
              textCapitalization: TextCapitalization.characters,
              onChanged: (_) => _syncOwnerSecret(),
              decoration: const InputDecoration(
                labelText: 'Owner secret',
                hintText: '32 HEX символа',
                helperText:
                    'Сгенерируй secret, затем открой pairing window кнопкой STOP на устройстве.',
              ),
            ),
          ),
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 8, 16, 0),
            child: Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                OutlinedButton(
                  onPressed: isConnected ? _generateOwnerSecret : null,
                  child: const Text('Сгенерировать'),
                ),
                FilledButton(
                  onPressed: isConnected ? _pairDevice : null,
                  child: const Text('Привязать'),
                ),
                OutlinedButton(
                  onPressed: isConnected ? _authenticateOwner : null,
                  child: const Text('Авторизоваться'),
                ),
                OutlinedButton(
                  onPressed: isConnected ? _clearPairing : null,
                  child: const Text('Сбросить pairing'),
                ),
              ],
            ),
          ),
          _securityTile('Paired', secPaired ? 'YES' : 'NO'),
          _securityTile('Auth', secAuth ? 'YES' : 'NO'),
          _securityTile('Pair window', secPairWindowOpen ? 'OPEN' : 'CLOSED'),
          _securityTile('Last reject', secReject),
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
