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
#include <vector>
#include "MathUtils.h"
#include "Collider.h"

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

// A research rover on the ground. It steers only while it is moving, and reversing mirrors the
// steering, like a real car. Its wheels turn by (distance / radius).
//
// It COLLIDES: the rover is a circle of kBodyRadius, and every obstacle (Collider.h) is a circle. On overlap
// it is pushed back out, its velocity is reflected with a restitution of kRestitution (a head-on hit at
// speed v sends it back at 0.35 v, a glancing hit slides along the obstacle), a glancing hit also kicks its
// yaw, and the impact speed drives three damped springs: a nose dive (or lift), a roll, and a hop.
struct Rover {
    static constexpr float kMaxSpeed = 8.0f, kMaxReverse = 4.0f, kTurnRate = 1.1f;   // units/s, units/s, rad/s
    static constexpr float kMaxRadius = 150.0f;                                      // world edge, from the volcano axis
    static constexpr float kWheelRadius = 0.45f;
    static constexpr float kBodyRadius = 1.5f;       // collision circle (the body is 1.7 wide, 3.0 long)
    static constexpr float kRestitution = 0.35f;
    static constexpr float kBumpThreshold = 1.2f;    // impact speed below this is a scrape, not a bump
    static constexpr float kVolcanoRadius = 19.5f;   // the cone's footprint is a solid obstacle
    // The lake is not drivable: an ellipse a little larger than the water (Boat uses the same centre).
    static constexpr float kLakeCx = -125.0f, kLakeCz = -70.0f, kLakeA = 31.0f, kLakeB = 21.0f;

    Vec3 pos = Vec3(-28.0f, 0.1f, -2.0f);
    float yaw = 1.5708f;                    // starts facing the volcano (+X)
    float speed = 0.0f, yawRate = 0.0f, wheelAngle = 0.0f, rollVis = 0.0f;

    // Bump response, visible on the model and the camera.
    float pitchVis = 0.0f;                  // nose dive (+) / lift (-), radians
    float hop = 0.0f;                       // height the body has been thrown up, units
    float bumpShake = 0.0f;                 // 0..1.2, decays; drives a camera jolt
    int hits = 0;                           // bumps since the start
    int lastKind = -1;                      // what the last bump was against
    float sinceHit = 99.0f;                 // seconds since the last bump
    float lastImpact = 0.0f;                // impact speed of the last bump

    // Damped-spring state for the three visual responses.
    float bumpPitch = 0.0f, bumpPitchVel = 0.0f;
    float bumpRoll = 0.0f, bumpRollVel = 0.0f;
    float hopVel = 0.0f;
    float leanRoll = 0.0f;                  // the cornering lean, kept separate from the bump roll

