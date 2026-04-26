#pragma once

#include <Arduino.h>

namespace DebugWifiOta {

using OtaStartCallback = void (*)();

void begin(OtaStartCallback onStart = nullptr);
void loop();
bool enabled();
bool wifiConnected();
const char* ipString();
bool bleMayStart();

} // namespace DebugWifiOta
