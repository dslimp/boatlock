#include "BLEBoatLock.h"
#include "Logger.h"
#include <algorithm>
#include <cstring>

namespace {
bool startsWith(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}
} // namespace

// --- Server callbacks ---
class BLEBoatLock::ServerCallbacks : public NimBLEServerCallbacks {
    BLEBoatLock* parent;
public:
    ServerCallbacks(BLEBoatLock* p) : parent(p) {}
    void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
        if (parent) {
            parent->updateBleStatusFromConnections();
            parent->lastDataNotifyMs = 0;
            parent->lastLogNotifyMs = 0;
            parent->lastConnParamReqMs = 0;
            parent->connEstablishedMs = millis();
            parent->ensureAdvertisingForMultiClient();
        }

        // Request stable params for control traffic.
        // Units: interval 1.25ms, timeout 10ms.
        // 24..48 => 30..60ms interval, timeout 300 => 3s.
        server->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 300);
        server->setDataLen(connInfo.getConnHandle(), 251);
        logMessage("[BLE] Client connected! Address: %s mtu=%u int=%u timeout=%u peers=%u\n",
            connInfo.getAddress().toString().c_str(),
            connInfo.getMTU(),
            connInfo.getConnInterval(),
            connInfo.getConnTimeout(),
            (unsigned)server->getConnectedCount());
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
        if (parent) {
            parent->clearConnectionSubscriptions(connInfo.getConnHandle());
            parent->updateBleStatusFromConnections();
            parent->lastConnParamReqMs = 0;
            parent->connEstablishedMs = 0;
            parent->ensureAdvertisingForMultiClient();
        }
        logMessage("[BLE] Client disconnected! Address: %s, Reason: %d peers=%u\n",
            connInfo.getAddress().toString().c_str(), reason, (unsigned)server->getConnectedCount());
    }
};

// --- Characteristic for command/param requests ---
class BLEBoatLock::CmdCallbacks : public NimBLECharacteristicCallbacks {
    BLEBoatLock* parent;
public:
    CmdCallbacks(BLEBoatLock* p) : parent(p) {}
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo&) override {
        std::string param = pCharacteristic->getValue();
        logMessage("[BLE] Param request: %s\n", param.c_str());
        if (parent) parent->handleParamRequest(param);
    }
};

// --- Track CCCD subscription state for notify characteristics ---
class BLEBoatLock::NotifyCallbacks : public NimBLECharacteristicCallbacks {
    BLEBoatLock* parent;
    bool isLog;
public:
    NotifyCallbacks(BLEBoatLock* p, bool logChar) : parent(p), isLog(logChar) {}

    void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo& connInfo, uint16_t subValue) override {
        const bool enabled = (subValue & 0x01) || (subValue & 0x02);
        if (!parent) return;
        parent->setNotifySubscription(isLog, connInfo.getConnHandle(), enabled);
        logMessage("[BLE] subscribe %s=%d sub=0x%04X conn=%u\n",
                   isLog ? "log" : "data",
                   enabled ? 1 : 0,
                   subValue,
                   (unsigned)connInfo.getConnHandle());
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

    // Data char: отправка значений/JSON
    pDataChar = pService->createCharacteristic(
        "34cd", NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
    );
    pDataChar->setCallbacks(new NotifyCallbacks(this, false));

    // Command char: получение запроса на параметр
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

    // Регистрируем базовые параметры (можно добавить свои!)
    registerParam("distance", [this]() { char buf[16]; snprintf(buf, sizeof(buf), "%.2f", distance); return std::string(buf); });
    registerParam("battery", [this]() { return std::to_string(battery); });
    registerParam("status", [this]() { return status; });
}

void BLEBoatLock::registerParam(const std::string& name, ParamGetter getter) {
    paramMap[name] = getter;
}

void BLEBoatLock::setCommandHandler(CommandHandler handler) {
    cmdHandler = handler;
}

void BLEBoatLock::loop() {
    processQueuedCommands();
    maintainConnParams();
    processQueuedLogs();
}

void BLEBoatLock::setNotifySubscription(bool isLog, uint16_t connHandle, bool enabled) {
    if (isLog) {
        logNotifyByConn[connHandle] = enabled;
        logNotifyEnabled = hasAnySubscribers(true);
        return;
    }
    dataNotifyByConn[connHandle] = enabled;
    dataNotifyEnabled = hasAnySubscribers(false);
}

