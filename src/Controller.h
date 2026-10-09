// Controller.h -- the keyboard-flown pilot drone (IMPLEMENTATION_GUIDE.md §10.3).
//
// The only object in the scene whose motion is not a closed form of time: its
// position depends on past key presses, so it is integrated each frame with dt.
// It is not a pre-recorded animation, and it is frame-rate independent.
//
// Limits, not collisions: altitude is clamped between a floor (ground or cone
// surface plus a margin) and a ceiling below the storm layer, and the distance
// from the volcano axis is clamped. There is no collision system (PRD §3.2).
#pragma once
#include <algorithm>
#include <cmath>
#include "MathUtils.h"

struct PilotInput {
    float fwd = 0.0f;   // +1 forward, -1 back
    float turn = 0.0f;  // +1 turn left, -1 turn right
    float vert = 0.0f;  // +1 up, -1 down
};

struct PilotDrone {
    static constexpr float kMaxSpeed = 14.0f;      // units/s
    static constexpr float kMaxClimb = 8.0f;       // units/s
    static constexpr float kMaxYawRate = 1.5708f;  // 90 deg/s
    static constexpr float kTilt = 0.21f;          // 12 deg visual lean
    static constexpr float kMaxAltitude = 55.0f;   // below the storm layer at 62
    static constexpr float kMaxRadius = 150.0f;    // from the volcano axis

    Vec3 pos = Vec3(17.0f, 14.0f, 29.0f);
    float yaw = std::atan2(-17.0f, -29.0f);        // starts facing the volcano
    float speed = 0.0f, climb = 0.0f, yawRate = 0.0f;
    float pitchVis = 0.0f, rollVis = 0.0f;

    // floorAt(x, z) returns the lowest allowed altitude at that point.
    template <class FloorFn>
    void update(float dt, const PilotInput& in, FloorFn floorAt) {
        if (dt <= 0.0f) return;
        // Ease toward the commanded rates: smooth start and stop.
        speed   += (in.fwd  * kMaxSpeed   - speed)   * std::min(1.0f, 3.0f * dt);
        climb   += (in.vert * kMaxClimb   - climb)   * std::min(1.0f, 4.0f * dt);
        yawRate += (in.turn * kMaxYawRate - yawRate) * std::min(1.0f, 6.0f * dt);

        yaw += yawRate * dt;
        pos += Vec3(std::sin(yaw), 0.0f, std::cos(yaw)) * (speed * dt);
        pos.y += climb * dt;

        // Lean into the motion: nose down when moving forward, bank into turns.
        pitchVis = kTilt * speed / kMaxSpeed;
        rollVis = -kTilt * yawRate / kMaxYawRate;

        float r = std::sqrt(pos.x * pos.x + pos.z * pos.z);
        if (r > kMaxRadius) { pos.x *= kMaxRadius / r; pos.z *= kMaxRadius / r; }
        pos.y = std::max(floorAt(pos.x, pos.z), std::min(kMaxAltitude, pos.y));
    }
};

// ---------------------------------------------------------------------------
// The other three controllable objects (the pilot drone above is the fourth). All three
// use the same keys as the drone: W/S forward and back, A/D turn, and for the helicopter
// Space/Shift up and down. Like the drone, each is integrated from live input every frame
// (the one exception to "all motion is a closed form of time"), with limits instead of
// collisions.
// ---------------------------------------------------------------------------

// A research rover on the ground. It steers only while it is moving, and reversing mirrors
// the steering, like a real car. Its wheels turn by (distance / radius).
struct Rover {
    static constexpr float kMaxSpeed = 8.0f, kMaxReverse = 4.0f, kTurnRate = 1.1f;   // units/s, units/s, rad/s
    static constexpr float kMinRadius = 22.0f, kMaxRadius = 150.0f;                  // from the volcano axis
    static constexpr float kWheelRadius = 0.45f;

    Vec3 pos = Vec3(-28.0f, 0.1f, -2.0f);
    float yaw = 1.5708f;                    // starts facing the volcano (+X)
    float speed = 0.0f, yawRate = 0.0f, wheelAngle = 0.0f, rollVis = 0.0f;

    void update(float dt, const PilotInput& in) {
        if (dt <= 0.0f) return;
        float target = in.fwd >= 0.0f ? in.fwd * kMaxSpeed : in.fwd * kMaxReverse;
        speed += (target - speed) * std::min(1.0f, 2.5f * dt);
        float steer = std::max(-1.0f, std::min(1.0f, speed / 3.0f));      // no steering while stationary
        yawRate += (in.turn * kTurnRate * steer - yawRate) * std::min(1.0f, 6.0f * dt);
        yaw += yawRate * dt;
        pos += Vec3(std::sin(yaw), 0.0f, std::cos(yaw)) * (speed * dt);
        wheelAngle += speed * dt / kWheelRadius;
        rollVis = -0.10f * yawRate * speed / (kTurnRate * kMaxSpeed);     // the body leans out of a corner

        float r = std::sqrt(pos.x * pos.x + pos.z * pos.z);
        if (r < kMinRadius) { pos.x *= kMinRadius / r; pos.z *= kMinRadius / r; }
        if (r > kMaxRadius) { pos.x *= kMaxRadius / r; pos.z *= kMaxRadius / r; }
        pos.y = 0.1f;
    }
};

