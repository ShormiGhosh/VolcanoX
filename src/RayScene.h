// RayScene.h -- the analytic "proxy scene" that rays are traced against
// (IMPLEMENTATION_GUIDE.md §11.2). It is never drawn: it is a small set of
// closed-form shapes standing in for the volcano, trees, hut, windmill tower and
// drones, used for
//   * camera-ray shadows and reflections in the fragment shader (uploaded as uniforms), and
//   * the CPU photon tracer that fires light rays from the lava (PhotonMap.h).
//
// The C++ intersection code below and the GLSL in shaders/raytrace.glsl implement
// the same formulas and must agree.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>
#include "MathUtils.h"
#include "Shader.h"

// A (truncated) cone about a vertical axis, tilted about Z by `tilt` radians
// (the burnt trees lean). radius(y) = r0 + k*y with k = (r1 - r0) / h, y in [0, h].
struct RayCone {
    Vec3 base;
    float r0 = 1.0f, r1 = 0.0f, h = 1.0f, tilt = 0.0f;
    bool shadowsCrater = true;     // false for the volcano (the crater light sits inside it)
    Vec3 color = Vec3(0.2f, 0.2f, 0.2f);
};
struct RaySphere { Vec3 c; float r = 1.0f; };
struct RayBox { Vec3 c; Vec3 half; float yaw = 0.0f; Vec3 color; bool present = false; };

enum RayHitKind { HitNone = 0, HitGround = 1, HitVolcano = 2, HitOther = 3 };
struct RayHit {
    float t = -1.0f;
    Vec3 n = Vec3(0, 1, 0);
    int kind = HitNone;
};

struct RayScene {
    static constexpr int kMaxCones = 8;
    static constexpr int kMaxSpheres = 4;
    static constexpr int kMaxBoxes = 3;      // the hut, the rover, the boat

    std::vector<RayCone> cones;    // cones[0] is the volcano
    std::vector<RaySphere> spheres;
    std::vector<RayBox> boxes;
    float volcanoRadius = 19.0f;
    Vec3 groundColor = Vec3(0.14f, 0.12f, 0.11f);

    // ---- intersection primitives (return t > eps or -1) ----
    static float hitSphere(const Vec3& o, const Vec3& d, const RaySphere& s) {
        Vec3 oc = o - s.c;
        float b = dot3(oc, d), c = dot3(oc, oc) - s.r * s.r, h = b * b - c;
        if (h < 0.0f) return -1.0f;
        h = std::sqrt(h);
        float t = -b - h;
        if (t > 1e-4f) return t;
        t = -b + h;
        return t > 1e-4f ? t : -1.0f;
    }

    static float hitCone(const Vec3& o, const Vec3& d, const RayCone& c, Vec3& nOut) {
        Vec3 p = o - c.base, dd = d;
        if (c.tilt != 0.0f) {                       // rotate the ray about Z by -tilt so the axis is vertical
            float cs = std::cos(-c.tilt), sn = std::sin(-c.tilt);
            p = Vec3(cs * p.x - sn * p.y, sn * p.x + cs * p.y, p.z);
            dd = Vec3(cs * d.x - sn * d.y, sn * d.x + cs * d.y, d.z);
        }
        float k = (c.r1 - c.r0) / c.h, q = c.r0 + k * p.y;
        float a = dd.x * dd.x + dd.z * dd.z - k * k * dd.y * dd.y;
        float b = p.x * dd.x + p.z * dd.z - k * q * dd.y;
        float cc = p.x * p.x + p.z * p.z - q * q;
        float best = 1e30f;
        Vec3 nl(0, 1, 0);
        if (std::fabs(a) > 1e-9f) {
            float disc = b * b - a * cc;
            if (disc >= 0.0f) {
                float s = std::sqrt(disc);
                float t1 = (-b - s) / a, t2 = (-b + s) / a;
                if (t1 > t2) std::swap(t1, t2);
                for (int i = 0; i < 2; ++i) {
                    float t = i == 0 ? t1 : t2;
                    if (t > 1e-4f) {
                        float y = p.y + t * dd.y;
                        if (y >= 0.0f && y <= c.h && t < best) {
                            best = t;
                            Vec3 hp = p + dd * t;
                            nl = normalize3(Vec3(hp.x, -k * (c.r0 + k * hp.y), hp.z));
                        }
                    }
                }
            }
        }
        if (std::fabs(dd.y) > 1e-9f) {
            float t = (c.h - p.y) / dd.y;           // top cap
            if (t > 1e-4f && t < best) {
                float x = p.x + t * dd.x, z = p.z + t * dd.z;
                if (x * x + z * z <= c.r1 * c.r1) { best = t; nl = Vec3(0, 1, 0); }
            }
            t = -p.y / dd.y;                         // base cap
            if (t > 1e-4f && t < best) {
                float x = p.x + t * dd.x, z = p.z + t * dd.z;
                if (x * x + z * z <= c.r0 * c.r0) { best = t; nl = Vec3(0, -1, 0); }
            }
        }
        if (best >= 1e29f) return -1.0f;
        if (c.tilt != 0.0f) {                        // rotate the normal back
            float cs = std::cos(c.tilt), sn = std::sin(c.tilt);
            nl = Vec3(cs * nl.x - sn * nl.y, sn * nl.x + cs * nl.y, nl.z);
        }
        nOut = nl;
        return best;
    }

