#pragma once

#include <string.h>
#include <strings.h>
#include "Settings.h"

enum class AnchorProfileId : int {
    QUIET = 0,
    NORMAL = 1,
    CURRENT = 2,
};

struct AnchorProfileValues {
    float holdRadiusM;
    float deadbandM;
    float maxThrustPct;
    float thrustRampPctPerS;
};

inline const AnchorProfileValues& anchorProfileValues(AnchorProfileId id) {
    static const AnchorProfileValues quiet = {
        3.0f, 2.2f, 45.0f, 20.0f
    };
    static const AnchorProfileValues normal = {
        2.0f, 1.5f, 75.0f, 35.0f
    };
    static const AnchorProfileValues current = {
        1.5f, 1.0f, 90.0f, 55.0f
    };
    switch (id) {
        case AnchorProfileId::QUIET:
            return quiet;
        case AnchorProfileId::CURRENT:
            return current;
        case AnchorProfileId::NORMAL:
        default:
            return normal;
    }
}

inline const char* anchorProfileName(AnchorProfileId id) {
    switch (id) {
        case AnchorProfileId::QUIET:
            return "quiet";
        case AnchorProfileId::CURRENT:
            return "current";
        case AnchorProfileId::NORMAL:
        default:
            return "normal";
    }
}

inline bool parseAnchorProfile(const char* raw, AnchorProfileId* out) {
    if (!raw || !out) {
        return false;
    }

    if (strcmp(raw, "0") == 0) {
        *out = AnchorProfileId::QUIET;
        return true;
    }
    if (strcmp(raw, "1") == 0) {
        *out = AnchorProfileId::NORMAL;
        return true;
    }
    if (strcmp(raw, "2") == 0) {
        *out = AnchorProfileId::CURRENT;
        return true;
    }

    if (strcasecmp(raw, "quiet") == 0) {
        *out = AnchorProfileId::QUIET;
        return true;
    }
    if (strcasecmp(raw, "normal") == 0) {
        *out = AnchorProfileId::NORMAL;
        return true;
    }
    if (strcasecmp(raw, "current") == 0) {
        *out = AnchorProfileId::CURRENT;
        return true;
    }
    return false;
}

inline bool applyAnchorProfile(Settings& settings, AnchorProfileId id) {
    const AnchorProfileValues p = anchorProfileValues(id);

    const float prevHold = settings.get("HoldRadius");
    const float prevDeadband = settings.get("DeadbandM");
    const float prevMaxThrust = settings.get("MaxThrustA");
    const float prevRamp = settings.get("ThrRampA");

    bool ok = true;
    ok = ok && settings.setStrict("HoldRadius", p.holdRadiusM);
    ok = ok && settings.setStrict("DeadbandM", p.deadbandM);
    ok = ok && settings.setStrict("MaxThrustA", p.maxThrustPct);
    ok = ok && settings.setStrict("ThrRampA", p.thrustRampPctPerS);

    if (!ok) {
        settings.set("HoldRadius", prevHold);
        settings.set("DeadbandM", prevDeadband);
        settings.set("MaxThrustA", prevMaxThrust);
        settings.set("ThrRampA", prevRamp);
        return false;
    }

    settings.save();
    return true;
}
