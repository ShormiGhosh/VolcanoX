// Effects.cpp -- the effects that accompany an eruption burst (see Eruption.h):
//   * the shockwave: expanding rings racing out across the ground from the foot of the volcano;
//   * pyroclastic flows: dark, fast, ground-hugging clouds that roll down the slopes and out
//     across the plain after a BIG burst;
//   * lake bombs: after a big burst a salvo of long-range volcanic bombs, with glowing trails,
//     is thrown toward the lake (about 140 units away, beyond the reach of ordinary ejecta),
//     and each one lands with a splash, ripples, a spray of droplets and a puff of steam.
//
// Everything is a closed-form function of time and of the burst start times, so nothing is
// stored. Flat colours only; the rings, ripples, spray and steam are unlit, the flows are lit ash.
#include "SceneObjects.h"
#include "Eruption.h"
#include <algorithm>
#include <cmath>

namespace Scene {

static void put(const Shader& sh, const Mesh& mesh, const Mat4& model, float r, float g, float b, float a = 1.0f) {
    setModel(sh, model);
    sh.setVec4("uColor", r, g, b, a);
    mesh.draw();
}

static float rnd(int seed) { return 0.5f + 0.5f * hashNoise(seed); }

static const Vec3 kLakeCentre(-125.0f, 0.11f, -70.0f);   // matches Environment.cpp and Boat in Controller.h
static const float kGravity = 9.8f;

// ---------------------------------------------------------------- shockwave
// Two rings per burst, the second a little behind the first. The ring leaves the foot of the cone
// at about 52 units/s, widens as it goes (its thickness is a fixed fraction of its radius) and fades.
static void drawShockwave(const Shader& sh, const MeshLibrary& lib, float baseRadius, const Eruption::Burst& b, float time) {
    for (int k = 0; k < 2; ++k) {
        float tau = time - b.start - 0.15f * k;
        if (tau < 0.0f || tau > 3.2f) continue;
        float radius = baseRadius + 52.0f * tau - 7.0f * k;
        float alpha = 0.55f * b.amp * std::pow(1.0f - tau / 3.2f, 1.5f);
        put(sh, lib.ring, Mat4::translate(Vec3(0.0f, 0.22f, 0.0f)) * Mat4::scale(Vec3(radius, 1.0f, radius)),
            1.0f, 0.88f, 0.66f, alpha);
    }
}

// ---------------------------------------------------------------- pyroclastic flows
// Four lobes per big burst, each a train of dark billowing puffs behind a front that races down the
// cone (about 30 units/s at first) and slows as it spreads across the plain:
//   front(tau) = 95 * (1 - exp(-tau / 3.2))      distance travelled from the rim
static void drawPyroclastic(const Shader& sh, const MeshLibrary& lib, float baseR, float height, const Eruption::Burst& b, float time) {
    const float tau = time - b.start - 0.8f;
    if (tau < 0.0f || tau > 14.0f) return;
    const float rim = 0.14f * baseR + 0.1f;
    const float front = 95.0f * (1.0f - std::exp(-tau / 3.2f));
    const float fadeIn = std::min(1.0f, tau / 0.6f);
    const float fadeOut = std::max(0.0f, std::min(1.0f, (14.0f - tau) / 6.0f));
    const float heat = std::exp(-tau / 1.4f);                      // glowing at first, dark soon after

    sh.setInt("uMat", MatAsh);
    for (int lobe = 0; lobe < 4; ++lobe) {
        float az = 6.2831853f * rnd(b.id * 53 + lobe * 7 + 1);
        for (int row = 0; row < 14; ++row) {
            float back = row * 2.4f * (1.0f + 0.05f * tau);
            float r = rim + front - back;
            if (r < rim) continue;
            for (int across = -1; across <= 1; ++across) {
                int seed = b.id * 977 + lobe * 131 + row * 17 + (across + 2) * 5;
                float ang = az + across * (2.2f + 0.10f * r) / r;
                float x = r * std::cos(ang), z = r * std::sin(ang);
                float radius = (2.0f + 0.04f * r + 1.2f * rnd(seed)) * (0.7f + 0.3f * std::min(1.0f, tau / 2.0f));
                float y = terrainHeightAt(x, z, baseR, height) + radius * 0.6f + 0.4f * std::sin(time * 2.3f + seed);
                float h = row < 5 ? heat : 0.0f;
                float alpha = 0.7f * fadeIn * fadeOut * (0.55f + 0.45f * (1.0f - row / 14.0f));
                sh.setFloat("uEmit", 0.5f * h);
                put(sh, lib.sphere, Mat4::translate(Vec3(x, y, z)) * Mat4::scale(Vec3(radius, radius * 0.75f, radius)),
                    0.13f + 0.5f * h, 0.105f + 0.15f * h, 0.09f, alpha);
            }
        }
    }
    sh.setFloat("uEmit", 0.0f);
}

// ---------------------------------------------------------------- splash
// Everything that happens where a bomb lands in the lake, as a function of the time since impact:
// a flash of fire, four expanding ripples, a spray of droplets (each making a small ripple of its
// own where it falls back) and a few puffs of steam, because molten rock meets water.
static void drawSplash(const Shader& sh, const MeshLibrary& lib, const Vec3& pt, float tau, int seed) {
    sh.setInt("uMat", MatUnlit);
    const float surface = pt.y;

    if (tau < 0.3f) {                                           // flash of fire
        float k = tau / 0.3f;
        put(sh, lib.sphere, Mat4::translate(pt + Vec3(0, 0.4f, 0)) * Mat4::scale(0.8f + 3.0f * k), 1.0f, 0.55f, 0.15f, 0.7f * (1.0f - k));
    }
    for (int k = 0; k < 4; ++k) {                               // ripples
        float tk = tau - 0.28f * k;
        if (tk < 0.0f || tk > 2.4f) continue;
        float r = std::min(7.0f, 0.4f + 3.1f * tk);
        put(sh, lib.ring, Mat4::translate(pt + Vec3(0, 0.06f, 0)) * Mat4::scale(Vec3(r, 1.0f, r)),
            0.85f, 0.93f, 1.0f, 0.65f * std::pow(1.0f - tk / 2.4f, 1.3f));
    }
    for (int d = 0; d < 12; ++d) {                              // spray, thrown up and out under gravity
        float az = 6.2831853f * rnd(seed * 41 + d * 7 + 1);
        float up = 4.0f + 3.0f * rnd(seed * 41 + d * 7 + 2), out = 0.6f + 2.4f * rnd(seed * 41 + d * 7 + 3);
        float flight = 2.0f * up / kGravity;                    // time to fall back to the surface
        float ox = std::cos(az) * out, oz = std::sin(az) * out;
        if (tau < flight) {
            float y = up * tau - 0.5f * kGravity * tau * tau;
            put(sh, lib.sphere, Mat4::translate(pt + Vec3(ox * tau, y, oz * tau)) * Mat4::scale(0.22f + 0.18f * rnd(seed * 41 + d * 7 + 4)),
                0.85f, 0.93f, 1.0f, 0.9f);
        } else if (d < 6 && tau - flight < 1.0f) {              // where it falls back, a small ripple
            float tk = tau - flight, r = 0.15f + 1.4f * tk;
            put(sh, lib.ring, Mat4::translate(Vec3(pt.x + ox * flight, surface + 0.06f, pt.z + oz * flight)) * Mat4::scale(Vec3(r, 1.0f, r)),
                0.9f, 0.96f, 1.0f, 0.6f * (1.0f - tk));
        }
    }
    for (int p = 0; p < 7; ++p) {                               // steam
        float st = tau - 0.1f * p;
        if (st < 0.0f || st > 4.0f) continue;
        float lx = 1.2f * (rnd(seed * 59 + p * 3 + 1) - 0.5f), lz = 1.2f * (rnd(seed * 59 + p * 3 + 2) - 0.5f);
        put(sh, lib.sphere, Mat4::translate(pt + Vec3(lx + 0.5f * st, 0.4f + 0.9f * st, lz)) * Mat4::scale(0.45f + 0.5f * st),
            0.93f, 0.95f, 0.97f, 0.28f * std::min(1.0f, st / 0.3f) * (1.0f - st / 4.0f));
    }
}

// ---------------------------------------------------------------- lake bombs
// A salvo of two bombs per big burst, thrown toward random points on the lake. The launch speed is
// SOLVED so each bomb lands on its target: with horizontal distance D, elevation theta, launch height
// h0 and the water level hw,   T = sqrt(2 (h0 - hw + D tan(theta)) / g)   and   v = D / (T cos(theta)).
static void drawLakeBombs(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, const Eruption::Burst& b, float time) {
    for (int j = 0; j < 2; ++j) {
        float age = time - (b.start + 0.15f + 0.3f * j);
        if (age < 0.0f) continue;
        int seed = b.id * 211 + j * 37;
        Vec3 target(kLakeCentre.x + (2.0f * rnd(seed + 1) - 1.0f) * 11.0f, kLakeCentre.y, kLakeCentre.z + (2.0f * rnd(seed + 2) - 1.0f) * 7.0f);
        Vec3 origin = craterPos + Vec3(0, 0.5f, 0);
        float dx = target.x - origin.x, dz = target.z - origin.z;
        float D = std::sqrt(dx * dx + dz * dz);
        float theta = radians(36.0f + 10.0f * rnd(seed + 3));
        float T = std::sqrt(2.0f * (origin.y - target.y + D * std::tan(theta)) / kGravity);
        float v = D / (T * std::cos(theta));
        Vec3 dir(dx / D, 0.0f, dz / D);

        auto at = [&](float a) {
            return Vec3(origin.x + dir.x * v * std::cos(theta) * a, origin.y + v * std::sin(theta) * a - 0.5f * kGravity * a * a,
                        origin.z + dir.z * v * std::cos(theta) * a);
        };

        if (age < T) {
            // The bomb: a tumbling, white-hot rock, with a glowing trail of 16 shrinking puffs behind it.
            float heat = std::exp(-age / 3.0f);
            sh.setInt("uMat", MatRock);
            sh.setFloat("uEmit", 0.9f * heat + 0.2f);
            Vec3 axis = normalize3(Vec3(hashNoise(seed + 5), hashNoise(seed + 6) + 0.2f, hashNoise(seed + 7)));
            put(sh, lib.rock[(b.id + j) % 4], Mat4::translate(at(age)) * Mat4::rotate(axis, age * 3.0f) * Mat4::scale(1.2f),
                0.24f + 0.76f * heat, 0.20f + 0.25f * heat, 0.18f - 0.10f * heat);
            sh.setInt("uMat", MatLava);
            sh.setFloat("uEmit", 1.0f);
            for (int k = 1; k <= 16; ++k) {
                float a = age - 0.07f * k;
                if (a < 0.0f) break;
                float f = 1.0f - k / 17.0f;
                put(sh, lib.sphere, Mat4::translate(at(a)) * Mat4::scale(0.36f * f + 0.08f), 1.0f, 0.35f + 0.3f * f, 0.06f, 0.75f * f);
            }
            sh.setFloat("uEmit", 0.0f);
        } else if (age - T < 5.0f) {
            drawSplash(sh, lib, Vec3(target.x, target.y, target.z), age - T, seed);
        }
    }
}

// ---------------------------------------------------------------- entry point
void drawEruptionEffects(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float baseR, float height, float time) {
    Eruption::Burst bursts[Eruption::kMaxRecent];
    int n = Eruption::recent(time, 20.0f, bursts);
    if (n == 0) return;

    glDepthMask(GL_FALSE);                       // translucent: don't hide what is behind or inside
    for (int i = 0; i < n; ++i) {
        const Eruption::Burst& b = bursts[i];
        sh.setInt("uMat", MatUnlit);
        drawShockwave(sh, lib, baseR, b, time);
        if (b.big) {
            drawPyroclastic(sh, lib, baseR, height, b, time);
            drawLakeBombs(sh, lib, craterPos, b, time);
        }
    }
    glDepthMask(GL_TRUE);
    sh.setFloat("uEmit", 0.0f);
    sh.setInt("uMat", MatAsh);
}

} // namespace Scene
