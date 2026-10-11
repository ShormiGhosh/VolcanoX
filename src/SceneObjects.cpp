#include "SceneObjects.h"
#include "Eruption.h"
#include <cmath>

namespace Scene {

static float rnd01(int seed) { return 0.5f + 0.5f * hashNoise(seed); }

// ---------------------------------------------------------------------
// MeshLibrary: build every reusable primitive exactly once at startup.
// ---------------------------------------------------------------------
void MeshLibrary::build() {
    cylinder.upload(Geometry::cylinder(1.0f, 1.0f, 16), GL_TRIANGLES);
    cone.upload(Geometry::cone(1.0f, 0.0f, 1.0f, 16), GL_TRIANGLES);
    sphere.upload(Geometry::sphere(1.0f, 10, 16), GL_TRIANGLES);
    box.upload(Geometry::box(1.0f, 1.0f, 1.0f), GL_TRIANGLES);
    disc.upload(Geometry::disc(1.0f, 24), GL_TRIANGLES);

    for (int i = 0; i < 4; ++i) {
        rock[i].upload(Geometry::irregularRock(1.0f, i + 1, 0.4f), GL_TRIANGLES);
    }

    // Volcano cone (wide base, small open top = the crater mouth) and the lava pool disc:
    // the two meshes whose tessellation the + / - keys change.
    rebuildTessellation(tessLevel);

    ring.upload(Geometry::ring(0.86f, 64), GL_TRIANGLES);
    line.upload(Geometry::line(Vec3(0, 0, 0), Vec3(0, 1, 0)), GL_LINES);

    for (int i = 0; i < kLightningBoltVariants; ++i) {
        VertexList bolt = Geometry::lightningBolt(Vec3(0, 0, 0), Vec3(0, -1, 0), i * 31 + 5, 4, 0.22f);
        lightningBolt[i].upload(bolt, GL_LINES);
    }
}

int MeshLibrary::coneSegments(int level) {
    static const int kSeg[kTessLevels] = { 4, 6, 8, 12, 16, 28, 48 };
    return kSeg[level < 0 ? 0 : (level >= kTessLevels ? kTessLevels - 1 : level)];
}

int MeshLibrary::discSegments(int level) {
    static const int kSeg[kTessLevels] = { 3, 4, 6, 8, 12, 24, 48 };
    return kSeg[level < 0 ? 0 : (level >= kTessLevels ? kTessLevels - 1 : level)];
}

void MeshLibrary::rebuildTessellation(int level) {
    tessLevel = level < 0 ? 0 : (level >= kTessLevels ? kTessLevels - 1 : level);
    // The cone has an OPEN top (no lid); the bowl is the crater's inside wall, from the floor at
    // 0.93 of the height up to the rim (unit-cone coordinates: rim radius 0.14, floor radius 0.105).
    volcanoCone.upload(Geometry::cone(1.0f, 0.14f, 1.0f, coneSegments(tessLevel), false), GL_TRIANGLES);
    craterBowl.upload(Geometry::craterBowl(0.14f, 0.105f, 0.07f, coneSegments(tessLevel)), GL_TRIANGLES);
    poolDisc.upload(Geometry::disc(1.0f, discSegments(tessLevel)), GL_TRIANGLES);
}

// Small helper: send model/view/projection-independent draw (model set by caller
// via uModel; view/projection are set once per frame in main.cpp before the
// scene draw loop, so every function here only needs to touch uModel + uColor).
static void drawMesh(const Shader& sh, const Mesh& mesh, const Mat4& model,
                      float r, float g, float b, float a = 1.0f) {
    setModel(sh, model);
    sh.setVec4("uColor", r, g, b, a);
    mesh.draw();
}

// =======================================================================
// Ground plane
// =======================================================================
void drawGroundPlane(const Shader& sh, const MeshLibrary& lib) {
    Mat4 model = Mat4::translate(Vec3(0, 0, 0)) * Mat4::scale(Vec3(90.0f, 0.2f, 90.0f));
    drawMesh(sh, lib.box, model, 0.14f, 0.12f, 0.11f);
}

// =======================================================================
// Volcano + crater
// =======================================================================
void drawVolcano(const Shader& sh, const MeshLibrary& lib, const Vec3& base, float baseRadius, float height) {
    Mat4 model = Mat4::translate(base) * Mat4::scale(Vec3(baseRadius, height, baseRadius));
    drawMesh(sh, lib.volcanoCone, model, 0.20f, 0.16f, 0.15f);
    // Inside of the crater: from the floor (0.93 of the height, where the glow disc sits) to the rim.
    Mat4 bowl = Mat4::translate(base + Vec3(0.0f, 0.93f * height, 0.0f)) * Mat4::scale(Vec3(baseRadius, height, baseRadius));
    drawMesh(sh, lib.craterBowl, bowl, 0.12f, 0.09f, 0.08f);
}

void drawCraterGlowDisc(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float radius, float time) {
    // Purely a scale pulse (transformation, not an emission/lighting effect):
    // represents the roiling surface of magma visible at the vent mouth.
    float pulse = 1.0f + 0.08f * std::sin(3.0f * time);
    Mat4 model = Mat4::translate(craterPos) * Mat4::scale(Vec3(radius * pulse, 1.0f, radius * pulse));
    drawMesh(sh, lib.disc, model, 0.85f, 0.28f, 0.05f);
}

void drawCraterRimRocks(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float ringRadius, int count,
                        float baseRadius, float height) {
    // Non-instanced loop by design (see PRD risk mitigation): verify ring
    // placement this way first; converting to a single instanced draw call
    // is an optimization left for a later phase.
    // The ring is a child of the crater (same angle/offset transform) but is lifted onto the
    // cone's outer surface at that radius, so the rocks rest on the rim instead of being buried.
    Vec3 ringCentre(craterPos.x, terrainHeightAt(ringRadius, 0.0f, baseRadius, height) + 0.25f, craterPos.z);
    for (int i = 0; i < count; ++i) {
        float angleDeg = 360.0f * i / count;
        float sizeJitter = 0.55f + 0.35f * (0.5f + 0.5f * hashNoise(i * 13 + 3));
        Mat4 model = Mat4::translate(ringCentre)
                   * Mat4::rotateY(radians(angleDeg))
                   * Mat4::translate(Vec3(ringRadius, 0.0f, 0.0f))
                   * Mat4::scale(sizeJitter);
        drawMesh(sh, lib.rock[i % 4], model, 0.22f, 0.19f, 0.18f);
    }
}

// =======================================================================
// Lava pool + flow channel
// =======================================================================
void drawLavaPool(const Shader& sh, const MeshLibrary& lib, const Vec3& pos, float time, float arrivalTime, const Vec3& inflowDir) {
    // The basin fills once the flow that feeds it has arrived: radius = R_max * smoothstep(t_a - 3, t_a + 22, t).
    const float kMaxRadius = 7.5f;
    const float R = kMaxRadius * smoothstepf(arrivalTime - 3.0f, arrivalTime + 22.0f, time);
    if (R < 0.15f) return;

    // Flat layers, each lifted a little above the last (the ground top is y = 0.1).
    const float yCrust = pos.y + 0.04f, yCore = pos.y + 0.07f, yIn = pos.y + 0.10f, yPlate = pos.y + 0.13f;

    // 1) Cooled crust: the whole pool, dark red. This is the poolDisc mesh, so its tessellation still follows + / -.
    sh.setFloat("uEmit", 0.30f);
    drawMesh(sh, lib.poolDisc, Mat4::translate(Vec3(pos.x, yCrust, pos.z)) * Mat4::scale(Vec3(R, 1.0f, R)), 0.36f, 0.08f, 0.025f);

    // 2) Molten core: bright and flickering, with a slow breathing radius. Same mesh as the crust, so the
    //    Gouraud-vs-Phong highlight demo (a glossy, flat, low-polygon surface) still works on it.
    float flick = 0.5f + 0.5f * std::sin(2.1f * time) * std::sin(0.7f * time + 1.0f);
    float coreR = R * 0.82f * (1.0f + 0.03f * std::sin(1.7f * time));
    sh.setFloat("uEmit", 0.62f + 0.16f * flick);
    drawMesh(sh, lib.poolDisc, Mat4::translate(Vec3(pos.x, yCore, pos.z)) * Mat4::scale(Vec3(coreR, 1.0f, coreR)),
             0.95f, 0.26f + 0.10f * flick, 0.04f);

    // 3) Inflow: a hot patch on the side the flow arrives from, pulsing with the same travelling wave as the flow.
    float wave = 0.5f + 0.5f * std::sin(26.0f - time * 3.0f);
    Vec3 in = pos + inflowDir * (0.50f * R);
    float inR = R * (0.26f + 0.05f * wave);
    sh.setFloat("uEmit", 1.0f);
    drawMesh(sh, lib.poolDisc, Mat4::translate(Vec3(in.x, yIn, in.z)) * Mat4::scale(Vec3(inR, 1.0f, inR)), 1.0f, 0.70f + 0.15f * wave, 0.22f);

    // 4) Crust plates: dark slabs carried round by the circulation of the lake, faster in the middle than at
    //    the edge. Each one grows, breaks up and sinks, then re-forms: scale = 0.6 + 0.4 sin(0.35 t + phase).
    sh.setInt("uMat", MatRock);                     // a crust is matte, unlike the glossy melt
    sh.setFloat("uEmit", 0.06f);
    for (int k = 0; k < 10; ++k) {
        float rf = 0.12f + 0.68f * rnd01(k * 7 + 1);                          // radial position, as a fraction of R
        float ang = 6.2831853f * rnd01(k * 13 + 2) + 0.16f * (1.15f - rf) * time;
        float life = 0.6f + 0.4f * std::sin(0.35f * time + 6.0f * rnd01(k * 5 + 3));
        float s = R * (0.11f + 0.09f * rnd01(k * 3 + 4)) * life;
        Mat4 m = Mat4::translate(Vec3(pos.x + std::cos(ang) * rf * R, yPlate, pos.z + std::sin(ang) * rf * R))
               * Mat4::rotateY(2.0f * ang + k) * Mat4::scale(Vec3(s * 1.4f, 1.0f, s));
        drawMesh(sh, lib.disc, m, 0.12f, 0.05f, 0.04f);
    }
    sh.setInt("uMat", MatLava);
    sh.setFloat("uEmit", 0.6f);
}

void drawLavaFlowChannel(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, const Vec3& poolPos, float baseR, float height) {
    // Static channel geometry (a handful of boxes bridging crater -> pool).
    // The travelling brightness pulse described in the PRD is a shading
    // concept and is deferred; only the static geometry is drawn here.
    // The path hugs the cone surface (matching Geometry::cone(1, 0.14, 1) scaled
    // by the volcano's radius/height in main.cpp), then runs along the ground
    // to the pool. It is sampled densely and each piece is pitched to the
    // local slope and slightly overlapped, so it reads as one continuous flow.
    const float topFrac = 0.14f;
    const float rimR = baseR * topFrac + 0.1f;
    const float poolR = std::sqrt(poolPos.x * poolPos.x + poolPos.z * poolPos.z);
    const float az = std::atan2(poolPos.z, poolPos.x);
    const float groundY = 0.13f;

    auto surfaceY = [&](float r) {
        float y = height * (1.0f - r / baseR) / (1.0f - topFrac);
        return (y < 0.0f ? 0.0f : y);
    };

    const int segs = 70;
    Vec3 pts[segs + 1];
    for (int i = 0; i <= segs; ++i) {
        float u = float(i) / segs;
        float r = rimR + (poolR - rimR) * u;
        float y = surfaceY(r) + 0.15f;
        if (y < groundY) y = groundY;
        float a = az + 0.05f * std::sin(u * 9.0f); // gentle meander
        pts[i] = Vec3(r * std::cos(a), y, r * std::sin(a));
    }

    for (int i = 0; i < segs; ++i) {
        Vec3 diff = pts[i + 1] - pts[i];
        Vec3 mid = (pts[i] + pts[i + 1]) * 0.5f;
        float segLen = length3(diff);
        float horiz = std::sqrt(diff.x * diff.x + diff.z * diff.z);
        float yaw = std::atan2(diff.x, diff.z);
        float pitch = std::atan2(diff.y, horiz);
        float u = float(i) / segs;
        float width = 2.0f - 0.7f * u;
        float heat = 0.10f * std::sin(u * 14.0f - 0.0f);

        Mat4 model = Mat4::translate(mid) * Mat4::rotateY(yaw) * Mat4::rotateX(-pitch)
                   * Mat4::scale(Vec3(width, 0.28f, segLen * 1.25f));
        drawMesh(sh, lib.box, model, 0.75f + heat, 0.24f + heat, 0.03f);
    }
}

static const int kFlowSegs = 110;

// The flow's centre line over the cone and out across the ground, shared by the drawing code
// and the photon emitters so the two always agree.
static void flowPoints(const LavaFlowStyle& st, float baseR, float height, Vec3* pts) {
    const float topFrac = 0.14f;
    const float rimR = baseR * topFrac + 0.1f;
    const float az0 = radians(st.azimuthDeg);
    const float groundY = 0.13f;
    for (int i = 0; i <= kFlowSegs; ++i) {
        float u = float(i) / kFlowSegs;
        float r = rimR + (st.endRadius - rimR) * u;
        float surface = std::max(0.0f, height * (1.0f - r / baseR) / (1.0f - topFrac));
        float y = std::max(groundY, surface + 0.15f);
        float a = az0 + st.meanderAmp * std::sin(u * st.meanderFreq + st.meanderPhase) * (0.3f + u);
        pts[i] = Vec3(r * std::cos(a), y, r * std::sin(a));
    }
}

void lavaFlowSamples(const LavaFlowStyle& st, float baseR, float height, float time, int count,
                     std::vector<FlowSample>& out) {
    const float topFrac = 0.14f;
    const float rimR = baseR * topFrac + 0.1f;
    Vec3 pts[kFlowSegs + 1];
    flowPoints(st, baseR, height, pts);
    float grow = std::min(1.0f, 0.25f + st.growSpeed * time);
    int last = int(kFlowSegs * grow);
    if (last < 1) return;
    float segLen = (st.endRadius - rimR) / kFlowSegs;
    float slope = height / (baseR * (1.0f - topFrac));          // |dy/dr| of the cone surface
    for (int k = 0; k < count; ++k) {
        int i = std::min(last, int((k + 0.5f) / count * last));
        float u = float(i) / kFlowSegs;
        float pulse = 0.5f + 0.5f * std::sin(u * 26.0f - time * 3.0f);
        float heat = 0.22f * pulse + ((i > last - 8) ? 0.25f : 0.0f);
        float r = std::sqrt(pts[i].x * pts[i].x + pts[i].z * pts[i].z);
        float width = st.widthStart + (st.widthEnd - st.widthStart) * u;
        if (r < baseR) width = std::min(width, 0.28f * r + 0.3f);
        FlowSample fs;
        fs.pos = pts[i];
        if (r < baseR && r > 1e-3f) fs.normal = normalize3(Vec3(slope * pts[i].x / r, 1.0f, slope * pts[i].z / r));
        else fs.normal = Vec3(0, 1, 0);
        fs.width = width;
        fs.heat = heat;
        fs.length = segLen * float(last) / count;
        out.push_back(fs);
    }
}

Vec3 lavaFlowEnd(const LavaFlowStyle& st, float baseR, float height) {
    Vec3 pts[kFlowSegs + 1];
    flowPoints(st, baseR, height, pts);
    return pts[kFlowSegs];
}

void drawLavaFlow(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float baseR, float height,
                  const LavaFlowStyle& st, float time) {
    // The flow keeps advancing, then holds at its full length.
    float grow = std::min(1.0f, 0.25f + st.growSpeed * time);

    const int segs = kFlowSegs;
    Vec3 pts[kFlowSegs + 1];
    flowPoints(st, baseR, height, pts);

    int last = int(segs * grow);
    for (int i = 0; i < last; ++i) {
        Vec3 diff = pts[i + 1] - pts[i];
        Vec3 mid = (pts[i] + pts[i + 1]) * 0.5f;
        float segLen = length3(diff);
        float horiz = std::sqrt(diff.x * diff.x + diff.z * diff.z);
        float yaw = std::atan2(diff.x, diff.z);
        float pitch = std::atan2(diff.y, horiz);
        float u = float(i) / segs;
        float width = st.widthStart + (st.widthEnd - st.widthStart) * u;
        // On the cone a wide flat slab would stick out past the curved surface,
        // so the flow stays narrow there and only spreads once it reaches the ground.
        float rHere = std::sqrt(mid.x * mid.x + mid.z * mid.z);
        if (rHere < baseR) width = std::min(width, 0.28f * rHere + 0.3f);
        else if (rHere < baseR * 1.6f) width = std::min(width, 0.28f * baseR + 0.3f + (rHere - baseR) * 0.6f);
        // Bright pulses travel down the flow; the front end is hottest.
        float pulse = 0.5f + 0.5f * std::sin(u * 26.0f - time * 3.0f);
        float front = (i > last - 8) ? 0.25f : 0.0f;
        // Bright pulses travel down the flow, clearly visible now (amplitude 0.22; it was 0.10).
        float heat = 0.22f * pulse + front;

        Mat4 model = Mat4::translate(mid) * Mat4::rotateY(yaw) * Mat4::rotateX(-pitch)
                   * Mat4::scale(Vec3(width, 0.24f, segLen * 1.3f));
        drawMesh(sh, lib.box, model, st.r + heat, st.g + heat * 0.8f, st.b + heat * 0.1f);
    }

    // Irregular bulging front made of a few lumpy blobs, not a neat disc.
    if (st.lobeEnd && last > 0) {
        const Vec3& tip = pts[last];
        Vec3 d = pts[last] - pts[std::max(0, last - 1)];
        float yaw = std::atan2(d.x, d.z);
        float w = st.widthEnd;
        for (int k = 0; k < 4; ++k) {
            float side = (k - 1.5f) * 0.32f * w;
            float fwd = w * (0.15f + 0.35f * (0.5f + 0.5f * hashNoise(k * 7 + int(st.azimuthDeg))));
            float sz = w * (0.42f + 0.25f * (0.5f + 0.5f * hashNoise(k * 5 + 2)));
            sz *= 1.0f + 0.07f * std::sin(1.6f * time + 1.9f * k);   // the front breathes: lava keeps moving at its tip
            Mat4 model = Mat4::translate(tip + Vec3(0.0f, 0.03f, 0.0f))
                       * Mat4::rotateY(yaw) * Mat4::translate(Vec3(side, 0.0f, fwd))
                       * Mat4::rotateY(radians(60.0f * k)) * Mat4::scale(Vec3(sz, 0.14f, sz * 0.9f));
            drawMesh(sh, lib.sphere, model, st.r + 0.2f, st.g + 0.14f, st.b + 0.02f);
        }
    }
}

// =======================================================================
// Particle pools: lava droplets, rock chunks, ash puffs.
// Each pool is a fixed-size array of slots; every field is a closed-form
// function of `age` (time since spawn) -- no stored keyframes, nothing
// baked. Inactive slots are simply skipped when drawing.
// =======================================================================
struct BallisticSlot {
    bool active = false;
    bool landed = false;
    float spawnTime = 0.0f;
    float landTime = 0.0f;
    Vec3 origin;
    Vec3 velocity;
    Vec3 landPos;
    int seed = 0;
};

static const Vec3 kGravity(0.0f, -9.8f, 0.0f);

static Vec3 ballisticPos(const BallisticSlot& s, float age) {
    return s.origin + s.velocity * age + kGravity * (0.5f * age * age);
}

// Height of the terrain (cone slope, then flat ground) under a point, so
// ejecta land on the volcano's flanks or the plain instead of flying through them.
float terrainHeightAt(float x, float z, float baseR, float height) {
    float r = std::sqrt(x * x + z * z);
    const float topFrac = 0.14f;
    if (r >= baseR) return 0.1f;
    // Inside the crater mouth the surface is the crater floor, not the cone's slope extended
    // above its flat top (which put ejecta "landing" in mid-air above the vent).
    if (r < topFrac * baseR) return 0.93f * height;
    float y = height * (1.0f - r / baseR) / (1.0f - topFrac);
    return std::max(0.1f, y);
}

// Launches a slot from a random point inside the crater mouth.
static void launchBallistic(BallisticSlot& s, const Vec3& craterPos, float time, int idx,
                            float speedMin, float speedMax, float elevMinDeg, float elevMaxDeg) {
    float angle = radians(360.0f * rnd01(idx * 7 + 1));
    float speed = speedMin + (speedMax - speedMin) * rnd01(idx * 11 + 2);
    float elevation = radians(elevMinDeg + (elevMaxDeg - elevMinDeg) * rnd01(idx * 17 + 3));
    float mouth = 1.8f * rnd01(idx * 5 + 4);
    float mouthAng = radians(360.0f * rnd01(idx * 13 + 5));
    s.origin = craterPos + Vec3(mouth * std::cos(mouthAng), 0.0f, mouth * std::sin(mouthAng));
    s.velocity = Vec3(speed * std::cos(elevation) * std::cos(angle),
                      speed * std::sin(elevation),
                      speed * std::cos(elevation) * std::sin(angle));
    s.spawnTime = time;
    s.seed = idx;
    s.active = true;
    s.landed = false;
}

static int s_activeDroplets = 0;   // droplets drawn this frame: the emitters for the lava-light tracing
static std::vector<Collider> s_landedRocks;      // rebuilt by updateAndDrawRockChunks every frame
void landedRockColliders(std::vector<Collider>& out) { out.insert(out.end(), s_landedRocks.begin(), s_landedRocks.end()); }
static const int kDropletPool = 24;   // was 8; bursts spawn droplets several times faster
static Vec3 s_dropPos[kDropletPool];
static float s_dropBright[kDropletPool];
int lavaDropletEmitters(Vec3* pos, float* brightness, int maxCount) {
    int n = std::min(std::min(s_activeDroplets, kDropletPool), maxCount);
    for (int i = 0; i < n; ++i) { pos[i] = s_dropPos[i]; brightness[i] = s_dropBright[i]; }
    return n;
}

void updateAndDrawLavaDroplets(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float /*baseR*/, float /*height*/, float time) {
    static const int kCount = kDropletPool;
    static BallisticSlot slots[kCount];
    static float lastSpawn = -1.0f;
    static int nextSlot = 0, counter = 0;

    // A lava fountain: one droplet roughly every 0.2 s, lobbed a short distance. During an eruption
    // burst it spawns up to 4x faster and throws harder (Eruption::intensity).
    const float burst = Eruption::intensity(time);
    if (time - lastSpawn >= 0.2f / (1.0f + 3.0f * burst)) {
        lastSpawn = time;
        const float vs = 1.0f + 0.8f * burst;
        launchBallistic(slots[nextSlot], craterPos, time, ++counter, 5.5f * vs, 8.0f * vs, 55.0f, 75.0f);
        slots[nextSlot].origin = craterPos;
        nextSlot = (nextSlot + 1) % kCount;
    }

    int drawn = 0;
    for (int i = 0; i < kCount; ++i) {
        BallisticSlot& s = slots[i];
        if (!s.active) continue;
        float age = time - s.spawnTime;
        Vec3 p = ballisticPos(s, age);
        if (p.y <= craterPos.y - 6.0f || age > 3.5f) { s.active = false; continue; }
        ++drawn;

        float shrink = std::max(0.25f, 1.0f - 0.18f * age);
        float coolT = std::min(1.0f, age / 1.5f);
        float r = 1.0f;
        float g = 0.85f - 0.55f * coolT;
        float b = 0.15f - 0.10f * coolT;

        if (drawn <= kDropletPool) { s_dropPos[drawn - 1] = p; s_dropBright[drawn - 1] = shrink * (1.0f - 0.5f * coolT); }
        Mat4 model = Mat4::translate(p) * Mat4::scale(0.75f * shrink);
        drawMesh(sh, lib.sphere, model, r, g, b);
    }
    s_activeDroplets = drawn;
}

void updateAndDrawRockChunks(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float baseR, float height, float time) {
    static const int kCount = 36;
    static BallisticSlot slots[kCount];
    static float lastSpawn = -1.0f;
    static int nextSlot = 0, counter = 0;

    // Volcanic rocks: lobbed out in a modest arc, they land and rest on the ground before fading.
    // More and faster during an eruption burst.
    const float burst = Eruption::intensity(time);
    if (time - lastSpawn >= 0.3f / (1.0f + 3.0f * burst)) {
        lastSpawn = time;
        const float vs = 1.0f + 0.6f * burst;
        launchBallistic(slots[nextSlot], craterPos, time, 1000 + ++counter, 6.0f * vs, 13.0f * vs, 45.0f, 70.0f);
        nextSlot = (nextSlot + 1) % kCount;
    }

    s_landedRocks.clear();
    for (int i = 0; i < kCount; ++i) {
        BallisticSlot& s = slots[i];
        if (!s.active) continue;
        float age = time - s.spawnTime;

        Vec3 p;
        float spin;
        if (!s.landed) {
            p = ballisticPos(s, age);
            float ground = terrainHeightAt(p.x, p.z, baseR, height);
            if (age > 0.3f && p.y <= ground + 0.3f) {
                s.landed = true;
                s.landTime = time;
                s.landPos = Vec3(p.x, ground + 0.3f, p.z);
            }
            spin = age * (2.5f + 1.5f * rnd01(s.seed * 3 + 1));
        } else {
            if (time - s.landTime > 4.0f) { s.active = false; continue; }
            p = s.landPos;
            spin = (s.landTime - s.spawnTime) * (2.5f + 1.5f * rnd01(s.seed * 3 + 1));
        }

        Vec3 axis = normalize3(Vec3(hashNoise(s.seed * 5 + 1), hashNoise(s.seed * 5 + 2) + 0.2f, hashNoise(s.seed * 5 + 3)));
        float size = 0.55f + 0.5f * rnd01(s.seed * 3 + 2);

        // The larger rocks are volcanic BOMBS: bigger, glowing white-hot from the vent and cooling to
        // rock over a few seconds (heat = e^{-age/2.2}), and in flight they drag a glowing trail made of
        // the bomb's own earlier positions (the ballistic formula evaluated at age - k*dt).
        const bool bomb = size > 0.86f;
        float heat = 0.0f;
        if (bomb) {
            size *= 1.35f;
            heat = std::exp(-(time - s.spawnTime) / 2.2f);
            if (!s.landed) {
                glDepthMask(GL_FALSE);
                sh.setFloat("uEmit", 1.0f);
                for (int k = 1; k <= 12; ++k) {
                    float a = age - 0.05f * k;
                    if (a < 0.0f) break;
                    float f = 1.0f - k / 13.0f;
                    drawMesh(sh, lib.sphere, Mat4::translate(ballisticPos(s, a)) * Mat4::scale(0.38f * size * f + 0.05f),
                             1.0f, 0.38f + 0.30f * f, 0.06f, 0.7f * f * (0.4f + 0.6f * heat));
                }
                glDepthMask(GL_TRUE);
            }
            sh.setFloat("uEmit", 0.85f * heat);
        }
        if (s.landed) {   // a rock lying on the ground is an obstacle (the rover bumps into it)
            Collider c; c.x = p.x; c.z = p.z; c.r = 0.8f * size; c.kind = KindRock;
            s_landedRocks.push_back(c);
        }
        Mat4 model = Mat4::translate(p) * Mat4::rotate(axis, spin) * Mat4::scale(size);
        drawMesh(sh, lib.rock[s.seed % 4], model,
                 0.24f + 0.76f * heat, 0.20f + 0.25f * heat, 0.18f - 0.10f * heat);
        if (bomb) sh.setFloat("uEmit", 0.0f);
    }
}

void updateAndDrawAshPuffs(const Shader& sh, const MeshLibrary& lib, const Vec3& craterPos, float time) {
    static const int kCount = 6;
    static float spawnTimes[kCount];
    static bool active[kCount];
    static bool initialized = false;
    static float lastSpawnCheck = -1.0f;
    static int nextSlot = 0;

    if (!initialized) {
        for (int i = 0; i < kCount; ++i) active[i] = false;
        initialized = true;
    }

    const float spawnInterval = 0.5f;
    if (time - lastSpawnCheck >= spawnInterval) {
        lastSpawnCheck = time;
        spawnTimes[nextSlot] = time;
        active[nextSlot] = true;
        nextSlot = (nextSlot + 1) % kCount;
    }

    const float life = 3.0f;
    for (int i = 0; i < kCount; ++i) {
        if (!active[i]) continue;
        float age = time - spawnTimes[i];
        if (age > life) { active[i] = false; continue; }

        Vec3 driftX(hashNoise(i * 41 + 9) * 0.6f, 0.0f, hashNoise(i * 43 + 10) * 0.6f);
        Vec3 p = craterPos + Vec3(0, 1.2f, 0) * age + driftX * age;
        float grow = 0.6f + 0.5f * age;
        float alpha = 1.0f - age / life;

        Mat4 model = Mat4::translate(p) * Mat4::scale(grow);
        drawMesh(sh, lib.sphere, model, 0.55f, 0.55f, 0.56f, alpha * 0.85f);
    }

}

} // namespace Scene
