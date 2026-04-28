# BoatLock Release Process

BoatLock releases are cut from stabilization branches and delivered through
GitHub Releases. GitHub Pages is not a firmware-update channel.

## Branches

- `main` is the integration branch.
- `release/vX.Y.x` is the stabilization branch for a patch series.
- Tags use `vX.Y.Z` and must be created from a commit contained in the matching
  `release/vX.Y.x` branch.

Example:

```bash
git checkout main
git pull --ff-only
git checkout -b release/v0.2.x
git push -u origin release/v0.2.x
```

## Release Gate

Before tagging:

```bash
python3 tools/ci/check_firmware_version.py
python3 tools/ci/check_config_schema_version.py
pytest tools/ci/test_*.py
cd boatlock && platformio test -e native
cd ../boatlock_ui && flutter test
```

For hardware-bearing changes, also run the `nh02` and Android acceptance
wrappers documented in `docs/HARDWARE_NH02.md`.

## Tagging

On the release branch, bump `kFirmwareVersion`, `CHANGELOG.md`, and
`docs/releases/<version>.md` together. Then tag and push:

```bash
git checkout release/v0.2.x
git pull --ff-only
python3 tools/ci/check_firmware_version.py --tag v0.2.1
python3 tools/ci/check_release_ref.py --tag v0.2.1 --require-current-commit
git tag v0.2.1
git push origin release/v0.2.x v0.2.1
```

CI publishes the GitHub Release from `v*` tags only.

## Release Assets

The release job flattens CI artifacts into unique GitHub Release asset names:

- `manifest.json`
- `firmware-esp32s3.bin`
- `bootloader-esp32s3.bin`
- `partitions-esp32s3.bin`
- `firmware-esp32s3.elf`
- `BUILD_INFO-esp32s3.txt`
- `SHA256SUMS-esp32s3.txt`
- Android/macOS/web app artifacts and simulation reports

The release app reads `manifest.json` from the latest GitHub Release for
`dslimp/boatlock`. The manifest points at `firmware-esp32s3.bin` and must use
`channel=release`, `branch=release/vX.Y.x`, `platformioEnv=esp32s3`, and
`commandProfile=release`.

## Android App Signing

Tagged Android release APKs must be signed with the same long-lived release
certificate for the lifetime of the app. Android accepts an APK update only when
the new APK certificate matches the installed app certificate, so GitHub Release
APKs must not use the runner's generated debug keystore.

Keep the private keystore outside the repository and configure these GitHub
Secrets before pushing a `v*` tag:

- `BOATLOCK_ANDROID_KEYSTORE_BASE64`
- `BOATLOCK_ANDROID_KEYSTORE_PASSWORD`
- `BOATLOCK_ANDROID_KEY_ALIAS`
- `BOATLOCK_ANDROID_KEY_PASSWORD`

Example secret setup from a local private keystore:

```bash
mkdir -p ~/.boatlock
keytool -genkeypair \
  -v \
  -keystore ~/.boatlock/boatlock-release.jks \
  -storetype JKS \
  -keyalg RSA \
  -keysize 4096 \
  -validity 10000 \
  -alias boatlock-release
base64 < ~/.boatlock/boatlock-release.jks | gh secret set BOATLOCK_ANDROID_KEYSTORE_BASE64
gh secret set BOATLOCK_ANDROID_KEYSTORE_PASSWORD
gh secret set BOATLOCK_ANDROID_KEY_ALIAS
gh secret set BOATLOCK_ANDROID_KEY_PASSWORD
```

Branch and pull-request Android builds may fall back to debug signing for CI
coverage. A tagged `v*` build fails if release signing secrets are missing.

## Local App Builds

```bash
tools/android/build-app-apk.sh
tools/macos/build-app.sh
tools/macos/acceptance.sh --static-only
```

For bench-only `nh02` OTA proof with a temporary served manifest:

```bash
tools/hw/nh02/android-run-app-check.sh \
  --ota-latest-release \
  --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin \
  --wait-secs 1800
```
