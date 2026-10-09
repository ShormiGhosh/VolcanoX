// NewObjects.cpp
//
// Objects added for the "objects-only" stage of VolcanoX: burnt trees, a
// burnt/abandoned hut, a windmill, the ash/smoke cloud, volcanic lightning,
// and a research drone. Every object below is justified in
// a real volcanic setting -- see the comment at the top of each function.
//
// None of these use lighting, materials, or a shading model: every draw
// call sets a single flat uniform color, same as SceneObjects.cpp.
#include "SceneObjects.h"
#include "Eruption.h"
#include <cmath>
#include <vector>

namespace Scene {

static void drawMeshLocal(const Shader& sh, const Mesh& mesh, const Mat4& model,
                           float r, float g, float b, float a = 1.0f) {
    setModel(sh, model);
    sh.setVec4("uColor", r, g, b, a);
    mesh.draw();
}

// =======================================================================
// Burnt Tree
//   Real-world basis: intense heat, ashfall, and pyroclastic surges kill
//   and char vegetation near an active volcano ("ghost forest" scenes are
//   documented at Mount St. Helens and other eruptions). A healthy tree
//   would look wrong here; this one is a bare, blackened trunk with a few
//   snapped branches and no foliage.
//   Geometry: cone (tapered trunk) + cylinders (branches) + irregular rock
//   primitives (charred stumps/debris at the base).
// =======================================================================
void drawBurntTree(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float scale, float leanDeg, int seed) {
    const float r = 0.08f, g = 0.06f, b = 0.05f; // charred black-brown

    Mat4 base = Mat4::translate(pos)
              * Mat4::rotateZ(radians(leanDeg))     // leans away from the wind, like real fire/wind-damaged trees
              * Mat4::rotateY(radians(hashNoise(seed) * 180.0f));

    // Trunk: a slightly tapered cone standing on end, cracked look implied by irregular color already.
    float trunkHeight = 3.2f * scale;
    float trunkRadius = 0.22f * scale;
    Mat4 trunkModel = base * Mat4::scale(Vec3(trunkRadius, trunkHeight, trunkRadius));
    drawMeshLocal(sh, lib.cone, trunkModel, r, g, b);

    // A handful of bare, broken branches jutting out at irregular angles/heights.
    int branchCount = 4;
    for (int i = 0; i < branchCount; ++i) {
        float hFrac = 0.45f + 0.45f * (i / float(branchCount));
        float branchHeight = trunkHeight * hFrac;
        float outAngle = 360.0f * i / branchCount + hashNoise(seed * 7 + i) * 40.0f;
        float tilt = 35.0f + hashNoise(seed * 11 + i) * 25.0f; // upward angle
        float branchLen = trunkHeight * (0.28f + 0.12f * hashNoise(seed * 13 + i));

        Mat4 branchModel = base
            * Mat4::translate(Vec3(0, branchHeight, 0))
            * Mat4::rotateY(radians(outAngle))
            * Mat4::rotateZ(radians(-tilt))
            * Mat4::translate(Vec3(0, branchLen * 0.5f, 0))
            * Mat4::scale(Vec3(0.06f * scale, branchLen, 0.06f * scale));
        drawMeshLocal(sh, lib.cylinder, branchModel, r * 1.1f, g * 1.1f, b * 1.1f);
    }

    // Charred stump/debris fragments scattered at the foot of the tree.
    for (int i = 0; i < 2; ++i) {
        float ang = 360.0f * i / 2.0f + hashNoise(seed * 17 + i) * 90.0f;
        float dist = 0.5f * scale + 0.3f * scale * hashNoise(seed * 19 + i);
        Vec3 offset(dist * std::cos(radians(ang)), 0.12f * scale, dist * std::sin(radians(ang)));
        Mat4 debrisModel = Mat4::translate(pos + offset) * Mat4::scale(0.22f * scale);
        drawMeshLocal(sh, lib.rock[(seed + i) % 4], debrisModel, 0.05f, 0.04f, 0.04f);
    }
}

// =======================================================================
// Burnt / Damaged Hut
//   Real-world basis: rural dwellings near active volcanoes (e.g. around
//   Sinabung, Merapi) are routinely damaged or destroyed by ashfall, heat,
//   and pyroclastic material. This hut represents that aftermath: intact on
//   one side, collapsed/charred on the other.
//   Geometry: boxes for walls + a collapsed roof made from two angled box
//   panels (one knocked askew) + dark debris blocks.
// =======================================================================
void drawBurntHut(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float rotationDeg) {
    Mat4 base = Mat4::translate(pos) * Mat4::rotateY(radians(rotationDeg));
    const float wallW = 3.2f, wallH = 2.0f, wallT = 0.18f, depth = 2.6f;
    const float wallCol[3] = { 0.32f, 0.24f, 0.16f };     // weathered wood/mud brick
    const float burntCol[3] = { 0.10f, 0.08f, 0.07f };    // charred

    // Back wall (intact).
    drawMeshLocal(sh, lib.box,
        base * Mat4::translate(Vec3(0, wallH * 0.5f, -depth * 0.5f)) * Mat4::scale(Vec3(wallW, wallH, wallT)),
        wallCol[0], wallCol[1], wallCol[2]);

    // Left wall (intact).
    drawMeshLocal(sh, lib.box,
        base * Mat4::translate(Vec3(-wallW * 0.5f, wallH * 0.5f, 0)) * Mat4::scale(Vec3(wallT, wallH, depth)),
        wallCol[0], wallCol[1], wallCol[2]);

    // Right wall -- burnt and partially collapsed (shorter, tilted, dark).
    drawMeshLocal(sh, lib.box,
        base * Mat4::translate(Vec3(wallW * 0.5f, wallH * 0.28f, 0))
             * Mat4::rotateZ(radians(-10.0f))
             * Mat4::scale(Vec3(wallT, wallH * 0.55f, depth * 0.85f)),
        burntCol[0], burntCol[1], burntCol[2]);

    // Front wall -- mostly gone; only a low burnt stub remains either side of the doorway.
    drawMeshLocal(sh, lib.box,
        base * Mat4::translate(Vec3(-wallW * 0.32f, wallH * 0.18f, depth * 0.5f)) * Mat4::scale(Vec3(wallW * 0.32f, wallH * 0.35f, wallT)),
        burntCol[0], burntCol[1], burntCol[2]);
    drawMeshLocal(sh, lib.box,
        base * Mat4::translate(Vec3(wallW * 0.32f, wallH * 0.18f, depth * 0.5f)) * Mat4::scale(Vec3(wallW * 0.32f, wallH * 0.35f, wallT)),
        burntCol[0], burntCol[1], burntCol[2]);

    // Roof: two sloped panels forming a peak. The left panel is intact; the
    // right panel has slid down and tilted, showing storm/blast damage.
    float roofY = wallH;
    Mat4 leftRoof = base
        * Mat4::translate(Vec3(-wallW * 0.28f, roofY + 0.55f, 0))
        * Mat4::rotateZ(radians(22.0f))
        * Mat4::scale(Vec3(wallW * 0.62f, 0.12f, depth + 0.4f));
    drawMeshLocal(sh, lib.box, leftRoof, 0.22f, 0.16f, 0.10f);

    Mat4 rightRoof = base
        * Mat4::translate(Vec3(wallW * 0.30f, roofY + 0.15f, 0.15f))
        * Mat4::rotateZ(radians(-48.0f)) // slumped, no longer meeting the ridge
        * Mat4::scale(Vec3(wallW * 0.55f, 0.12f, depth + 0.2f));
    drawMeshLocal(sh, lib.box, rightRoof, burntCol[0], burntCol[1], burntCol[2]);

    // Scattered burnt debris around the base.
    for (int i = 0; i < 3; ++i) {
        float ang = 100.0f * i + 20.0f;
        float dist = 2.6f + 0.6f * i;
        Vec3 offset(dist * std::cos(radians(ang)), 0.1f, dist * std::sin(radians(ang)));
        Mat4 debrisModel = base * Mat4::translate(offset) * Mat4::scale(0.3f + 0.1f * i);
        drawMeshLocal(sh, lib.rock[i % 4], debrisModel, 0.07f, 0.06f, 0.05f);
    }
}

// =======================================================================
// Windmill (optional -- included)
//   Decision: a windmill is not a volcanic phenomenon, but the burnt hut
//   above establishes this scene includes a small rural homestead at the
//   volcano's foot. A simple water-pumping/grain windmill next to that hut
//   is a natural, common real-world pairing, so it earns its place here
//   rather than being forced in on its own. It is shown storm/blast-damaged
//   (one drooping blade) to stay consistent with the aftermath theme.
//   Geometry: cylinder (tower) + box (hub) + boxes (blades).
// =======================================================================
void drawWindmill(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float rotationDeg, float time) {
    Mat4 base = Mat4::translate(pos) * Mat4::rotateY(radians(rotationDeg));
    const float towerHeight = 4.5f;

    // Stone footing, tapered-looking tower, and a nacelle (the turning head) with a tail vane,
    // so it reads as a windmill rather than a post with a cross on it.
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, 0.15f, 0)) * Mat4::scale(Vec3(1.1f, 0.3f, 1.1f)), 0.22f, 0.20f, 0.18f);
    drawMeshLocal(sh, lib.cylinder, base * Mat4::scale(Vec3(0.18f, towerHeight, 0.18f)), 0.28f, 0.22f, 0.16f);
    drawMeshLocal(sh, lib.cylinder, base * Mat4::scale(Vec3(0.30f, 2.0f, 0.30f)), 0.28f, 0.22f, 0.16f);   // thicker lower tower
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, towerHeight + 0.12f, 0.05f)) * Mat4::scale(Vec3(0.36f, 0.32f, 0.9f)),
                  0.20f, 0.15f, 0.11f);                                                                   // nacelle
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, towerHeight + 0.25f, -0.85f)) * Mat4::scale(Vec3(0.04f, 0.55f, 0.7f)),
                  0.30f, 0.26f, 0.20f);                                                                   // tail vane

    // The rotor sits in front of the tower (+Z), so its blades turn clear of it. It spins about
    // its own axis: R_z(spin * t) is a child transform of the nacelle.
    Vec3 hubPos(0, towerHeight + 0.12f, 0.62f);
    Mat4 hubModel = base * Mat4::translate(hubPos) * Mat4::scale(0.2f);
    drawMeshLocal(sh, lib.box, hubModel, 0.12f, 0.10f, 0.08f);
    float spin = 0.9f * time;

    // Four blades; one is snapped and droops (blast/storm damage).
    for (int i = 0; i < 4; ++i) {
        float bladeAngle = 90.0f * i;
        float droop = (i == 2) ? 55.0f : 0.0f; // one damaged blade
        float bladeLen = (i == 2) ? 1.1f : 1.7f;

        // Droop is applied about the local X axis *before* the blade is pushed outward along Y,
        // so it bends the blade tip out of the rotor's plane.
        Mat4 bladeModel = base
            * Mat4::translate(hubPos)
            * Mat4::rotateZ(spin + radians(bladeAngle))
            * Mat4::rotateX(radians(droop))
            * Mat4::translate(Vec3(0, 0.1f + bladeLen * 0.5f, 0))
            * Mat4::scale(Vec3(0.5f, bladeLen, 0.04f));
        drawMeshLocal(sh, lib.box, bladeModel, 0.34f, 0.29f, 0.22f);
        // A darker spar down the blade's leading edge gives it the look of a sail frame.
        Mat4 sparModel = base
            * Mat4::translate(hubPos)
            * Mat4::rotateZ(spin + radians(bladeAngle))
            * Mat4::rotateX(radians(droop))
            * Mat4::translate(Vec3(-0.22f, 0.1f + bladeLen * 0.5f, 0.02f))
            * Mat4::scale(Vec3(0.06f, bladeLen, 0.06f));
        drawMeshLocal(sh, lib.box, sparModel, 0.18f, 0.13f, 0.09f);
    }
}