    void update(float dt, const PilotInput& in, const std::vector<Collider>& obstacles) {
        if (dt <= 0.0f) return;
        float target = in.fwd >= 0.0f ? in.fwd * kMaxSpeed : in.fwd * kMaxReverse;
        speed += (target - speed) * std::min(1.0f, 2.5f * dt);
        float steer = std::max(-1.0f, std::min(1.0f, speed / 3.0f));      // no steering while stationary
        yawRate += (in.turn * kTurnRate * steer - yawRate) * std::min(1.0f, 6.0f * dt);
        yaw += yawRate * dt;
        pos += Vec3(std::sin(yaw), 0.0f, std::cos(yaw)) * (speed * dt);
        wheelAngle += speed * dt / kWheelRadius;
        leanRoll = -0.10f * yawRate * speed / (kTurnRate * kMaxSpeed);    // the body leans out of a corner

        sinceHit += dt;
        bumpShake *= std::exp(-dt / 0.35f);

        // ---- collisions ----
        for (const Collider& c : obstacles) resolveCircle(c.x, c.z, c.r, c.kind);
        resolveCircle(0.0f, 0.0f, kVolcanoRadius, KindVolcano);
        resolveLake();
        float r = std::sqrt(pos.x * pos.x + pos.z * pos.z);                // the edge of the world
        if (r > kMaxRadius - kBodyRadius) {
            float nx = -pos.x / r, nz = -pos.z / r;                        // pointing inward
            float over = r - (kMaxRadius - kBodyRadius);
            pos.x += nx * over; pos.z += nz * over;
            bounce(nx, nz, KindWorldEdge);
        }
        pos.y = 0.1f;

        // ---- the springs: x'' = -k x - c x' ----
        auto spring = [&](float& x, float& v, float k, float c) {
            v += (-k * x - c * v) * dt;
            x += v * dt;
        };
        spring(bumpPitch, bumpPitchVel, 70.0f, 7.5f);
        spring(bumpRoll, bumpRollVel, 70.0f, 7.5f);
        hopVel += (-90.0f * hop - 5.0f * hopVel) * dt;       // a stiffer, shorter hop
        hop += hopVel * dt;
        if (hop < 0.0f) { hop = 0.0f; if (hopVel < 0.0f) hopVel = 0.0f; }

        pitchVis = bumpPitch;
        rollVis = leanRoll + bumpRoll;
    }

private:
    // Push the rover out of a circular obstacle and bounce it if it was moving into it.
    void resolveCircle(float cx, float cz, float cr, int kind) {
        float dx = pos.x - cx, dz = pos.z - cz;
        float minD = kBodyRadius + cr;
        float d2 = dx * dx + dz * dz;
        if (d2 >= minD * minD) return;
        float d = std::sqrt(d2);
        float nx, nz;
        if (d < 1e-4f) { nx = -std::sin(yaw); nz = -std::cos(yaw); }      // dead centre: back out the way we came
        else           { nx = dx / d; nz = dz / d; }
        float push = minD - d;
        pos.x += nx * push;
        pos.z += nz * push;
        bounce(nx, nz, kind);
    }

    // The ellipse of the lake: if the rover's centre is inside it, push it radially back out to the shore.
    void resolveLake() {
        float ex = (pos.x - kLakeCx) / kLakeA, ez = (pos.z - kLakeCz) / kLakeB;
        float q = ex * ex + ez * ez;
        if (q >= 1.0f) return;
        float s = (q > 1e-6f) ? 1.0f / std::sqrt(q) : 1.0f;
        float nx = ex / kLakeA, nz = ez / kLakeB;                          // outward normal = gradient of q
        float nl = std::sqrt(nx * nx + nz * nz);
        if (nl < 1e-6f) { nx = 1.0f; nz = 0.0f; } else { nx /= nl; nz /= nl; }
        pos.x = kLakeCx + (pos.x - kLakeCx) * s + nx * 0.02f;
        pos.z = kLakeCz + (pos.z - kLakeCz) * s + nz * 0.02f;
        bounce(nx, nz, KindLake);
    }

    // (nx, nz) is the unit normal pointing away from the obstacle.
    void bounce(float nx, float nz, int kind) {
        float hx = std::sin(yaw), hz = std::cos(yaw);                      // heading
        float vx = hx * speed, vz = hz * speed;
        float vn = vx * nx + vz * nz;
        if (vn >= 0.0f) return;                                            // already moving away
        float impact = -vn;

        // Reflect: v' = v - (1 + e) (v . n) n, then keep only the part along the heading (a rover cannot slide sideways).
        float rx = vx - (1.0f + kRestitution) * vn * nx;
        float rz = vz - (1.0f + kRestitution) * vn * nz;
        speed = rx * hx + rz * hz;

        if (impact < kBumpThreshold || sinceHit < 0.25f) return;           // a scrape, or a bump still being felt

        float cross = hx * nz - hz * nx;                                   // which side of the nose was hit
        float front = -(hx * nx + hz * nz);                                // +1 head-on, -1 from behind
        float k = std::min(impact, 8.0f);
        yawRate -= 0.18f * k * cross;                                      // a glancing blow swings the nose away
        bumpPitchVel += 1.6f * 0.1f * k * front * 3.0f;                    // nose dives on a head-on hit, lifts if hit from behind
        bumpRollVel += 0.9f * 0.1f * k * cross * 3.0f;
        hopVel += 0.45f * k;
        bumpShake = std::max(bumpShake, std::min(1.2f, 0.14f * k));
        ++hits;
        lastKind = kind;
        sinceHit = 0.0f;
        lastImpact = impact;
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
