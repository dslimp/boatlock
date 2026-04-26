import 'package:crypto/crypto.dart' as crypto;

const int boatLockOtaChunkBytes = 180;

String? normalizeBoatLockSha256Hex(String? value) {
  final normalized = value?.trim().toLowerCase();
  if (normalized == null || normalized.length != 64) {
    return null;
  }
  final ok = RegExp(r'^[0-9a-f]{64}$').hasMatch(normalized);
  return ok ? normalized : null;
}

String boatLockSha256Hex(List<int> bytes) {
  return crypto.sha256.convert(bytes).toString();
}

bool isBoatLockOtaImageSizeValid(int size) {
  return size > 0;
}

Iterable<List<int>> boatLockOtaChunks(List<int> bytes) sync* {
  for (var offset = 0; offset < bytes.length; offset += boatLockOtaChunkBytes) {
    final end = (offset + boatLockOtaChunkBytes < bytes.length)
        ? offset + boatLockOtaChunkBytes
        : bytes.length;
    yield bytes.sublist(offset, end);
  }
}
