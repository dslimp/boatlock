#pragma once

#include "BLEBoatLock.h"

template<typename F>
inline BLEBoatLock::ParamGetter makeFloatParam(F valueFunc, const char* fmt) {
    return [valueFunc, fmt]() {
        char buf[20];
        snprintf(buf, sizeof(buf), fmt, valueFunc());
        return std::string(buf);
    };
}
