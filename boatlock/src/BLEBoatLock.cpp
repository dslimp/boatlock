#include "BLEBoatLock.h"

// --- Server callbacks ---
class BLEBoatLock::ServerCallbacks : public NimBLEServerCallbacks {
    BLEBoatLock* parent;
public:
    ServerCallbacks(BLEBoatLock* p) : parent(p) {}
    void onConnect(NimBLEServer*, NimBLEConnInfo& connInfo) override {
        if (parent) parent->bleStatus = CONNECTED;
        Serial.printf("[BLE] Client connected! Address: %s\n", connInfo.getAddress().toString().c_str());
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo& connInfo, int reason) override {
        if (parent) parent->bleStatus = ADVERTISING;
        Serial.printf("[BLE] Client disconnected! Address: %s, Reason: %d\n",
            connInfo.getAddress().toString().c_str(), reason);
        NimBLEDevice::getAdvertising()->start();
        Serial.println("[BLE] Restart advertising");
    }
};

// --- Characteristic for command/param requests ---
class BLEBoatLock::CmdCallbacks : public NimBLECharacteristicCallbacks {
    BLEBoatLock* parent;
public:
    CmdCallbacks(BLEBoatLock* p) : parent(p) {}
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo&) override {
        std::string param = pCharacteristic->getValue();
        Serial.printf("[BLE] Param request: %s\n", param.c_str());
        if (parent) parent->handleParamRequest(param);
    }
};

BLEBoatLock::BLEBoatLock() {}

void BLEBoatLock::begin() {
    NimBLEDevice::init("BoatLock");
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(this));
    pService = pServer->createService("12ab");

    // Data char: отправка значений/JSON
    pDataChar = pService->createCharacteristic(
        "34cd", NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
    );

    // Command char: получение запроса на параметр
    pCmdChar = pService->createCharacteristic(
        "56ef", NIMBLE_PROPERTY::WRITE
    );
    pCmdChar->setCallbacks(new CmdCallbacks(this));

    pService->start();
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->setName("BoatLock");
    pAdvertising->start();
    bleStatus = ADVERTISING;

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
    // Можешь уведомлять клиента о всех параметрах по таймеру (если нужно)
    // notifyAll();
}

void BLEBoatLock::handleParamRequest(const std::string& param) {
    // Если пришёл специальный запрос "all" — отправляем все параметры JSON-ом
    if (param == "all") {
        std::string all = collectAllParams();
        if (pDataChar) {
            pDataChar->setValue(all);
            if (bleStatus == CONNECTED)
                pDataChar->notify();
        }
        return;
    }
    // Если в paramMap есть такой параметр — отдаём его значение
    auto it = paramMap.find(param);
    if (it != paramMap.end()) {
        std::string val = it->second();
        if (pDataChar) {
            pDataChar->setValue(val);
            if (bleStatus == CONNECTED)
                pDataChar->notify();
        }
        return;
    }
    // Неизвестный параметр
    if (pDataChar) {
        pDataChar->setValue("N/A");
        if (bleStatus == CONNECTED)
            pDataChar->notify();
    }
    // Для расширения: можешь обработать другие команды
    if (cmdHandler) cmdHandler(param);
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
    if (pDataChar && bleStatus == CONNECTED) {
        std::string all = collectAllParams();
        pDataChar->setValue(all);
        pDataChar->notify();
    }
}

const char* BLEBoatLock::statusString() const {
    switch (bleStatus) {
        case ADVERTISING: return "ADVERTISING";
        case CONNECTED:   return "CONNECTED";
        default:          return "UNKNOWN";
    }
}
