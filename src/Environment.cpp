// Environment.cpp
//
// Everything that makes the scene feel like a real place instead of objects
// on a plate: a night sky dome with stars and a moon, a storm-cloud layer high
// above (the source of the thunder), wide terrain with hills, a lake, a
// healthy forest, and mountain ranges receding into the distance.
// Same rules as the rest of the project: flat uniform colors only.
#include "SceneObjects.h"
#include <cmath>

namespace Scene {

static void put(const Shader& sh, const Mesh& mesh, const Mat4& model,
                float r, float g, float b, float a = 1.0f) {
    setModel(sh, model);
    sh.setVec4("uColor", r, g, b, a);
    mesh.draw();
}

static float rnd01(int seed) { return 0.5f + 0.5f * hashNoise(seed); }

// =======================================================================
// Sky: gradient dome + stars + moon. Centered on the camera so it never
// gets closer or farther; drawn first, without writing depth.
// =======================================================================
void drawSky(const Shader& sh, const MeshLibrary& lib, const Vec3& eye, const SkyState& sky, float flash) {
    glDepthMask(GL_FALSE);

    // Gradient dome: horizon colour at the bottom, zenith colour at the top.
    // Both follow the day/night clock; a lightning flash brightens them.
    glDisable(GL_CULL_FACE);   // the dome is seen from inside
    sh.setVec4("uSky", 1, 0, 0, 0);
    sh.setVec4("uColor2", sky.zenith.x + 0.25f * flash, sky.zenith.y + 0.26f * flash, sky.zenith.z + 0.34f * flash, 1.0f);
    setModel(sh, Mat4::translate(eye) * Mat4::scale(900.0f));
    sh.setVec4("uColor", sky.horizon.x + 0.30f * flash, sky.horizon.y + 0.31f * flash, sky.horizon.z + 0.38f * flash, 1.0f);
    lib.sphere.draw();
    sh.setVec4("uSky", 0, 0, 0, 0);
    glEnable(GL_CULL_FACE);

    // Stars fade out with daylight and with each lightning flash.
    float starA = sky.starAlpha * (1.0f - flash);
    if (starA > 0.05f) {
        for (int i = 0; i < 220; ++i) {
            float az = radians(360.0f * rnd01(i * 3 + 1));
            float el = radians(8.0f + 80.0f * rnd01(i * 3 + 2));
            Vec3 d(std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az));
            float size = 1.2f + 1.8f * rnd01(i * 3 + 3);
            put(sh, lib.sphere, Mat4::translate(eye + d * 850.0f) * Mat4::scale(size), 0.9f, 0.92f, 1.0f, starA);
        }
    }

    // Moon: opposite the sun, faint by day, hidden below the horizon.
    float moonVis = smoothstepf(-0.08f, 0.05f, sky.moonDir.y) * lerpf(1.0f, 0.25f, sky.daylight);
    if (moonVis > 0.01f) {
        Vec3 mp = eye + sky.moonDir * 800.0f;
        put(sh, lib.sphere, Mat4::translate(mp) * Mat4::scale(60.0f), 0.85f, 0.88f, 1.0f, 0.10f * moonVis);
        put(sh, lib.sphere, Mat4::translate(mp) * Mat4::scale(32.0f), 0.93f, 0.94f, 0.88f, moonVis);
    }

    // Sun: a bright disc with a halo, reddening near the horizon.
    float sunVis = smoothstepf(-0.08f, 0.05f, sky.sunDir.y);
    if (sunVis > 0.01f) {
        Vec3 sp = eye + sky.sunDir * 800.0f;
        float rr = 1.0f, gg = lerpf(0.95f, 0.55f, sky.twilight), bb = lerpf(0.75f, 0.25f, sky.twilight);
        put(sh, lib.sphere, Mat4::translate(sp) * Mat4::scale(95.0f), rr, gg * 0.85f, bb * 0.7f, 0.16f * sunVis);
        put(sh, lib.sphere, Mat4::translate(sp) * Mat4::scale(40.0f), rr, gg, bb, sunVis);
    }

    glDepthMask(GL_TRUE);
}

