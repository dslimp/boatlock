#!/usr/bin/env python3
from __future__ import annotations

import argparse
import asyncio
import json
import threading
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Callable, Optional

try:
    from bleak import BleakClient, BleakScanner
except Exception as exc:  # pragma: no cover
    raise SystemExit(f"bleak import failed: {exc}")

try:
    import serial
except Exception:
    serial = None


@dataclass
class ScenarioResult:
    mode: str
    scenario: str
    status: dict
    report: dict


DEFAULT_SCENARIOS = [
    "S0_hold_still_good",
    "S1_current_0p4_good",
    "S2_current_0p8_hard",
    "S3_gusts",
    "S4_gnss_dropout_3s",
    "S5_position_jump_12m_once",
    "S6_hdop_degrade_then_recover",
    "S7_sats_drop",
    "S8_control_loop_stall",
    "S9_nan_heading_injection",
    "S10_random_1hz_noisy_hold",
    "S11_random_cross_current_hold",
    "S12_compass_dropout_5s",
    "S13_compass_flap_then_timeout",
    "S14_power_loss_recover",
    "S15_power_loss_double",
    "S16_display_loss_transient",
    "S17_actuator_derate_45pct",
    "S18_compass_loss_with_display_off",
    "S19_random_emergency_mix",
]


class LinePump:
    def __init__(self, log_path: Path):
        self.queue: asyncio.Queue[str] = asyncio.Queue()
        self.log_path = log_path
        self._buf = ""
        self._fh = log_path.open("w", encoding="utf-8")

    def close(self) -> None:
        self._fh.close()

    def feed_bytes(self, data: bytes) -> None:
        text = data.decode("utf-8", errors="replace")
        self._buf += text
        while "\n" in self._buf:
            line, self._buf = self._buf.split("\n", 1)
            line = line.replace("\x00", "").rstrip("\r")
            if not line:
                continue
            ts = time.strftime("%H:%M:%S")
            self._fh.write(f"{ts} {line}\n")
            self._fh.flush()
            self.queue.put_nowait(line)

    async def wait_for(self, pred: Callable[[str], bool], timeout: float) -> str:
        deadline = time.monotonic() + timeout
        while True:
            rem = deadline - time.monotonic()
            if rem <= 0:
                raise TimeoutError("wait_for timeout")
            line = await asyncio.wait_for(self.queue.get(), rem)
            if pred(line):
                return line


class SerialLogger:
    def __init__(self, port: str, out_path: Path):
        self.port = port
        self.out_path = out_path
        self._stop = threading.Event()
        self._thr: Optional[threading.Thread] = None

    def start(self) -> None:
        if serial is None:
            return

        def run() -> None:
            try:
                with serial.Serial(self.port, 115200, timeout=0.2) as ser, self.out_path.open(
                    "w", encoding="utf-8"
                ) as fh:
                    while not self._stop.is_set():
                        raw = ser.readline()
                        if not raw:
                            continue
                        line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                        ts = time.strftime("%H:%M:%S")
                        fh.write(f"{ts} {line}\n")
                        fh.flush()
            except Exception as exc:
                with self.out_path.open("a", encoding="utf-8") as fh:
                    fh.write(f"[SERIAL_LOGGER_ERROR] {exc}\n")

        self._thr = threading.Thread(target=run, daemon=True)
        self._thr.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thr:
            self._thr.join(timeout=1.0)


async def discover_address(name_substr: str, timeout: float) -> str:
    t_end = time.monotonic() + timeout
    while time.monotonic() < t_end:
        devices = await BleakScanner.discover(timeout=2.0)
        for d in devices:
            n = (d.name or "").lower()
            if name_substr.lower() in n:
                return d.address
    raise RuntimeError(f"device with name containing '{name_substr}' not found")


def parse_json_from_line(prefix: str, line: str) -> Optional[dict]:
    idx = line.find(prefix)
    if idx < 0:
        return None
    payload = line[idx + len(prefix) :].strip()
    try:
        return json.loads(payload)
    except Exception:
        return None


def extract_json_object(text: str) -> Optional[dict]:
    decoder = json.JSONDecoder()
    for i, ch in enumerate(text):
        if ch != "{":
            continue
        try:
            obj, _ = decoder.raw_decode(text[i:])
            if isinstance(obj, dict):
                return obj
        except Exception:
            continue
    return None


async def send_cmd(client: BleakClient, char_uuid: str, cmd: str) -> None:
    await client.write_gatt_char(char_uuid, cmd.encode("utf-8"), response=True)


