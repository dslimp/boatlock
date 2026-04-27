import 'dart:async';

import 'package:flutter/material.dart';

typedef ManualControlSender =
    Future<bool> Function({
      required int steer,
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
  static const int _forwardPct = 35;
  static const int _reversePct = -25;

  Timer? _sendTimer;
  bool _sendInFlight = false;
  int _steer = 0;
  int _throttlePct = 0;
  String _status = 'Удерживайте кнопку для движения';

  @override
  void dispose() {
    _sendTimer?.cancel();
    if (_steer != 0 || _throttlePct != 0) {
      unawaited(widget.onManualOff());
    }
    super.dispose();
  }

  void _setSteer(int value, bool pressed) {
    setState(() {
      _steer = pressed ? value : 0;
    });
    _syncManualState();
  }

  void _setThrottle(int value, bool pressed) {
    setState(() {
      _throttlePct = pressed ? value : 0;
    });
    _syncManualState();
  }

  void _syncManualState() {
    if (!widget.enabled) return;
    if (_steer == 0 && _throttlePct == 0) {
      _sendTimer?.cancel();
      _sendTimer = null;
      _sendManualOff();
      return;
    }
    _sendManualSet();
    _sendTimer ??= Timer.periodic(_sendEvery, (_) => _sendManualSet());
  }

  Future<void> _sendManualSet() async {
    if (_sendInFlight || !widget.enabled) return;
    _sendInFlight = true;
    final steer = _steer;
    final throttlePct = _throttlePct;
    final ok = await widget.onManualControl(
      steer: steer,
      throttlePct: throttlePct,
      ttlMs: _ttlMs,
    );
    if (mounted) {
      setState(() {
        _status = ok
            ? 'MANUAL_SET steer=$steer thrust=$throttlePct%'
            : 'MANUAL_SET отклонен';
      });
    }
    _sendInFlight = false;
  }

  Future<void> _sendManualOff() async {
    final ok = await widget.onManualOff();
    if (!mounted) return;
    setState(() {
      _status = ok ? 'MANUAL_OFF отправлен' : 'MANUAL_OFF отклонен';
    });
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
            const SizedBox(height: 8),
            Text(
              'Deadman: движение только пока кнопка удерживается. Отпускание останавливает ручной режим.',
              style: theme.textTheme.bodyMedium,
            ),
            const SizedBox(height: 16),
            _ManualPadButton(
              label: 'Вперед',
              icon: Icons.keyboard_arrow_up,
              enabled: enabled,
              onPressedChanged: (pressed) => _setThrottle(_forwardPct, pressed),
            ),
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                _ManualPadButton(
                  label: 'Лево',
                  icon: Icons.keyboard_arrow_left,
                  enabled: enabled,
                  onPressedChanged: (pressed) => _setSteer(-1, pressed),
                ),
                Padding(
                  padding: const EdgeInsets.all(10),
                  child: FilledButton(
                    style: FilledButton.styleFrom(
                      backgroundColor: Colors.orange.shade700,
                      foregroundColor: Colors.white,
                      minimumSize: const Size(86, 56),
                    ),
                    onPressed: enabled ? _sendManualOff : null,
                    child: const FittedBox(child: Text('MANUAL OFF')),
                  ),
                ),
                _ManualPadButton(
                  label: 'Право',
                  icon: Icons.keyboard_arrow_right,
                  enabled: enabled,
                  onPressedChanged: (pressed) => _setSteer(1, pressed),
                ),
              ],
            ),
            _ManualPadButton(
              label: 'Назад',
              icon: Icons.keyboard_arrow_down,
              enabled: enabled,
              onPressedChanged: (pressed) => _setThrottle(_reversePct, pressed),
            ),
            const SizedBox(height: 12),
            Text(
              enabled ? _status : 'Нет BLE-связи',
              style: theme.textTheme.bodySmall,
            ),
          ],
        ),
      ),
    );
  }
}

class _ManualPadButton extends StatelessWidget {
  const _ManualPadButton({
    required this.label,
    required this.icon,
    required this.enabled,
    required this.onPressedChanged,
  });

  final String label;
  final IconData icon;
  final bool enabled;
  final ValueChanged<bool> onPressedChanged;

  @override
  Widget build(BuildContext context) {
    final child = SizedBox(
      width: 96,
      height: 64,
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: enabled
              ? Theme.of(context).colorScheme.secondaryContainer
              : Colors.black12,
          borderRadius: BorderRadius.circular(18),
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [Icon(icon), Text(label)],
        ),
      ),
    );

    if (!enabled) {
      return Padding(padding: const EdgeInsets.all(6), child: child);
    }

    return Padding(
      padding: const EdgeInsets.all(6),
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTapDown: (_) => onPressedChanged(true),
        onTapUp: (_) => onPressedChanged(false),
        onTapCancel: () => onPressedChanged(false),
        child: child,
      ),
    );
  }
}
