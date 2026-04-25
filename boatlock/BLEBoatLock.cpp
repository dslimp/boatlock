#include "BLEBoatLock.h"
#include "BleAdvertisingWatchdog.h"
#include "Logger.h"
#include "RuntimeBleCommandLog.h"
#include "RuntimeBleCommandQueue.h"
#include "RuntimeBleConnectionLog.h"
#include "RuntimeBleDataPacket.h"
#include "RuntimeBleLogText.h"
#include <cstring>

// --- Server callbacks ---
class BLEBoatLock::ServerCallbacks : public NimBLEServerCallbacks {
    BLEBoatLock* parent;
public:
    ServerCallbacks(BLEBoatLock* p) : parent(p) {}
    void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
        const bool firstClient = !server || server->getConnectedCount() <= 1;
        if (parent) {
            parent->bleStatus = CONNECTED;
            if (firstClient) {
                parent->clearQueuedData();
                parent->dataNotifyEnabled = false;
                parent->logNotifyEnabled = false;
                parent->streamEnabled = false;
                parent->lastDataNotifyMs = 0;
                parent->lastLogNotifyMs = 0;
                parent->lastConnParamReqMs = 0;
                parent->connEstablishedMs = millis();
            }
        }

        // Request stable params for control traffic.
        // Units: interval 1.25ms, timeout 10ms.
        // 24..48 => 30..60ms interval, timeout 300 => 3s.
        server->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 300);
        server->setDataLen(connInfo.getConnHandle(), 251);
        const std::string address = connInfo.getAddress().toString();
        char line[128];
        if (runtimeBleFormatConnectLog(line,
                                       sizeof(line),
                                       address.c_str(),
                                       connInfo.getMTU(),
                                       connInfo.getConnInterval(),
                                       connInfo.getConnTimeout()) > 0) {
            logMessage("%s", line);
        }
    }

    void onMTUChange(uint16_t mtu, NimBLEConnInfo& connInfo) override {
        NimBLEDevice::getServer()->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 300);
        logMessage("[BLE] MTU updated: %u\n", mtu);
    }

    void onConnParamsUpdate(NimBLEConnInfo& connInfo) override {
        logMessage("[BLE] Conn params updated: int=%u lat=%u timeout=%u\n",
            connInfo.getConnInterval(),
            connInfo.getConnLatency(),
            connInfo.getConnTimeout());
    }

    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
        const bool stillConnected = server && server->getConnectedCount() > 0;
        if (parent) {
            parent->bleStatus = stillConnected ? CONNECTED : ADVERTISING;
            if (!stillConnected) {
                parent->clearQueuedData();
                parent->dataNotifyEnabled = false;
                parent->logNotifyEnabled = false;
                parent->streamEnabled = false;
                parent->lastConnParamReqMs = 0;
                parent->connEstablishedMs = 0;
            }
        }
        const std::string address = connInfo.getAddress().toString();
        char line[128];
        if (runtimeBleFormatDisconnectLog(line,
                                          sizeof(line),
                                          address.c_str(),
                                          reason,
                                          server ? server->getConnectedCount() : 0) > 0) {
            logMessage("%s", line);
        }
        NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
        const bool advOk = advertising && (advertising->isAdvertising() || advertising->start());
        logMessage("[BLE] Restart advertising %s\n", advOk ? "started" : "failed");
    }
};

// --- Characteristic for command/param requests ---
class BLEBoatLock::CmdCallbacks : public NimBLECharacteristicCallbacks {
    BLEBoatLock* parent;
public:
    CmdCallbacks(BLEBoatLock* p) : parent(p) {}
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo&) override {
        std::string command = pCharacteristic->getValue();
        if (runtimeBleShouldLogCommand(command)) {
            const std::string logCommand = runtimeBleLogCommandText(command);
            logMessage("[BLE] Control point: %s\n", logCommand.c_str());
        }
        if (parent) parent->handleControlPoint(command);
    }
};

// --- Track CCCD subscription state for notify characteristics ---
class BLEBoatLock::NotifyCallbacks : public NimBLECharacteristicCallbacks {
    BLEBoatLock* parent;
    bool isLog;
public:
    NotifyCallbacks(BLEBoatLock* p, bool logChar) : parent(p), isLog(logChar) {}

    void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) override {
        const bool enabled = (subValue & 0x01) || (subValue & 0x02);
        if (!parent) return;
        if (isLog) {
            parent->logNotifyEnabled = enabled;
        } else {
            parent->dataNotifyEnabled = enabled;
        }
        logMessage("[BLE] subscribe %s=%d sub=0x%04X\n", isLog ? "log" : "data", enabled ? 1 : 0, subValue);
    }
};

BLEBoatLock::BLEBoatLock() {}

