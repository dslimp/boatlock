import 'package:boatlock_ui/ble/ble_ota_payload.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('normalizes SHA-256 hex', () {
    expect(
      normalizeBoatLockSha256Hex(
        '00112233445566778899AABBCCDDEEFF00112233445566778899AABBCCDDEEFF',
      ),
      '00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff',
    );
    expect(normalizeBoatLockSha256Hex('001122'), isNull);
    expect(
      normalizeBoatLockSha256Hex(
        '00112233445566778899aabbccddeeff00112233445566778899aabbccddeefg',
      ),
      isNull,
    );
  });

  test('chunks firmware payload into BLE-safe writes', () {
    final bytes = List<int>.generate(
      boatLockOtaFallbackChunkBytes * 2 + 7,
      (i) => i & 0xff,
    );
    final chunks = boatLockOtaChunks(bytes).toList();
    expect(chunks.map((c) => c.length), [180, 180, 7]);
    expect(chunks.expand((c) => c), bytes);
  });

  test('sizes OTA chunks from negotiated MTU', () {
    expect(boatLockOtaChunkBytesForMtu(247), 244);
    expect(boatLockOtaChunkBytesForMtu(512), 244);
    expect(boatLockOtaChunkBytesForMtu(23), 20);
  });

  test('chunks firmware payload using requested chunk size', () {
    final bytes = List<int>.generate(244 * 2 + 3, (i) => i & 0xff);
    final chunks = boatLockOtaChunks(bytes, chunkBytes: 244).toList();
    expect(chunks.map((c) => c.length), [244, 244, 3]);
    expect(chunks.expand((c) => c), bytes);
  });

  test('computes SHA-256 for firmware bytes', () {
    expect(
      boatLockSha256Hex([1, 2, 3]),
      '039058c6f2c0cb492c533b0a4d14ef77cc0f78abccced5287d84a1a2011cfb81',
    );
  });
}
