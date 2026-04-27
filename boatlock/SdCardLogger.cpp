#include "SdCardLogger.h"

#include <SD.h>
#include <SPI.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr int kSdSclkPin = 39;
constexpr int kSdMisoPin = 40;
constexpr int kSdMosiPin = 38;
constexpr int kSdCsPin = 41;
constexpr uint32_t kSdSpiHz = 4000000;
constexpr char kLogDir[] = "/boatlock";
constexpr char kLogPrefix[] = "bl";
constexpr char kLogSuffix[] = ".jsonl";
constexpr uint32_t kMaxLogIndex = 99999;
constexpr size_t kMaxDrainPerService = 4;
constexpr unsigned long kFlushIntervalMs = 2000;
constexpr uint64_t kMaxFileBytes = 8ULL * 1024ULL * 1024ULL;
constexpr uint64_t kMinFreeBytes = 8ULL * 1024ULL * 1024ULL;
constexpr uint64_t kTargetFreeBytes = 16ULL * 1024ULL * 1024ULL;

void appendf(char* out, size_t outSize, const char* fmt, ...) {
  if (!out || outSize == 0) {
    return;
  }
  const size_t used = strnlen(out, outSize);
  if (used >= outSize - 1) {
    return;
  }
  va_list args;
  va_start(args, fmt);
  vsnprintf(out + used, outSize - used, fmt, args);
  va_end(args);
}

uint32_t nextLogIndex(uint32_t index) {
  return index >= kMaxLogIndex ? 0 : index + 1;
}
} // namespace

SdCardLogger sdCardLogger;

bool SdCardLogger::begin(const char* firmwareVersion, unsigned long bootMs) {
  if (!queueMutex_) {
    queueMutex_ = xSemaphoreCreateMutex();
  }
  if (!mountCard()) {
    Serial.printf("[SD] mount failed cs=%d sclk=%d miso=%d mosi=%d\n",
                  kSdCsPin,
                  kSdSclkPin,
                  kSdMisoPin,
                  kSdMosiPin);
    return false;
  }
  if (!ensureLogDir() || !openExistingOrNew()) {
    ready_ = false;
    Serial.println("[SD] logger open failed");
    return false;
  }
  updateSpace();
  deleteOldLogsForSpace();
  emitSessionLine(firmwareVersion, bootMs);
  return true;
}

void SdCardLogger::enqueueLog(const char* data, size_t len) {
  if (!data || len == 0) {
    return;
  }
  char line[kLineMax];
  char payload[256];
  size_t copyLen = min(len, sizeof(payload) - 1);
  memcpy(payload, data, copyLen);
  while (copyLen > 0 && (payload[copyLen - 1] == '\n' || payload[copyLen - 1] == '\r')) {
    copyLen--;
  }
  payload[copyLen] = '\0';
  char jsonPayload[sizeof(payload) * 2 + 4];
  jsonString(jsonPayload, sizeof(jsonPayload), payload);
  snprintf(line,
           sizeof(line),
           "{\"type\":\"log\",\"ms\":%lu,\"msg\":%s,\"sd_dropped\":%lu}\n",
           millis(),
           jsonPayload,
           static_cast<unsigned long>(droppedLines_));
  enqueueLine(line);
}

