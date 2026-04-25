#pragma once

#include <math.h>

struct BNO08xEulerDeg {
  float yaw = 0.0f;
  float pitch = 0.0f;
  float roll = 0.0f;
};

inline float bno08xNormalize360(float deg) {
  if (!isfinite(deg)) {
    return 0.0f;
  }
  float v = fmodf(deg, 360.0f);
  if (v < 0.0f) {
    v += 360.0f;
  }
  return v;
}

inline float bno08xNormalize180(float deg) {
  if (!isfinite(deg)) {
    return 0.0f;
  }
  float norm = fmodf(deg, 360.0f);
  if (norm > 180.0f) {
    norm -= 360.0f;
  }
  if (norm < -180.0f) {
    norm += 360.0f;
  }
  return norm;
}

inline float bno08xRadToDeg(float rad) {
  return rad * 57.29577951308232f;
}

inline float bno08xVectorNorm3(float x, float y, float z) {
  return sqrtf(x * x + y * y + z * z);
}

inline float bno08xClampUnit(float value) {
  if (value > 1.0f) {
    return 1.0f;
  }
  if (value < -1.0f) {
    return -1.0f;
  }
  return value;
}

inline BNO08xEulerDeg bno08xEulerFromQuaternion(float i,
                                                float j,
                                                float k,
                                                float real) {
  BNO08xEulerDeg out;
  const float sinrCosp = 2.0f * (real * i + j * k);
  const float cosrCosp = 1.0f - 2.0f * (i * i + j * j);
  out.roll = bno08xRadToDeg(atan2f(sinrCosp, cosrCosp));

  const float sinp = 2.0f * (real * j - k * i);
  out.pitch = bno08xRadToDeg(asinf(bno08xClampUnit(sinp)));

  const float sinyCosp = 2.0f * (real * k + i * j);
  const float cosyCosp = 1.0f - 2.0f * (j * j + k * k);
  out.yaw = bno08xNormalize360(bno08xRadToDeg(atan2f(sinyCosp, cosyCosp)));
  return out;
}
