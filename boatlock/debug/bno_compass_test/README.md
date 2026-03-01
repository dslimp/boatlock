# BNO Compass Test Sketch

Debug test sketch for BNO08x compass and display.

Purpose:
- verify I2C wiring and BNO detection
- observe heading/quality live
- check calibration behavior on device screen

Temporary build:
- in `platformio.ini`, switch `build_src_filter` to this file:
- `+<debug/bno_compass_test/bno_calib_test.cpp>`
- and keep `+<display.cpp>`