// =======================================================================
// Volcanic Ash / Smoke Cloud
//   Real-world basis: an eruption column rises from the vent and spreads
//   into a broad umbrella/mushroom shape as it loses momentum in the
//   atmosphere, and wind pushes the whole plume sideways downwind. This is
//   exactly the classic "volcano -> eruption column -> spreading ash cloud"
//   silhouette used in every textbook diagram of an eruption.
//   Geometry: a stacked column of spheres (the rising stem) topped with a
//   wider cauliflower cluster of spheres (the spreading head), both
//   increasingly offset along the wind direction with height.
// =======================================================================
void drawAshCloud(const Shader& sh, const MeshLibrary& lib, const Vec3& centerAboveCrater, const Vec3& windDir, float time) {
    Vec3 wind = normalize3(windDir);

    // The umbrella breathes with the eruption: it swells and climbs after each burst (the smoke
    // piles up under it) and thins out again before the next one. `level` is 0.35 when quiet.
    const float level = std::min(1.5f, 0.35f + Eruption::plume(time));
    const float size = 0.62f + 0.5f * level;       // 0.8 quiet .. 1.4 after a big burst
    const float lift = 0.75f + 0.35f * level;      // the head rises as well

    // Translucent puffs must not write depth, or the ones drawn first hide
    // everything behind/inside them (including the lightning).
    glDepthMask(GL_FALSE);

    // Rising stem: spheres above the crater, widening with height.
    for (int i = 0; i < 5; ++i) {
        float h = (2.0f + i * 2.4f) * lift;
        float radius = (1.8f + i * 0.5f) * (0.8f + 0.3f * level);
        Vec3 p = centerAboveCrater + Vec3(0, h, 0) + wind * (h * 0.12f);
        float bob = 0.08f * std::sin(time * 0.6f + i);
        Mat4 model = Mat4::translate(p + Vec3(0, bob, 0)) * Mat4::scale(radius);
        drawMeshLocal(sh, lib.sphere, model, 0.30f, 0.29f, 0.29f, 0.55f);
    }

    // Spreading head: a broad cluster of overlapping spheres, offset downwind.
    Vec3 headCenter = centerAboveCrater + Vec3(0, 13.5f * lift, 0) + wind * 2.5f;
    const int headCount = 12;
    for (int i = 0; i < headCount; ++i) {
        float ang = 360.0f * i / headCount + 6.0f * time * 0.2f;   // slowly rolls
        float ring = (4.6f + 0.8f * hashNoise(i * 3 + 1)) * size;
        Vec3 ringOffset(ring * std::cos(radians(ang)), 1.0f * hashNoise(i * 5 + 2), ring * std::sin(radians(ang)));
        Vec3 p = headCenter + ringOffset + wind * (1.5f + 1.0f * (i / float(headCount)));
        float radius = (2.8f + 0.9f * (0.5f + 0.5f * hashNoise(i * 7 + 3))) * size;
        float bob = 0.12f * std::sin(time * 0.5f + i * 1.3f);

        Mat4 model = Mat4::translate(p + Vec3(0, bob, 0)) * Mat4::scale(radius);
        float shade = 0.36f + 0.06f * hashNoise(i * 9 + 4); // dark storm-grey with subtle variation
        drawMeshLocal(sh, lib.sphere, model, shade, shade * 0.98f, shade * 0.97f, 0.5f);
    }

    // Central mass to keep the head visually solid rather than a ring of holes.
    Mat4 coreModel = Mat4::translate(headCenter) * Mat4::scale(Vec3(4.6f, 3.2f, 4.6f) * size);
    drawMeshLocal(sh, lib.sphere, coreModel, 0.30f, 0.30f, 0.31f, 0.6f);

    glDepthMask(GL_TRUE);
}

