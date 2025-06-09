#pragma once
#include <NimBLEDevice.h>
#include <functional>
#include <string>
#include <map>

class BLEBoatLock {
public:
    using ParamGetter = std::function<std::string(void)>;
    using CommandHandler = std::function<void(const std::string&)>;

    enum Status { ADVERTISING, CONNECTED } bleStatus = ADVERTISING;

    BLEBoatLock();
    void begin();
    void loop();

    // Регистрация параметра: имя и функция, которая вернёт строку
    void registerParam(const std::string& name, ParamGetter getter);

    // Обработка команды (SET что-то, если потребуется)
    void setCommandHandler(CommandHandler handler);

    // Быстро отдать notify с JSON-строкой (например, все параметры)
    void notifyAll();

    // Получить статус как строку
    const char* statusString() const;

private:
    NimBLEServer* pServer = nullptr;
    NimBLEService* pService = nullptr;
    NimBLECharacteristic* pDataChar = nullptr;
    NimBLECharacteristic* pCmdChar = nullptr;

    unsigned long lastNotify = 0;

    std::map<std::string, ParamGetter> paramMap;
    CommandHandler cmdHandler = nullptr;

    // Колбэки BLE
    class ServerCallbacks;
    class CmdCallbacks;
    friend class ServerCallbacks;
    friend class CmdCallbacks;

    // Обработка запроса параметра по имени
    void handleParamRequest(const std::string& param);
    std::string collectAllParams();

    // Для примера: фиктивные значения параметров
    float distance = 0.0;
    int battery = 0;
    std::string status = "HOLD";
public:
    // Установить фиктивные значения (или подключи свои датчики)
    void setDistance(float d) { distance = d; }
    void setBattery(int b) { battery = b; }
    void setStatus(const std::string& s) { status = s; }
};
