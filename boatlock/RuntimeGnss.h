#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <math.h>

#include "AnchorReasons.h"
#include "GnssQualityGate.h"
#include "Settings.h"

class RuntimeGnss {
public:
  static constexpr int kMaxFilterWindow = 20;
  static constexpr unsigned long kPhoneGpsTimeoutMs = 5000;
  static constexpr float kGpsCorrMinSpeedKmh = 3.0f;
  static constexpr float kGpsCorrMinMoveM = 4.0f;
  static constexpr float kGpsCorrAlpha = 0.18f;
  static constexpr float kGpsCorrMaxAbsDeg = 90.0f;
  static constexpr unsigned long kGpsCorrMaxAgeMs = 180000;
  static constexpr unsigned long kAnchorBearingCacheMaxAgeMs = 120000;

  enum class ApplyResult : uint8_t {
    NONE = 0,
    APPLIED,
    JUMP_REJECTED,
    INVALID_FIX,
  };

  struct HardwareFixInput {
    float lat = 0.0f;
    float lon = 0.0f;
    float speedKmh = 0.0f;
    int satellites = 0;
    float hdop = NAN;
    unsigned long ageMs = 0;
  };

  RuntimeGnss() { reset(kDefaultFilterWindow); }

  void reset(int filterWindow) {
    gpsFilter_.reset(filterWindow);
    lastLat_ = 0.0f;
    lastLon_ = 0.0f;
    distanceM_ = 0.0f;
    bearingDeg_ = 0.0f;
    currentSpeedKmh_ = 0.0f;
    currentSatellites_ = 0;
    currentGpsHdop_ = NAN;
    phoneLat_ = 0.0f;
    phoneLon_ = 0.0f;
    phoneSpeedKmh_ = 0.0f;
    phoneSatellites_ = 0;
    phoneGpsUpdatedMs_ = 0;
    phoneGpsValid_ = false;
    gpsSourcePhone_ = false;
    gpsHeadingCorrDeg_ = 0.0f;
    gpsHeadingCorrUpdatedMs_ = 0;
    gpsHeadingCorrValid_ = false;
    gpsCorrRefLat_ = 0.0f;
    gpsCorrRefLon_ = 0.0f;
    gpsCorrRefValid_ = false;
    anchorBearingCacheDeg_ = 0.0f;
    anchorBearingCacheUpdatedMs_ = 0;
    anchorBearingCacheValid_ = false;
    currentGpsAgeMs_ = 999999;
    currentPosJumpM_ = 0.0f;
    currentPosJumpRejected_ = false;
    pendingJumpLat_ = 0.0f;
    pendingJumpLon_ = 0.0f;
    pendingJumpValid_ = false;
    currentSpeedMps_ = 0.0f;
    currentAccelMps2_ = 0.0f;
    currentAccelValid_ = false;
    speedSampleMs_ = 0;
    prevSpeedMps_ = NAN;
    hasHardwareSpeedSample_ = false;
    hwFixSamplesCount_ = 0;
    lastGnssFailReason_ = GnssQualityFailReason::NO_FIX;
    gpsFix_ = false;
  }

  void setPhoneFix(float lat,
                   float lon,
                   float speedKmh,
                   int satellites,
                   unsigned long nowMs) {
    if (!validPosition(lat, lon)) {
      phoneLat_ = 0.0f;
      phoneLon_ = 0.0f;
      phoneSpeedKmh_ = 0.0f;
      phoneSatellites_ = 0;
      phoneGpsUpdatedMs_ = nowMs;
      phoneGpsValid_ = false;
      if (gpsSourcePhone_) {
        clearFix();
      }
      return;
    }
    phoneLat_ = lat;
    phoneLon_ = lon;
    phoneSpeedKmh_ = speedKmh;
    phoneSatellites_ = max(0, satellites);
    phoneGpsUpdatedMs_ = nowMs;
    phoneGpsValid_ = true;
  }