// =======================================================================
// Eruption burst smoke
//   Every eruption burst (Eruption.h) throws a plume of puffs up out of the vent. Each puff
//   leaves at a hash-chosen direction and speed, slows under drag, drifts downwind, swells,
//   cools from glowing orange to grey and fades out. Position, size, colour and alpha are all
//   closed-form functions of the puff's age, so the whole effect is computed from time alone.
//   A flash of fire at the vent marks the start of each burst.
// =======================================================================
void drawEruptionSmoke(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, const Vec3& windDir, float time) {
    const int kPuffs = 22;          // puffs per burst
    const float kStagger = 0.035f;  // seconds between the puffs of one burst
    const float kLife = 10.0f;      // seconds a puff lives
    const float kDrag = 0.75f;      // 1/s: the initial speed decays as exp(-kDrag * age)
    Vec3 wind = normalize3(windDir);

    Eruption::Burst bursts[Eruption::kMaxRecent];
    int nBursts = Eruption::recent(time, kLife + kPuffs * kStagger, bursts);
    if (nBursts == 0) return;

    glDepthMask(GL_FALSE);          // translucent: don't hide what is behind or inside the puffs
    sh.setInt("uMat", MatAsh);
    for (int bi = 0; bi < nBursts; ++bi) {
        const float start = bursts[bi].start, amp = bursts[bi].amp;
        const int b = bursts[bi].id;

        // Fire flash at the vent in the first half second: an expanding, fading orange fireball.
        float tau0 = time - start;
        if (tau0 >= 0.0f && tau0 < 0.75f) {
            float k = tau0 / 0.75f;
            sh.setInt("uMat", MatUnlit);
            // Two shells: a white-hot core and a wider orange fireball, both expanding and fading.
            drawMeshLocal(sh, lib.sphere, Mat4::translate(craterPos + Vec3(0, 2.0f + 3.0f * k, 0)) * Mat4::scale((3.5f + 11.0f * k) * amp),
                          1.0f, 0.55f - 0.25f * k, 0.15f, 0.65f * (1.0f - k));
            drawMeshLocal(sh, lib.sphere, Mat4::translate(craterPos + Vec3(0, 1.5f + 2.0f * k, 0)) * Mat4::scale((2.0f + 5.0f * k) * amp),
                          1.0f, 0.95f, 0.7f, 0.9f * (1.0f - k) * (1.0f - k));
            sh.setInt("uMat", MatAsh);
        }

        for (int j = 0; j < kPuffs; ++j) {
            float age = time - (start + j * kStagger);
            if (age < 0.0f || age > kLife) continue;
            int seed = b * 131 + j * 7;
            auto rnd = [&](int k) { return 0.5f + 0.5f * hashNoise(seed + k); };

            // Launch direction: mostly up, spread sideways by up to ~25 degrees.
            float az = 6.2831853f * rnd(1), lean = 0.05f + 0.40f * rnd(2);
            Vec3 dir(std::sin(lean) * std::cos(az), std::cos(lean), std::sin(lean) * std::sin(az));
            float speed = (14.0f + 12.0f * rnd(3)) * amp;

            // Distance travelled under drag: speed * (1 - e^{-k age}) / k, plus a steady buoyant rise and wind drift.
            float travelled = speed * (1.0f - std::exp(-kDrag * age)) / kDrag;
            Vec3 p = craterPos + dir * travelled + Vec3(0, 1.1f * age, 0) + wind * (0.9f * age * (0.4f + 0.6f * std::min(1.0f, age / 3.0f)));

            float radius = (1.2f + 4.4f * (1.0f - std::exp(-0.7f * age))) * (0.8f + 0.4f * rnd(4)) * (0.7f + 0.3f * amp);
            float fadeIn = std::min(1.0f, age / 0.12f);
            float fadeOut = std::pow(std::max(0.0f, 1.0f - age / kLife), 1.3f);
            float alpha = 0.75f * fadeIn * fadeOut;

            // Hot at the vent, grey by the time it has risen: colour and emission both follow exp(-age).
            float heat = std::exp(-age / 0.9f);
            float r = 0.38f + 0.62f * heat, g = 0.37f + 0.18f * heat, b2 = 0.36f - 0.18f * heat;
            sh.setFloat("uEmit", 0.9f * heat);
            drawMeshLocal(sh, lib.sphere, Mat4::translate(p) * Mat4::scale(radius), r, g, b2, alpha);
        }
    }
    sh.setFloat("uEmit", 0.0f);
    glDepthMask(GL_TRUE);
}

