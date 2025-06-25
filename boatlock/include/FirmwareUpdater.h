#pragma once
#include <Arduino.h>
#include <Update.h>

class FirmwareUpdater {
public:
    void begin(size_t totalSize);
    void writeChunk(const uint8_t* data, size_t len);
    void end();
    bool inProgress() const { return updating; }
private:
    bool updating = false;
    size_t written = 0;
};

extern FirmwareUpdater firmwareUpdater;
