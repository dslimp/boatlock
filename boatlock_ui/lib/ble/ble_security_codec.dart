import 'dart:convert';
import 'dart:math';

String generateOwnerSecretHex() {
  final random = Random.secure();
  final bytes = List<int>.generate(16, (_) => random.nextInt(256));
  return bytes
      .map((b) => b.toRadixString(16).padLeft(2, '0'))
      .join()
      .toUpperCase();
}

String? normalizeOwnerSecret(String? raw) {
  if (raw == null) return null;
  final compact = raw.replaceAll(RegExp(r'[^0-9a-fA-F]'), '').toUpperCase();
  if (compact.length != 32) return null;
  return compact;
}

String buildAuthProofHex(String ownerSecret, String nonceHex) {
  final secret = normalizeOwnerSecret(ownerSecret);
  final nonce = _normalizeNonceHex(nonceHex);
  if (secret == null || nonce == null) return '0000000000000000';
  final hash = _sipHash64(_hexToBytes(secret), utf8.encode('AUTH:$nonce'));
  return _hex64(hash);
}

String buildSecureCommand({
  required String ownerSecret,
  required String nonceHex,
  required String payload,
  required int counter,
}) {
  final secret = normalizeOwnerSecret(ownerSecret);
  final nonce = _normalizeNonceHex(nonceHex);
  if (secret == null || nonce == null || counter <= 0) {
    return 'SEC_CMD:00000000:0000000000000000:$payload';
  }
  final message = 'CMD:$nonce:${_hex32(counter)}:$payload';
  final hash = _sipHash64(_hexToBytes(secret), utf8.encode(message));
  return 'SEC_CMD:${_hex32(counter)}:${_hex64(hash)}:$payload';
}

String? _normalizeNonceHex(String raw) {
  final compact = raw.replaceAll(RegExp(r'[^0-9a-fA-F]'), '').toUpperCase();
  if (compact.length != 16) return null;
  return compact;
}

List<int> _hexToBytes(String hex) {
  final out = <int>[];
  for (var i = 0; i < hex.length; i += 2) {
    out.add(int.parse(hex.substring(i, i + 2), radix: 16));
  }
  return out;
}

String _hex32(int value) {
  final v = value & 0xFFFFFFFF;
  return v.toRadixString(16).toUpperCase().padLeft(8, '0');
}

String _hex64(int value) {
  final v = value & 0xFFFFFFFFFFFFFFFF;
  return v.toRadixString(16).toUpperCase().padLeft(16, '0');
}

int _rotl64(int value, int shift) {
  final v = value & 0xFFFFFFFFFFFFFFFF;
  return ((v << shift) | (v >> (64 - shift))) & 0xFFFFFFFFFFFFFFFF;
}

int _sipHash64(List<int> keyBytes, List<int> data) {
  if (keyBytes.length != 16) {
    return 0;
  }
  int read64(List<int> bytes, int offset) {
    var out = 0;
    for (var i = 0; i < 8; ++i) {
      out |= (bytes[offset + i] & 0xFF) << (i * 8);
    }
    return out & 0xFFFFFFFFFFFFFFFF;
  }

  void sipRound(List<int> state) {
    state[0] = (state[0] + state[1]) & 0xFFFFFFFFFFFFFFFF;
    state[1] = _rotl64(state[1], 13);
    state[1] ^= state[0];
    state[0] = _rotl64(state[0], 32);
    state[2] = (state[2] + state[3]) & 0xFFFFFFFFFFFFFFFF;
    state[3] = _rotl64(state[3], 16);
    state[3] ^= state[2];
    state[0] = (state[0] + state[3]) & 0xFFFFFFFFFFFFFFFF;
    state[3] = _rotl64(state[3], 21);
    state[3] ^= state[0];
    state[2] = (state[2] + state[1]) & 0xFFFFFFFFFFFFFFFF;
    state[1] = _rotl64(state[1], 17);
    state[1] ^= state[2];
    state[2] = _rotl64(state[2], 32);
  }

  final k0 = read64(keyBytes, 0);
  final k1 = read64(keyBytes, 8);
  final state = <int>[
    0x736f6d6570736575 ^ k0,
    0x646f72616e646f6d ^ k1,
    0x6c7967656e657261 ^ k0,
    0x7465646279746573 ^ k1,
  ];

  final blocks = data.length ~/ 8;
  for (var i = 0; i < blocks; ++i) {
    final m = read64(data, i * 8);
    state[3] ^= m;
    sipRound(state);
    sipRound(state);
    state[0] ^= m;
  }

  var b = (data.length & 0xFF) << 56;
  final tailOffset = blocks * 8;
  for (var i = data.length - tailOffset; i > 0; --i) {
    b |= (data[tailOffset + i - 1] & 0xFF) << ((i - 1) * 8);
  }

  state[3] ^= b;
  sipRound(state);
  sipRound(state);
  state[0] ^= b;
  state[2] ^= 0xFF;
  sipRound(state);
  sipRound(state);
  sipRound(state);
  sipRound(state);
  return (state[0] ^ state[1] ^ state[2] ^ state[3]) & 0xFFFFFFFFFFFFFFFF;
}
