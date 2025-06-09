import 'package:flutter/foundation.dart';

class LogService extends ChangeNotifier {
  final List<String> _logs = [];

  List<String> get logs => List.unmodifiable(_logs);

  void add(String line) {
    _logs.add(line);
    if (_logs.length > 200) _logs.removeAt(0); // ограничим, чтобы не захламлять память
    notifyListeners();
  }

  void clear() {
    _logs.clear();
    notifyListeners();
  }
}

// Глобальный синглтон (просто для примера)
final logService = LogService();
