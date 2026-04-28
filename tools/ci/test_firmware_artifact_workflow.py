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
    assert "pio run -e esp32s3" in workflow
    assert "command_profile=release" in workflow
    assert "sha256sum firmware.bin bootloader.bin partitions.bin firmware.elf > SHA256SUMS" in workflow


def test_release_publishes_firmware_update_manifest():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()

    assert "Prepare GitHub Release assets" in workflow
    assert '--out "${release_dir}/manifest.json"' in workflow
    assert "firmware-esp32s3.bin" in workflow
    assert "--base-url \"https://github.com/${{ github.repository }}/releases/download/${{ github.ref_name }}\"" in workflow
    assert "--binary-filename \"firmware-esp32s3.bin\"" in workflow
    assert "--channel release" in workflow
    assert "--branch \"${release_branch}\"" in workflow
    assert "release-assets/*" in workflow


def test_ci_uses_release_branches_and_no_pages_channel():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()

    assert '- "release/**"' in workflow
    assert "check_release_ref.py --tag" in workflow
    assert "firmware-main-channel" not in workflow
    assert "dist/pages" not in workflow
    assert "firmware/main" not in workflow
    assert "github.io" not in workflow
    assert "actions/configure-pages" not in workflow
    assert "actions/upload-pages-artifact" not in workflow
    assert "actions/deploy-pages" not in workflow
    assert "pages: write" not in workflow
    assert "id-token: write" not in workflow


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


def test_ci_builds_single_apps_with_latest_release_firmware_source():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()

    assert "Build Android APK (release)" in workflow
    assert "boatlock-app.apk" in workflow
    assert "Build macOS app" in workflow
    assert "boatlock-macos.zip" in workflow
    assert "--dart-" + "define" not in workflow


def test_android_release_apk_uses_stable_signing_for_tags():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()
    gradle = (ROOT / "boatlock_ui/android/app/build.gradle.kts").read_text()

    assert "Prepare Android release signing" in workflow
    assert "BOATLOCK_ANDROID_KEYSTORE_BASE64" in workflow
    assert "BOATLOCK_ANDROID_KEYSTORE_PATH" in workflow
    assert "BOATLOCK_ANDROID_REQUIRE_RELEASE_SIGNING" in workflow
    assert "refs/tags/v*" in workflow

    assert "BOATLOCK_ANDROID_KEYSTORE_PATH" in gradle
    assert "BOATLOCK_ANDROID_KEYSTORE_PASSWORD" in gradle
    assert "BOATLOCK_ANDROID_KEY_ALIAS" in gradle
    assert "BOATLOCK_ANDROID_KEY_PASSWORD" in gradle
    assert "BOATLOCK_ANDROID_REQUIRE_RELEASE_SIGNING" in gradle
    assert "boatlockRelease" in gradle


def test_flutter_ci_runs_setup_ui_tests():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()

    assert "Test setup UI" in workflow
    assert "flutter test test/settings_page_setup_ui_test.dart" in workflow


def test_macos_build_wrapper_builds_single_release_app():
    script = (ROOT / "tools/macos/build-app.sh").read_text()

    assert "build macos" in script
    assert "--release" in script
    assert "--dart-" + "define" not in script
    assert "--debug" not in script
    assert "BoatLock.app" in script


def test_macos_acceptance_wrapper_covers_app_artifact_and_runtime_smoke():
    script = (ROOT / "tools/macos/acceptance.sh").read_text()

    assert "--artifact-zip" in script
    assert "boatlock-macos.zip" in script
    assert "ditto -x -k" in script
    assert "plutil -lint -s" in script
    assert "codesign --verify --verbose" in script
    assert "codesign -d --entitlements :-" in script
    assert "com.apple.security.device.bluetooth" in script
    assert "com.apple.security.network.client" in script
    assert "com.apple.security.personal-information.location" in script
    assert "CoreBluetooth.framework" in script
    assert "open -n -F -W" in script
    assert "--manual" in script
    assert "Without BLE hardware" in script


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


def test_app_visible_metadata_uses_boatlock_name():
    mac_info = (ROOT / "boatlock_ui/macos/Runner/Info.plist").read_text()
    mac_config = (
        ROOT / "boatlock_ui/macos/Runner/Configs/AppInfo.xcconfig"
    ).read_text()
    android_manifest = (
        ROOT / "boatlock_ui/android/app/src/main/AndroidManifest.xml"
    ).read_text()
    ios_info = (ROOT / "boatlock_ui/ios/Runner/Info.plist").read_text()
    windows_rc = (ROOT / "boatlock_ui/windows/runner/Runner.rc").read_text()
    linux_app = (ROOT / "boatlock_ui/linux/runner/my_application.cc").read_text()

    assert "<string>BoatLock</string>" in mac_info
    assert "PRODUCT_COPYRIGHT = Copyright © 2026 BoatLock. All rights reserved." in mac_config
    assert 'android:label="BoatLock"' in android_manifest
    assert "<string>BoatLock</string>" in ios_info
    assert 'VALUE "CompanyName", "BoatLock"' in windows_rc
    assert 'VALUE "ProductName", "BoatLock"' in windows_rc
    assert "Copyright (C) 2026 BoatLock. All rights reserved." in windows_rc
    assert 'gtk_header_bar_set_title(header_bar, "BoatLock")' in linux_app

    visible_metadata = "\n".join(
        [mac_info, mac_config, android_manifest, ios_info, windows_rc, linux_app]
    )
    assert "PRODUCT_NAME = BoatLock" in mac_config
    assert "PRODUCT_BUNDLE_IDENTIFIER = com.boatlock.app" in mac_config
    assert 'set(APPLICATION_ID "com.boatlock.app")' in (
        ROOT / "boatlock_ui/linux/CMakeLists.txt"
    ).read_text()
    assert "Copyright © 2025 com.example" not in visible_metadata
    assert "Copyright (C) 2025 com.example" not in visible_metadata
    assert 'android:label="boatlock_ui"' not in visible_metadata