  ApplyResult applyHardwareFix(const HardwareFixInput& input,
                               Settings& settings,
                               bool compassReady,
                               float compassHeadingDeg,
                               unsigned long nowMs) {
    if (!validPosition(input.lat, input.lon)) {
      clearFix();
      currentGpsAgeMs_ = input.ageMs;
      return ApplyResult::INVALID_FIX;
    }

    const int requestedWindow =
        constrain((int)settings.get("GpsFWin"), 1, kMaxFilterWindow);
    if (requestedWindow != gpsFilter_.window) {
      gpsFilter_.reset(requestedWindow);
    }

    const float maxPosJumpM = constrain(settings.get("MaxPosJumpM"), 1.0f, 200.0f);
    bool acceptedPendingJump = false;
    if (gpsFilter_.count > 0) {
      const float jumpM =
          TinyGPSPlus::distanceBetween(lastLat_, lastLon_, input.lat, input.lon);
      currentPosJumpM_ = isfinite(jumpM) ? jumpM : 0.0f;
      if (isfinite(jumpM) && jumpM > maxPosJumpM) {
        if (!pendingJumpMatches(input.lat, input.lon, maxPosJumpM)) {
          pendingJumpLat_ = input.lat;
          pendingJumpLon_ = input.lon;
          pendingJumpValid_ = true;
          currentPosJumpRejected_ = true;
          currentGpsAgeMs_ = input.ageMs;
          return ApplyResult::JUMP_REJECTED;
        }
        gpsFilter_.reset(requestedWindow);
        acceptedPendingJump = true;
      }
    }

    gpsFilter_.push(input.lat, input.lon);
    lastLat_ = gpsFilter_.avgLat();
    lastLon_ = gpsFilter_.avgLon();
    currentSpeedKmh_ =
        (isfinite(input.speedKmh) && input.speedKmh > 0.0f) ? input.speedKmh : 0.0f;
    currentSpeedMps_ = currentSpeedKmh_ / 3.6f;
    if (hasHardwareSpeedSample_ && isfinite(prevSpeedMps_)) {
      const float dt = (nowMs - speedSampleMs_) / 1000.0f;
      if (isfinite(dt) && dt > 0.05f && dt < 5.0f) {
        currentAccelMps2_ = (currentSpeedMps_ - prevSpeedMps_) / dt;
        currentAccelValid_ = true;
      } else {
        currentAccelValid_ = false;
      }
    } else {
      currentAccelValid_ = false;
    }
    speedSampleMs_ = nowMs;
    prevSpeedMps_ = currentSpeedMps_;
    hasHardwareSpeedSample_ = true;
    currentSatellites_ = max(0, input.satellites);
    currentGpsHdop_ = input.hdop;
    currentGpsAgeMs_ = input.ageMs;
    currentPosJumpM_ = acceptedPendingJump ? 0.0f : currentPosJumpM_;
    currentPosJumpRejected_ = false;
    pendingJumpValid_ = false;
    ++hwFixSamplesCount_;
    gpsSourcePhone_ = false;
    gpsFix_ = true;
    maybeUpdateHeadingCorrection(
        lastLat_, lastLon_, currentSpeedKmh_, compassReady, compassHeadingDeg, nowMs);
    return ApplyResult::APPLIED;
  }

  bool applyPhoneFallback(bool compassReady,
                          float compassHeadingDeg,
                          unsigned long nowMs) {
    if (!phoneGpsValid_ || nowMs - phoneGpsUpdatedMs_ > kPhoneGpsTimeoutMs) {
      return false;
    }

    lastLat_ = phoneLat_;
    lastLon_ = phoneLon_;
    currentSpeedKmh_ =
        (isfinite(phoneSpeedKmh_) && phoneSpeedKmh_ > 0.0f) ? phoneSpeedKmh_ : 0.0f;
    currentSpeedMps_ = currentSpeedKmh_ / 3.6f;
    currentAccelValid_ = false;
    resetHardwareMotionBaseline();
    gpsFilter_.reset(gpsFilter_.window);
    currentSatellites_ = max(0, phoneSatellites_);
    currentGpsHdop_ = NAN;
    currentGpsAgeMs_ = nowMs - phoneGpsUpdatedMs_;
    currentPosJumpRejected_ = false;
    currentPosJumpM_ = 0.0f;
    pendingJumpValid_ = false;
    gpsSourcePhone_ = true;
    gpsFix_ = true;
    maybeUpdateHeadingCorrection(
        lastLat_, lastLon_, currentSpeedKmh_, compassReady, compassHeadingDeg, nowMs);
    return true;
  }

  void clearFix() {
    currentSpeedKmh_ = 0.0f;
    currentSpeedMps_ = 0.0f;
    currentAccelMps2_ = 0.0f;
    currentAccelValid_ = false;
    resetHardwareMotionBaseline();
    gpsFilter_.reset(gpsFilter_.window);
    currentSatellites_ = 0;
    currentGpsHdop_ = NAN;
    currentGpsAgeMs_ = 999999;
    currentPosJumpRejected_ = false;
    currentPosJumpM_ = 0.0f;
    pendingJumpValid_ = false;
    gpsSourcePhone_ = false;
    gpsFix_ = false;
    gpsCorrRefValid_ = false;
    lastGnssFailReason_ = GnssQualityFailReason::NO_FIX;
  }

