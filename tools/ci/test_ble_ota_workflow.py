from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text()


def test_firmware_ble_ota_protocol_is_wired():
    ble_cpp = read("boatlock/BLEBoatLock.cpp")
    handler = read("boatlock/BleCommandHandler.h")
    ota_cpp = read("boatlock/BleOtaUpdate.cpp")

    assert '"9abc"' in ble_cpp
    assert "OtaCallbacks" in ble_cpp
    assert "bleOta.writeChunk" in ble_cpp

    assert "OTA_BEGIN:" in handler
    assert "OTA_FINISH" in handler
    assert "OTA_ABORT" in handler
    assert "stopAllMotionNow()" in handler
    assert "command rejected reason=ota_active" in handler

    assert "Update.begin" in ota_cpp
    assert "Update.write" in ota_cpp
    assert "Update.end(true)" in ota_cpp
    assert "mbedtls_sha256" in ota_cpp


def test_flutter_ble_ota_protocol_is_wired():
    ids = read("boatlock_ui/lib/ble/ble_ids.dart")
    transport = read("boatlock_ui/lib/ble/ble_boatlock.dart")
    payload = read("boatlock_ui/lib/ble/ble_ota_payload.dart")
    settings = read("boatlock_ui/lib/pages/settings_page.dart")

    assert "boatLockOtaCharacteristicUuid = '9abc'" in ids
    assert "uploadFirmwareOtaBytes" in transport
    assert "OTA_BEGIN:${firmware.length},$sha" in transport
    assert "OTA_FINISH" in transport
    assert "withoutResponse: false" in transport
    assert "boatLockSha256Hex" in payload
    assert "Обновить по BLE" in settings


def test_ble_protocol_docs_include_ota_surface():
    protocol = read("docs/BLE_PROTOCOL.md")
    assert "Firmware OTA characteristic `9abc`" in protocol
    assert "OTA_BEGIN:<size>,<sha256>" in protocol
    assert "During active OTA" in protocol
