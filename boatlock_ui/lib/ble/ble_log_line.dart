import 'dart:convert';

String decodeBoatLockLogLine(List<int> value) {
  final nul = value.indexOf(0);
  final end = nul < 0 ? value.length : nul;
  return utf8.decode(value.sublist(0, end), allowMalformed: true);
}
