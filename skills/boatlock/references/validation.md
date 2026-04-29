# Validation Reference

## Pick The Smallest Relevant Check

- Firmware logic change:
  - run targeted native tests first
  - build the affected firmware env if the change touches production code
- BLE command or telemetry change:
  - native BLE command tests
  - Flutter tests for parsing/UI
  - update `docs/BLE_PROTOCOL.md`
  - if the app must be installed before BLE OTA can update already-deployed
    firmware, keep enough parser bridge for the currently installed live-frame
    version to start OTA and prove the rollout through `tools/hw/nh02/deploy.sh`
- GNSS, anchor-control, or safety change:
  - native tests
  - HIL simulation checks if behavior is control-loop sensitive
- Settings/schema change:
  - update `docs/CONFIG_SCHEMA.md`
  - keep `Settings::VERSION` and CI schema checks aligned
- NVS/EEPROM write-policy change:
  - audit all `settings.save()` call sites, not only the file currently being changed
  - remember that `Settings::load()` can persist on boot during migration/CRC/normalization flows
  - cover both `test_settings` and the command/runtime tests that exercise the changed write paths
- Version or release-note change:
  - keep `boatlock/main.cpp`, `CHANGELOG.md`, `docs/releases/`, and CI version checks aligned
- Release process change:
  - keep `docs/RELEASE_PROCESS.md`, `.github/workflows/ci.yml`, release branch checks, and app firmware source defines aligned

## GitHub Actions And Release API Checks

- GitHub Actions and GitHub Release proof must come from GitHub API data, not
  from browser pages, local artifacts, or assumptions from `git push` output.
- `gh` is not required on this host. Use the GitHub REST API through `curl`
  and the existing git credential helper; do not print the token.
- If API access fails only inside the sandbox with network/permission errors,
  rerun the same API probe with host access before treating GitHub as broken.
- Canonical credential setup for one shell session:

```sh
GITHUB_TOKEN="$(
  printf 'protocol=https\nhost=github.com\n\n' |
    git credential fill |
    awk -F= '$1 == "password" { print $2 }'
)"
```

- Find the workflow run for a pushed branch, release branch, tag, or SHA:

```sh
REF="v0.2.7"
curl -fsS \
  -H "Accept: application/vnd.github+json" \
  -H "Authorization: Bearer ${GITHUB_TOKEN}" \
  "https://api.github.com/repos/dslimp/boatlock/actions/runs?per_page=50" |
  jq -r --arg ref "${REF}" '
    .workflow_runs[]
    | select(.head_branch == $ref or .head_sha == $ref)
    | [.id, .name, .status, .conclusion, .head_branch, .head_sha, .html_url]
    | @tsv
  '
```

- Inspect failed or pending jobs for a run before guessing:

```sh
RUN_ID="25080503445"
curl -fsS \
  -H "Accept: application/vnd.github+json" \
  -H "Authorization: Bearer ${GITHUB_TOKEN}" \
  "https://api.github.com/repos/dslimp/boatlock/actions/runs/${RUN_ID}/jobs?per_page=100" |
  jq -r '.jobs[] | [.name, .status, .conclusion, .html_url] | @tsv'
```

- Verify the published release and required OTA assets:

```sh
TAG="v0.2.7"
curl -fsS \
  -H "Accept: application/vnd.github+json" \
  -H "Authorization: Bearer ${GITHUB_TOKEN}" \
  "https://api.github.com/repos/dslimp/boatlock/releases/tags/${TAG}" |
  jq '{tag_name, draft, prerelease, target_commitish, asset_count: (.assets | length), assets: [.assets[].name]}'
```

- Verify the app-facing firmware manifest, because the phone `Последняя с
  GitHub` button depends on this file:

```sh
TAG="v0.2.7"
curl -fsSL \
  -H "Authorization: Bearer ${GITHUB_TOKEN}" \
  "https://github.com/dslimp/boatlock/releases/download/${TAG}/manifest.json" |
  jq '{repo, branch, channel, firmwareVersion, platformioEnv, commandProfile, workflowRunId, binaryUrl, size, sha256, gitSha}'
```

