#include "Logger.h"
#include "BLEBoatLock.h"
#include <stdarg.h>

extern BLEBoatLock bleBoatLock;

void logMessage(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(buf);
    if (bleBoatLock.bleStatus == BLEBoatLock::CONNECTED) {
        bleBoatLock.sendLog(buf);
    }
}
