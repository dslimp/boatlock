# TODO — BoatLock Project

> Active tasks and plans for the Virtual Anchor (BoatLock) based on ESP32

---

## Hardware

- [ ] Add battery charge level indication (OLED or analog)
- [ ] Finalize and implement battery placement
- [ ] Test all components' power stability under real load
- [ ] Test and refine mechanical mounting of the motor and anchor mechanism

---

## ESP32 Firmware

- [ ] Add heading hold mode using BNO055 compass
- [x] Switch from classic Bluetooth to BLE
- [ ] Add GPS coordinate smoothing (moving average)
- [ ] Add system status indication (mode, errors, connection)
- [ ] Implement route logging to microSD (optional)
- [ ] Add firmware update support via BLE (DFU, optional)

---

## Mobile App (Flutter)

- [x] Implement map-based anchor point selection and send via BLE
- [ ] Display device status (signal, battery, active point, mode)
- [ ] Route/logs visualization screen
- [ ] Support OTA/DFU firmware update over BLE

---

## Documentation & Repository

- [ ] Describe BLE protocol and command structure in documentation
- [ ] Update README: features, device photos, wiring diagram
- [ ] Usage examples and typical scenarios (anchoring, holding, returning to point)
- [ ] Upload latest schematics and firmware to the repo
- [ ] Standardize code style, add tests and CI/CD

---

_Features with `[x]` are already implemented. Update this file as you complete more tasks or new ideas come up!_
