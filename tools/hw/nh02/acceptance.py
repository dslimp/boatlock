#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
import sys
import time
from pathlib import Path

BAUD_RATE = 115200
REQUIRED_CHECKS = (
    (
        "compass_ready",
        re.compile(r"^\[COMPASS\] ready=1 source=BNO08x-RVC\b.*\brx=12\b.*\bbaud=115200\b"),
    ),
    ("compass_heading_events", re.compile(r"^\[COMPASS\] heading events ready\b")),
    ("display_ready", re.compile(r"^\[DISPLAY\] ready=1\b")),
    ("settings_loaded", re.compile(r"^\[NVS\] settings loaded\b")),
    ("security_state", re.compile(r"^\[SEC\] paired=[01]\b")),
    ("ble_init", re.compile(r"^\[BLE\] init name=BoatLock service=12ab data=34cd cmd=56ef log=78ab\b")),
    ("ble_advertising", re.compile(r"^\[BLE\] advertising started\b")),
    ("stepper_cfg", re.compile(r"^\[STEP\] cfg\b")),
    ("stop_button", re.compile(r"^\[STOP\] button pin=\d+ active=LOW\b")),
)
GPS_UART_CHECK = ("gps_uart", re.compile(r"^\[GPS\] UART data detected\b"))
ERROR_PATTERNS = (
    ("compass_not_ready", re.compile(r"^\[COMPASS\] ready=0\b")),
    ("compass_lost", re.compile(r"^\[COMPASS\] lost\b")),
    ("compass_retry_failed", re.compile(r"^\[COMPASS\] retry ready=0\b")),
    ("display_not_ready", re.compile(r"^\[DISPLAY\] ready=0\b")),
    ("ble_advertising_failed", re.compile(r"^\[BLE\] advertising failed\b")),
    ("arduino_error_log", re.compile(r"^\[\s*\d+\]\[E\]\[")),
    ("panic", re.compile(r"(guru meditation|panic|assert|abort|backtrace|loadprohibited|storeprohibited)", re.I)),
)


def capture_lines(url: str, seconds: float) -> list[str]:
    import serial

    deadline = time.monotonic() + seconds
    lines: list[str] = []
    buffer = ""
    ser = serial.serial_for_url(url, BAUD_RATE, timeout=0.25)
    try:
        while time.monotonic() < deadline:
            chunk = ser.read(4096)
            if not chunk:
                continue
            text = chunk.decode("utf-8", errors="replace")
            buffer += text.replace("\r\n", "\n").replace("\r", "\n")
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                line = line.strip()
                if line:
                    lines.append(line)
    finally:
        ser.close()
    if buffer.strip():
        lines.append(buffer.strip())
    return lines


def analyze_lines(lines: list[str], require_gps_uart: bool = True) -> dict:
    matched: dict[str, str] = {}
    errors: list[dict[str, str]] = []
    warnings: list[str] = []
    error_seen: set[str] = set()

    for raw in lines:
        line = raw.strip()
        if not line:
            continue
        for key, pattern in REQUIRED_CHECKS:
            if key not in matched and pattern.search(line):
                matched[key] = line
        if GPS_UART_CHECK[0] not in matched and GPS_UART_CHECK[1].search(line):
            matched[GPS_UART_CHECK[0]] = line
        for key, pattern in ERROR_PATTERNS:
            if key in error_seen:
                continue
            if pattern.search(line):
                error_seen.add(key)
                errors.append({"key": key, "line": line})

    missing = [key for key, _ in REQUIRED_CHECKS if key not in matched]
    if require_gps_uart:
        if GPS_UART_CHECK[0] not in matched:
            missing.append(GPS_UART_CHECK[0])
    elif GPS_UART_CHECK[0] not in matched:
        warnings.append("gps_uart_not_seen")

    if not lines:
        errors.append({"key": "no_serial_data", "line": ""})

    result = {
        "pass": not missing and not errors,
        "lineCount": len(lines),
        "missing": missing,
        "warnings": warnings,
        "errors": errors,
        "matched": matched,
        "lines": lines,
    }
    return result


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", required=True)
    parser.add_argument("--seconds", type=float, default=12.0)
    parser.add_argument("--allow-missing-gps-uart", action="store_true")
    parser.add_argument("--log-out")
    parser.add_argument("--json-out")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    lines = capture_lines(args.url, args.seconds)
    result = analyze_lines(lines, require_gps_uart=not args.allow_missing_gps_uart)

    if args.log_out:
        Path(args.log_out).write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")

    json_text = json.dumps(result, ensure_ascii=True, indent=2)
    if args.json_out:
        Path(args.json_out).write_text(json_text + "\n", encoding="utf-8")

    status = "PASS" if result["pass"] else "FAIL"
    print(f"[ACCEPT] {status} lines={result['lineCount']}")
    for key, line in result["matched"].items():
        print(f"[ACCEPT] match {key}: {line}")
    for item in result["errors"]:
        suffix = f": {item['line']}" if item["line"] else ""
        print(f"[ACCEPT] error {item['key']}{suffix}")
    for item in result["missing"]:
        print(f"[ACCEPT] missing {item}")
    for item in result["warnings"]:
        print(f"[ACCEPT] warn {item}")

    if not result["pass"]:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