async def wait_idle(client: BleakClient, cmd_uuid: str, pump: LinePump, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        await send_cmd(client, cmd_uuid, "SIM_STATUS")
        line = await pump.wait_for(lambda x: "[SIM] STATUS " in x, timeout=4.0)
        st = parse_json_from_line("[SIM] STATUS ", line) or {}
        if st.get("state") in {"IDLE", "DONE", "ABORTED"}:
            return
        await asyncio.sleep(0.6)
    raise TimeoutError("wait_idle timeout")


async def collect_report(client: BleakClient, cmd_uuid: str, pump: LinePump, timeout: float) -> dict:
    await send_cmd(client, cmd_uuid, "SIM_REPORT")
    chunks: list[str] = []
    last_chunk = time.monotonic()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            line = await asyncio.wait_for(pump.queue.get(), timeout=0.9)
        except asyncio.TimeoutError:
            if chunks and time.monotonic() - last_chunk > 0.8:
                break
            continue
        if "[SIM] REPORT " in line:
            chunks.append(line.split("[SIM] REPORT ", 1)[1])
            last_chunk = time.monotonic()
    payload = "".join(chunks)
    if not payload:
        raise RuntimeError("empty SIM_REPORT")
    obj = extract_json_object(payload)
    if obj is None:
        raise RuntimeError("cannot decode SIM_REPORT payload")
    return obj


async def run() -> int:
    ap = argparse.ArgumentParser(description="Run BoatLock on-hardware HIL over BLE")
    ap.add_argument("--address", default="", help="BLE address (optional, autodetect by name if empty)")
    ap.add_argument("--name", default="BoatLock", help="BLE device name substring for autodetect")
    ap.add_argument("--cmd-uuid", default="56ef")
    ap.add_argument("--log-uuid", default="78ab")
    ap.add_argument("--serial-port", default="/dev/cu.usbmodem2101")
    ap.add_argument("--modes", default="POSITION,POSITION_HEADING", help="Comma-separated")
    ap.add_argument("--scenarios", default=",".join(DEFAULT_SCENARIOS), help="Comma-separated scenario IDs")
    ap.add_argument("--status-poll-s", type=float, default=1.0)
    ap.add_argument("--out-dir", default="dist/hil")
    args = ap.parse_args()

    ts = datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    ble_log = out_dir / f"ble_{ts}.log"
    serial_log = out_dir / f"serial_{ts}.log"
    summary_json = out_dir / f"hil_summary_{ts}.json"

    serial_logger = SerialLogger(args.serial_port, serial_log)
    serial_logger.start()

    address = args.address.strip() or await discover_address(args.name.strip(), timeout=30.0)
    pump = LinePump(ble_log)

    results: list[ScenarioResult] = []
    modes = [m.strip().upper() for m in args.modes.split(",") if m.strip()]

    async with BleakClient(address, timeout=20.0) as client:
        await client.start_notify(args.log_uuid, lambda _h, data: pump.feed_bytes(data))
        try:
            await asyncio.sleep(0.6)
            await send_cmd(client, args.cmd_uuid, "SIM_ABORT")
            await wait_idle(client, args.cmd_uuid, pump, timeout=8.0)
            scenarios = [s.strip() for s in args.scenarios.split(",") if s.strip()]

            for mode in modes:
                await send_cmd(client, args.cmd_uuid, f"SET_ANCHOR_MODE:{mode}")
                await asyncio.sleep(0.3)
                await send_cmd(client, args.cmd_uuid, "SIM_ABORT")
                await wait_idle(client, args.cmd_uuid, pump, timeout=8.0)

                for sid in scenarios:
                    await send_cmd(client, args.cmd_uuid, f"SIM_RUN:{sid},0")
                    status = {}
                    while True:
                        await asyncio.sleep(args.status_poll_s)
                        await send_cmd(client, args.cmd_uuid, "SIM_STATUS")
                        line = await pump.wait_for(lambda x: "[SIM] STATUS " in x, timeout=5.0)
                        st = parse_json_from_line("[SIM] STATUS ", line)
                        if st and st.get("id") == sid:
                            status = st
                        if st and st.get("state") in {"DONE", "ABORTED"}:
                            break

                    report = await collect_report(client, args.cmd_uuid, pump, timeout=10.0)
                    results.append(ScenarioResult(mode=mode, scenario=sid, status=status, report=report))
        finally:
            try:
                await client.stop_notify(args.log_uuid)
            except Exception:
                pass

    pump.close()
    serial_logger.stop()

    out = {
        "timestamp": ts,
        "ble_log": str(ble_log.resolve()),
        "serial_log": str(serial_log.resolve()),
        "modes": modes,
        "results": [
            {
                "mode": r.mode,
                "scenario": r.scenario,
                "state": r.status.get("state"),
                "status": r.status,
                "report": r.report,
            }
            for r in results
        ],
    }
    summary_json.write_text(json.dumps(out, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
    print(str(summary_json.resolve()))
    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(run()))
