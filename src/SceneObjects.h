// SceneObjects.h
//
// Declares the shared mesh library (every object reuses one of these few
// GPU buffers) and one draw function per object category. Every function
// takes the object's own placement parameters (position / scale / rotation
// / seed) so objects can be repositioned or duplicated by the caller without
// touching this file -- see main.cpp for where each object is actually
// placed in the scene.
//
// NOTE ON SCOPE: this stage of VolcanoX is objects-only. No lighting,
// materials, or shading model is applied -- every draw call below just sets
// a flat uniform color. Motion that is purely a transformation (position /
// rotation / scale changing with time, e.g. the lava fountain's parabolic
// arc) is implemented, because that is the transformation requirement the
// whole project is graded on and is independent of the lighting phase.
#pragma once
#include <vector>
#include "Mesh.h"
#include "Shader.h"
#include "MathUtils.h"
#include "DayNight.h"

namespace Scene {

constexpr int kLightningBoltVariants = 3;

// Material presets, selected per draw with the uMat uniform (see shaders/lighting.glsl):
// matte/default and rock share k_s = 0.05, n_s = 4; lava is glossy (n_s = 140) and emissive;
// ash is near-Lambertian (n_s = 2); unlit draws the colour as is (sky, stars, lightning, clouds).
enum Material { MatMatte = 0, MatRock = 1, MatLava = 2, MatAsh = 3, MatUnlit = 4 };

// Procedural texture ids, selected per draw with uTexId (see shaders/basic.frag).
enum TexId { TexNone = 0, TexRock = 1, TexAsh = 2, TexWood = 3 };
inline void setTexture(const Shader& sh, int id, float repeatU = 1.0f, float repeatV = 1.0f) {
    sh.setInt("uTexId", id);
    sh.setVec4("uTexScale", repeatU, repeatV, 0.0f, 0.0f);
}

// Set the model matrix and its normal matrix (inverse-transpose of the upper 3x3).
inline void setModel(const Shader& sh, const Mat4& model) {
    sh.setMat4("uModel", model);
    sh.setMat3("uNormalMat", normalMatrix(model));
}

struct MeshLibrary {
    Mesh cylinder;      // unit radius, unit height, base at y=0
    Mesh cone;          // unit base radius, sharp point at y=1
    Mesh sphere;        // unit radius, centered at origin
    Mesh box;           // 1x1x1, centered at origin
    Mesh disc;          // unit radius, flat in XZ at y=0
    Mesh rock[4];       // irregular rock variants, ~unit radius, centered at origin
    Mesh volcanoCone;   // wide base, small open top (the crater mouth); rebuildable (tessellation)
    Mesh craterBowl;    // the crater's inside wall (open-topped cone + this = a real crater)
    Mesh poolDisc;      // the lava pool's disc; rebuildable (tessellation)
    Mesh ring;          // flat ring, outer radius 1 and inner 0.86: shockwave and water ripples
    Mesh line;          // single segment (0,0,0)-(0,1,0)
    Mesh lightningBolt[kLightningBoltVariants]; // jagged bolts hanging from (0,0,0) to (0,-1,0)

    // Tessellation ladder for the `+` / `-` keys: the volcano cone and the pool disc are
    // rebuilt with fewer or more segments so Flat/Gouraud/Phong differences can be shown.
    static constexpr int kTessLevels = 7;
    int tessLevel = 5;                         // default level: 28 cone segments, 24 disc segments
    static int coneSegments(int level);
    static int discSegments(int level);
    void rebuildTessellation(int level);

