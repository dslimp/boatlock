#pragma once

#include <Arduino.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct SdLogTelemetrySnapshot {
  unsigned long ms = 0;
  const char* mode = "";
  const char* status = "";
  const char* reasons = "";
  bool gpsFix = false;
  bool gpsSourcePhone = false;
  float lat = NAN;
  float lon = NAN;
  unsigned long gpsAgeMs = 0;
  int satellites = 0;
  float hdop = NAN;
  float speedKmh = NAN;
  bool accelValid = false;
  float accelMps2 = NAN;
  bool headingValid = false;
  float headingDeg = NAN;
  float headingRawDeg = NAN;
  float headingCorrDeg = NAN;
  float pitchDeg = NAN;
  float rollDeg = NAN;
  float rvAccuracyDeg = NAN;
  int compassQuality = 0;
  bool anchorConfigured = false;
  bool anchorEnabled = false;
  float anchorLat = NAN;
  float anchorLon = NAN;
  float distanceM = NAN;
  bool bearingValid = false;
  float bearingDeg = NAN;
  float diffDeg = NAN;
  int motorPwmPct = 0;
  long stepperPosition = 0;
  long stepperTarget = 0;
  bool bleConnected = false;
};

class SdCardLogger {
public:
  bool begin(const char* firmwareVersion, unsigned long bootMs);
  void enqueueLog(const char* data, size_t len);
  void enqueueTelemetry(const SdLogTelemetrySnapshot& snapshot);
  void service(unsigned long nowMs);

  bool ready() const { return ready_; }
  const char* currentPath() const { return currentPath_; }
  uint64_t totalBytes() const { return totalBytes_; }
  uint64_t freeBytes() const { return freeBytes_; }
  uint32_t droppedLines() const { return droppedLines_; }

private:
  static constexpr size_t kLineMax = 768;
  static constexpr size_t kQueueDepth = 32;
  static constexpr size_t kPathMax = 32;

  struct QueueLine {
    char text[kLineMax];
  };

  bool enqueueLine(const char* line);
  void emitSessionLine(const char* firmwareVersion, unsigned long bootMs);
  bool mountCard();
  bool ensureLogDir();
  bool openExistingOrNew();
  bool openFile(uint32_t index);
  bool rotateFile();
  void updateSpace();
  void deleteOldLogsForSpace();
  bool findOldestLog(uint32_t* indexOut, char* pathOut, size_t pathOutSize) const;
  static bool parseLogIndex(const char* name, uint32_t* indexOut);
  static void jsonString(char* out, size_t outSize, const char* value);
  static void appendFloat(char* out, size_t outSize, const char* key, float value, int precision);
  static void appendStringField(char* out, size_t outSize, const char* key, const char* value);
  bool lockQueue();
  void unlockQueue();

  QueueLine queue_[kQueueDepth] = {};
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t count_ = 0;
  SemaphoreHandle_t queueMutex_ = nullptr;

  fs::File currentFile_;
  char currentPath_[kPathMax] = "";
  uint32_t currentIndex_ = 0;
  uint32_t droppedLines_ = 0;
  uint64_t totalBytes_ = 0;
  uint64_t freeBytes_ = 0;
  unsigned long lastFlushMs_ = 0;
  bool ready_ = false;
};

extern SdCardLogger sdCardLogger;
