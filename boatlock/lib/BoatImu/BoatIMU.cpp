#include "BoatIMU.h"

bool BoatIMU::begin(uint8_t addr) {
    mpu.setWire(&Wire);
    mpu.beginAccel(addr);
    mpu.beginGyro(addr);
    mpu.beginMag(addr);
    return true; // По-хорошему, добавить проверки ошибок
}

void BoatIMU::update() {
    mpu.magUpdate();
    mpu.gyroUpdate();
    mpu.accelUpdate();
}

float BoatIMU::accelX() { return mpu.accelX(); }
float BoatIMU::accelY() { return mpu.accelY(); }
float BoatIMU::accelZ() { return mpu.accelZ(); }

float BoatIMU::gyroX() { return mpu.gyroX(); }
float BoatIMU::gyroY() { return mpu.gyroY(); }
float BoatIMU::gyroZ() { return mpu.gyroZ(); }

float BoatIMU::magX() { return mpu.magX(); }
float BoatIMU::magY() { return mpu.magY(); }
float BoatIMU::magZ() { return mpu.magZ(); }

// float BoatIMU::temperature() { return mpu.temp; }  // Поле, а не метод