    static float hitBox(const Vec3& o, const Vec3& d, const RayBox& b, Vec3& nOut) {
        float cy = std::cos(-b.yaw), sy = std::sin(-b.yaw);
        Vec3 p = o - b.c;
        Vec3 pl(cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z);
        Vec3 dl(cy * d.x + sy * d.z, d.y, -sy * d.x + cy * d.z);
        float tn = -1e30f, tf = 1e30f;
        Vec3 nl(0, 1, 0);
        const float pv[3] = { pl.x, pl.y, pl.z }, dv[3] = { dl.x, dl.y, dl.z }, hv[3] = { b.half.x, b.half.y, b.half.z };
        for (int i = 0; i < 3; ++i) {
            if (std::fabs(dv[i]) < 1e-9f) {
                if (std::fabs(pv[i]) > hv[i]) return -1.0f;
                continue;
            }
            float t1 = (-hv[i] - pv[i]) / dv[i], t2 = (hv[i] - pv[i]) / dv[i];
            float sign = -1.0f;
            if (t1 > t2) { std::swap(t1, t2); sign = 1.0f; }
            if (t1 > tn) {
                tn = t1;
                nl = Vec3(0, 0, 0);
                if (i == 0) nl.x = sign; else if (i == 1) nl.y = sign; else nl.z = sign;
            }
            if (t2 < tf) tf = t2;
        }
        if (tn > tf || tf < 1e-4f) return -1.0f;
        float t = tn > 1e-4f ? tn : tf;
        float cs = std::cos(b.yaw), sn = std::sin(b.yaw);
        nOut = Vec3(cs * nl.x + sn * nl.z, nl.y, -sn * nl.x + cs * nl.z);
        return t;
    }

    // Nearest hit among the proxies and the ground plane y = 0 (used by the photon tracer).
    RayHit trace(const Vec3& o, const Vec3& d, bool includeVolcano = true) const {
        RayHit best;
        best.t = 1e30f;
        for (size_t i = 0; i < cones.size(); ++i) {
            if (i == 0 && !includeVolcano) continue;
            Vec3 n;
            float t = hitCone(o, d, cones[i], n);
            if (t > 0.0f && t < best.t) { best.t = t; best.n = n; best.kind = (i == 0) ? HitVolcano : HitOther; }
        }
        for (const RaySphere& s : spheres) {
            float t = hitSphere(o, d, s);
            if (t > 0.0f && t < best.t) { best.t = t; best.n = normalize3(o + d * t - s.c); best.kind = HitOther; }
        }
        for (const RayBox& bx : boxes) {
            Vec3 n;
            float t = hitBox(o, d, bx, n);
            if (t > 0.0f && t < best.t) { best.t = t; best.n = n; best.kind = HitOther; }
        }
        if (d.y < -1e-9f) {
            float t = -o.y / d.y;
            if (t > 1e-4f && t < best.t) { best.t = t; best.n = Vec3(0, 1, 0); best.kind = HitGround; }
        }
        if (best.kind == HitNone) best.t = -1.0f;
        return best;
    }

    // Upload the proxy scene as uniforms for raytrace.glsl.
    void upload(const Shader& sh) const {
        int nc = (int)std::min<size_t>(cones.size(), kMaxCones);
        sh.setInt("uNumCones", nc);
        for (int i = 0; i < nc; ++i) {
            const RayCone& c = cones[i];
            char name[32];
            std::snprintf(name, sizeof(name), "uConeA[%d]", i); sh.setVec4(name, c.base.x, c.base.y, c.base.z, c.r0);
            std::snprintf(name, sizeof(name), "uConeB[%d]", i); sh.setVec4(name, c.r1, c.h, c.tilt, c.shadowsCrater ? 1.0f : 0.0f);
            std::snprintf(name, sizeof(name), "uConeCol[%d]", i); sh.setVec3(name, c.color.x, c.color.y, c.color.z);
        }
        int ns = (int)std::min<size_t>(spheres.size(), kMaxSpheres);
        sh.setInt("uNumSph", ns);
        for (int i = 0; i < ns; ++i) {
            char name[32];
            std::snprintf(name, sizeof(name), "uSph[%d]", i);
            sh.setVec4(name, spheres[i].c.x, spheres[i].c.y, spheres[i].c.z, spheres[i].r);
        }
        int nb = (int)std::min<size_t>(boxes.size(), kMaxBoxes);
        sh.setInt("uNumBox", nb);
        for (int i = 0; i < nb; ++i) {
            const RayBox& bx = boxes[i];
            char name[32];
            std::snprintf(name, sizeof(name), "uBoxC[%d]", i);   sh.setVec4(name, bx.c.x, bx.c.y, bx.c.z, bx.yaw);
            std::snprintf(name, sizeof(name), "uBoxH[%d]", i);   sh.setVec3(name, bx.half.x, bx.half.y, bx.half.z);
            std::snprintf(name, sizeof(name), "uBoxCol[%d]", i); sh.setVec3(name, bx.color.x, bx.color.y, bx.color.z);
        }
        sh.setVec3("uGroundCol", groundColor.x, groundColor.y, groundColor.z);
        sh.setFloat("uVolcR", volcanoRadius);
    }
};
