from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_ci_uploads_firmware_artifact_with_hash_manifest():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()

    assert 'name: firmware-esp32s3' in workflow
    assert 'path: dist/firmware-esp32s3/*' in workflow
    assert 'if-no-files-found: error' in workflow
    assert 'firmware_sha256="$(sha256sum "${artifact_dir}/firmware.bin"' in workflow
    assert 'echo "firmware_sha256=${firmware_sha256}"' in workflow
    assert 'echo "artifact_name=firmware-esp32s3"' in workflow
    assert "sha256sum firmware.bin bootloader.bin partitions.bin firmware.elf > SHA256SUMS" in workflow


def test_ci_delivery_jobs_are_split_by_failure_domain():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()

    for job_name in (
        "firmware-checks",
        "firmware-test",
        "firmware-sim",
        "firmware-build",
        "flutter-checks",
        "flutter-build-android",
        "flutter-build-web",
        "flutter-build-macos",
    ):
        assert f"  {job_name}:" in workflow
        assert f"name: {job_name}" in workflow

    assert "name: flutter-android-apk" in workflow
    assert "name: flutter-web" in workflow
    assert "name: flutter-macos-app" in workflow
    assert "flutter build macos --release" in workflow
    assert "flutter-android-web" not in workflow


def test_macos_permissions_cover_ble_location_and_network():
    info_plist = (ROOT / "boatlock_ui/macos/Runner/Info.plist").read_text()
    debug_entitlements = (
        ROOT / "boatlock_ui/macos/Runner/DebugProfile.entitlements"
    ).read_text()
    release_entitlements = (
        ROOT / "boatlock_ui/macos/Runner/Release.entitlements"
    ).read_text()

    assert "NSBluetoothAlwaysUsageDescription" in info_plist
    assert "NSLocationUsageDescription" in info_plist
    for entitlements in (debug_entitlements, release_entitlements):
        assert "com.apple.security.device.bluetooth" in entitlements
        assert "com.apple.security.network.client" in entitlements
        assert "com.apple.security.personal-information.location" in entitlements