// =======================================================================
// Volcanic Lightning
//   Real-world basis: volcanic lightning ("dirty thunderstorms") is a
//   documented, real phenomenon caused by static charge building up from
//   colliding ash and rock particles inside an eruption plume -- it is not
//   generic fantasy thunder, and it only makes sense drawn inside/around
//   an ash cloud, which is why this function takes the cloud's center.
//   Geometry: jagged line segments (fractal midpoint-displacement bolts),
//   kept static/visible per this stage's scope (no flicker animation yet).
// =======================================================================
// Strike schedule shared by the bolts and the sky flash: one strike every
// kStrikePeriod seconds, a quick double-flicker lasting ~0.25s.
static const float kStrikePeriod = 2.2f;

static float strikeIntensity(float time, int& strikeIndex) {
    strikeIndex = (int)std::floor(time / kStrikePeriod);
    float local = time - strikeIndex * kStrikePeriod;
    if (local < 0.07f) return 1.0f;
    if (local > 0.12f && local < 0.24f) return 0.75f * (1.0f - (local - 0.12f) / 0.12f) + 0.25f;
    return 0.0f;
}

float lightningFlash(float time) {
    int k;
    return strikeIntensity(time, k);
}

void drawVolcanicLightning(const Shader& sh, const MeshLibrary& lib, const Vec3& cloudCenter, float time) {
    int strike;
    float intensity = strikeIntensity(time, strike);
    if (intensity <= 0.0f) return;

    // Bolt polylines (unit length, hanging down -Y) built once; drawn as thin
    // boxes because 1px GL lines are practically invisible and unreliable in
    // a core profile.
    static std::vector<Vec3> polylines[kLightningBoltVariants];
    static bool built = false;
    if (!built) {
        for (int v = 0; v < kLightningBoltVariants; ++v) {
            VertexList vl = Geometry::lightningBolt(Vec3(0, 0, 0), Vec3(0, -1, 0), v * 31 + 5, 6, 0.10f);
            const size_t S = kVertexFloats;                  // floats per vertex
            for (size_t i = 0; i + 2 * S <= vl.size(); i += 2 * S) {   // GL_LINES: 2 points per segment
                polylines[v].push_back(Vec3(vl[i], vl[i + 1], vl[i + 2]));
                polylines[v].push_back(Vec3(vl[i + S], vl[i + S + 1], vl[i + S + 2]));
            }
        }
        built = true;
    }

    // cloudCenter.y is the height of the storm-cloud base; bolts run from it to the ground.
    const float H = cloudCenter.y;
    glDepthMask(GL_FALSE);
    for (int n = 0; n < 2; ++n) {
        int variant = (strike + n) % kLightningBoltVariants;
        float ang = radians(360.0f * (0.5f + 0.5f * hashNoise(strike * 13 + n * 5 + 1)));
        float dist = 22.0f + 40.0f * (0.5f + 0.5f * hashNoise(strike * 7 + n * 3 + 2));
        Vec3 origin(cloudCenter.x + dist * std::cos(ang), H, cloudCenter.z + dist * std::sin(ang));
        float length = (n == 0 ? H : H * 0.75f);
        float thick = (n == 0 ? 0.35f : 0.2f);

        const std::vector<Vec3>& pl = polylines[variant];
        for (size_t i = 0; i + 1 < pl.size(); i += 2) {
            Vec3 a = origin + Vec3(pl[i].x * length, pl[i].y * length, pl[i].z * length);
            Vec3 b = origin + Vec3(pl[i + 1].x * length, pl[i + 1].y * length, pl[i + 1].z * length);
            Vec3 d = b - a;
            float len = length3(d);
            if (len < 1e-4f) continue;
            float horiz = std::sqrt(d.x * d.x + d.z * d.z);
            Mat4 m = Mat4::translate((a + b) * 0.5f) * Mat4::rotateY(std::atan2(d.x, d.z))
                   * Mat4::rotateX(-std::atan2(d.y, horiz))
                   * Mat4::scale(Vec3(thick, thick, len * 1.1f));
            drawMeshLocal(sh, lib.box, m, 0.85f, 0.90f, 1.0f, intensity);
        }
    }
    glDepthMask(GL_TRUE);
}