void BLEBoatLock::clearConnectionSubscriptions(uint16_t connHandle) {
    dataNotifyByConn.erase(connHandle);
    logNotifyByConn.erase(connHandle);
    dataNotifyEnabled = hasAnySubscribers(false);
    logNotifyEnabled = hasAnySubscribers(true);
}

bool BLEBoatLock::hasAnySubscribers(bool isLog) const {
    const auto& m = isLog ? logNotifyByConn : dataNotifyByConn;
    for (const auto& kv : m) {
        if (kv.second) {
            return true;
        }
    }
    return false;
}

void BLEBoatLock::updateBleStatusFromConnections() {
    if (pServer && pServer->getConnectedCount() > 0) {
        bleStatus = CONNECTED;
    } else {
        bleStatus = ADVERTISING;
    }
}

void BLEBoatLock::ensureAdvertisingForMultiClient() {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (!adv) return;

    const size_t peers = pServer ? pServer->getConnectedCount() : 0;
    if (peers >= kTargetBleClients) {
        return;
    }
    const bool started = adv->start();
    if (started) {
        logMessage("[BLE] advertising active peers=%u/%u\n",
                   (unsigned)peers,
                   (unsigned)kTargetBleClients);
    }
}

void BLEBoatLock::handleParamRequest(const std::string& param) {
    // Если пришёл специальный запрос "all" — отправляем все параметры JSON-ом
    if (param == "all") {
        std::string all = collectAllParams();
        notifyDataValue(all);
        return;
    }

    // Если в paramMap есть такой параметр — отдаём его значение
    auto it = paramMap.find(param);
    if (it != paramMap.end()) {
        notifyDataValue(it->second());
        return;
    }

    if (likelyCommand(param)) {
        // Handle SET_*/SIM_*/control commands from the main loop context.
        enqueueCommand(param);
        return;
    }

    // Unknown read-like request.
    notifyDataValue("N/A");
}

void BLEBoatLock::enqueueCommand(const std::string& cmd) {
    if (!cmdQueue) {
        return;
    }

    char payload[kCmdMaxLen];
    memset(payload, 0, sizeof(payload));
    const size_t n = std::min(cmd.size(), kCmdMaxLen - 1);
    memcpy(payload, cmd.data(), n);

    if (xQueueSend(cmdQueue, payload, 0) != pdTRUE) {
        logMessage("[BLE] command queue full, dropped: %s\n", payload);
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

void BLEBoatLock::enqueueLogLine(const char* line) {
    if (!line || !logQueue) {
        return;
    }

    char payload[kLogMaxLen];
    memset(payload, 0, sizeof(payload));
    const size_t n = std::min(strlen(line), kLogMaxLen - 1);
    memcpy(payload, line, n);

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

    pLogChar->setValue(payload);
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

bool BLEBoatLock::notifyDataValue(const std::string& payload) {
    if (!pDataChar || bleStatus != CONNECTED || !dataNotifyEnabled) {
        return false;
    }

    const unsigned long now = millis();
    if (now - lastDataNotifyMs < kMinDataNotifyGapMs) {
        return false;
    }

    pDataChar->setValue(payload);
    if (pDataChar->notify()) {
        lastDataNotifyMs = now;
        return true;
    }
    return false;
}

bool BLEBoatLock::likelyCommand(const std::string& text) {
    if (text.empty()) {
        return false;
    }

    if (text.find(':') != std::string::npos || text.find('=') != std::string::npos) {
        return true;
    }

    return startsWith(text, "SET_") ||
           startsWith(text, "SIM_") ||
           startsWith(text, "AUTH_") ||
           startsWith(text, "PAIR_") ||
           startsWith(text, "SEC_CMD:") ||
           startsWith(text, "ANCHOR_") ||
           startsWith(text, "MANUAL_") ||
           startsWith(text, "NUDGE_") ||
           text == "STOP" ||
           text == "HEARTBEAT";
}

std::string BLEBoatLock::collectAllParams() {
    std::string json = "{";
    bool first = true;
    for (auto& pair : paramMap) {
        if (!first) json += ",";
        json += "\"" + pair.first + "\":\"" + pair.second() + "\"";
        first = false;
    }
    json += "}";
    return json;
}

void BLEBoatLock::notifyAll() {
    notifyDataValue(collectAllParams());
}

bool BLEBoatLock::notifyJson(const std::string& payload) {
    return notifyDataValue(payload);
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
