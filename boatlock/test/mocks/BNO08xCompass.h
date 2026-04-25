#pragma once
class BNO08xCompass {
public:
  float az = 0;
  float rawAz = 0;
  float headingOffset = 0;
  float rvAccuracyDeg = 0;
  float magNormUT = 0;
  float gyroNormDps = 0;
  float pitchDeg = 0;
  float rollDeg = 0;
  unsigned char headingQuality = 3;
  unsigned char magQuality = 0;
  unsigned char gyroQuality = 0;
  int startDynamicCalibrationCalls = 0;
  int saveDynamicCalibrationCalls = 0;
  int setDcdAutoSaveCalls = 0;
  int tareHeadingNowCalls = 0;
  int saveTareCalls = 0;
  int clearTareCalls = 0;
  bool lastDcdAutoSave = false;

  float getAzimuth() const { return az; }
  float getRawAzimuth() const { return rawAz; }
  unsigned char getHeadingQuality() const { return headingQuality; }
  float getRvAccuracyDeg() const { return rvAccuracyDeg; }
  float getMagNormUT() const { return magNormUT; }
  float getGyroNormDps() const { return gyroNormDps; }
  unsigned char getMagQuality() const { return magQuality; }
  unsigned char getGyroQuality() const { return gyroQuality; }
  float getPitchDeg() const { return pitchDeg; }
  float getRollDeg() const { return rollDeg; }
  void setHeadingOffsetDeg(float v) { headingOffset = v; }
  float getHeadingOffsetDeg() const { return headingOffset; }
  bool startDynamicCalibration(bool) {
    ++startDynamicCalibrationCalls;
    return false;
  }
  bool saveDynamicCalibration() {
    ++saveDynamicCalibrationCalls;
    return false;
  }
  bool setDcdAutoSave(bool enabled) {
    ++setDcdAutoSaveCalls;
    lastDcdAutoSave = enabled;
    return false;
  }
  bool tareHeadingNow() {
    ++tareHeadingNowCalls;
    return false;
  }
  bool saveTare() {
    ++saveTareCalls;
    return false;
  }
  bool clearTare() {
    ++clearTareCalls;
    return false;
  }
  const char* sourceName() const { return "mock"; }
};
