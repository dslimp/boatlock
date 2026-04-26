from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_ci_uploads_firmware_artifact_with_hash_manifest():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()

    assert 'name: firmware-esp32s3' in workflow
    assert 'name: firmware-esp32s3-service' in workflow
    assert 'path: dist/firmware-esp32s3/*' in workflow
    assert 'path: dist/firmware-esp32s3-service/*' in workflow
    assert 'if-no-files-found: error' in workflow
    assert 'firmware_sha256="$(sha256sum "${artifact_dir}/firmware.bin"' in workflow
    assert 'echo "firmware_sha256=${firmware_sha256}"' in workflow
    assert 'echo "artifact_name=firmware-esp32s3"' in workflow
    assert 'echo "artifact_name=firmware-esp32s3-service"' in workflow
    assert "pio run -e esp32s3_service" in workflow
    assert "command_profile=service" in workflow
    assert "sha256sum firmware.bin bootloader.bin partitions.bin firmware.elf > SHA256SUMS" in workflow


def test_release_publishes_service_firmware_update_manifest():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()

    assert "Add release firmware update manifest" in workflow
    assert "artifacts/firmware-esp32s3-service/manifest.json" in workflow
    assert "--base-url \"https://github.com/${{ github.repository }}/releases/download/${{ github.ref_name }}\"" in workflow
    assert "artifacts/firmware-esp32s3-service/*" in workflow


def test_ci_publishes_latest_main_service_firmware_channel():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()

    assert "firmware-main-channel" in workflow
    assert "github.ref == 'refs/heads/main'" in workflow
    assert "pio run -e esp32s3_service" in workflow
    assert "dist/pages" in workflow
    assert "firmware/main" in workflow
    assert "generate_firmware_update_manifest.py" in workflow
    assert "artifact_name=firmware-esp32s3-service" in workflow
    assert "platformio_env=esp32s3_service" in workflow
    assert "command_profile=service" in workflow
    assert "manifest_url=${base_url}/manifest.json" in workflow
    assert "uses: actions/configure-pages@v5" in workflow
    assert "uses: actions/upload-pages-artifact@v4" in workflow
    assert "uses: actions/deploy-pages@v4" in workflow
    assert "pages: write" in workflow
    assert "id-token: write" in workflow


def test_ci_delivery_jobs_are_split_by_failure_domain():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()

    for job_name in (
        "firmware-checks",
        "firmware-test",
        "firmware-sim",
        "firmware-build",
        "firmware-main-channel",
        "flutter-checks",
        "flutter-build-android",
        "flutter-build-web",
        "flutter-build-macos",
    ):
        assert f"  {job_name}:" in workflow
        assert f"name: {job_name}" in workflow

    assert "name: flutter-android-apk" in workflow
    assert "name: flutter-android-service-apk" in workflow
    assert "name: flutter-web" in workflow
    assert "name: flutter-macos-app" in workflow
    assert "name: flutter-macos-service-app" in workflow
    assert "flutter build macos --release" in workflow
    assert "flutter-android-web" not in workflow


def test_ci_builds_service_apps_with_latest_main_firmware_source():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()

    assert "Build Android service APK (latest main)" in workflow
    assert "boatlock-service-main.apk" in workflow
    assert "Build macOS service app (latest main)" in workflow
    assert "boatlock-macos-service-main.zip" in workflow
    assert "--dart-define=BOATLOCK_SERVICE_UI=true" in workflow
    assert "--dart-define=BOATLOCK_FIRMWARE_UPDATE_MANIFEST_URL=https://${{ github.repository_owner }}.github.io/${{ github.event.repository.name }}/firmware/main/manifest.json" in workflow
    assert "artifacts/flutter-android-service-apk/*" in workflow
    assert "artifacts/flutter-macos-service-app/*" in workflow


def test_macos_build_wrapper_supports_latest_main_service_variant():
    script = (ROOT / "tools/macos/build-app.sh").read_text()

    assert "--latest-main-service" in script
    assert "BOATLOCK_SERVICE_UI=true" in script
    assert "BOATLOCK_FIRMWARE_UPDATE_MANIFEST_URL=" in script
    assert "BOATLOCK_FIRMWARE_UPDATE_GITHUB_REPO=" in script
    assert "https://dslimp.github.io/boatlock/firmware/main/manifest.json" in script
    assert "build macos" in script
    assert "boatlock_ui.app" in script


def test_android_build_uses_ci_caches():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()
    gradle_properties = (ROOT / "boatlock_ui/android/gradle.properties").read_text()

    assert "image: ghcr.io/${{ github.repository }}/flutter-android-ci" not in workflow
    assert "Trust Flutter SDK" not in workflow
    assert "uses: gradle/actions/setup-gradle@v6" in workflow
    assert "Cache Android SDK packages" in workflow
    assert "/usr/local/lib/android/sdk/ndk/28.2.13676358" in workflow
    assert "/usr/local/lib/android/sdk/platforms/android-36" in workflow
    assert "/usr/local/lib/android/sdk/build-tools/35.0.0" in workflow
    assert "/usr/local/lib/android/sdk/build-tools/36.0.0" in workflow
    assert "/usr/local/lib/android/sdk/cmake/4.1.2" in workflow
    assert "uses: actions/setup-java@v5" in workflow
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
    assert "IMAGE_TAG: 3.41.7-android36-ndk28.2" in workflow
    assert '- ".dockerignore"' in workflow
    assert "uses: docker/login-action@v4" in workflow
    assert "uses: docker/setup-buildx-action@v4" in workflow
    assert "uses: docker/build-push-action@v7" in workflow
    assert "platforms: linux/amd64" in workflow
    assert "provenance: false" in workflow
    assert "cache-to: type=gha" not in workflow
    assert "ARG FLUTTER_VERSION=3.41.7" in dockerfile
    assert "ARG ANDROID_PLATFORM_CURRENT=android-36" in dockerfile
    assert "ARG ANDROID_PLATFORM_LEGACY" not in dockerfile
    assert "ARG ANDROID_BUILD_TOOLS_VERSION=36.0.0" in dockerfile
    assert "ARG ANDROID_BUILD_TOOLS_FLUTTER_VERSION=35.0.0" in dockerfile
    assert "ARG ANDROID_NDK_VERSION=28.2.13676358" in dockerfile
    assert "ARG ANDROID_CMAKE_VERSION=4.1.2" in dockerfile
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
