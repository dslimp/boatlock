# NH02 Hardware Bench

`nh02` (`192.168.88.61`) is the hardware bench host for the ESP32-S3 stand with GPS, display, compass, and stepper connected over USB.

## Remote runtime

- Remote USB device:
  - `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:88:E0:03:BA:5C-if00`
- Remote tool runtime:
  - `/opt/boatlock-hw/.venv`
- Remote runtime root:
  - `/opt/boatlock-hw`
- Remote RFC2217 service:
  - `boatlock-esp32s3-rfc2217.service`
- RFC2217 URL:
  - `rfc2217://192.168.88.61:4000?ign_set_control`

## Change boundary on nh02

- BoatLock-owned host changes are limited to:
  - `/opt/boatlock-hw/`
  - `/etc/systemd/system/boatlock-esp32s3-rfc2217.service`
- Android USB validation on `nh02` may additionally install the Ubuntu `adb` package through the tracked remote helper when that workflow is explicitly used.
- `install.sh` must be rerun after changing the tracked service or remote flash helper.
- `install.sh` must also be rerun after changing tracked remote Android helpers.
- `install.sh` restarts only `boatlock-esp32s3-rfc2217.service`; it does not touch unrelated services on `nh02`.

## Local workflow

- Install or refresh the remote bench runtime:
  - `tools/hw/nh02/install.sh`
- Build locally and flash through `nh02`:
  - `tools/hw/nh02/flash.sh`
- Flash the current build without rebuilding:
  - `tools/hw/nh02/flash.sh --no-build`
- Run post-flash hardware acceptance:
  - `tools/hw/nh02/acceptance.sh`
- Run acceptance without forcing a reset first:
  - `tools/hw/nh02/acceptance.sh --no-reset`
- Open the serial monitor through RFC2217:
  - `tools/hw/nh02/monitor.sh`
- Check service/device status on the bench host:
  - `tools/hw/nh02/status.sh`
- Install or refresh Android USB tooling on `nh02`:
  - `tools/hw/nh02/android-install.sh`
- Check Android USB and `adb` visibility on `nh02`:
  - `tools/hw/nh02/android-status.sh`
- Build, install/update, and run the Android BLE smoke app through `nh02`:
  - `tools/hw/nh02/android-run-smoke.sh`
- Build, install/update, and run the Android BLE reconnect smoke app through `nh02`:
  - `tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130`
- Build, install/update, and prove phone recovery after ESP32 reboot:
  - `tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130`
- Build, install/update, and prove compass calibration command delivery:
  - `tools/hw/nh02/android-run-app-e2e.sh --compass --wait-secs 130`
- Build, install/update, and prove BLE-visible hardware GPS fix:
  - `tools/hw/nh02/android-run-app-e2e.sh --gps --wait-secs 180`
- Run the smoke app without reinstalling the APK:
  - `tools/hw/nh02/android-run-smoke.sh --no-install`

## Deploy path

1. Local `pio run -e esp32s3` builds the firmware.
2. `flash.sh` copies `bootloader.bin`, `partitions.bin`, and `firmware.bin` to `nh02`.
3. Remote helper `/opt/boatlock-hw/bin/boatlock-flash-esp32s3.sh` temporarily stops the RFC2217 bridge, flashes the ESP32-S3 with remote `esptool`, then starts the bridge again.
4. `acceptance.sh` optionally forces a clean reset, captures boot logs over RFC2217, checks required boot markers, and fails on fatal log patterns.

## Android USB Path

1. Rerun `tools/hw/nh02/install.sh` after changing tracked Android helpers.
2. `tools/hw/nh02/android-install.sh` ensures `adb` exists on `nh02`.
3. `tools/hw/nh02/android-status.sh` checks whether the phone is visible over USB and whether `adb devices -l` sees it.
4. `tools/hw/nh02/android-run-smoke.sh` copies the smoke APK to `nh02`, installs or updates it with remote `adb`, launches the app, and waits for the `BOATLOCK_SMOKE_RESULT` log line.
5. `tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` additionally waits for first telemetry, cycles phone Bluetooth through ADB, and requires telemetry recovery without restarting the app.
6. `tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` waits for first telemetry, resets the ESP32-S3 with the tracked remote reset helper, and requires telemetry recovery without restarting the app.
7. `tools/hw/nh02/android-run-app-e2e.sh --compass --wait-secs 130` sends safe compass-service commands (`COMPASS_CAL_START`, `COMPASS_DCD_AUTOSAVE_OFF`, `COMPASS_DCD_SAVE`) and requires device log acknowledgements.
8. `tools/hw/nh02/android-run-app-e2e.sh --gps --wait-secs 180` waits for production-app BLE telemetry with non-zero valid coordinates and GNSS quality `>0`; this can run while ESP32 is powered away from USB if BLE remains reachable.
9. If the phone appears only as `MTP` or a vendor USB device and not in `adb devices`, the cable path is alive but USB debugging is still off on the phone.

## Xiaomi Install Note

- On the current Xiaomi test phone, the first `adb install` was blocked by MIUI policy with `INSTALL_FAILED_USER_RESTRICTED`.
- The tracked `adb install -r` update path succeeded after the phone-side MIUI flow was satisfied: Xiaomi account, inserted SIM card, and `Install via USB` approval.
- Treat first-install policy and later USB update as separate checkpoints; do not assume the first failure means all future `adb install -r` updates are blocked.

## Debug path

- Runtime logs go through the persistent RFC2217 bridge on `nh02`.
- `monitor.sh` uses local `pyserial-miniterm` against the RFC2217 endpoint.
- If the monitor path fails, inspect `status.sh` before touching the USB device manually.

## Compass Wiring

- Accepted bench compass transport is currently BNO08x UART-RVC.
- Current `nh02` wiring:
  - BNO08x `SDA/TX` -> ESP32-S3 `GPIO12`
  - BNO08x `RST` -> ESP32-S3 `GPIO13`
  - BNO08x `P0/PS0` -> `3V3`
  - BNO08x `P1/PS1` -> `GND`
  - UART baud `115200`
- SH2-UART DCD migration target:
  - BNO08x `TXO` -> ESP32-S3 `GPIO12`
  - BNO08x `RXI` -> ESP32-S3 `GPIO11`
  - BNO08x `RST` -> ESP32-S3 `GPIO13`
  - BNO08x `P0/PS0` -> `GND`
  - BNO08x `P1/PS1` -> `3V3`
  - build with `BOATLOCK_PIO_ENV=esp32s3_bno08x_sh2_uart`
- Full DCD/tare functionality is not accepted on the current RVC wiring. RVC can only prove command delivery with `ok=0`; SH2-UART acceptance needs the wiring above plus a fresh flash of the explicit SH2 target.
- `GPIO12` was verified by grounding the RX line and by reading live RVC frames with `uart_rvc_probe_rx12`.
- `GPIO13` was verified as reset by grounding the reset wire and watching `gpio_probe` transitions.
- Hardware acceptance requires `[COMPASS] ready=1 source=BNO08x-RVC rx=12 ... baud=115200` plus `[COMPASS] heading events ready`.
- The old ESP32-S3 I2C compass path is removed from production firmware. Historical failure notes are kept in `WORKLOG.md`, not as a fallback path.