// =======================================================================
// Research / Camera Drone
//   Real-world basis: quadcopter drones are the standard real-world tool
//   for close-range volcanic monitoring (thermal, gas, and visual survey)
//   because they can approach a hazardous vent without risking a person --
//   e.g. the kind of platform volcanology teams fly near active craters.
//   Geometry: a central box body, 4 box arms, disc "propellers", and a
//   small box sensor/camera slung underneath, angled to look down at the
//   volcano. Held at one simple static hover position for this stage.
// =======================================================================
void drawResearchDrone(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float time,
                       float yawRad, float pitchRad, float rollRad) {
    float bob = 0.12f * std::sin(time * 1.2f); // gentle hover bob
    // Hierarchy root: world position, heading (yaw), then lean (pitch/roll).
    // Every arm, hub, propeller and the camera is a child of this transform.
    Mat4 base = Mat4::translate(pos + Vec3(0, bob, 0)) * Mat4::rotateY(yawRad)
              * Mat4::rotateX(pitchRad) * Mat4::rotateZ(rollRad);

    // Central body.
    drawMeshLocal(sh, lib.box, base * Mat4::scale(Vec3(0.5f, 0.18f, 0.5f)), 0.85f, 0.85f, 0.86f);

    // 4 arms + rotors at 45/135/225/315 degrees.
    for (int i = 0; i < 4; ++i) {
        float ang = 45.0f + 90.0f * i;
        float rad = radians(ang);
        Vec3 dir(std::cos(rad), 0, std::sin(rad));
        float armLen = 0.9f;
        Vec3 tip = dir * armLen;

        Mat4 armModel = base * Mat4::translate(tip * 0.5f) * Mat4::rotateY(rad)
                      * Mat4::scale(Vec3(0.06f, 0.06f, armLen));
        drawMeshLocal(sh, lib.box, armModel, 0.25f, 0.25f, 0.26f);

        // Rotor hub (child of the arm tip).
        Mat4 hubModel = base * Mat4::translate(tip) * Mat4::scale(Vec3(0.08f, 0.08f, 0.08f));
        drawMeshLocal(sh, lib.cylinder, hubModel, 0.15f, 0.15f, 0.15f);

        // Propeller: two crossed blades spinning about the hub, R_y(w*t), in a
        // child transform of the hub. Alternate rotors turn in opposite
        // directions, like a real quadcopter. A faint disc suggests the blur.
        float spin = (i % 2 == 0 ? 1.0f : -1.0f) * time * 38.0f + i;
        Mat4 hubFrame = base * Mat4::translate(tip + Vec3(0, 0.09f, 0)) * Mat4::rotateY(spin);
        drawMeshLocal(sh, lib.box, hubFrame * Mat4::scale(Vec3(0.84f, 0.012f, 0.07f)), 0.12f, 0.12f, 0.13f);
        drawMeshLocal(sh, lib.box, hubFrame * Mat4::rotateY(radians(90.0f)) * Mat4::scale(Vec3(0.84f, 0.012f, 0.07f)), 0.12f, 0.12f, 0.13f);
        Mat4 propModel = base * Mat4::translate(tip + Vec3(0, 0.09f, 0)) * Mat4::scale(Vec3(0.42f, 0.01f, 0.42f));
        drawMeshLocal(sh, lib.cylinder, propModel, 0.7f, 0.7f, 0.72f, 0.18f);
    }

    // Camera/sensor gimbal underneath, tilted to look down at the volcano.
    Mat4 camModel = base * Mat4::translate(Vec3(0, -0.22f, 0.15f)) * Mat4::rotateX(radians(35.0f))
                  * Mat4::scale(Vec3(0.16f, 0.16f, 0.22f));
    drawMeshLocal(sh, lib.box, camModel, 0.08f, 0.08f, 0.09f);
}

