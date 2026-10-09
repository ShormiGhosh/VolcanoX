// DayNight.h -- automatic day/night clock (IMPLEMENTATION_GUIDE.md §10.1).
//
// The clock is a closed-form function of simulated time, like every other
// motion in the scene: no keyframe tables.
//
//   hour(t)  = fmod(kStartHour + 24 * t / kDayLength, 24)
//   theta    = 2*pi * (hour - 6) / 24        sunElev = sin(theta)
//   daylight = smoothstep(-0.12, 0.25, sunElev)
//   twilight = clamp(1 - |sunElev| / 0.25, 0, 1)
//
// The sun and moon sweep along a great circle spanned by the camera's right
// vector R and the world up vector U, tilted toward the default camera's
// forward direction F so that both cross the default view (a circle exactly
// perpendicular to the view direction would keep them out of frame):
//   sunDir  = normalize( cos(theta) R + sin(theta) U + k F)
//   moonDir = normalize(-cos(theta) R - sin(theta) U + k F)
// i.e. the moon is always on the opposite side of the horizon from the sun.
//
// Pressing N cycles Auto -> lock Day -> lock Night. A lock eases in over
// about 1.5 s so the sky never snaps.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include "MathUtils.h"

inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline Vec3 lerp3(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
inline float smoothstepf(float a, float b, float x) {
    float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

struct SkyState {
    float daylight = 0.0f;   // 0 = full night, 1 = full day
    float twilight = 0.0f;   // peaks at sunrise and sunset
    float hour = 20.0f;      // clock hour (follows the clock even while locked)
    float starAlpha = 1.0f;  // 1 - daylight
    Vec3 sunDir = Vec3(0, -1, 0);
    Vec3 moonDir = Vec3(0, 1, 0);
    Vec3 horizon = Vec3(0.11f, 0.12f, 0.18f);
    Vec3 zenith = Vec3(0.008f, 0.012f, 0.035f);
};

class DayNight {
public:
    enum Mode { Auto = 0, LockDay = 1, LockNight = 2 };

    static constexpr float kDayLengthSeconds = 120.0f; // one full 24 h cycle
    static constexpr float kStartHour = 20.0f;         // opens at 20:00, night
    static constexpr float kLockEaseSeconds = 1.5f;

    int mode = Auto;

    void cycleMode() { mode = (mode + 1) % 3; }

    // Advance the lock easing. `dt` is real (un-paused) time so a lock still
    // eases while the clock is paused.
    void update(float dt, float simTime) {
        if (mode == Auto) {
            lockLevel_ = daylightAtClock(simTime);
        } else {
            float target = (mode == LockDay) ? 1.0f : 0.0f;
            float step = dt / kLockEaseSeconds;
            lockLevel_ += std::max(-step, std::min(step, target - lockLevel_));
        }
        float wTarget = (mode == Auto) ? 0.0f : 1.0f;
        float step = dt / kLockEaseSeconds;
        lockW_ += std::max(-step, std::min(step, wTarget - lockW_));
    }

    // Jump straight to the locked state (used by the --lock debug option).
    void snapLock(int m) {
        mode = m;
        if (m != Auto) { lockW_ = 1.0f; lockLevel_ = (m == LockDay) ? 1.0f : 0.0f; }
    }

    SkyState at(float simTime) const {
        SkyState s;
        float hourClock = hourAt(simTime);
        float thetaC = 2.0f * kPi * (hourClock - 6.0f) / 24.0f;
        float sunElevC = std::sin(thetaC);
        float dayC = smoothstepf(-0.12f, 0.25f, sunElevC);
        float twC = clamp01(1.0f - std::fabs(sunElevC) / 0.25f);

        Vec3 sunC = sunDirAt(thetaC), moonC = moonDirAt(thetaC);
        // Locked directions: blend between a fixed night hour (22:00) and a
        // fixed day hour (10:00) by the eased lock level.
        float thetaN = 2.0f * kPi * (22.0f - 6.0f) / 24.0f;
        float thetaD = 2.0f * kPi * (10.0f - 6.0f) / 24.0f;
        Vec3 sunL = normalize3(lerp3(sunDirAt(thetaN), sunDirAt(thetaD), lockLevel_));
        Vec3 moonL = normalize3(lerp3(moonDirAt(thetaN), moonDirAt(thetaD), lockLevel_));

        s.hour = hourClock;
        s.daylight = lerpf(dayC, lockLevel_, lockW_);
        s.twilight = lerpf(twC, 0.0f, lockW_);
        s.sunDir = normalize3(lerp3(sunC, sunL, lockW_));
        s.moonDir = normalize3(lerp3(moonC, moonL, lockW_));
        s.starAlpha = 1.0f - s.daylight;

        const Vec3 horizonNight(0.11f, 0.12f, 0.18f), zenithNight(0.008f, 0.012f, 0.035f);
        const Vec3 horizonDay(0.62f, 0.76f, 0.90f), zenithDay(0.18f, 0.40f, 0.78f);
        const Vec3 sunsetTint(0.95f, 0.50f, 0.25f);
        s.horizon = lerp3(lerp3(horizonNight, horizonDay, s.daylight), sunsetTint, 0.65f * s.twilight);
        s.zenith = lerp3(lerp3(zenithNight, zenithDay, s.daylight), Vec3(0.10f, 0.10f, 0.28f), 0.30f * s.twilight);
        return s;
    }

    // Short text for the window title / HUD.
    std::string label(float simTime) const {
        char buf[64];
        SkyState s = at(simTime);
        const char* phase;
        if (mode != Auto && lockW_ > 0.5f) {
            std::snprintf(buf, sizeof(buf), "%s (locked)", mode == LockDay ? "DAY" : "NIGHT");
            return buf;
        }
        if (s.daylight > 0.6f) phase = "DAY";
        else if (s.daylight < 0.15f) phase = "NIGHT";
        else phase = (s.hour < 12.0f) ? "DAWN" : "DUSK";
        int h = (int)s.hour, m = (int)((s.hour - h) * 60.0f);
        std::snprintf(buf, sizeof(buf), "%02d:%02d %s (auto)", h, m, phase);
        return buf;
    }

private:
    static constexpr float kPi = 3.14159265358979f;
    // Weight of the default view's forward direction F in each body's direction. The moon sits in
    // front of the default camera so it is in view at night; the sun sits slightly behind the
    // view plane so the scene's camera-facing sides are lit by day (and it is visible when orbiting).
    static constexpr float kMoonTilt = 1.3f;
    static constexpr float kSunTilt = -0.2f;

    float lockW_ = 0.0f;      // 0 = follow the clock, 1 = fully locked
    float lockLevel_ = 0.0f;  // daylight of the lock (eases between 0 and 1)

    static float hourAt(float simTime) {
        return std::fmod(kStartHour + 24.0f * simTime / kDayLengthSeconds, 24.0f);
    }
    static float daylightAtClock(float simTime) {
        float theta = 2.0f * kPi * (hourAt(simTime) - 6.0f) / 24.0f;
        return smoothstepf(-0.12f, 0.25f, std::sin(theta));
    }
    // Basis: R = right of the default view, U = up, F = default view direction.
    static Vec3 sunDirAt(float theta) {
        const Vec3 R(0.82f, 0.0f, -0.57f), U(0.0f, 1.0f, 0.0f), F(-0.57f, 0.0f, -0.82f);
        return normalize3(R * std::cos(theta) + U * std::sin(theta) + F * kSunTilt);
    }
    static Vec3 moonDirAt(float theta) {
        const Vec3 R(0.82f, 0.0f, -0.57f), U(0.0f, 1.0f, 0.0f), F(-0.57f, 0.0f, -0.82f);
        return normalize3(R * -std::cos(theta) + U * -std::sin(theta) + F * kMoonTilt);
    }
};

// The directional "sky light" (Light 1): the moon at night, the sun by day, with
// direction, colour and intensity blended by the day/night state so it never
// snaps when the sun and moon swap (IMPLEMENTATION_GUIDE.md §10.1).
// Tuning: the scene's albedos are the dark colours chosen for the original flat-colour
// night scene, so the lit result is brightened by a camera exposure (like a real camera).
static const float kNightAmbient[3] = { 0.08f, 0.09f, 0.14f };
static const float kNightExposure = 9.0f;
static const float kDayExposure = 4.0f;

struct SkyLight {
    Vec3 toLight;    // unit vector from a surface toward the light
    Vec3 diffuse;
    Vec3 specular;
    Vec3 ambient;    // global ambient I_a
    float exposure;  // camera exposure applied to the lit result
};

inline SkyLight skyLightFor(const SkyState& s) {
    SkyLight l;
    l.toLight = normalize3(s.sunDir * s.daylight + s.moonDir * ((1.0f - s.daylight) * 0.5f));
    // Night values are the PRD's moonlight (dim, cool); day values are strong, warm sunlight.
    const Vec3 nightD(0.12f, 0.14f, 0.20f), nightS(0.15f, 0.16f, 0.18f);
    const Vec3 dayD(1.00f, 0.95f, 0.82f),   dayS(0.60f, 0.60f, 0.55f);
    const Vec3 warm(1.00f, 0.60f, 0.35f);
    l.diffuse = lerp3(lerp3(nightD, dayD, s.daylight), warm, 0.45f * s.twilight);
    l.specular = lerp3(nightS, dayS, s.daylight);
    l.ambient = lerp3(Vec3(kNightAmbient[0], kNightAmbient[1], kNightAmbient[2]), Vec3(0.55f, 0.62f, 0.78f), s.daylight);
    l.exposure = lerpf(kNightExposure, kDayExposure, s.daylight);
    return l;
}
