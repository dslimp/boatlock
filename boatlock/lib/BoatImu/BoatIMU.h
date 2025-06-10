#pragma once
#include <MPU9250_asukiaaa.h>

class BoatIMU {
public:
    bool begin(uint8_t addr = 0x68);
    void update();

    float accelX();
    float accelY();
    float accelZ();

    float gyroX();
    float gyroY();
    float gyroZ();

    float magX();
    float magY();
    float magZ();

private:
    MPU9250_asukiaaa mpu;
};
