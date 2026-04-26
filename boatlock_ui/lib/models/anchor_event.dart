enum AnchorEventLevel { info, warning, critical }

enum AnchorEventSource { app, telemetry, device }

class AnchorEvent {
  final DateTime timestamp;
  final String title;
  final String detail;
  final AnchorEventLevel level;
  final AnchorEventSource source;

  const AnchorEvent({
    required this.timestamp,
    required this.title,
    required this.detail,
    required this.level,
    required this.source,
  });
}
