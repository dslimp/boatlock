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
    ):
        assert f"  {job_name}:" in workflow
        assert f"name: {job_name}" in workflow

    assert "name: flutter-android-apk" in workflow
    assert "name: flutter-web" in workflow
    assert "flutter-android-web" not in workflow