// A helicopter that starts on its pad. It lifts off with Space, flies like the drone but
// larger and slower, and its rotor spins up while it is controlled or airborne.
struct Helicopter {
    static constexpr float kMaxSpeed = 16.0f, kMaxClimb = 6.0f, kMaxYawRate = 1.2f;
    static constexpr float kMaxAltitude = 55.0f, kMaxRadius = 170.0f;
    static constexpr float kTilt = 0.20f;

    Vec3 pos = Vec3(5.9f, 0.1f, -33.5f);
    float yaw = std::atan2(-5.9f, 33.5f);   // facing the volcano
    float speed = 0.0f, climb = 0.0f, yawRate = 0.0f;
    float pitchVis = 0.0f, rollVis = 0.0f, rotorAngle = 0.0f, rotorSpeed = 4.0f;

    // floorAt(x, z): the ground height under the helicopter; `active` = being controlled.
    template <class FloorFn>
    void update(float dt, const PilotInput& in, bool active, FloorFn floorAt) {
        if (dt <= 0.0f) return;
        float floorY = floorAt(pos.x, pos.z);
        bool airborne = pos.y > floorY + 0.4f;

        // It cannot move sideways or turn until it has lifted off; a helicopter on the ground stays put.
        float fwdCmd = airborne ? in.fwd : 0.0f, turnCmd = airborne ? in.turn : 0.0f;
        speed   += (fwdCmd * kMaxSpeed - speed) * std::min(1.0f, 1.6f * dt);
        climb   += (in.vert * kMaxClimb - climb) * std::min(1.0f, 3.0f * dt);
        yawRate += (turnCmd * kMaxYawRate - yawRate) * std::min(1.0f, 4.0f * dt);
        // The rotor must be at speed before it can lift: no climb below 60% rotor speed.
        if (rotorSpeed < 18.0f && climb > 0.0f) climb = 0.0f;

        yaw += yawRate * dt;
        pos += Vec3(std::sin(yaw), 0.0f, std::cos(yaw)) * (speed * dt);
        pos.y += climb * dt;

        pitchVis = kTilt * speed / kMaxSpeed;
        rollVis = -kTilt * yawRate / kMaxYawRate;

        float r = std::sqrt(pos.x * pos.x + pos.z * pos.z);
        if (r > kMaxRadius) { pos.x *= kMaxRadius / r; pos.z *= kMaxRadius / r; }
        floorY = floorAt(pos.x, pos.z);
        pos.y = std::max(floorY, std::min(kMaxAltitude, pos.y));
        if (pos.y <= floorY + 0.01f) { speed = 0.0f; pitchVis = rollVis = 0.0f; }   // on the ground: level

        float rotorTarget = (active || airborne) ? 30.0f : 4.0f;
        rotorSpeed += (rotorTarget - rotorSpeed) * std::min(1.0f, 0.8f * dt);
        rotorAngle += rotorSpeed * dt;
    }
};

// A research boat on the lake. It stays inside the lake's ellipse, bobbing on the water.
struct Boat {
    static constexpr float kMaxSpeed = 7.0f, kMaxReverse = 2.5f, kTurnRate = 0.9f;
    // The lake: centre (-125, -70), radii 30 x 20. The boat is kept slightly inside it.
    static constexpr float kCx = -125.0f, kCz = -70.0f, kA = 27.0f, kB = 17.0f;

    Vec3 pos = Vec3(-131.0f, 0.11f, -70.0f);
    float yaw = 1.5708f;
    float speed = 0.0f, yawRate = 0.0f;

    void update(float dt, const PilotInput& in) {
        if (dt <= 0.0f) return;
        float target = in.fwd >= 0.0f ? in.fwd * kMaxSpeed : in.fwd * kMaxReverse;
        speed += (target - speed) * std::min(1.0f, 1.2f * dt);                  // water: slow to speed up and to stop
        float steer = std::max(-1.0f, std::min(1.0f, speed / 2.0f));
        yawRate += (in.turn * kTurnRate * steer - yawRate) * std::min(1.0f, 3.0f * dt);
        yaw += yawRate * dt;
        pos += Vec3(std::sin(yaw), 0.0f, std::cos(yaw)) * (speed * dt);
        pos.y = 0.11f;

        // Keep it on the water: pull back to the ellipse edge and lose most of the speed.
        float ex = (pos.x - kCx) / kA, ez = (pos.z - kCz) / kB, q = ex * ex + ez * ez;
        if (q > 1.0f) {
            float s = 1.0f / std::sqrt(q);
            pos.x = kCx + (pos.x - kCx) * s;
            pos.z = kCz + (pos.z - kCz) * s;
            speed *= 0.3f;
        }
    }
};
