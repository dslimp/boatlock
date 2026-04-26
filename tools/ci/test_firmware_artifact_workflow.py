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


def test_android_build_uses_ci_caches():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()
    gradle_properties = (ROOT / "boatlock_ui/android/gradle.properties").read_text()

    assert "FLUTTER_CI_IMAGE" not in workflow
    assert "image: ghcr.io/${{ github.repository }}/flutter-android-ci:3.32.4-android33-ndk27.0" in workflow
    assert "username: ${{ github.actor }}" in workflow
    assert "password: ${{ secrets.GITHUB_TOKEN }}" in workflow
    assert "Trust Flutter SDK" in workflow
    assert "git config --global --add safe.directory /opt/flutter" in workflow
    assert "uses: gradle/actions/setup-gradle@v6" in workflow
    assert "Cache Android SDK packages" not in workflow
    assert "uses: actions/setup-java@v5" not in workflow
    assert "uses: subosito/flutter-action@v2" in workflow
    assert "GRADLE_OPTS: -Dorg.gradle.vfs.watch=false" in workflow
    assert "org.gradle.caching=true" in gradle_properties
    assert "org.gradle.parallel=true" in gradle_properties


def test_flutter_ci_image_workflow_publishes_pinned_ghcr_image():
    workflow = (ROOT / ".github/workflows/flutter-ci-image.yml").read_text()
    dockerfile = (ROOT / ".github/images/flutter-android-ci/Dockerfile").read_text()
    dockerignore = (ROOT / ".dockerignore").read_text()

    assert "packages: write" in workflow
    assert "ghcr.io/${{ github.repository }}/flutter-android-ci" in workflow
    assert "IMAGE_TAG: 3.32.4-android33-ndk27.0" in workflow
    assert '- ".dockerignore"' in workflow
    assert "uses: docker/login-action@v4" in workflow
    assert "uses: docker/setup-buildx-action@v4" in workflow
    assert "uses: docker/build-push-action@v7" in workflow
    assert "platforms: linux/amd64" in workflow
    assert "provenance: false" in workflow
    assert "cache-to: type=gha" not in workflow
    assert "ARG FLUTTER_VERSION=3.32.4" in dockerfile
    assert "ARG ANDROID_PLATFORM_LEGACY=android-33" in dockerfile
    assert "ARG ANDROID_NDK_VERSION=27.0.12077973" in dockerfile
    assert "ARG ANDROID_CMAKE_VERSION=3.22.1" in dockerfile
    assert "set +o pipefail" in dockerfile
    assert "git config --system --add safe.directory" in dockerfile
    assert "flutter precache --android --web" in dockerfile
    assert "flutter build apk --debug --no-pub" not in dockerfile
    assert "COPY boatlock_ui" not in dockerfile
    assert "**/local.properties" in dockerignore
    assert "**/build" in dockerignore


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
