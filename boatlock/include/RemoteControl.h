#pragma once
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLECharacteristic.h>

// --- UUID'ы: сгенерируй свои для продакшена! ---
#define REMOTE_SERVICE_UUID        "12ab0001-1234-5678-1234-56789abcdef0"
#define REMOTE_COMMAND_UUID        "12ab0002-1234-5678-1234-56789abcdef0"
#define REMOTE_STATUS_UUID         "12ab0003-1234-5678-1234-56789abcdef0"

// --- Коды команд (ориентированы на классический пульт Minn Kota i-Pilot) ---
enum RemoteCommand : uint8_t {
    CMD_NONE         = 0,
    CMD_SPOTLOCK     = 1,   // Включить удержание на месте (anchor)
    CMD_GO_TO_SPOT   = 2,   // Перейти к сохранённой точке
    CMD_AUTOPILOT    = 3,   // Держать курс (автопилот)
    CMD_SPEED_UP     = 4,   // Увеличить скорость
    CMD_SPEED_DOWN   = 5,   // Уменьшить скорость
    CMD_LEFT         = 6,   // Влево
    CMD_RIGHT        = 7,   // Вправо
    CMD_FORWARD      = 8,   // Вперёд
    CMD_BACK         = 9,   // Назад
    CMD_PROP_TOGGLE  = 10,  // Включить/выключить мотор
    CMD_RECORD_ROUTE = 11,  // Начать запись маршрута
    CMD_PLAY_ROUTE   = 12,  // Воспроизвести маршрут
    CMD_HOME         = 13   // Вернуться домой
    // ...добавляй свои!
};

// --- BLE сервер для приёма команд с пульта ---
class RemoteControlServer {
public:
    BLEServer* server = nullptr;
    BLECharacteristic* cmdChar = nullptr;
    BLECharacteristic* statusChar = nullptr;
    volatile RemoteCommand lastCmd = CMD_NONE;
    bool connected = false;

    void begin() {
        BLEDevice::init("BoatLock_RC");
        server = BLEDevice::createServer();
        server->setCallbacks(new ServerCallbacks(this));

        BLEService* service = server->createService(REMOTE_SERVICE_UUID);

        // --- Команда от пульта ---
        cmdChar = service->createCharacteristic(
            REMOTE_COMMAND_UUID,
            BLECharacteristic::PROPERTY_WRITE
        );
        cmdChar->setCallbacks(new CommandCallbacks(this));

        // --- Статус для пульта (например, HOLDING/IDLE/ALERT) ---
        statusChar = service->createCharacteristic(
            REMOTE_STATUS_UUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
        );
        statusChar->setValue("Idle");

        service->start();
        server->getAdvertising()->start();
    }

    void setStatus(const char* status) {
        if (statusChar) statusChar->setValue(status);
    }

    // Получить последнюю команду, сбросить после чтения
    RemoteCommand getLastCommand() {
        RemoteCommand cmd = lastCmd;
        lastCmd = CMD_NONE;
        return cmd;
    }

    bool isConnected() const {
        return connected;
    }

private:
    // --- BLE callbacks ---
    class ServerCallbacks : public BLEServerCallbacks {
        RemoteControlServer* parent;
    public:
        ServerCallbacks(RemoteControlServer* p) : parent(p) {}
        void onConnect(BLEServer*)    override { parent->connected = true; }
        void onDisconnect(BLEServer*) override { parent->connected = false; }
    };

    class CommandCallbacks : public BLECharacteristicCallbacks {
        RemoteControlServer* parent;
    public:
        CommandCallbacks(RemoteControlServer* p) : parent(p) {}
        void onWrite(BLECharacteristic* characteristic) override {
            std::string value = characteristic->getValue();
            if (value.length() >= 1) {
                parent->lastCmd = static_cast<RemoteCommand>(value[0]);
            }
        }
    };
};
