import os

Import("env")


def require_env(name):
    value = os.environ.get(name, "")
    if not value:
        raise RuntimeError(f"{name} is required for esp32s3_debug_wifi_ota")
    return value


def define_string(name, value):
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f"-D{name}=\\\"{escaped}\\\""


ssid = require_env("BOATLOCK_WIFI_SSID")
wifi_pass = require_env("BOATLOCK_WIFI_PASS")
ota_pass = require_env("BOATLOCK_OTA_PASS")

env.Append(
    BUILD_FLAGS=[
        "-DBOATLOCK_DEBUG_WIFI_OTA=1",
        define_string("BOATLOCK_DEBUG_WIFI_SSID", ssid),
        define_string("BOATLOCK_DEBUG_WIFI_PASS", wifi_pass),
        define_string("BOATLOCK_DEBUG_OTA_PASS", ota_pass),
    ],
)