// =======================================================================
// Controllable vehicles: rover, helicopter (+ helipad) and boat.
//   Each is a transform hierarchy: a body transform T * R_y(heading) * lean, and children
//   (wheels, rotors, radar) that move with it and also spin relative to it. The pilot drone
//   in drawResearchDrone is the fourth.
// =======================================================================
void drawRover(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float yawRad, float rollRad, float wheelAngle) {
    Mat4 base = Mat4::translate(pos) * Mat4::rotateY(yawRad) * Mat4::rotateZ(rollRad);
    const float yel[3] = { 0.85f, 0.65f, 0.08f };

    // Chassis, cabin with a windscreen, payload mast with a sensor dish, and two headlights.
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, 0.70f, 0)) * Mat4::scale(Vec3(1.7f, 0.5f, 3.0f)), yel[0], yel[1], yel[2]);
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, 1.22f, -0.35f)) * Mat4::scale(Vec3(1.4f, 0.55f, 1.5f)), yel[0] * 0.9f, yel[1] * 0.9f, yel[2]);
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, 1.28f, 0.42f)) * Mat4::scale(Vec3(1.3f, 0.4f, 0.06f)), 0.08f, 0.12f, 0.2f);
    drawMeshLocal(sh, lib.cylinder, base * Mat4::translate(Vec3(0.45f, 1.45f, -0.95f)) * Mat4::scale(Vec3(0.05f, 1.0f, 0.05f)), 0.3f, 0.3f, 0.32f);
    drawMeshLocal(sh, lib.sphere, base * Mat4::translate(Vec3(0.45f, 2.5f, -0.95f)) * Mat4::scale(Vec3(0.32f, 0.1f, 0.32f)), 0.8f, 0.8f, 0.82f);
    for (int s = -1; s <= 1; s += 2)
        drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(s * 0.58f, 0.78f, 1.51f)) * Mat4::scale(Vec3(0.28f, 0.16f, 0.06f)), 1.0f, 0.95f, 0.7f);

    // Four wheels. Each is a child of the chassis that also turns about its own axle: R_x(angle).
    // The cylinder mesh stands along Y, so R_z(90 deg) lays it along the axle (X).
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            Mat4 wheelFrame = base * Mat4::translate(Vec3(sx * 1.0f, 0.45f, sz * 1.0f)) * Mat4::rotateX(wheelAngle);
            drawMeshLocal(sh, lib.cylinder, wheelFrame * Mat4::rotateZ(radians(90.0f)) * Mat4::translate(Vec3(0, -0.2f, 0)) * Mat4::scale(Vec3(0.45f, 0.4f, 0.45f)),
                          0.07f, 0.07f, 0.08f);
            // A hub plate with a spoke, so the turning of the wheel can be seen.
            drawMeshLocal(sh, lib.box, wheelFrame * Mat4::translate(Vec3(sx * 0.21f, 0, 0)) * Mat4::scale(Vec3(0.04f, 0.1f, 0.78f)), 0.7f, 0.7f, 0.72f);
        }
    }
}

