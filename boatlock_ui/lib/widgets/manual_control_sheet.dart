import 'dart:async';

import 'package:flutter/material.dart';

typedef ManualControlSender =
    Future<bool> Function({
      required double angleDeg,
      required int throttlePct,
      int ttlMs,
    });
typedef ManualOffSender = Future<bool> Function();

class ManualControlSheet extends StatefulWidget {
  const ManualControlSheet({
    super.key,
    required this.enabled,
    required this.mode,
    required this.onManualControl,
    required this.onManualOff,
  });

  final bool enabled;
  final String mode;
  final ManualControlSender onManualControl;
  final ManualOffSender onManualOff;

  @override
  State<ManualControlSheet> createState() => _ManualControlSheetState();
}

class _ManualControlSheetState extends State<ManualControlSheet> {
  static const int _ttlMs = 1000;
  static const Duration _sendEvery = Duration(milliseconds: 250);
  static const double _angleStepDeg = 10.0;
  static const List<int> _speedSteps = [0, 20, 35, 50, 70, 100];

  Timer? _sendTimer;
  bool _sendInFlight = false;
  bool _manualActive = false;
  int _commandGeneration = 0;
  double _angleDeg = 0.0;
  int _speedIndex = 0;
  String _status = 'Готово';

  int get _throttlePct => _speedSteps[_speedIndex];
  bool get _hasManualTarget => _angleDeg != 0.0 || _throttlePct != 0;

  @override
  void didUpdateWidget(covariant ManualControlSheet oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.enabled && !widget.enabled) {
      _sendTimer?.cancel();
      _sendTimer = null;
      _manualActive = false;
      _commandGeneration++;
    }
  }

  @override
  void dispose() {
    _sendTimer?.cancel();
    if (_manualActive || _hasManualTarget) {
      unawaited(widget.onManualOff());
    }
    super.dispose();
  }

  void _setAngle(double value) {
    if (!widget.enabled) return;
    setState(() {
      _angleDeg = value.clamp(-90.0, 90.0).roundToDouble();
    });
    _scheduleSync();
  }

  void _changeAngle(double delta) {
    _setAngle(_angleDeg + delta);
  }

  void _changeSpeed(int delta) {
    if (!widget.enabled) return;
    setState(() {
      _speedIndex = (_speedIndex + delta)
          .clamp(0, _speedSteps.length - 1)
          .toInt();
    });
    _scheduleSync();
  }

  void _scheduleSync() {
    if (!widget.enabled) return;
    _commandGeneration++;
    if (!_hasManualTarget) {
      _sendTimer?.cancel();
      _sendTimer = null;
      if (_manualActive) {
        unawaited(_sendManualOff());
      }
      return;
    }
    _manualActive = true;
    unawaited(_sendManualTarget());
    _sendTimer ??= Timer.periodic(_sendEvery, (_) => _sendManualTarget());
  }

  Future<void> _sendManualTarget() async {
    if (_sendInFlight || !widget.enabled || !_hasManualTarget) return;
    _sendInFlight = true;
    final generation = _commandGeneration;
    final angleDeg = _angleDeg;
    final throttlePct = _throttlePct;
    final ok = await widget.onManualControl(
      angleDeg: angleDeg,
      throttlePct: throttlePct,
      ttlMs: _ttlMs,
    );
    _sendInFlight = false;
    if (!mounted || generation != _commandGeneration) return;
    setState(() {
      _status = ok
          ? 'angle=${angleDeg.toStringAsFixed(0)} pwm=$throttlePct%'
          : 'MANUAL_TARGET отклонен';
    });
  }

  Future<void> _sendManualOff() async {
    _commandGeneration++;
    _sendTimer?.cancel();
    _sendTimer = null;
    _manualActive = false;
    final ok = await widget.onManualOff();
    if (!mounted) return;
    setState(() {
      _status = ok ? 'Остановлено' : 'MANUAL_OFF отклонен';
    });
  }

  void _stopManual() {
    if (!widget.enabled) return;
    setState(() {
      _angleDeg = 0.0;
      _speedIndex = 0;
    });
    unawaited(_sendManualOff());
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final enabled = widget.enabled;
    return SafeArea(
      child: Padding(
        padding: const EdgeInsets.fromLTRB(20, 16, 20, 24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                const Icon(Icons.sports_esports),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    'Ручное управление',
                    style: theme.textTheme.titleLarge,
                  ),
                ),
                Chip(label: Text(widget.mode.isEmpty ? 'IDLE' : widget.mode)),
              ],
            ),
            const SizedBox(height: 14),
            Row(
              children: [
                Text('Угол', style: theme.textTheme.titleMedium),
                const Spacer(),
                Text(
                  '${_angleDeg.toStringAsFixed(0)}°',
                  style: theme.textTheme.titleMedium,
                ),
              ],
            ),
            Slider(
              value: _angleDeg,
              min: -90,
              max: 90,
              divisions: 18,
              label: '${_angleDeg.toStringAsFixed(0)}°',
              onChanged: enabled ? _setAngle : null,
            ),
            Row(
              children: [
                Expanded(
                  child: OutlinedButton.icon(
                    onPressed: enabled
                        ? () => _changeAngle(-_angleStepDeg)
                        : null,
                    icon: const Icon(Icons.keyboard_arrow_left),
                    label: const Text('Левее'),
                  ),
                ),
                const SizedBox(width: 8),
                IconButton.filledTonal(
                  tooltip: 'Центр',
                  onPressed: enabled ? () => _setAngle(0.0) : null,
                  icon: const Icon(Icons.adjust),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: OutlinedButton.icon(
                    onPressed: enabled
                        ? () => _changeAngle(_angleStepDeg)
                        : null,
                    icon: const Icon(Icons.keyboard_arrow_right),
                    label: const Text('Правее'),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 18),
            Row(
              children: [
                Text('PWM', style: theme.textTheme.titleMedium),
                const Spacer(),
                Text('$_throttlePct%', style: theme.textTheme.titleMedium),
              ],
            ),
            const SizedBox(height: 8),
            Row(
              children: [
                Expanded(
                  child: OutlinedButton.icon(
                    onPressed: enabled ? () => _changeSpeed(-1) : null,
                    icon: const Icon(Icons.remove),
                    label: const Text('Медленнее'),
                  ),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: OutlinedButton.icon(
                    onPressed: enabled ? () => _changeSpeed(1) : null,
                    icon: const Icon(Icons.add),
                    label: const Text('Быстрее'),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 14),
            FilledButton.icon(
              style: FilledButton.styleFrom(
                backgroundColor: Colors.orange.shade700,
                foregroundColor: Colors.white,
                minimumSize: const Size.fromHeight(48),
              ),
              onPressed: enabled ? _stopManual : null,
              icon: const Icon(Icons.stop_circle_outlined),
              label: const Text('Стоп'),
            ),
            const SizedBox(height: 10),
            Text(
              enabled ? _status : 'Нет BLE-связи',
              style: theme.textTheme.bodySmall,
              textAlign: TextAlign.center,
            ),
          ],
        ),
      ),
    );
  }
}
