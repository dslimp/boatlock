#include "Logger.h"
#include "BLEBoatLock.h"
#include "RuntimeLogText.h"
#include "SdCardLogger.h"
#include <stdarg.h>

extern BLEBoatLock bleBoatLock;

void logMessage(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    const int formatResult = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    const size_t len = runtimeLogFormattedLength(formatResult, sizeof(buf));
    if (len == 0) {
        return;
    }
    sdCardLogger.enqueueLog(buf, len);
    Serial.write(reinterpret_cast<const uint8_t*>(buf), len);
    if (bleBoatLock.bleStatus == BLEBoatLock::CONNECTED &&
        runtimeLogShouldForwardToBle(buf)) {
        bleBoatLock.sendLog(buf);
    }
}