void drawHelipad(const Shader& sh, const MeshLibrary& lib, const Vec3& pos) {
    drawMeshLocal(sh, lib.disc, Mat4::translate(pos + Vec3(0, 0.03f, 0)) * Mat4::scale(Vec3(4.4f, 1.0f, 4.4f)), 0.16f, 0.16f, 0.18f);
    drawMeshLocal(sh, lib.disc, Mat4::translate(pos + Vec3(0, 0.05f, 0)) * Mat4::scale(Vec3(3.9f, 1.0f, 3.9f)), 0.30f, 0.30f, 0.12f);
    drawMeshLocal(sh, lib.disc, Mat4::translate(pos + Vec3(0, 0.07f, 0)) * Mat4::scale(Vec3(3.6f, 1.0f, 3.6f)), 0.14f, 0.14f, 0.16f);
    // The "H".
    drawMeshLocal(sh, lib.box, Mat4::translate(pos + Vec3(-0.9f, 0.1f, 0)) * Mat4::scale(Vec3(0.3f, 0.04f, 2.4f)), 0.85f, 0.85f, 0.8f);
    drawMeshLocal(sh, lib.box, Mat4::translate(pos + Vec3(0.9f, 0.1f, 0)) * Mat4::scale(Vec3(0.3f, 0.04f, 2.4f)), 0.85f, 0.85f, 0.8f);
    drawMeshLocal(sh, lib.box, Mat4::translate(pos + Vec3(0.0f, 0.1f, 0)) * Mat4::scale(Vec3(1.5f, 0.04f, 0.3f)), 0.85f, 0.85f, 0.8f);
}

void drawHelicopter(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float yawRad, float pitchRad, float rollRad,
                    float rotorAngle) {
    Mat4 base = Mat4::translate(pos) * Mat4::rotateY(yawRad) * Mat4::rotateX(pitchRad) * Mat4::rotateZ(rollRad);
    const float red[3] = { 0.78f, 0.12f, 0.10f };

    // Fuselage, canopy, tail boom with a fin, landing skids on struts.
    drawMeshLocal(sh, lib.sphere, base * Mat4::translate(Vec3(0, 1.5f, 0.2f)) * Mat4::scale(Vec3(0.95f, 0.9f, 2.0f)), red[0], red[1], red[2]);
    drawMeshLocal(sh, lib.sphere, base * Mat4::translate(Vec3(0, 1.72f, 1.2f)) * Mat4::scale(Vec3(0.78f, 0.62f, 0.95f)), 0.15f, 0.27f, 0.38f);
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, 1.75f, -2.4f)) * Mat4::scale(Vec3(0.28f, 0.3f, 3.0f)), red[0], red[1], red[2]);
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, 2.15f, -3.75f)) * Mat4::scale(Vec3(0.07f, 1.0f, 0.75f)), red[0] * 0.9f, red[1], red[2]);
    for (int s = -1; s <= 1; s += 2) {
        drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(s * 0.85f, 0.10f, 0.1f)) * Mat4::scale(Vec3(0.09f, 0.09f, 2.8f)), 0.12f, 0.12f, 0.13f);
        drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(s * 0.85f, 0.55f, 0.9f)) * Mat4::scale(Vec3(0.06f, 0.9f, 0.06f)), 0.15f, 0.15f, 0.16f);
        drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(s * 0.85f, 0.55f, -0.7f)) * Mat4::scale(Vec3(0.06f, 0.9f, 0.06f)), 0.15f, 0.15f, 0.16f);
    }

    // Main rotor: a mast, then two crossed blades turning about the vertical mast, R_y(angle),
    // plus a faint disc for the blur. The tail rotor is a child of the fin, turning about X.
    drawMeshLocal(sh, lib.cylinder, base * Mat4::translate(Vec3(0, 2.25f, 0.1f)) * Mat4::scale(Vec3(0.1f, 0.5f, 0.1f)), 0.15f, 0.15f, 0.16f);
    Mat4 rotorFrame = base * Mat4::translate(Vec3(0, 2.78f, 0.1f)) * Mat4::rotateY(rotorAngle);
    drawMeshLocal(sh, lib.box, rotorFrame * Mat4::scale(Vec3(6.2f, 0.05f, 0.28f)), 0.1f, 0.1f, 0.11f);
    drawMeshLocal(sh, lib.box, rotorFrame * Mat4::rotateY(radians(90.0f)) * Mat4::scale(Vec3(6.2f, 0.05f, 0.28f)), 0.1f, 0.1f, 0.11f);
    drawMeshLocal(sh, lib.cylinder, base * Mat4::translate(Vec3(0, 2.8f, 0.1f)) * Mat4::scale(Vec3(3.1f, 0.01f, 3.1f)), 0.7f, 0.7f, 0.72f, 0.12f);
    Mat4 tailFrame = base * Mat4::translate(Vec3(0.12f, 2.3f, -3.8f)) * Mat4::rotateX(rotorAngle * 1.6f);
    drawMeshLocal(sh, lib.box, tailFrame * Mat4::scale(Vec3(0.04f, 1.0f, 0.12f)), 0.1f, 0.1f, 0.11f);
    drawMeshLocal(sh, lib.box, tailFrame * Mat4::rotateX(radians(90.0f)) * Mat4::scale(Vec3(0.04f, 1.0f, 0.12f)), 0.1f, 0.1f, 0.11f);
}

