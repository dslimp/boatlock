import '../ble/ble_command_rejection.dart';
import '../ble/ble_debug_snapshot.dart';

BleCommandRejection? findMatchingCommandRejectionAfter(
  BleDebugSnapshot snapshot, {
  required int baselineEvents,
  required String commandPrefix,
}) {
  return findAnyMatchingCommandRejectionAfter(
    snapshot,
    baselineEvents: baselineEvents,
    commandPrefixes: [commandPrefix],
  );
}

BleCommandRejection? findAnyMatchingCommandRejectionAfter(
  BleDebugSnapshot snapshot, {
  required int baselineEvents,
  required Iterable<String> commandPrefixes,
}) {
  final newEvents = snapshot.commandRejectEvents - baselineEvents;
  if (newEvents <= 0) return null;
  final prefixes = commandPrefixes.toList(growable: false);
  if (prefixes.isEmpty) return null;

  final rejects = snapshot.commandRejects;
  var firstNewIndex = rejects.length - newEvents;
  if (firstNewIndex < 0) firstNewIndex = 0;

  for (var i = rejects.length - 1; i >= firstNewIndex; i -= 1) {
    final rejection = rejects[i];
    if (prefixes.any(rejection.matchesCommandPrefix)) {
      return rejection;
    }
  }

  final last = snapshot.lastCommandRejection;
  if (last != null && prefixes.any(last.matchesCommandPrefix)) {
    return last;
  }
  return null;
}
