from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import acceptance


GOOD_LOG = [
    "[I2C] SDA=47 SCL=48 devices=1: 0x4B(BNO08x)",
    "[COMPASS] ready=1 source=BNO08x addr=0x4B",
    "[DISPLAY] ready=1",
    "[EEPROM] settings loaded (ver=21)",
    "[SEC] paired=0",
    "[BLE] init name=BoatLock service=12ab data=34cd cmd=56ef log=78ab",
    "[BLE] advertising started",
    "[STEP] cfg maxSpd=700 accel=250 spr=4096",
    "[STOP] button pin=15 active=LOW",
    "[GPS] UART data detected RX=17 TX=18 baud=9600",
]


def test_acceptance_passes_on_expected_boot_log():
    result = acceptance.analyze_lines(GOOD_LOG)

    assert result["pass"] is True
    assert result["missing"] == []
    assert result["errors"] == []
    assert result["matched"]["compass_ready"].startswith("[COMPASS] ready=1")


def test_acceptance_fails_on_missing_ble_and_error_log():
    result = acceptance.analyze_lines(
        [
            "[I2C] SDA=47 SCL=48 devices=0: none",
            "[COMPASS] ready=0 source=BNO08x",
            "[DISPLAY] ready=1",
            "[EEPROM] settings loaded (ver=21)",
            "[SEC] paired=1",
            "[BLE] advertising failed",
            "[STEP] cfg maxSpd=700 accel=250 spr=4096",
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