void drawBoat(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float yawRad, float speed, float time) {
    // The boat rides the waves: a gentle heave, pitch and roll, each a closed-form function of time.
    float heave = 0.05f * std::sin(time * 1.7f), pitch = 0.04f * std::sin(time * 1.3f + 1.0f), roll = 0.05f * std::sin(time * 1.1f);
    Mat4 base = Mat4::translate(pos + Vec3(0, heave, 0)) * Mat4::rotateY(yawRad) * Mat4::rotateX(pitch) * Mat4::rotateZ(roll);

    // Hull (white above the waterline, blue below), a pointed bow made from a rotated box, cabin, mast.
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, 0.12f, 0)) * Mat4::scale(Vec3(1.6f, 0.6f, 3.4f)), 0.88f, 0.9f, 0.93f);
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, -0.12f, 0)) * Mat4::scale(Vec3(1.64f, 0.16f, 3.44f)), 0.10f, 0.20f, 0.50f);
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, 0.12f, 1.7f)) * Mat4::rotateY(radians(45.0f)) * Mat4::scale(Vec3(1.13f, 0.6f, 1.13f)), 0.88f, 0.9f, 0.93f);
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, 0.78f, -0.3f)) * Mat4::scale(Vec3(1.1f, 0.7f, 1.3f)), 0.92f, 0.93f, 0.95f);
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, 0.88f, -0.3f)) * Mat4::scale(Vec3(1.14f, 0.24f, 1.34f)), 0.07f, 0.12f, 0.22f);
    drawMeshLocal(sh, lib.cylinder, base * Mat4::translate(Vec3(0, 1.1f, -0.3f)) * Mat4::scale(Vec3(0.04f, 1.4f, 0.04f)), 0.3f, 0.3f, 0.32f);
    drawMeshLocal(sh, lib.sphere, base * Mat4::translate(Vec3(0, 2.52f, -0.3f)) * Mat4::scale(0.07f), 0.9f, 0.1f, 0.1f);
    // Radar bar: a child of the mast, turning about the vertical, R_y(w t).
    drawMeshLocal(sh, lib.box, base * Mat4::translate(Vec3(0, 2.4f, -0.3f)) * Mat4::rotateY(time * 2.5f) * Mat4::scale(Vec3(0.9f, 0.06f, 0.12f)), 0.2f, 0.2f, 0.22f);

    // A V-shaped wake behind a moving boat: two thin translucent strips, longer the faster it goes.
    float amount = std::min(1.0f, std::fabs(speed) / 7.0f);
    if (amount > 0.05f) {
        glDepthMask(GL_FALSE);
        for (int s = -1; s <= 1; s += 2) {
            drawMeshLocal(sh, lib.box,
                Mat4::translate(pos + Vec3(0, 0.05f, 0)) * Mat4::rotateY(yawRad) * Mat4::translate(Vec3(0, 0, -1.7f))
                    * Mat4::rotateY(s * 0.38f) * Mat4::translate(Vec3(0, 0, -2.5f * amount)) * Mat4::scale(Vec3(0.25f, 0.02f, 5.0f * amount)),
                0.95f, 0.97f, 1.0f, 0.4f * amount);
        }
        glDepthMask(GL_TRUE);
    }
}

} // namespace Scene