- A release is not proven until the tag workflow is `completed/success`, the
  GitHub Release is not draft/prerelease, and the assets include at least
  `manifest.json`, `firmware-esp32s3.bin`, and `boatlock-app.apk`.

## Common Commands

- Local pre-powered validation gate:
  - `tools/ci/run_local_prepowered_gate.sh`
- Firmware build:
  - `cd boatlock && pio run -e esp32s3`
- Acceptance firmware profile build:
  - `cd boatlock && pio run -e esp32s3_acceptance`
- Normal phone BLE OTA firmware update:
  - operator path A: Settings -> `Файл на телефоне` with a local
    `firmware.bin` selected on the Android phone
  - operator path B: Settings -> `Последняя с GitHub` against the latest
    GitHub Release for `dslimp/boatlock`
  - bench automation only: `tools/hw/nh02/deploy.sh`
  - reuse already-built firmware/APK through the same path: `tools/hw/nh02/deploy.sh --no-build`
  - low-level app-check when bypassing the build wrapper is intentional: `tools/hw/nh02/android-run-app-check.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin --wait-secs 1800`
  - remember the release APK is installed before the firmware upload, so app-side
    telemetry parsing must still decode the currently installed firmware well
    enough to receive first telemetry and launch OTA
  - do not add operator-facing firmware URL/SHA fields back to Settings; the
    URL/SHA path is hidden bench automation only
  - USB flash is only the seed/recovery path when the target lacks an OTA-capable image or cannot reconnect over BLE
  - if BLE discovery may take time, add `--wait-secs 1800`; the wrapper wait
    starts before scan/connect and can expire during upload when advertising
    appears late
- Latest-release phone bridge:
  - release app builds use the built-in `dslimp/boatlock` GitHub release source
  - local shortcuts: `tools/android/build-app-apk.sh` and `tools/macos/build-app.sh`
  - public latest-release bench proof: `tools/hw/nh02/android-run-app-check.sh --ota-latest-release --wait-secs 1800`
  - local temporary-manifest bench proof for unpublished firmware: `tools/hw/nh02/android-run-app-check.sh --ota-latest-release --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin`
- Full native tests:
  - `cd boatlock && platformio test -e native`
- Do not run multiple `platformio test -e native ...` commands in parallel against the same checkout/build directory. PlatformIO shares `.pio/build/native`, and parallel suites can kill or corrupt each other; run suites sequentially or use isolated worktrees/build dirs.
- Example targeted native suites:
  - `cd boatlock && platformio test -e native -f test_ble_command_handler`
  - `cd boatlock && platformio test -e native -f test_gnss_quality_gate`
  - `cd boatlock && platformio test -e native -f test_hil_sim`
  - `cd boatlock && platformio test -e native -f test_settings`
- Flutter tests:
  - `cd boatlock_ui && flutter test`
- App release builds:
  - Android and macOS app wrappers build release artifacts only.
  - Do not add separate debug or alternate app variants; the release app includes
    setup controls hidden behind the Settings `Настройка оборудования` switch.
  - Local Android release builds auto-source
    `~/.boatlock/android-release-signing.env` when it exists, so `deploy.sh`
    and app-check install APKs signed with the same stable key as GitHub
    Releases. If that env file is absent, non-tag builds may still fall back to
    debug signing.
  - Android APK output is `boatlock_ui/build/app/outputs/flutter-apk/app-release.apk`.
  - macOS app output is `boatlock_ui/build/macos/Build/Products/Release/BoatLock.app`.
- Offline simulation harness:
  - `python3 tools/sim/test_sim_core.py`
  - `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json`
  - `python3 tools/sim/test_soak.py`
  - `python3 tools/sim/run_soak.py --hours 6 --check --json-out tools/sim/soak_report.json`