// Placement of the i-th healthy forest tree, shared by drawTerrain and forestTreeColliders so that a tree
// that is drawn is also solid. Returns false for the trees that fall on the lake and are skipped.
static bool forestTreeAt(int i, Vec3& p, float& s) {
    float ang = radians(360.0f * rnd01(i * 7 + 1));
    float r = 95.0f + 70.0f * rnd01(i * 7 + 2);
    p = Vec3(r * std::cos(ang), 0.06f, r * std::sin(ang));
    if ((p.x + 125.0f) * (p.x + 125.0f) / 1100.0f + (p.z + 70.0f) * (p.z + 70.0f) / 500.0f < 1.0f) return false;   // keep the lake clear
    s = 0.8f + 0.9f * rnd01(i * 7 + 3);
    return true;
}

void forestTreeColliders(std::vector<Collider>& out) {
    for (int i = 0; i < 90; ++i) {
        Vec3 p; float s;
        if (!forestTreeAt(i, p, s)) continue;
        Collider c; c.x = p.x; c.z = p.z; c.r = 0.9f * s; c.kind = KindForestTree;   // trunk plus the low foliage
        out.push_back(c);
    }
}

// =======================================================================
// Terrain: wide ground, ash field around the volcano, hills, lake,
// forest, distant mountains.
// =======================================================================
void drawTerrain(const Shader& sh, const MeshLibrary& lib) {
    // Outer land (top at y=0.06), then concentric ash-to-earth discs. Ash texture.
    setTexture(sh, TexAsh, 300.0f, 300.0f);
    put(sh, lib.box, Mat4::translate(Vec3(0, -0.5f, 0)) * Mat4::scale(Vec3(3000.0f, 1.12f, 3000.0f)), 0.045f, 0.075f, 0.05f);
    setTexture(sh, TexAsh, 90.0f, 90.0f);
    put(sh, lib.disc, Mat4::translate(Vec3(0, 0.07f, 0)) * Mat4::scale(Vec3(140.0f, 1, 140.0f)), 0.07f, 0.09f, 0.06f);
    put(sh, lib.disc, Mat4::translate(Vec3(0, 0.08f, 0)) * Mat4::scale(Vec3(105.0f, 1, 105.0f)), 0.10f, 0.10f, 0.08f);
    put(sh, lib.disc, Mat4::translate(Vec3(0, 0.09f, 0)) * Mat4::scale(Vec3(78.0f, 1, 78.0f)), 0.14f, 0.12f, 0.11f);

    setTexture(sh, TexNone);

    // Lake reflecting the sky, beyond the ash field. With ray tracing on it mirrors the sky
    // (uRefl < 0 selects a Schlick Fresnel reflectance).
    sh.setFloat("uRefl", -1.0f);
    put(sh, lib.disc, Mat4::translate(Vec3(-125.0f, 0.11f, -70.0f)) * Mat4::scale(Vec3(30.0f, 1, 20.0f)), 0.03f, 0.06f, 0.12f);
    sh.setFloat("uRefl", 0.0f);

    // Rolling hills (flattened spheres half-buried in the ground).
    for (int i = 0; i < 16; ++i) {
        float ang = radians(360.0f * i / 16.0f + 12.0f * hashNoise(i));
        float r = 160.0f + 50.0f * rnd01(i * 5 + 1);
        float sz = 24.0f + 20.0f * rnd01(i * 5 + 2);
        float g = 0.05f + 0.03f * rnd01(i * 5 + 3);
        // A hill must not sit on the lake: the boat sails there.
        float hx = r * std::cos(ang), hz = r * std::sin(ang);
        float lx = (hx + 125.0f) / (30.0f + sz * 1.6f), lz = (hz + 70.0f) / (20.0f + sz * 1.2f);
        if (lx * lx + lz * lz < 1.0f) continue;
        put(sh, lib.sphere, Mat4::translate(Vec3(r * std::cos(ang), 0, r * std::sin(ang))) * Mat4::scale(Vec3(sz * 1.6f, sz * 0.32f, sz * 1.2f)),
            g * 0.8f, g + 0.02f, g * 0.7f);
    }

    // Healthy forest well outside the ash zone (contrast with the burnt trees).
    for (int i = 0; i < 90; ++i) {
        Vec3 p; float s;
        if (!forestTreeAt(i, p, s)) continue;      // the same placement the collision shapes use
        float g = 0.07f + 0.05f * rnd01(i * 7 + 4);
        put(sh, lib.cylinder, Mat4::translate(p) * Mat4::scale(Vec3(0.25f * s, 1.6f * s, 0.25f * s)), 0.09f, 0.06f, 0.04f);
        put(sh, lib.cone, Mat4::translate(p + Vec3(0, 1.0f * s, 0)) * Mat4::scale(Vec3(1.7f * s, 3.4f * s, 1.7f * s)), 0.03f, g + 0.05f, 0.04f);
        put(sh, lib.cone, Mat4::translate(p + Vec3(0, 2.6f * s, 0)) * Mat4::scale(Vec3(1.2f * s, 2.6f * s, 1.2f * s)), 0.03f, g + 0.07f, 0.04f);
    }

    // Mountain ranges: near range darker, far range hazier. Snow caps on the tall ones. Rock texture.
    setTexture(sh, TexRock, 4.0f, 3.0f);
    for (int layer = 0; layer < 2; ++layer) {
        int n = layer == 0 ? 16 : 22;
        for (int i = 0; i < n; ++i) {
            int sd = layer * 100 + i;
            float ang = radians(360.0f * i / n + 8.0f * hashNoise(sd));
            float r = (layer == 0 ? 300.0f : 500.0f) + 40.0f * rnd01(sd * 3 + 1);
            float h = (layer == 0 ? 40.0f : 75.0f) + 35.0f * rnd01(sd * 3 + 2);
            float w = h * (0.75f + 0.35f * rnd01(sd * 3 + 3));
            Vec3 p(r * std::cos(ang), 0, r * std::sin(ang));
            float haze = layer == 0 ? 0.0f : 0.05f;
            put(sh, lib.cone, Mat4::translate(p) * Mat4::scale(Vec3(w, h, w)), 0.07f + haze, 0.08f + haze, 0.12f + haze * 1.3f);
            if (h > 60.0f) {
                float cap = 0.32f;
                put(sh, lib.cone, Mat4::translate(p + Vec3(0, h * (1.0f - cap), 0)) * Mat4::scale(Vec3(w * cap, h * cap, w * cap)),
                    0.55f, 0.58f, 0.66f);
            }
        }
    }
    setTexture(sh, TexNone);
}

