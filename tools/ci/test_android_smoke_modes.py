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
    assert _shell_modes("BOATLOCK_SMOKE_MODES") == [mode for mode in dart_modes if mode != "ota"]
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
    assert "main_smoke.dart" not in text
    assert '[[ ! -f "${BOATLOCK_ANDROID_APK}" ]]' in text


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
