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

## Deploy path

1. Local `pio run -e esp32s3` builds the firmware.
2. `flash.sh` copies `bootloader.bin`, `partitions.bin`, and `firmware.bin` to `nh02`.
3. Remote helper `/opt/boatlock-hw/bin/boatlock-flash-esp32s3.sh` temporarily stops the RFC2217 bridge, flashes the ESP32-S3 with remote `esptool`, then starts the bridge again.
4. `acceptance.sh` optionally forces a clean reset, captures boot logs over RFC2217, checks required boot markers, and fails on fatal log patterns.

## Android USB Path

1. Rerun `tools/hw/nh02/install.sh` after changing tracked Android helpers.
2. `tools/hw/nh02/android-install.sh` ensures `adb` exists on `nh02`.
3. `tools/hw/nh02/android-status.sh` checks whether the phone is visible over USB and whether `adb devices -l` sees it.
4. If the phone appears only as `MTP` or a vendor USB device and not in `adb devices`, the cable path is alive but USB debugging is still off on the phone.

## Debug path

- Runtime logs go through the persistent RFC2217 bridge on `nh02`.
- `monitor.sh` uses local `pyserial-miniterm` against the RFC2217 endpoint.
- If the monitor path fails, inspect `status.sh` before touching the USB device manually.