// =======================================================================
// Storm-cloud layer high above the scene. Thunder bolts start here.
// Puffs drift with the wind and fade at the layer's edges, so wrapping
// around is invisible. Lightning brightens the whole layer.
// =======================================================================
void drawStormClouds(const Shader& sh, const MeshLibrary& lib, float baseY, const Vec3& windDir, float time, float flash, float daylight) {
    Vec3 wind = normalize3(windDir);
    const float span = 130.0f;

    glDepthMask(GL_FALSE);
    for (int layer = 0; layer < 2; ++layer) {
        int n = layer == 0 ? 46 : 34;
        for (int i = 0; i < n; ++i) {
            int sd = layer * 200 + i;
            float along = std::fmod(span * 2.0f * rnd01(sd * 5 + 1) + time * (1.2f + 0.5f * layer), span * 2.0f) - span;
            float side = span * 1.6f * (rnd01(sd * 5 + 2) - 0.5f) * 1.25f;
            Vec3 p(wind.x * along - wind.z * side, baseY + 3.0f * rnd01(sd * 5 + 3) + layer * 3.5f,
                   wind.z * along + wind.x * side);
            float fade = std::fmin(1.0f, (span - std::fabs(along)) / 35.0f);
            float sx = 16.0f + 12.0f * rnd01(sd * 5 + 4);
            float sz = 12.0f + 10.0f * rnd01(sd * 5 + 5);
            float sy = 3.5f + 2.5f * rnd01(sd * 5 + 4);
            float shade = 0.12f + 0.05f * rnd01(sd) + (layer ? 0.04f : 0.0f) + 0.55f * flash + 0.35f * daylight;
            put(sh, lib.sphere, Mat4::translate(p) * Mat4::scale(Vec3(sx, sy, sz)), shade, shade * 1.02f, shade * 1.10f, 0.95f * fade);   // a thick deck: ~5% shows through one puff
        }
    }
    glDepthMask(GL_TRUE);
}

} // namespace Scene