void SdCardLogger::enqueueTelemetry(const SdLogTelemetrySnapshot& snapshot) {
  char line[kLineMax];
  line[0] = '\0';
  appendf(line, sizeof(line), "{\"type\":\"nav\",\"ms\":%lu", snapshot.ms);
  appendStringField(line, sizeof(line), "mode", snapshot.mode);
  appendStringField(line, sizeof(line), "status", snapshot.status);
  appendStringField(line, sizeof(line), "reasons", snapshot.reasons);
  appendf(line,
          sizeof(line),
          ",\"gps_fix\":%d,\"gps_phone\":%d,\"gps_age_ms\":%lu,\"sat\":%d",
          snapshot.gpsFix ? 1 : 0,
          snapshot.gpsSourcePhone ? 1 : 0,
          snapshot.gpsAgeMs,
          snapshot.satellites);
  appendFloat(line, sizeof(line), "lat", snapshot.lat, 7);
  appendFloat(line, sizeof(line), "lon", snapshot.lon, 7);
  appendFloat(line, sizeof(line), "hdop", snapshot.hdop, 2);
  appendFloat(line, sizeof(line), "speed_kmh", snapshot.speedKmh, 2);
  appendf(line, sizeof(line), ",\"accel_valid\":%d", snapshot.accelValid ? 1 : 0);
  appendFloat(line, sizeof(line), "accel_mps2", snapshot.accelMps2, 3);
  appendf(line, sizeof(line), ",\"heading_valid\":%d", snapshot.headingValid ? 1 : 0);
  appendFloat(line, sizeof(line), "heading_deg", snapshot.headingDeg, 2);
  appendFloat(line, sizeof(line), "heading_raw_deg", snapshot.headingRawDeg, 2);
  appendFloat(line, sizeof(line), "heading_corr_deg", snapshot.headingCorrDeg, 2);
  appendFloat(line, sizeof(line), "pitch_deg", snapshot.pitchDeg, 2);
  appendFloat(line, sizeof(line), "roll_deg", snapshot.rollDeg, 2);
  appendFloat(line, sizeof(line), "rv_accuracy_deg", snapshot.rvAccuracyDeg, 2);
  appendf(line, sizeof(line), ",\"compass_quality\":%d", snapshot.compassQuality);
  appendf(line,
          sizeof(line),
          ",\"anchor_configured\":%d,\"anchor_enabled\":%d",
          snapshot.anchorConfigured ? 1 : 0,
          snapshot.anchorEnabled ? 1 : 0);
  appendFloat(line, sizeof(line), "anchor_lat", snapshot.anchorLat, 7);
  appendFloat(line, sizeof(line), "anchor_lon", snapshot.anchorLon, 7);
  appendFloat(line, sizeof(line), "distance_m", snapshot.distanceM, 2);
  appendf(line, sizeof(line), ",\"bearing_valid\":%d", snapshot.bearingValid ? 1 : 0);
  appendFloat(line, sizeof(line), "bearing_deg", snapshot.bearingDeg, 2);
  appendFloat(line, sizeof(line), "diff_deg", snapshot.diffDeg, 2);
  appendf(line,
          sizeof(line),
          ",\"motor_pwm_pct\":%d,\"stepper_pos\":%ld,\"stepper_target\":%ld,\"ble_connected\":%d,"
          "\"sd_dropped\":%lu}\n",
          snapshot.motorPwmPct,
          snapshot.stepperPosition,
          snapshot.stepperTarget,
          snapshot.bleConnected ? 1 : 0,
          static_cast<unsigned long>(droppedLines_));
  enqueueLine(line);
}

void SdCardLogger::service(unsigned long nowMs) {
  if (!ready_) {
    return;
  }
  if (!currentFile_) {
    if (!openExistingOrNew()) {
      droppedLines_++;
      return;
    }
  }

  for (size_t drained = 0; drained < kMaxDrainPerService; ++drained) {
    QueueLine line;
    bool hasLine = false;
    if (lockQueue()) {
      if (count_ > 0) {
        line = queue_[tail_];
        tail_ = (tail_ + 1) % kQueueDepth;
        count_--;
        hasLine = true;
      }
      unlockQueue();
    }
    if (!hasLine) {
      break;
    }
    const size_t len = strnlen(line.text, sizeof(line.text));
    if (len == 0) {
      continue;
    }
    if (currentFile_.print(line.text) != len) {
      droppedLines_++;
      currentFile_.close();
      rotateFile();
      continue;
    }
    if (currentFile_.size() >= kMaxFileBytes) {
      currentFile_.flush();
      rotateFile();
    }
  }

  if (nowMs - lastFlushMs_ >= kFlushIntervalMs) {
    if (currentFile_) {
      currentFile_.flush();
    }
    lastFlushMs_ = nowMs;
    updateSpace();
    if (freeBytes_ < kMinFreeBytes) {
      deleteOldLogsForSpace();
    }
  }
}