void BLEBoatLock::begin() {
    NimBLEDevice::init("BoatLock");
    NimBLEDevice::setSecurityAuth(true, true, true);
    logMessage("[BLE] init name=BoatLock service=12ab data=34cd cmd=56ef log=78ab\n");
    // Large MTU (512) was unstable with CoreBluetooth under high traffic.
    NimBLEDevice::setMTU(247);
    NimBLEDevice::setPower(9);
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(this));
    pService = pServer->createService("12ab");

    // Live state characteristic: compact binary telemetry frames.
    pDataChar = pService->createCharacteristic(
        "34cd", NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
    );
    pDataChar->setCallbacks(new NotifyCallbacks(this, false));

    // Control point: stream control and runtime commands.
    pCmdChar = pService->createCharacteristic(
        "56ef", NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pCmdChar->setCallbacks(new CmdCallbacks(this));

    // Log char: отправка строк логов
    pLogChar = pService->createCharacteristic(
        "78ab", NIMBLE_PROPERTY::NOTIFY
    );
    pLogChar->setCallbacks(new NotifyCallbacks(this, true));

    pService->start();
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->setName("BoatLock");
    pAdvertising->setMinInterval(80);
    pAdvertising->setMaxInterval(160);
    pAdvertising->setPreferredParams(24, 48);
    pAdvertising->enableScanResponse(true);
    const bool advOk = pAdvertising->start();
    bleStatus = ADVERTISING;
    logMessage("[BLE] advertising %s\n", advOk ? "started" : "failed");

    if (!cmdQueue) {
        cmdQueue = xQueueCreate(kCmdQueueLen, kCmdMaxLen);
        if (!cmdQueue) {
            logMessage("[BLE] command queue create failed\n");
        }
    }
    if (!logQueue) {
        logQueue = xQueueCreate(kLogQueueLen, kLogMaxLen);
        if (!logQueue) {
            logMessage("[BLE] log queue create failed\n");
        }
    }
    if (!dataQueue) {
        dataQueue = xQueueCreate(kDataQueueLen, sizeof(DataPacket));
        if (!dataQueue) {
            logMessage("[BLE] data queue create failed\n");
        }
    }
}

void BLEBoatLock::setTelemetryProvider(TelemetryProvider provider) {
    telemetryProvider = provider;
}

void BLEBoatLock::setCommandHandler(CommandHandler handler) {
    cmdHandler = handler;
}

void BLEBoatLock::loop() {
    processQueuedCommands();
    processQueuedData();
    maintainAdvertising();
    maintainConnParams();
    processQueuedLogs();
}

void BLEBoatLock::handleControlPoint(const std::string& command) {
    if (command == "STREAM_START") {
        streamEnabled = true;
        clearQueuedData();
        enqueueLiveFrame();
        logMessage("[BLE] stream start\n");
        return;
    }

    if (command == "STREAM_STOP") {
        streamEnabled = false;
        clearQueuedData();
        logMessage("[BLE] stream stop\n");
        return;
    }

    if (command == "SNAPSHOT") {
        clearQueuedData();
        enqueueLiveFrame();
        return;
    }

    enqueueCommand(command);
}

void BLEBoatLock::enqueueCommand(const std::string& cmd) {
    if (!cmdQueue) {
        return;
    }

    char payload[kCmdMaxLen];
    if (!runtimeBleCopyCommandForQueue(payload, sizeof(payload), cmd)) {
        const std::string logCommand = runtimeBleLogCommandText(cmd);
        logMessage("[BLE] command queue reject reason=too_long command=%s\n", logCommand.c_str());
        return;
    }

    if (xQueueSend(cmdQueue, payload, 0) != pdTRUE) {
        const std::string logCommand = runtimeBleLogCommandText(std::string(payload));
        logMessage("[BLE] command queue full, dropped: %s\n", logCommand.c_str());
    }
}

void BLEBoatLock::processQueuedCommands() {
    if (!cmdQueue || !cmdHandler) {
        return;
    }

    char payload[kCmdMaxLen];
    while (xQueueReceive(cmdQueue, payload, 0) == pdTRUE) {
        payload[kCmdMaxLen - 1] = '\0';
        cmdHandler(std::string(payload));
    }
}

void BLEBoatLock::clearQueuedData() {
    if (!dataQueue) {
        return;
    }

    DataPacket payload;
    while (xQueueReceive(dataQueue, &payload, 0) == pdTRUE) {
    }
}

void BLEBoatLock::enqueueDataPacket(const std::vector<uint8_t>& payload) {
    if (!dataQueue ||
        runtimeBleFixedNotifyPayloadLength(payload.size(), kDataMaxLen) == 0) {
        return;
    }

    DataPacket item;
    item.len = (uint16_t)payload.size();
    memcpy(item.bytes, payload.data(), payload.size());

    if (xQueueSend(dataQueue, &item, 0) == pdTRUE) {
        return;
    }

    DataPacket dropped;
    if (xQueueReceive(dataQueue, &dropped, 0) == pdTRUE) {
        (void)xQueueSend(dataQueue, &item, 0);
    }
}

void BLEBoatLock::enqueueLiveFrame() {
    if (!telemetryProvider) {
        return;
    }

    const RuntimeBleLiveTelemetry telemetry = telemetryProvider();
    enqueueDataPacket(runtimeBleEncodeLiveFrame(telemetry, ++liveSequence));
}