  bool gpsQualityGoodForAnchorOn(Settings& settings) {
    lastGnssFailReason_ =
        evaluateGnssQuality(qualityConfigFromSettings(settings), qualitySample());
    return lastGnssFailReason_ == GnssQualityFailReason::NONE;
  }

  int gnssQualityLevel(Settings& settings) {
    if (!gpsFix_) {
      return 0;
    }
    if (gpsQualityGoodForAnchorOn(settings)) {
      return 2;
    }
    return 1;
  }

  void updateDistanceAndBearing(bool manualMode,
                                bool anchorEnabled,
                                bool holdHeading,
                                float anchorLat,
                                float anchorLon,
                                float anchorHeading,
                                unsigned long nowMs) {
    if (!manualMode && anchorEnabled) {
      if (holdHeading) {
        bearingDeg_ = anchorHeading;
        distanceM_ = controlGpsAvailable()
                         ? TinyGPSPlus::distanceBetween(lastLat_, lastLon_, anchorLat, anchorLon)
                         : 0.0f;
        return;
      }

      if (controlGpsAvailable()) {
        distanceM_ =
            TinyGPSPlus::distanceBetween(lastLat_, lastLon_, anchorLat, anchorLon);
        const float computedBearing =
            TinyGPSPlus::courseTo(lastLat_, lastLon_, anchorLat, anchorLon);
        if (isfinite(computedBearing)) {
          bearingDeg_ = computedBearing;
          anchorBearingCacheDeg_ = computedBearing;
          anchorBearingCacheUpdatedMs_ = nowMs;
          anchorBearingCacheValid_ = true;
        } else if (hasAnchorBearingCache(nowMs)) {
          bearingDeg_ = anchorBearingCacheDeg_;
        } else {
          bearingDeg_ = 0.0f;
        }
        return;
      }

      if (hasAnchorBearingCache(nowMs)) {
        bearingDeg_ = anchorBearingCacheDeg_;
        distanceM_ = 0.0f;
        return;
      }
    }

    distanceM_ = 0.0f;
    bearingDeg_ = 0.0f;
  }

  float currentHeadingValue(bool compassReady,
                            float compassHeadingDeg,
                            unsigned long nowMs) const {
    if (!compassReady) {
      return 0.0f;
    }
    return correctedCompassHeading(compassHeadingDeg, nowMs);
  }

  float headingCorrectionDeg(unsigned long nowMs) const {
    if (!gpsHeadingCorrValid_ || nowMs - gpsHeadingCorrUpdatedMs_ > kGpsCorrMaxAgeMs) {
      return 0.0f;
    }
    return gpsHeadingCorrDeg_;
  }

  bool anchorBearingAvailable(bool autoControlActive,
                              bool holdHeading,
                              unsigned long nowMs) const {
    return autoControlActive &&
           (holdHeading || controlGpsAvailable() || hasAnchorBearingCache(nowMs));
  }

  bool hasAnchorBearingCache(unsigned long nowMs) const {
    return anchorBearingCacheValid_ &&
           nowMs - anchorBearingCacheUpdatedMs_ <= kAnchorBearingCacheMaxAgeMs;
  }

  GnssQualityConfig qualityConfigFromSettings(Settings& settings) const {
    GnssQualityConfig cfg;
    cfg.maxGpsAgeMs =
        static_cast<unsigned long>(constrain(settings.get("MaxGpsAgeMs"), 300.0f, 20000.0f));
    cfg.minSats = constrain((int)settings.get("MinSats"), 3, 20);
    cfg.maxHdop = constrain(settings.get("MaxHdop"), 0.5f, 10.0f);
    cfg.maxPositionJumpM = constrain(settings.get("MaxPosJumpM"), 1.0f, 200.0f);
    cfg.enableSpeedAccelSanity = ((int)settings.get("SpdSanity") == 1);
    cfg.maxSpeedMps = constrain(settings.get("MaxSpdMps"), 1.0f, 60.0f);
    cfg.maxAccelMps2 = constrain(settings.get("MaxAccMps2"), 0.5f, 20.0f);
    cfg.requiredSentences = constrain((int)settings.get("ReqSent"), 0, 20);
    return cfg;
  }

