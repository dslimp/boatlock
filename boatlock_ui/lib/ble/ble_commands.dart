import '../models/boat_data.dart';

String? buildSetAnchorCommand(BoatData? data) {
  if (data == null) return null;
  return buildSetAnchorCommandFromCoords(data.lat, data.lon);
}

String? buildSetAnchorCommandFromCoords(double lat, double lon) {
  if (!lat.isFinite || !lon.isFinite) return null;
  if (lat == 0 || lon == 0) return null;
  if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return null;
  return 'SET_ANCHOR:${lat.toStringAsFixed(6)},${lon.toStringAsFixed(6)}';
}

String? buildSetPhoneGpsCommand(
  double lat,
  double lon, {
  double speedKmh = 0.0,
  int? satellites,
}) {
  if (!lat.isFinite || !lon.isFinite) return null;
  if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return null;
  final safeSpeed = (speedKmh.isFinite && speedKmh > 0) ? speedKmh : 0.0;
  final base =
      'SET_PHONE_GPS:${lat.toStringAsFixed(6)},${lon.toStringAsFixed(6)},${safeSpeed.toStringAsFixed(1)}';
  if (satellites == null) {
    return base;
  }
  final safeSat = satellites < 0 ? 0 : satellites;
  return '$base,$safeSat';
}

String? buildNudgeDirCommand(String direction) {
  final dir = direction.trim().toUpperCase();
  const allowed = {'FWD', 'BACK', 'LEFT', 'RIGHT'};
  if (!allowed.contains(dir)) return null;
  return 'NUDGE_DIR:$dir';
}

String? buildNudgeBearingCommand(double bearingDeg) {
  if (!bearingDeg.isFinite) return null;
  var norm = bearingDeg % 360.0;
  if (norm < 0) norm += 360.0;
  return 'NUDGE_BRG:${norm.toStringAsFixed(1)}';
}

String? buildSetAnchorProfileCommand(String profile) {
  final raw = profile.trim().toLowerCase();
  if (raw.isEmpty) return null;
  const allowed = {'quiet', 'normal', 'current'};
  if (!allowed.contains(raw)) return null;
  return 'SET_ANCHOR_PROFILE:$raw';
}

String? buildManualTargetCommand({
  required double angleDeg,
  required int throttlePct,
  int ttlMs = 1000,
}) {
  if (!angleDeg.isFinite || angleDeg < -90.0 || angleDeg > 90.0) return null;
  if (throttlePct < -100 || throttlePct > 100) return null;
  if (ttlMs < 100 || ttlMs > 1000) return null;
  return 'MANUAL_TARGET:${angleDeg.toStringAsFixed(1)},$throttlePct,$ttlMs';
}