void BLEBoatLock::processQueuedData() {
    if (!dataQueue || !pDataChar || bleStatus != CONNECTED || !dataNotifyEnabled) {
        return;
    }

    const unsigned long now = millis();
    if (now - lastDataNotifyMs < kMinDataNotifyGapMs) {
        return;
    }

    DataPacket payload;
    if (xQueueReceive(dataQueue, &payload, 0) != pdTRUE) {
        return;
    }

    const size_t payloadLen = runtimeBleFixedNotifyPayloadLength(payload.len, kDataMaxLen);
    if (payloadLen == 0) {
        return;
    }

    std::string value(reinterpret_cast<const char*>(payload.bytes), payloadLen);
    pDataChar->setValue(value);
    if (pDataChar->notify()) {
        lastDataNotifyMs = now;
    }
}

void BLEBoatLock::enqueueLogLine(const char* line) {
    if (!line || !logQueue) {
        return;
    }

    char payload[kLogMaxLen];
    runtimeBlePrepareLogPayload(payload, sizeof(payload), line);

    if (xQueueSend(logQueue, payload, 0) == pdTRUE) {
        return;
    }

    // Queue full: drop oldest and keep most recent logs.
    char dropped[kLogMaxLen];
    if (xQueueReceive(logQueue, dropped, 0) == pdTRUE) {
        (void)xQueueSend(logQueue, payload, 0);
    }
}

void BLEBoatLock::processQueuedLogs() {
    if (!logQueue || !pLogChar || bleStatus != CONNECTED || !logNotifyEnabled) {
        return;
    }

    const unsigned long now = millis();
    if (now - lastLogNotifyMs < kMinLogNotifyGapMs) {
        return;
    }

    char payload[kLogMaxLen];
    if (xQueueReceive(logQueue, payload, 0) != pdTRUE) {
        return;
    }

    const std::string value = runtimeBleLogValue(payload, kLogMaxLen);
    if (value.empty()) {
        return;
    }

    pLogChar->setValue(value);
    if (pLogChar->notify()) {
        lastLogNotifyMs = now;
    }
}

void BLEBoatLock::maintainConnParams() {
    if (!pServer || bleStatus != CONNECTED || pServer->getConnectedCount() == 0) {
        return;
    }

    const unsigned long now = millis();
    const unsigned long sinceConnect = (connEstablishedMs > 0 && now >= connEstablishedMs)
                                           ? (now - connEstablishedMs)
                                           : 0UL;
    const unsigned long reqGapMs = (sinceConnect < 5000UL) ? 250UL : 1500UL;
    if (now - lastConnParamReqMs < reqGapMs) {
        return;
    }
    lastConnParamReqMs = now;

    NimBLEConnInfo conn = pServer->getPeerInfo((uint8_t)0);
    if (conn.getConnTimeout() >= 200) {
        return;
    }

    // Keep asking for a safer supervision timeout (3s) if central negotiated too low.
    pServer->updateConnParams(conn.getConnHandle(), 24, 48, 0, 300);
    logMessage("[BLE] Conn params re-request: int=%u timeout=%u -> timeout=300\n",
               conn.getConnInterval(), conn.getConnTimeout());
}

void BLEBoatLock::maintainAdvertising() {
    if (!pServer) {
        return;
    }

    const unsigned long now = millis();
    if (now - lastAdvertisingWatchdogMs < kAdvertisingWatchdogGapMs) {
        return;
    }
    lastAdvertisingWatchdogMs = now;

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    if (!advertising) {
        return;
    }

    const bool hasClients = pServer->getConnectedCount() > 0;
    const bool advertisingActive = advertising->isAdvertising();
    const bool statusConnected = (bleStatus == CONNECTED);
    const BleAdvertisingWatchdogAction action =
        bleAdvertisingWatchdogAction(statusConnected, hasClients, advertisingActive);

    if (action == BleAdvertisingWatchdogAction::NONE) {
        return;
    }

    if (action == BleAdvertisingWatchdogAction::MARK_CONNECTED) {
        bleStatus = CONNECTED;
        return;
    }

    if (action == BleAdvertisingWatchdogAction::START_CONNECTED_ADVERTISING) {
        const bool advOk = advertising->start();
        logMessage("[BLE] connected advertising restart %s\n", advOk ? "started" : "failed");
        return;
    }

    clearQueuedData();
    dataNotifyEnabled = false;
    logNotifyEnabled = false;
    streamEnabled = false;
    lastConnParamReqMs = 0;
    connEstablishedMs = 0;
    bleStatus = ADVERTISING;
    const bool advOk = advertising->start();
    logMessage("[BLE] advertising watchdog restart %s\n", advOk ? "started" : "failed");
}

void BLEBoatLock::notifyLive() {
    if (!streamEnabled) {
        return;
    }

    enqueueLiveFrame();
}

void BLEBoatLock::sendLog(const char* line) {
    enqueueLogLine(line);
}

const char* BLEBoatLock::statusString() const {
    switch (bleStatus) {
        case ADVERTISING: return "ADVERTISING";
        case CONNECTED:   return "CONNECTED";
        default:          return "UNKNOWN";
    }
}
