import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../ble/ble_boatlock.dart';
import '../ble/ble_debug_snapshot.dart';
import '../models/boat_data.dart';

class DiagnosticsPage extends StatelessWidget {
  const DiagnosticsPage({super.key, required this.ble, required this.data});

  final BleBoatLock ble;
  final BoatData? data;

  @override
  Widget build(BuildContext context) {
    return ValueListenableBuilder<BleDebugSnapshot>(
      valueListenable: ble.diagnostics,
      builder: (context, snapshot, _) {
        return Scaffold(
          appBar: AppBar(
            title: const Text('Диагностика'),
            actions: [
              IconButton(
                icon: const Icon(Icons.copy),
                tooltip: 'Скопировать',
                onPressed: () => _copySnapshot(context, snapshot),
              ),
              IconButton(
                icon: const Icon(Icons.refresh),
                tooltip: 'Snapshot',
                onPressed: ble.requestSnapshot,
              ),
            ],
          ),
          body: ListView(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 24),
            children: [
              _Section(
                title: 'BLE',
                children: [
                  _KV('Adapter', snapshot.adapterState),
                  _KV('Connection', snapshot.connectionState),
                  _KV('Scanning', snapshot.isScanning ? 'YES' : 'NO'),
                  _KV('Device', _deviceText(snapshot)),
                  _KV(
                    'RSSI',
                    snapshot.rssi == 0 ? '-' : '${snapshot.rssi} dBm',
                  ),
                  _KV('MTU', snapshot.mtu == 0 ? '-' : snapshot.mtu.toString()),
                  _KV('Core chars', snapshot.coreReady ? 'OK' : 'MISSING'),
                  _KV('OTA char', snapshot.otaReady ? 'OK' : 'MISSING'),
                  _KV('Connected for', _age(snapshot.connectedAt)),
                ],
              ),
              _Section(
                title: 'Telemetry',
                children: [
                  _KV('Data events', snapshot.dataEvents.toString()),
                  _KV('Last data', _age(snapshot.lastDataAt)),
                  _KV('Mode', data?.mode ?? '-'),
                  _KV('Status', data?.status ?? '-'),
                  _KV('Reasons', data?.statusReasons ?? '-'),
                  _KV('GNSS Q', data?.gnssQ.toString() ?? '-'),
                  _KV(
                    'Position',
                    data == null
                        ? '-'
                        : '${data!.lat.toStringAsFixed(6)}, ${data!.lon.toStringAsFixed(6)}',
                  ),
                  _KV('Security', _securityText(data)),
                ],
              ),
              _Section(
                title: 'Runtime',
                children: [
                  _KV('Device logs', snapshot.deviceLogEvents.toString()),
                  _KV('App logs', snapshot.appLogEvents.toString()),
                  _KV('Last device log', _age(snapshot.lastDeviceLogAt)),
                  _KV('Last app log', _age(snapshot.lastAppLogAt)),
                  if (snapshot.lastError.isNotEmpty)
                    _KV('Last error', snapshot.lastError),
                ],
              ),
              _LogSection(title: 'Device log', lines: snapshot.deviceLogLines),
              _LogSection(title: 'App log', lines: snapshot.appLogLines),
            ],
          ),
        );
      },
    );
  }

  String _deviceText(BleDebugSnapshot snapshot) {
    if (snapshot.deviceId.isEmpty && snapshot.deviceName.isEmpty) {
      return '-';
    }
    if (snapshot.deviceName.isEmpty) {
      return snapshot.deviceId;
    }
    if (snapshot.deviceId.isEmpty) {
      return snapshot.deviceName;
    }
    return '${snapshot.deviceName} / ${snapshot.deviceId}';
  }

  String _securityText(BoatData? data) {
    if (data == null) return '-';
    final paired = data.secPaired ? 'paired' : 'open';
    final auth = data.secAuth ? 'auth' : 'no-auth';
    final pairWindow = data.secPairWindowOpen ? 'pair-window' : 'closed';
    return '$paired, $auth, $pairWindow, reject=${data.secReject}';
  }

  String _age(DateTime? at) {
    if (at == null) return '-';
    final elapsed = DateTime.now().difference(at);
    if (elapsed.inSeconds < 2) return 'now';
    if (elapsed.inMinutes < 1) return '${elapsed.inSeconds}s';
    if (elapsed.inHours < 1) {
      return '${elapsed.inMinutes}m ${elapsed.inSeconds % 60}s';
    }
    return '${elapsed.inHours}h ${elapsed.inMinutes % 60}m';
  }

  Future<void> _copySnapshot(
    BuildContext context,
    BleDebugSnapshot snapshot,
  ) async {
    final lines = <String>[
      'adapter=${snapshot.adapterState}',
      'connection=${snapshot.connectionState}',
      'device=${_deviceText(snapshot)}',
      'rssi=${snapshot.rssi}',
      'mtu=${snapshot.mtu}',
      'coreReady=${snapshot.coreReady}',
      'otaReady=${snapshot.otaReady}',
      'dataEvents=${snapshot.dataEvents}',
      'deviceLogEvents=${snapshot.deviceLogEvents}',
      'appLogEvents=${snapshot.appLogEvents}',
      'mode=${data?.mode ?? '-'}',
      'status=${data?.status ?? '-'}',
      'reasons=${data?.statusReasons ?? '-'}',
      'gnssQ=${data?.gnssQ ?? '-'}',
      if (snapshot.lastError.isNotEmpty) 'lastError=${snapshot.lastError}',
    ];
    await Clipboard.setData(ClipboardData(text: lines.join('\n')));
    if (!context.mounted) return;
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(const SnackBar(content: Text('Диагностика скопирована')));
  }
}

class _Section extends StatelessWidget {
  const _Section({required this.title, required this.children});

  final String title;
  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 14),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(title, style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 6),
          DecoratedBox(
            decoration: BoxDecoration(
              border: Border.all(color: Theme.of(context).dividerColor),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Column(children: children),
          ),
        ],
      ),
    );
  }
}

class _KV extends StatelessWidget {
  const _KV(this.label, this.value);

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 116,
            child: Text(label, style: Theme.of(context).textTheme.bodySmall),
          ),
          Expanded(
            child: SelectableText(
              value,
              style: Theme.of(context).textTheme.bodyMedium,
            ),
          ),
        ],
      ),
    );
  }
}

class _LogSection extends StatelessWidget {
  const _LogSection({required this.title, required this.lines});

  final String title;
  final List<String> lines;

  @override
  Widget build(BuildContext context) {
    final text = lines.isEmpty ? '-' : lines.reversed.take(40).join('\n');
    return Padding(
      padding: const EdgeInsets.only(bottom: 14),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(title, style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 6),
          DecoratedBox(
            decoration: BoxDecoration(
              color: Theme.of(context).colorScheme.surfaceContainerHighest,
              borderRadius: BorderRadius.circular(8),
            ),
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: SelectableText(
                text,
                style: const TextStyle(
                  fontFamily: 'monospace',
                  fontSize: 12,
                  height: 1.25,
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
