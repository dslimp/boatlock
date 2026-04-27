from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]


def _read(path: str) -> str:
    return (ROOT / path).read_text()


def _dart_smoke_modes() -> list[str]:
    text = _read("boatlock_ui/lib/smoke/ble_smoke_mode.dart")
    match = re.search(r"enum\s+BleSmokeMode\s*\{([^}]*)\}", text)
    assert match is not None
    modes = [
        value.strip()
        for value in match.group(1).split(",")
        if value.strip()
    ]
    return ["sim_suite" if mode == "simSuite" else mode for mode in modes]


def _shell_modes(var_name: str) -> list[str]:
    text = _read("tools/android/common.sh")
    match = re.search(rf"{var_name}=\(([^)]*)\)", text)
    assert match is not None
    return match.group(1).split()


def test_android_smoke_modes_match_flutter_parser() -> None:
    dart_modes = _dart_smoke_modes()
    assert _shell_modes("BOATLOCK_APP_CHECK_MODES") == dart_modes


def test_android_smoke_wrappers_use_common_validator() -> None:
    for path in (
        "tools/android/build-app-apk.sh",
        "tools/android/run-app-check.sh",
        "tools/android/run-smoke.sh",
        "tools/hw/nh02/android-run-app-check.sh",
        "tools/hw/nh02/android-run-smoke.sh",
    ):
        text = _read(path)
        assert "basic|reconnect|manual|status|sim|anchor" not in text


def test_android_app_check_build_targets_main_app() -> None:
    text = _read("tools/android/build-app-apk.sh")
    assert "--target lib/main.dart" in text
    assert "main_" + "smoke.dart" not in text
    assert "BOATLOCK_APP_" + "E2E" not in text
    assert "BOATLOCK_" + "SMOKE_MODE" not in text
    assert '[[ ! -f "${BOATLOCK_ANDROID_APK}" ]]' in text


def test_android_app_build_embeds_latest_release_source() -> None:
    text = _read("tools/android/build-app-apk.sh")
    assert "--release" in text
    assert "--dart-" + "define" not in text
    assert "--debug" not in text


def test_android_build_cache_reuses_stable_gradle_home() -> None:
    text = _read("tools/android/common.sh")
    assert "BOATLOCK_ANDROID_GRADLE_USER_HOME" in text
    assert "GRADLE_USER_HOME=/tmp/boatlock-gradle" not in text


def test_android_app_check_wrappers_reuse_canonical_runner() -> None:
    for path in (
        "tools/android/run-app-check.sh",
        "tools/hw/nh02/android-run-app-check.sh",
    ):
        text = _read(path)
        assert "build-app-apk.sh" in text
        assert "run-smoke.sh" in text
        assert "build-" + "smoke-apk.sh" not in text


def test_android_checks_are_runtime_app_commands() -> None:
    local_runner = _read("tools/android/run-smoke.sh")
    remote_runner = _read("tools/hw/nh02/remote/boatlock-run-android-smoke.sh")
    activity = _read("boatlock_ui/android/app/src/main/kotlin/com/example/boatlock_ui/MainActivity.kt")
    dart_runtime = _read("boatlock_ui/lib/app_runtime_command.dart")
    map_page = _read("boatlock_ui/lib/pages/map_page.dart")

    assert "--es boatlock_check_mode" in local_runner
    assert "--es boatlock_check_mode" in remote_runner
    assert "boatlock_ota_url" in local_runner
    assert "boatlock_ota_sha256" in remote_runner
    assert "MethodChannel" in activity
    assert "boatlock/runtime" in activity
    assert "BoatLockRuntimeCommand.readInitial" in map_page
    assert "BoatLockRuntimeCommand" in dart_runtime


def test_android_app_check_supports_manifest_backed_ota() -> None:
    app_check = _read("boatlock_ui/lib/app_check/app_check_probe.dart")
    local_wrapper = _read("tools/android/run-app-check.sh")
    nh02_wrapper = _read("tools/hw/nh02/android-run-app-check.sh")
    nh02_smoke = _read("tools/hw/nh02/android-run-smoke.sh")
    remote_runner = _read("tools/hw/nh02/remote/boatlock-run-android-smoke.sh")

    assert "fetchLatestFirmware" in app_check
    assert "BoatLockAppCheckConfig" in app_check
    assert "--ota-latest-release" in local_wrapper
    assert "--app-check-ota-latest-release" not in local_wrapper
    assert "--firmware-manifest-url" in local_wrapper
    assert "--ota-latest-release" in nh02_wrapper
    assert "generate_firmware_update_manifest.py" in nh02_wrapper
    assert "http://127.0.0.1:${OTA_PORT}/manifest.json" in remote_runner
    assert "--ota-manifest" in nh02_smoke


def test_android_app_check_supports_full_sim_suite() -> None:
    app_check = _read("boatlock_ui/lib/app_check/app_check_probe.dart")
    local_wrapper = _read("tools/android/run-app-check.sh")
    nh02_wrapper = _read("tools/hw/nh02/android-run-app-check.sh")
    nh02_suite = _read("tools/hw/nh02/run-sim-suite.sh")

    assert "SIM_LIST" in app_check
    assert "SIM_REPORT" in app_check
    assert "app_sim_suite_all_passed" in app_check
    assert "--sim-suite" in local_wrapper
    assert "--sim-suite" in nh02_wrapper
    assert "flash.sh\" --profile release" in nh02_suite
    assert "android-run-app-check.sh\" \"${app_args[@]}\"" in nh02_suite
    assert "flash.sh\" --profile acceptance" not in nh02_suite