  GnssQualitySample qualitySample() const {
    const bool controlFix = controlGpsAvailable();
    GnssQualitySample sample;
    sample.fix = controlFix;
    sample.ageMs = currentGpsAgeMs_;
    sample.sats = controlFix ? currentSatellites_ : 0;
    sample.hasHdop = controlFix && isfinite(currentGpsHdop_) && currentGpsHdop_ > 0.0f;
    sample.hdop = currentGpsHdop_;
    sample.jumpRejected = currentPosJumpRejected_;
    sample.jumpM = currentPosJumpM_;
    sample.hasSpeed = controlFix && isfinite(currentSpeedMps_);
    sample.speedMps = currentSpeedMps_;
    sample.hasAccel = controlFix && currentAccelValid_ && isfinite(currentAccelMps2_);
    sample.accelMps2 = currentAccelMps2_;
    sample.hasSentences = controlFix;
    sample.sentences = controlFix ? (int)hwFixSamplesCount_ : 0;
    return sample;
  }

  static float normalizeDiffDeg(float targetDeg, float currentDeg) {
    return normalize180Deg(targetDeg - currentDeg);
  }

  static float normalize360Deg(float deg) {
    float wrapped = fmodf(deg, 360.0f);
    if (wrapped < 0.0f) {
      wrapped += 360.0f;
    }
    return wrapped;
  }

  static float normalize180Deg(float deg) {
    float wrapped = fmodf(deg, 360.0f);
    if (wrapped > 180.0f) {
      wrapped -= 360.0f;
    }
    if (wrapped < -180.0f) {
      wrapped += 360.0f;
    }
    return wrapped;
  }

  static bool validPosition(float lat, float lon) {
    return isfinite(lat) &&
           isfinite(lon) &&
           lat >= -90.0f &&
           lat <= 90.0f &&
           lon >= -180.0f &&
           lon <= 180.0f &&
           !(lat == 0.0f && lon == 0.0f);
  }

  bool fix() const { return gpsFix_; }
  bool controlGpsAvailable() const { return gpsFix_ && !gpsSourcePhone_; }
  bool gpsSourcePhone() const { return gpsSourcePhone_; }
  float lastLat() const { return lastLat_; }
  float lastLon() const { return lastLon_; }
  float distanceM() const { return distanceM_; }
  float bearingDeg() const { return bearingDeg_; }
  float currentSpeedKmh() const { return currentSpeedKmh_; }
  int currentSatellites() const { return currentSatellites_; }
  float currentGpsHdop() const { return currentGpsHdop_; }
  unsigned long currentGpsAgeMs() const { return currentGpsAgeMs_; }
  float currentPosJumpM() const { return currentPosJumpM_; }
  bool currentPosJumpRejected() const { return currentPosJumpRejected_; }
  float currentSpeedMps() const { return currentSpeedMps_; }
  float currentAccelMps2() const { return currentAccelMps2_; }
  bool currentAccelValid() const { return currentAccelValid_; }
  unsigned long hwFixSamplesCount() const { return hwFixSamplesCount_; }
  GnssQualityFailReason lastGnssFailReason() const { return lastGnssFailReason_; }
  const char* currentFailReasonString() const {
    return gnssFailReasonString(lastGnssFailReason_);
  }
  AnchorDeniedReason currentDeniedReason() const {
    return deniedReasonFromGnss(lastGnssFailReason_);
  }

private:
  static constexpr int kDefaultFilterWindow = 5;

  struct GpsFilterState {
    float latBuf[kMaxFilterWindow] = {0.0f};
    float lonBuf[kMaxFilterWindow] = {0.0f};
    int count = 0;
    int index = 0;
    int window = kDefaultFilterWindow;
    float latSum = 0.0f;
    float lonSum = 0.0f;

    void reset(int newWindow) {
      count = 0;
      index = 0;
      latSum = 0.0f;
      lonSum = 0.0f;
      window = constrain(newWindow, 1, kMaxFilterWindow);
    }

    void push(float lat, float lon) {
      if (count < window) {
        latBuf[index] = lat;
        lonBuf[index] = lon;
        latSum += lat;
        lonSum += lon;
        ++count;
      } else {
        latSum -= latBuf[index];
        lonSum -= lonBuf[index];
        latBuf[index] = lat;
        lonBuf[index] = lon;
        latSum += lat;
        lonSum += lon;
      }
      index = (index + 1) % window;
    }

    float avgLat() const { return (count > 0) ? (latSum / count) : 0.0f; }
    float avgLon() const { return (count > 0) ? (lonSum / count) : 0.0f; }
  };

