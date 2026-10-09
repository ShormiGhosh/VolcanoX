// PhotonMap.h -- inverse (light-ray) tracing of the lava's glow
// (IMPLEMENTATION_GUIDE.md §11.4).
//
// Camera-ray tracing asks "what does this pixel see?". This asks the opposite
// question, starting from the light: every frame a few thousand photons are fired
// from the glowing lava flows and the flying droplets, traced through the proxy scene
// (RayScene.h), and the ones that land on the ground or the volcano's slope are
// deposited into a top-down "light map" texture. The terrain and objects then read the
// map and add the lava's glow to their diffuse light. Photons that hit a tree, the
// hut, the windmill or a drone are absorbed, which leaves shadow-shaped gaps.
//
// Because the density of photons falls off as 1/d^2 on its own, the attenuation comes
// out of the geometry here instead of being a formula.
#pragma once
#include <GL/glew.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "MathUtils.h"
#include "RayScene.h"

struct PhotonEmitter {
    Vec3 pos;
    Vec3 normal;       // hemisphere axis (cosine-weighted); ignored when `sphere`
    float power = 1.0f;
    bool sphere = false;   // true: emit uniformly over the sphere (a flying droplet)
};

class PhotonMap {
public:
    static constexpr int kSize = 128;              // cells per side
    static constexpr float kExtent = 160.0f;       // the map covers [-kExtent, kExtent] in x and z
    static constexpr int kPhotons = 4096;          // photons per frame
    static constexpr float kBlend = 0.15f;         // temporal smoothing (new frame's weight)

    GLuint texture = 0;
    int lastHits = 0;                              // photons deposited last frame (debug)

    void init() {
        accum_.assign(kSize * kSize * 3, 0.0f);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, kSize, kSize, 0, GL_RGB, GL_FLOAT, accum_.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void clear() { std::fill(accum_.begin(), accum_.end(), 0.0f); upload(); }

    // Fire the photons, deposit them, blend into the running map and upload it.
    void update(const RayScene& scene, const std::vector<PhotonEmitter>& emitters, int frame) {
        std::vector<float> cur(kSize * kSize * 3, 0.0f);
        lastHits = 0;

        float total = 0.0f;
        for (const auto& e : emitters) total += e.power;
        if (total > 1e-9f) {
            const float cellArea = (2.0f * kExtent / kSize) * (2.0f * kExtent / kSize);
            const float flux = total / kPhotons;            // each photon carries 1/N of the total emitted power,
                                                            // so more or brighter lava means more light
            const Vec3 lavaColor(1.0f, 0.42f, 0.10f);
            size_t k = 0;
            float acc = 0.0f;
            for (int i = 0; i < kPhotons; ++i) {
                // Stratified choice of the emitter: photon i takes the emitter whose share of the
                // total power covers (i + 0.5) / N. Deterministic, no stored random state.
                float target = (i + 0.5f) / kPhotons * total;
                while (k + 1 < emitters.size() && acc + emitters[k].power < target) { acc += emitters[k].power; ++k; }
                const PhotonEmitter& e = emitters[k];

                int seed = frame * 100003 + i * 31;
                float u1 = rnd(seed + 1), u2 = rnd(seed + 2);
                Vec3 d;
                float phi = 6.2831853f * u2;
                if (e.sphere) {
                    float y = 1.0f - 2.0f * u1, r = std::sqrt(std::max(0.0f, 1.0f - y * y));
                    d = Vec3(r * std::cos(phi), y, r * std::sin(phi));
                } else {                                    // cosine-weighted hemisphere about the normal
                    float r = std::sqrt(u1), y = std::sqrt(std::max(0.0f, 1.0f - u1));
                    Vec3 n = e.normal;
                    Vec3 a = std::fabs(n.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
                    Vec3 t = normalize3(cross3(n, a)), b = cross3(n, t);
                    d = t * (r * std::cos(phi)) + b * (r * std::sin(phi)) + n * y;
                }
                Vec3 o = e.pos + (e.sphere ? Vec3(0, 0, 0) : e.normal * 0.25f);

                RayHit h = scene.trace(o, d, true);
                if (h.kind != HitGround && h.kind != HitVolcano) continue;   // escaped, or absorbed by an object
                Vec3 p = o + d * h.t;
                if (h.n.y <= 0.02f) continue;                 // grazing or downward-facing: no useful deposit
                // Flux per surface area: the cosine factor is already in the photon density.
                float w = flux * h.n.y / cellArea;
                splat(cur, p.x, p.z, lavaColor * w);
                ++lastHits;
            }
        }

        for (size_t i = 0; i < accum_.size(); ++i) accum_[i] += (cur[i] - accum_[i]) * kBlend;
        upload();
    }

    static float cellSize() { return 2.0f * kExtent / kSize; }

private:
    std::vector<float> accum_;

    static float rnd(int seed) { return 0.5f + 0.5f * hashNoise(seed); }

    // Bilinear splat of a deposit into the four nearest cells.
    static void splat(std::vector<float>& map, float x, float z, const Vec3& c) {
        float gx = (x + kExtent) / cellSize() - 0.5f, gz = (z + kExtent) / cellSize() - 0.5f;
        int x0 = (int)std::floor(gx), z0 = (int)std::floor(gz);
        float fx = gx - x0, fz = gz - z0;
        for (int dz = 0; dz < 2; ++dz)
            for (int dx = 0; dx < 2; ++dx) {
                int cx = x0 + dx, cz = z0 + dz;
                if (cx < 0 || cz < 0 || cx >= kSize || cz >= kSize) continue;
                float wgt = (dx ? fx : 1.0f - fx) * (dz ? fz : 1.0f - fz);
                size_t idx = ((size_t)cz * kSize + cx) * 3;
                map[idx] += c.x * wgt; map[idx + 1] += c.y * wgt; map[idx + 2] += c.z * wgt;
            }
    }

    void upload() {
        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kSize, kSize, GL_RGB, GL_FLOAT, accum_.data());
    }
};
