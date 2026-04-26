import 'package:crypto/crypto.dart' as crypto;

const int boatLockOtaFallbackChunkBytes = 180;
const int boatLockOtaMaxChunkBytes = 244;
const int boatLockOtaMinChunkBytes = 20;
const int boatLockOtaAttHeaderBytes = 3;

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

int boatLockOtaChunkBytesForMtu(int mtu) {
  final payloadBytes = mtu - boatLockOtaAttHeaderBytes;
  if (payloadBytes < boatLockOtaMinChunkBytes) {
    return boatLockOtaMinChunkBytes;
  }
  if (payloadBytes > boatLockOtaMaxChunkBytes) {
    return boatLockOtaMaxChunkBytes;
  }
  return payloadBytes;
}

Iterable<List<int>> boatLockOtaChunks(
  List<int> bytes, {
  int chunkBytes = boatLockOtaFallbackChunkBytes,
}) sync* {
  final safeChunkBytes = chunkBytes < boatLockOtaMinChunkBytes
      ? boatLockOtaMinChunkBytes
      : chunkBytes;
  for (var offset = 0; offset < bytes.length; offset += safeChunkBytes) {
    final end = (offset + safeChunkBytes < bytes.length)
        ? offset + safeChunkBytes
        : bytes.length;
    yield bytes.sublist(offset, end);
  }
}