- NH02 hardware bench:
  - `tools/hw/nh02/install.sh`
  - `tools/hw/nh02/deploy.sh`
  - `tools/hw/nh02/deploy.sh --no-build`
  - `tools/hw/nh02/android-run-app-check.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin --wait-secs 1800` for the lower-level OTA check
  - `tools/hw/nh02/flash.sh` (USB seed/recovery)
  - `tools/hw/nh02/flash.sh --profile acceptance`
  - `tools/hw/nh02/acceptance.sh`
  - `pytest -q tools/hw/nh02/test_acceptance.py`
  - `tools/hw/nh02/monitor.sh`
  - `tools/hw/nh02/status.sh`
  - `tools/hw/nh02/android-install.sh`
  - `tools/hw/nh02/android-install-app.sh`
  - `tools/hw/nh02/android-status.sh`
  - `tools/hw/nh02/android-wifi-debug.sh`
  - rerun `install.sh` after changing the tracked service unit or remote flash helper
- microSD logger hardware proof:
  - flash a build with a FAT-formatted microSD inserted
  - require `[SD] logger ready=1`
  - verify display redraws still work
  - verify `/boatlock/*.jsonl` grows on the card
  - for rotation/low-space work, test file rotation or low-space deletion on real media before water use
- Android USB + BLE app checks:
  - `tools/android/status.sh`
  - `tools/android/build-app-apk.sh`
  - `tools/android/run-app-check.sh`
  - `tools/android/run-smoke.sh`
  - `tools/hw/nh02/android-run-app-check.sh --manual --wait-secs 130`
  - `tools/hw/nh02/android-run-app-check.sh --compass --wait-secs 130`
  - `tools/hw/nh02/android-run-app-check.sh --gps --wait-secs 180`
  - `tools/hw/nh02/android-run-app-check.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin`
  - `tools/hw/nh02/android-run-app-check.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin --wait-secs 1800`
  - `tools/hw/nh02/android-run-app-check.sh --ota-latest-release --wait-secs 1800`
  - `tools/hw/nh02/android-run-app-check.sh --sim-suite --wait-secs 1800`
  - `tools/hw/nh02/run-sim-suite.sh`
  - `tools/hw/nh02/android-run-app-check.sh --reconnect --wait-secs 130`
  - `tools/hw/nh02/android-run-app-check.sh --esp-reset --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --compass --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --gps --wait-secs 180`
  - `tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130`
- App check mode constants and parser live in `boatlock_ui/lib/smoke/ble_smoke_mode.dart`.
- Android checks build `lib/main.dart` once and pass runtime extras to the same release app; do not use alternate entrypoints or compile-time switches.
- Shell app-check mode allowlist lives in `tools/android/common.sh`; do not duplicate hard-coded mode case lists in individual wrappers.
- When the check mode contract changes, run its Flutter unit test, `pytest -q tools/ci/test_android_smoke_modes.py`, and `tools/android/build-app-apk.sh` at minimum.
- CI helper tests:
  - `pytest tools/ci/test_*.py`
- Quick audit for persistence call sites:
  - `rg -n "settings\\.save\\(" boatlock`

## Release/Dev/HIL Gate Validation

Firmware command enforcement is implemented in the shared BLE command path. PlatformIO profile aliases and the `nh02` flash wrapper profile report select which command scopes are compiled into the firmware command gate.

Profile rules:

- `release` profile: accepts release commands, including setup, BLE OTA, and
  `SIM_*`, and rejects dev/HIL commands such as `SET_PHONE_GPS`.
- `acceptance` profile: accepts release plus dev/HIL for broad bench
  validation.
- `esp32s3` is the normal firmware build and release artifact source.
- `esp32s3_acceptance` is the explicit dev/HIL profile alias.
- `tools/hw/nh02/flash.sh --profile acceptance` maps to
  `esp32s3_acceptance`; the wrapper defaults to `esp32s3`.
- `BOATLOCK_PIO_ENV=acceptance` maps to `esp32s3_acceptance`; an empty or
  unknown `BOATLOCK_PIO_ENV` is an error.
- `esp32s3_bno08x_sh2_uart` uses the normal command profile with SH2-UART
  compass support.
- `gpio_probe` and `uart_rvc_probe_rx12` are debug probe builds with no release
  artifact role.