    void build();
};

// ---- Existing VolcanoX objects (volcano / crater / lava / ash) ----
void drawGroundPlane(const Shader& sh, const MeshLibrary& lib);
void drawVolcano(const Shader& sh, const MeshLibrary& lib, const Vec3& base, float baseRadius, float height);
void drawCraterGlowDisc(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float radius, float time);
void drawCraterRimRocks(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float ringRadius, int count,
                        float baseRadius, float height);
void drawLavaPool(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float time, float sizeScale = 1.0f);
void drawLavaFlowChannel(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, const Vec3& poolPos, float baseRadius, float height);
// One lava flow running from the crater down the cone and out across the ground.
struct LavaFlowStyle {
    float azimuthDeg;    // direction around the volcano
    float endRadius;     // how far from the volcano axis the front finally reaches
    float widthStart, widthEnd;
    float meanderAmp;    // radians of sideways swing
    float meanderFreq, meanderPhase;
    float r, g, b;       // base crust color
    bool lobeEnd;        // finish with an irregular bulging flow front
    float growSpeed;     // fraction of the path advanced per second
};
void drawLavaFlow(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float baseRadius, float height,
                  const LavaFlowStyle& style, float time);
// Points sampled along the advanced part of a lava flow, for the inverse-ray-tracing emitters.
struct FlowSample {
    Vec3 pos;        // on the flow, just above the surface
    Vec3 normal;     // surface normal (cone slope or ground)
    float width;
    float heat;      // 0.. : travelling pulse plus the hot front
    float length;    // length of flow this sample stands for
};
void lavaFlowSamples(const LavaFlowStyle& style, float baseRadius, float height, float time, int count,
                     std::vector<FlowSample>& out);
// Currently active lava droplets: positions and brightness (valid after the droplets were drawn).
int lavaDropletEmitters(Vec3* pos, float* brightness, int maxCount);

void updateAndDrawLavaDroplets(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float baseRadius, float height, float time);
void updateAndDrawRockChunks(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float baseRadius, float height, float time);
void updateAndDrawAshPuffs(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float time);

// ---- New objects added for this task ----
void drawBurntTree(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float scale, float leanDeg, int seed);
void drawBurntHut(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float rotationDeg);
void drawWindmill(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float rotationDeg, float time);
void drawAshCloud(const Shader& sh, const MeshLibrary& lib, const Vec3& centerAboveCrater, const Vec3& windDir, float time);
// The burst smoke: each eruption burst throws a plume of puffs up from the vent. Closed-form in time.
// Sets the material/emission itself and leaves the ash material selected.
void drawEruptionSmoke(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, const Vec3& windDir, float time);
// The rest of an eruption burst (Effects.cpp): the shockwave rings and, after a big burst, the pyroclastic
// flows and the long-range bombs thrown at the lake with their splash, ripples and steam.
void drawEruptionEffects(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float baseRadius, float height, float time);
void drawVolcanicLightning(const Shader& sh, const MeshLibrary& lib, const Vec3& cloudCenter, float time);
// ---- Environment (Environment.cpp) ----
void drawSky(const Shader& sh, const MeshLibrary& lib, const Vec3& eye, const SkyState& sky, float flash);
void drawTerrain(const Shader& sh, const MeshLibrary& lib);
void drawStormClouds(const Shader& sh, const MeshLibrary& lib, float baseY, const Vec3& windDir, float time, float flash, float daylight);

float lightningFlash(float time); // 0..1 strike brightness, used by main for the sky flash
// yawRad turns the drone to face its direction of travel (its camera is on the +Z side);
// pitchRad/rollRad lean it into motion (pilot drone).
void drawResearchDrone(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float time,
                       float yawRad = 0.0f, float pitchRad = 0.0f, float rollRad = 0.0f);

// ---- Controllable vehicles (Controller.h holds their state; main.cpp passes it in) ----
// All models face +Z and have their origin on the ground (the boat: on the water line).
void drawRover(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float yawRad, float rollRad, float wheelAngle);
void drawHelicopter(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float yawRad, float pitchRad, float rollRad,
                    float rotorAngle);
void drawHelipad(const Shader& sh, const MeshLibrary& lib, const Vec3& pos);
void drawBoat(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float yawRad, float speed, float time);

// Height of the ground / cone surface under (x, z); used for ballistic landings
// and for the pilot drone's altitude floor.
float terrainHeightAt(float x, float z, float baseR, float height);

// How many pooled particles were drawn (active) in the last frame, for the HUD.
int activeLavaDroplets();
int activeAshPuffs();

} // namespace Scene
