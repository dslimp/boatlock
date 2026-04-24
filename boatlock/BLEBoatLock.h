#pragma once
#include <NimBLEDevice.h>
#include <functional>
#include <string>
#include <map>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

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

    // Отправить логовую строку клиенту
    void sendLog(const char* line);

    // Получить статус как строку
    const char* statusString() const;

private:
    static constexpr size_t kCmdMaxLen = 192;
    static constexpr size_t kCmdQueueLen = 16;
    static constexpr size_t kLogMaxLen = 244;
    static constexpr size_t kLogQueueLen = 32;
    static constexpr unsigned long kMinDataNotifyGapMs = 40;
    static constexpr unsigned long kMinLogNotifyGapMs = 12;
    NimBLEServer* pServer = nullptr;
    NimBLEService* pService = nullptr;
    NimBLECharacteristic* pDataChar = nullptr;
    NimBLECharacteristic* pCmdChar = nullptr;
    NimBLECharacteristic* pLogChar = nullptr;

    unsigned long lastDataNotifyMs = 0;
    unsigned long lastLogNotifyMs = 0;
    unsigned long lastConnParamReqMs = 0;
    unsigned long connEstablishedMs = 0;
    QueueHandle_t cmdQueue = nullptr;
    QueueHandle_t logQueue = nullptr;
    bool dataNotifyEnabled = false;
    bool logNotifyEnabled = false;

    std::map<std::string, ParamGetter> paramMap;
    CommandHandler cmdHandler = nullptr;

    // Колбэки BLE
    class ServerCallbacks;
    class CmdCallbacks;
    class NotifyCallbacks;
    friend class ServerCallbacks;
    friend class CmdCallbacks;
    friend class NotifyCallbacks;

    // Обработка запроса параметра по имени
    void handleParamRequest(const std::string& param);
    void enqueueCommand(const std::string& cmd);
    void processQueuedCommands();
    void enqueueLogLine(const char* line);
    void processQueuedLogs();
    void maintainConnParams();
    bool notifyDataValue(const std::string& payload);
    static bool likelyCommand(const std::string& text);
    std::string collectAllParams();

    // Для примера: фиктивные значения параметров
    float distance = 0.0;
    int battery = 0;
    std::string status = "OK";
public:
    // Установить фиктивные значения (или подключи свои датчики)
    void setDistance(float d) { distance = d; }
    void setBattery(int b) { battery = b; }
    void setStatus(const std::string& s) { status = s; }
};
