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
    manifest = read("boatlock_ui/lib/ota/firmware_update_manifest.dart")
    client = read("boatlock_ui/lib/ota/firmware_update_client.dart")
    app_check = read("boatlock_ui/lib/app_check/app_check_probe.dart")

    assert "boatLockOtaCharacteristicUuid = '9abc'" in ids
    assert "uploadFirmwareOtaBytes" in transport
    assert "sendCustomCommand(\n        'OTA_BEGIN:${firmware.length},$sha'" in transport
    assert "OTA_FINISH" in transport
    assert "OTA_ABORT" in transport
    assert "otaFound: otaFound" in transport
    assert "clearGattCache" in transport
    assert "BLE OTA unavailable reason=missing_ota_char" in transport
    assert "_otaUploadInProgress" in transport
    assert "reconnect deferred: OTA upload in progress" in transport
    assert "scan skipped: OTA upload in progress" in transport
    assert "requestConnectionPriority" in transport
    assert "requestMtu" in transport
    assert "withoutResponse: transport.withoutResponse" in transport
    assert "ota_waiting_transport" in app_check
    assert "diagnostics.hasCommandChar" in app_check
    assert "diagnostics.hasOtaChar" in app_check
    assert "_waitForBleOtaTransportReady" in app_check
    assert "app_ota_transport_unavailable" in app_check
    assert "boatLockOtaChunkBytesForMtu" in payload
    assert "boatLockSha256Hex" in payload
    assert "Файл на телефоне" in settings
    assert "Последняя с GitHub" in settings
    assert "Firmware URL" not in settings
    assert "Обновить по BLE" not in settings
    assert "Обновить до релиза" not in settings
    assert "dslimp/boatlock" in client
    assert "isBoatLockFirmwareUpdateUrlAllowed" in client
    assert "boatLockFirmwareUpdateDownloadHeaders" in client
    assert "platformioEnv != 'esp32s3'" in manifest
    assert "commandProfile != 'release'" in manifest
    assert "binaryUrl must use HTTPS or localhost HTTP" in manifest


def test_ble_protocol_docs_include_ota_surface():
    protocol = read("docs/BLE_PROTOCOL.md")
    assert "Firmware OTA characteristic `9abc`" in protocol
    assert "OTA_BEGIN:<size>,<sha256>" in protocol
    assert "During active OTA" in protocol
    assert "latest GitHub Release" in protocol
    assert "esp32s3" in protocol
