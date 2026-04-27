from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import acceptance


GOOD_LOG = [
    "[COMPASS] reset pin=13 pulse=1",
    "[COMPASS] ready=1 source=BNO08x-RVC rx=12 baud=115200",
    "[COMPASS] heading events ready age=0",
    "[DISPLAY] ready=1",
    "[NVS] settings loaded (ver=24)",
    "[SEC] paired=0",
    "[BLE] init name=BoatLock service=12ab data=34cd cmd=56ef log=78ab",
    "[BLE] advertising started",
    "[STEP] cfg maxSpd=700 accel=250 spr=7200",
    "[STOP] button pin=15 active=LOW",
    "[GPS] UART data detected RX=17 TX=18 baud=9600",
]


def test_acceptance_passes_on_expected_boot_log():
    result = acceptance.analyze_lines(GOOD_LOG)

    assert result["pass"] is True
    assert result["missing"] == []
    assert result["errors"] == []
    assert result["matched"]["compass_ready"].startswith("[COMPASS] ready=1")
    assert result["matched"]["compass_heading_events"].startswith(
        "[COMPASS] heading events ready"
    )


def test_acceptance_fails_when_compass_has_no_heading_events():
    result = acceptance.analyze_lines(
        [line for line in GOOD_LOG if not line.startswith("[COMPASS] heading events ready")]
    )

    assert result["pass"] is False
    assert "compass_heading_events" in result["missing"]


def test_acceptance_fails_on_missing_ble_and_error_log():
    result = acceptance.analyze_lines(
        [
            "[COMPASS] ready=0 source=BNO08x-RVC rx=12 baud=115200",
            "[DISPLAY] ready=1",
            "[NVS] settings loaded (ver=24)",
            "[SEC] paired=1",
            "[BLE] advertising failed",
            "[STEP] cfg maxSpd=700 accel=250 spr=7200",
            "[STOP] button pin=15 active=LOW",
        ]
    )

    assert result["pass"] is False
    assert "ble_init" in result["missing"]
    assert "gps_uart" in result["missing"]
    assert {item["key"] for item in result["errors"]} >= {
        "compass_not_ready",
        "ble_advertising_failed",
    }


def test_acceptance_fails_on_arduino_error_log():
    result = acceptance.analyze_lines(
        GOOD_LOG + ["[ 13792][E][HardwareSerial.cpp:513] uartRead returned Error -1"]
    )

    assert result["pass"] is False
    assert {item["key"] for item in result["errors"]} == {"arduino_error_log"}


def test_acceptance_fails_on_compass_runtime_loss():
    result = acceptance.analyze_lines(
        GOOD_LOG
        + [
            "[COMPASS] lost reason=FIRST_EVENT_TIMEOUT age=4294967295",
            "[COMPASS] retry ready=0 source=BNO08x-RVC rx=12 baud=115200 reset=1",
        ]
    )

    assert result["pass"] is False
    assert {item["key"] for item in result["errors"]} >= {
        "compass_lost",
        "compass_retry_failed",
    }
