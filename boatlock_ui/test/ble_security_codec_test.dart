import 'package:boatlock_ui/ble/ble_security_codec.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('normalizeOwnerSecret accepts exactly 16 hex bytes', () {
    expect(
      normalizeOwnerSecret('00112233445566778899aabbccddeeff'),
      '00112233445566778899AABBCCDDEEFF',
    );
    expect(
      normalizeOwnerSecret('00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff'),
      '00112233445566778899AABBCCDDEEFF',
    );
    expect(normalizeOwnerSecret('123456'), isNull);
  });

  test('buildAuthProofHex and buildSecureCommand are deterministic', () {
    final proof = buildAuthProofHex(
      '00112233445566778899AABBCCDDEEFF',
      '0123456789ABCDEF',
    );
    expect(proof.length, 16);

    final secure = buildSecureCommand(
      ownerSecret: '00112233445566778899AABBCCDDEEFF',
      nonceHex: '0123456789ABCDEF',
      payload: 'ANCHOR_ON',
      counter: 1,
    );
    expect(secure.startsWith('SEC_CMD:00000001:'), isTrue);
    expect(secure.endsWith(':ANCHOR_ON'), isTrue);
  });

  test('generateOwnerSecretHex produces normalized random owner secret', () {
    final secret = generateOwnerSecretHex();

    expect(secret, hasLength(32));
    expect(normalizeOwnerSecret(secret), secret);
  });
}
