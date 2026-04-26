import '../ble/ble_command_rejection.dart';
import '../ble/ble_debug_snapshot.dart';

BleCommandRejection? findMatchingCommandRejectionAfter(
  BleDebugSnapshot snapshot, {
  required int baselineEvents,
  required String commandPrefix,
}) {
  final newEvents = snapshot.commandRejectEvents - baselineEvents;
  if (newEvents <= 0) return null;

  final rejects = snapshot.commandRejects;
  var firstNewIndex = rejects.length - newEvents;
  if (firstNewIndex < 0) firstNewIndex = 0;

  for (var i = rejects.length - 1; i >= firstNewIndex; i -= 1) {
    final rejection = rejects[i];
    if (rejection.matchesCommandPrefix(commandPrefix)) {
      return rejection;
    }
  }

  final last = snapshot.lastCommandRejection;
  if (last != null && last.matchesCommandPrefix(commandPrefix)) {
    return last;
  }
  return null;
}