  void maybeUpdateHeadingCorrection(float lat,
                                    float lon,
                                    float speedKmh,
                                    bool compassReady,
                                    float compassHeadingDeg,
                                    unsigned long nowMs) {
    if (!compassReady) {
      gpsCorrRefValid_ = false;
      return;
    }
    if (!isfinite(lat) || !isfinite(lon) || !isfinite(speedKmh) ||
        speedKmh < kGpsCorrMinSpeedKmh) {
      return;
    }

    if (!gpsCorrRefValid_) {
      gpsCorrRefLat_ = lat;
      gpsCorrRefLon_ = lon;
      gpsCorrRefValid_ = true;
      return;
    }

    const float movedM =
        TinyGPSPlus::distanceBetween(gpsCorrRefLat_, gpsCorrRefLon_, lat, lon);
    if (!isfinite(movedM) || movedM < kGpsCorrMinMoveM) {
      return;
    }

    const float gpsCourse =
        TinyGPSPlus::courseTo(gpsCorrRefLat_, gpsCorrRefLon_, lat, lon);
    gpsCorrRefLat_ = lat;
    gpsCorrRefLon_ = lon;

    float targetCorr = normalize180Deg(gpsCourse - compassHeadingDeg);
    targetCorr = constrain(targetCorr, -kGpsCorrMaxAbsDeg, kGpsCorrMaxAbsDeg);
    if (!gpsHeadingCorrValid_) {
      gpsHeadingCorrDeg_ = targetCorr;
    } else {
      const float delta = normalize180Deg(targetCorr - gpsHeadingCorrDeg_);
      gpsHeadingCorrDeg_ =
          normalize180Deg(gpsHeadingCorrDeg_ + delta * kGpsCorrAlpha);
    }

    gpsHeadingCorrDeg_ =
        constrain(gpsHeadingCorrDeg_, -kGpsCorrMaxAbsDeg, kGpsCorrMaxAbsDeg);
    gpsHeadingCorrUpdatedMs_ = nowMs;
    gpsHeadingCorrValid_ = true;
  }

  float correctedCompassHeading(float headingDeg, unsigned long nowMs) const {
    if (!gpsHeadingCorrValid_ || nowMs - gpsHeadingCorrUpdatedMs_ > kGpsCorrMaxAgeMs) {
      return normalize360Deg(headingDeg);
    }
    return normalize360Deg(headingDeg + gpsHeadingCorrDeg_);
  }

  void resetHardwareMotionBaseline() {
    speedSampleMs_ = 0;
    prevSpeedMps_ = NAN;
    hasHardwareSpeedSample_ = false;
  }

  bool pendingJumpMatches(float lat, float lon, float maxPosJumpM) const {
    if (!pendingJumpValid_) {
      return false;
    }
    const float pendingDeltaM =
        TinyGPSPlus::distanceBetween(pendingJumpLat_, pendingJumpLon_, lat, lon);
    return isfinite(pendingDeltaM) && pendingDeltaM <= maxPosJumpM;
  }

  GpsFilterState gpsFilter_;
  float lastLat_ = 0.0f;
  float lastLon_ = 0.0f;
  float distanceM_ = 0.0f;
  float bearingDeg_ = 0.0f;
  bool gpsFix_ = false;
  float currentSpeedKmh_ = 0.0f;
  int currentSatellites_ = 0;
  float currentGpsHdop_ = NAN;
  float phoneLat_ = 0.0f;
  float phoneLon_ = 0.0f;
  float phoneSpeedKmh_ = 0.0f;
  int phoneSatellites_ = 0;
  unsigned long phoneGpsUpdatedMs_ = 0;
  bool phoneGpsValid_ = false;
  bool gpsSourcePhone_ = false;
  float gpsHeadingCorrDeg_ = 0.0f;
  unsigned long gpsHeadingCorrUpdatedMs_ = 0;
  bool gpsHeadingCorrValid_ = false;
  float gpsCorrRefLat_ = 0.0f;
  float gpsCorrRefLon_ = 0.0f;
  bool gpsCorrRefValid_ = false;
  float anchorBearingCacheDeg_ = 0.0f;
  unsigned long anchorBearingCacheUpdatedMs_ = 0;
  bool anchorBearingCacheValid_ = false;
  unsigned long currentGpsAgeMs_ = 999999;
  float currentPosJumpM_ = 0.0f;
  bool currentPosJumpRejected_ = false;
  float pendingJumpLat_ = 0.0f;
  float pendingJumpLon_ = 0.0f;
  bool pendingJumpValid_ = false;
  float currentSpeedMps_ = 0.0f;
  float currentAccelMps2_ = 0.0f;
  bool currentAccelValid_ = false;
  unsigned long speedSampleMs_ = 0;
  float prevSpeedMps_ = NAN;
  bool hasHardwareSpeedSample_ = false;
  unsigned long hwFixSamplesCount_ = 0;
  GnssQualityFailReason lastGnssFailReason_ = GnssQualityFailReason::NO_FIX;
};
