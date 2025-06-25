#include "FirmwareUpdater.h"
#include "Logger.h"

FirmwareUpdater firmwareUpdater;

void FirmwareUpdater::begin(size_t totalSize) {
    logMessage("[OTA] Begin update, size=%u\n", (unsigned)totalSize);
    if (!Update.begin(totalSize)) {
        logMessage("[OTA] Update begin failed\n");
        updating = false;
        return;
    }
    updating = true;
    written = 0;
}

void FirmwareUpdater::writeChunk(const uint8_t* data, size_t len) {
    if (!updating) return;
    Update.write(data, len);
    written += len;
}

void FirmwareUpdater::end() {
    if (!updating) return;
    if (Update.end(true)) {
        logMessage("[OTA] Update success, rebooting...\n");
        updating = false;
        ESP.restart();
    } else {
        logMessage("[OTA] Update failed: %s\n", Update.errorString());
        updating = false;
    }
}