Minimum validation order after profile-gate changes:

1. Local docs/static checks:
   - `bash -n tools/hw/nh02/flash.sh`
   - `git diff --check -- boatlock/platformio.ini tools/hw/nh02/flash.sh docs/BLE_PROTOCOL.md docs/HARDWARE_NH02.md skills/boatlock/references/validation.md boatlock/TODO.md`
   - `rg -n "esp32s3_acceptance|release|dev/HIL|SIM_|OTA|BOATLOCK_PIO_ENV|PlatformIO|nh02" boatlock/platformio.ini tools/hw/nh02/flash.sh docs/BLE_PROTOCOL.md docs/HARDWARE_NH02.md skills/boatlock/references/validation.md boatlock/TODO.md`
2. Local firmware/unit checks, run sequentially in one build directory:
   - native BLE command tests cover each profile's allow/deny matrix
   - native HIL tests cover `SIM_*` as a release-scope safe simulation mode
   - each PlatformIO firmware profile builds without relying on stale `.pio` artifacts
3. Normal firmware bench checks:
   - flash the normal firmware through `tools/hw/nh02/flash.sh`
   - run `tools/hw/nh02/acceptance.sh`
   - prove `SET_PHONE_GPS` is rejected with stable gate logs and no output motion
   - run Android `sim` smoke/check and verify `SIM` telemetry without motor/stepper output
4. BLE OTA bench checks:
   - build the normal firmware and update the target through phone BLE OTA by running `tools/hw/nh02/deploy.sh`
   - prove telemetry recovers after the ESP32 reboots into the uploaded image
5. Release-profile full HIL bench checks:
   - run `tools/hw/nh02/run-sim-suite.sh` when the standard full on-device HIL suite is required; it runs `SIM_LIST`, every listed scenario through `SIM_RUN:<id>,0`, `SIM_STATUS`, and `SIM_REPORT` on the normal firmware
6. Recovery check:
   - flash the normal firmware again through `tools/hw/nh02/flash.sh` and confirm the normal BLE reconnect/manual/anchor readiness smoke still uses the release surface only

Risk review items for the rollout:

- An acceptance image must not be left on the boat for water use because it exposes injected sensor and HIL commands.
- Wrapper output must distinguish profile-gated rejection from BLE timeout and parser failure.

## Useful Test Inventory

- Native firmware test directories live under `boatlock/test/`.
- High-signal suites for protocol/runtime work:
  - `test_ble_command_handler`
  - `test_gnss_quality_gate`
  - `test_anchor_supervisor`
  - `test_hil_sim`
  - `test_settings`
- Flutter tests live under `boatlock_ui/test/`.

## Version And Schema Sync

- Firmware version string lives in `boatlock/main.cpp` as `kFirmwareVersion`.
- Version consistency is checked by `tools/ci/check_firmware_version.py`.
- Release tags use `vX.Y.Z`, are cut from `release/vX.Y.x`, and are checked by `tools/ci/check_release_ref.py`.
- GitHub Releases, not GitHub Pages, are the standard release delivery channel.
- Settings schema version lives in `boatlock/Settings.h` as `Settings::VERSION`.
- Schema consistency is checked by `tools/ci/check_config_schema_version.py`.
- Release-note generation helpers live under `tools/ci/generate_release_notes.py`.

## Documentation Sync Rules

- If you change commands, telemetry fields, or status reasons, update `docs/BLE_PROTOCOL.md`.
- If you change settings defaults, ranges, or schema version, update `docs/CONFIG_SCHEMA.md`.
- If you change NVS/EEPROM write policy or persistence semantics, update the skill references and any operator docs that mention save behavior.
- If you change user-visible firmware behavior, update `CHANGELOG.md` and the relevant release note when appropriate.
- When prose docs disagree with code, fix the docs in the same change instead of preserving the mismatch.

## Wrapper Hygiene

- Repo bash wrappers must work under macOS bash 3.2 with `set -u`; do not expand empty arrays directly as `"${ARGS[@]}"`. Branch on `${#ARGS[@]}` before passing optional arg arrays.
