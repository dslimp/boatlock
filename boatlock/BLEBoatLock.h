#pragma once
#include <NimBLEDevice.h>
#include <functional>
#include <string>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "RuntimeBleLiveFrame.h"

class BLEBoatLock {
public:
    using CommandHandler = std::function<void(const std::string&)>;
    using TelemetryProvider = std::function<RuntimeBleLiveTelemetry(void)>;

    enum Status { ADVERTISING, CONNECTED } bleStatus = ADVERTISING;

    BLEBoatLock();
    void begin();
    void loop();

    void setTelemetryProvider(TelemetryProvider provider);

    // Обработка команды (SET что-то, если потребуется)
    void setCommandHandler(CommandHandler handler);

    void notifyLive();

    // Отправить логовую строку клиенту
    void sendLog(const char* line);

    // Получить статус как строку
    const char* statusString() const;
    const std::string& runtimeStatus() const { return status; }

private:
    static constexpr size_t kCmdMaxLen = 192;
    static constexpr size_t kCmdQueueLen = 16;
    static constexpr size_t kDataMaxLen = 96;
    static constexpr size_t kDataQueueLen = 24;
    static constexpr size_t kLogMaxLen = 244;
    static constexpr size_t kLogQueueLen = 32;
    static constexpr unsigned long kMinDataNotifyGapMs = 40;
    static constexpr unsigned long kMinLogNotifyGapMs = 12;
    struct DataPacket {
        uint16_t len = 0;
        uint8_t bytes[kDataMaxLen] = {0};
    };

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
    QueueHandle_t dataQueue = nullptr;
    QueueHandle_t logQueue = nullptr;
    bool dataNotifyEnabled = false;
    bool logNotifyEnabled = false;
    bool streamEnabled = false;

    TelemetryProvider telemetryProvider = nullptr;
    CommandHandler cmdHandler = nullptr;
    uint16_t liveSequence = 0;

    // Колбэки BLE
    class ServerCallbacks;
    class CmdCallbacks;
    class NotifyCallbacks;
    friend class ServerCallbacks;
    friend class CmdCallbacks;
    friend class NotifyCallbacks;

    void handleControlPoint(const std::string& command);
    void enqueueCommand(const std::string& cmd);
    void processQueuedCommands();
    void clearQueuedData();
    void enqueueDataPacket(const std::vector<uint8_t>& payload);
    void enqueueLiveFrame();
    void processQueuedData();
    void enqueueLogLine(const char* line);
    void processQueuedLogs();
    void maintainConnParams();

    std::string status = "OK";
public:
    void setStatus(const std::string& s) { status = s; }
};
