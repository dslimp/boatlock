#include "FirmwareUpdater.h"
#include "Logger.h"

void startOtaUpdate(size_t size) {
    firmwareUpdater.begin(size);
}

void otaWriteChunk(const uint8_t* data, size_t len) {
    firmwareUpdater.writeChunk(data, len);
}

void finishOtaUpdate() {
    firmwareUpdater.end();
}