bool SdCardLogger::enqueueLine(const char* line) {
  if (!line || line[0] == '\0') {
    return false;
  }
  if (!lockQueue()) {
    droppedLines_++;
    return false;
  }
  if (count_ >= kQueueDepth) {
    tail_ = (tail_ + 1) % kQueueDepth;
    count_--;
    droppedLines_++;
  }
  strlcpy(queue_[head_].text, line, sizeof(queue_[head_].text));
  head_ = (head_ + 1) % kQueueDepth;
  count_++;
  unlockQueue();
  return true;
}

void SdCardLogger::emitSessionLine(const char* firmwareVersion, unsigned long bootMs) {
  char fw[64];
  jsonString(fw, sizeof(fw), firmwareVersion ? firmwareVersion : "");
  char line[kLineMax];
  snprintf(line,
           sizeof(line),
           "{\"type\":\"session\",\"ms\":%lu,\"fw\":%s,\"path\":\"%s\",\"sd_total\":%llu,"
           "\"sd_free\":%llu}\n",
           bootMs,
           fw,
           currentPath_,
           static_cast<unsigned long long>(totalBytes_),
           static_cast<unsigned long long>(freeBytes_));
  enqueueLine(line);
}

bool SdCardLogger::mountCard() {
  SPI.begin(kSdSclkPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
  ready_ = SD.begin(kSdCsPin, SPI, kSdSpiHz, "/sd", 5, false);
  return ready_;
}

bool SdCardLogger::ensureLogDir() {
  if (SD.exists(kLogDir)) {
    return true;
  }
  return SD.mkdir(kLogDir);
}

bool SdCardLogger::openExistingOrNew() {
  uint32_t maxIndex = 0;
  bool found = false;
  fs::File root = SD.open(kLogDir);
  if (root && root.isDirectory()) {
    fs::File file = root.openNextFile();
    while (file) {
      uint32_t index = 0;
      if (!file.isDirectory() && parseLogIndex(file.name(), &index)) {
        if (!found || index > maxIndex) {
          maxIndex = index;
          found = true;
        }
      }
      file = root.openNextFile();
    }
  }
  uint32_t index = found ? maxIndex : 0;
  if (found) {
    char path[kPathMax];
    snprintf(path, sizeof(path), "%s/%s%05lu%s", kLogDir, kLogPrefix, static_cast<unsigned long>(index), kLogSuffix);
    fs::File file = SD.open(path, FILE_READ);
    const bool needsRotation = file && file.size() >= kMaxFileBytes;
    if (file) {
      file.close();
    }
    if (needsRotation) {
      index = nextLogIndex(index);
    }
  }
  return openFile(index);
}

bool SdCardLogger::openFile(uint32_t index) {
  if (currentFile_) {
    currentFile_.close();
  }
  currentIndex_ = index;
  snprintf(currentPath_,
           sizeof(currentPath_),
           "%s/%s%05lu%s",
           kLogDir,
           kLogPrefix,
           static_cast<unsigned long>(currentIndex_),
           kLogSuffix);
  currentFile_ = SD.open(currentPath_, FILE_APPEND);
  return static_cast<bool>(currentFile_);
}

bool SdCardLogger::rotateFile() {
  updateSpace();
  if (freeBytes_ < kMinFreeBytes) {
    deleteOldLogsForSpace();
  }
  return openFile(nextLogIndex(currentIndex_));
}

void SdCardLogger::updateSpace() {
  if (!ready_) {
    totalBytes_ = 0;
    freeBytes_ = 0;
    return;
  }
  totalBytes_ = SD.totalBytes();
  const uint64_t used = SD.usedBytes();
  freeBytes_ = totalBytes_ > used ? totalBytes_ - used : 0;
}

void SdCardLogger::deleteOldLogsForSpace() {
  updateSpace();
  for (size_t i = 0; freeBytes_ < kTargetFreeBytes && i < kMaxLogIndex; ++i) {
    uint32_t index = 0;
    char path[kPathMax];
    if (!findOldestLog(&index, path, sizeof(path))) {
      break;
    }
    if (index == currentIndex_) {
      break;
    }
    SD.remove(path);
    updateSpace();
  }
}

bool SdCardLogger::findOldestLog(uint32_t* indexOut, char* pathOut, size_t pathOutSize) const {
  if (!indexOut || !pathOut || pathOutSize == 0) {
    return false;
  }
  bool found = false;
  uint32_t oldestIndex = 0;
  fs::File root = SD.open(kLogDir);
  if (!root || !root.isDirectory()) {
    return false;
  }
  fs::File file = root.openNextFile();
  while (file) {
    uint32_t index = 0;
    if (!file.isDirectory() && parseLogIndex(file.name(), &index) && index != currentIndex_) {
      if (!found || index < oldestIndex) {
        oldestIndex = index;
        found = true;
      }
    }
    file = root.openNextFile();
  }
  if (!found) {
    return false;
  }
  *indexOut = oldestIndex;
  snprintf(pathOut,
           pathOutSize,
           "%s/%s%05lu%s",
           kLogDir,
           kLogPrefix,
           static_cast<unsigned long>(oldestIndex),
           kLogSuffix);
  return true;
}

bool SdCardLogger::parseLogIndex(const char* name, uint32_t* indexOut) {
  if (!name || !indexOut) {
    return false;
  }
  const char* base = strrchr(name, '/');
  base = base ? base + 1 : name;
  const size_t prefixLen = strlen(kLogPrefix);
  const size_t suffixLen = strlen(kLogSuffix);
  const size_t len = strlen(base);
  if (len <= prefixLen + suffixLen || strncmp(base, kLogPrefix, prefixLen) != 0 ||
      strcmp(base + len - suffixLen, kLogSuffix) != 0) {
    return false;
  }
  uint32_t value = 0;
  for (const char* p = base + prefixLen; p < base + len - suffixLen; ++p) {
    if (!isdigit(static_cast<unsigned char>(*p))) {
      return false;
    }
    value = value * 10 + static_cast<uint32_t>(*p - '0');
    if (value > kMaxLogIndex) {
      return false;
    }
  }
  *indexOut = value;
  return true;
}

void SdCardLogger::jsonString(char* out, size_t outSize, const char* value) {
  if (!out || outSize == 0) {
    return;
  }
  size_t pos = 0;
  out[pos++] = '"';
  if (value) {
    for (size_t i = 0; value[i] != '\0' && pos + 2 < outSize; ++i) {
      const char c = value[i];
      if (c == '"' || c == '\\') {
        out[pos++] = '\\';
        out[pos++] = c;
      } else if (c == '\n') {
        out[pos++] = '\\';
        out[pos++] = 'n';
      } else if (c == '\r') {
        out[pos++] = '\\';
        out[pos++] = 'r';
      } else if (static_cast<unsigned char>(c) >= 0x20) {
        out[pos++] = c;
      }
    }
  }
  if (pos < outSize - 1) {
    out[pos++] = '"';
  }
  out[pos] = '\0';
}

void SdCardLogger::appendFloat(char* out, size_t outSize, const char* key, float value, int precision) {
  if (!isfinite(value)) {
    appendf(out, outSize, ",\"%s\":null", key);
    return;
  }
  appendf(out, outSize, ",\"%s\":%.*f", key, precision, value);
}

void SdCardLogger::appendStringField(char* out, size_t outSize, const char* key, const char* value) {
  char escaped[96];
  jsonString(escaped, sizeof(escaped), value ? value : "");
  appendf(out, outSize, ",\"%s\":%s", key, escaped);
}

bool SdCardLogger::lockQueue() {
  if (!queueMutex_) {
    return true;
  }
  return xSemaphoreTake(queueMutex_, 0) == pdTRUE;
}

void SdCardLogger::unlockQueue() {
  if (queueMutex_) {
    xSemaphoreGive(queueMutex_);
  }
}
