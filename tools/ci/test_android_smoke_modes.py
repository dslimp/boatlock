from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]


def _read(path: str) -> str:
    return (ROOT / path).read_text()


def _dart_smoke_modes() -> list[str]:
    text = _read("boatlock_ui/lib/smoke/ble_smoke_mode.dart")
    match = re.search(r"enum\s+BleSmokeMode\s*\{([^}]*)\}", text)
    assert match is not None
    return [
        value.strip()
        for value in match.group(1).split(",")
        if value.strip()
    ]


def _shell_modes(var_name: str) -> list[str]:
    text = _read("tools/android/common.sh")
    match = re.search(rf"{var_name}=\(([^)]*)\)", text)
    assert match is not None
    return match.group(1).split()


def test_android_smoke_modes_match_flutter_parser() -> None:
    dart_modes = _dart_smoke_modes()
    assert _shell_modes("BOATLOCK_SMOKE_MODES") == [
        mode for mode in dart_modes if mode not in ("ota", "sim_suite")
    ]
    assert _shell_modes("BOATLOCK_APP_E2E_MODES") == dart_modes


def test_android_smoke_wrappers_use_common_validator() -> None:
    for path in (
        "tools/android/build-app-apk.sh",
        "tools/android/build-smoke-apk.sh",
        "tools/android/run-app-e2e.sh",
        "tools/android/run-smoke.sh",
        "tools/hw/nh02/android-run-app-e2e.sh",
        "tools/hw/nh02/android-run-smoke.sh",
    ):
        text = _read(path)
        assert "basic|reconnect|manual|status|sim|anchor" not in text


def test_android_app_e2e_build_targets_main_app() -> None:
    text = _read("tools/android/build-app-apk.sh")
    assert "--target lib/main.dart" in text
    assert "BOATLOCK_APP_E2E_MODE=" in text
    assert "BOATLOCK_APP_E2E_OTA_LATEST_RELEASE=true" in text
    assert "main_smoke.dart" not in text
    assert '[[ ! -f "${BOATLOCK_ANDROID_APK}" ]]' in text


def test_android_app_build_supports_latest_release_service_variant() -> None:
    text = _read("tools/android/build-app-apk.sh")
    assert "--latest-release-service" in text
    assert "BOATLOCK_SERVICE_UI=true" in text
    assert "BOATLOCK_FIRMWARE_UPDATE_MANIFEST_URL=" in text
    assert "BOATLOCK_FIRMWARE_UPDATE_GITHUB_REPO=" in text
    assert "BOATLOCK_LATEST_RELEASE_GITHUB_REPO" in text


def test_android_smoke_build_checks_apk_output() -> None:
    text = _read("tools/android/build-smoke-apk.sh")
    assert "--target lib/main_smoke.dart" in text
    assert '[[ ! -f "${BOATLOCK_ANDROID_APK}" ]]' in text


def test_android_build_cache_reuses_stable_gradle_home() -> None:
    text = _read("tools/android/common.sh")
    assert "BOATLOCK_ANDROID_GRADLE_USER_HOME" in text
    assert "GRADLE_USER_HOME=/tmp/boatlock-gradle" not in text


def test_android_app_e2e_wrappers_reuse_canonical_runner() -> None:
    for path in (
        "tools/android/run-app-e2e.sh",
        "tools/hw/nh02/android-run-app-e2e.sh",
    ):
        text = _read(path)
        assert "build-app-apk.sh" in text
        assert "run-smoke.sh" in text
        assert "build-smoke-apk.sh" not in text


def test_android_app_e2e_supports_manifest_backed_ota() -> None:
    app_e2e = _read("boatlock_ui/lib/e2e/app_e2e_probe.dart")
    local_wrapper = _read("tools/android/run-app-e2e.sh")
    nh02_wrapper = _read("tools/hw/nh02/android-run-app-e2e.sh")
    nh02_smoke = _read("tools/hw/nh02/android-run-smoke.sh")

    assert "fetchLatestFirmware" in app_e2e
    assert "BOATLOCK_APP_E2E_OTA_LATEST_RELEASE" in app_e2e
    assert "--ota-latest-release" in local_wrapper
    assert "--e2e-ota-latest-release" in local_wrapper
    assert "--firmware-manifest-url" in local_wrapper
    assert "--ota-latest-release" in nh02_wrapper
    assert "generate_firmware_update_manifest.py" in nh02_wrapper
    assert "http://127.0.0.1:${OTA_PORT}/manifest.json" in nh02_wrapper
    assert "--ota-manifest" in nh02_smoke


def test_android_app_e2e_supports_full_sim_suite() -> None:
    app_e2e = _read("boatlock_ui/lib/e2e/app_e2e_probe.dart")
    local_wrapper = _read("tools/android/run-app-e2e.sh")
    nh02_wrapper = _read("tools/hw/nh02/android-run-app-e2e.sh")
    nh02_suite = _read("tools/hw/nh02/run-sim-suite.sh")

    assert "SIM_LIST" in app_e2e
    assert "SIM_REPORT" in app_e2e
    assert "app_sim_suite_all_passed" in app_e2e
    assert "--sim-suite" in local_wrapper
    assert "--sim-suite" in nh02_wrapper
    assert "flash.sh\" --profile acceptance" in nh02_suite
    assert "android-run-app-e2e.sh\" \"${app_args[@]}\"" in nh02_suite
    assert "flash.sh\" --profile release" in nh02_suite
