// Camera.h -- simple orbit camera (arrow keys to orbit, +/- to zoom).
//
// A mouse-look FPS camera needs cursor capture, which is an extra source of
// demo-day failure (wrong window focus, capture not releasing, etc). An
// orbit camera driven by discrete key presses is just as good for showing
// off a static-ish object scene and is trivial to reason about and explain.
#pragma once
#include "MathUtils.h"

class Camera {
public:
    Vec3 target = Vec3(0.0f, 24.0f, 0.0f); // looks between the volcano and the ash cloud above it
    float distance = 68.0f;
    float yawDeg = 35.0f;
    float pitchDeg = 10.0f;

    Mat4 viewMatrix() const {
        Vec3 eye = eyePosition();
        return Mat4::lookAt(eye, target, Vec3(0, 1, 0));
    }

    Vec3 eyePosition() const {
        float yaw = radians(yawDeg);
        float pitch = radians(pitchDeg);
        Vec3 offset(
            distance * std::cos(pitch) * std::sin(yaw),
            distance * std::sin(pitch),
            distance * std::cos(pitch) * std::cos(yaw)
        );
        return target + offset;
    }

    void orbit(float dYawDeg, float dPitchDeg) {
        yawDeg += dYawDeg;
        pitchDeg += dPitchDeg;
        if (pitchDeg > 85.0f) pitchDeg = 85.0f;
        if (pitchDeg < -10.0f) pitchDeg = -10.0f;
    }

    void zoom(float delta) {
        distance += delta;
        if (distance < 8.0f) distance = 8.0f;
        if (distance > 300.0f) distance = 300.0f;
    }
};
