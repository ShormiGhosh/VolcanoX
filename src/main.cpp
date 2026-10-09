// VolcanoX -- objects stage + Phase 1b (day/night, drone cameras, pilot drone).
//
// This build still has no lighting, materials, or shading model: every object
// is drawn with a single flat uniform color so the geometry and placement of
// each object in the scene is clear on its own. Phase 1b adds, on top of that:
//   * an automatic day/night clock (DayNight.h)             key N
//   * views through the drones' cameras                     keys C, V
//   * a keyboard-flown pilot drone (Controller.h)           Tab + W/A/S/D/Space/Shift
// See PRD_MoltenGround.md and IMPLEMENTATION_GUIDE.md (§10) for the design.
//
// Debug options (for screenshots / automated checks), all optional:
//   --start <sec>   begin the simulation at this time
//   --shot <file>   render --frames frames, write a PPM screenshot, then exit
//   --frames <n>    frames to render before the screenshot (default 5, fixed 1/60 s steps)
//   --cam <0-3>     camera: 0 orbit, 1 drone 1, 2 drone 2, 3 pilot drone
//   --lock <0-2>    time of day: 0 auto, 1 day, 2 night
//   --orbit <yaw> <pitch> <dist>   orbit-camera angles/distance
//   --forward       drone cameras aim forward (gimbal tilt) instead of at the crater
//   --shade <0-2>   shading: 0 Flat, 1 Gouraud, 2 Phong (default)
//   --lights <0-2>  0 both, 1 crater only, 2 sky light only
//   --terms <0-3>   0 ambient, 1 +diffuse, 2 specular only, 3 full
//   --tess <0-6>    tessellation level (cone/pool segments)
//   --blinn         Blinn-Phong instead of Phong specular
//   --bench <sec>   run in real time with vsync off for this long, then print average FPS and draw calls
//   --rt <0-2>      ray tracing: 0 off, 1 shadows, 2 shadows + reflections
//   --photons       start with inverse tracing of the lava glow on
//   --map           show the light-map minimap
//   --gain <g>      photon light-map gain
//   --fire <t>      fire an extra big burst (like the E key) at simulated time t; repeatable
//   --noshake       start with the camera shake off
//   --selftest      check the C++ ray intersections against hand-computed values, then exit
//   --notex         start with textures off
//   --nohud         start with the HUD hidden
//   --pilotpos <x> <y> <z>   teleport the pilot drone (the orbit camera follows it when piloting)
//   --pilot         take control of the pilot drone at startup
//   --control <0-4> take control at startup: 0 none, 1 drone, 2 rover, 3 helicopter, 4 boat
//   --ctlpos <x y z> teleport the controlled object (the orbit camera follows it)
//   --autopilot     with --pilot: hold forward + left-turn inputs
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "MathUtils.h"
#include "Shader.h"
#include "SceneObjects.h"
#include "Camera.h"
#include "DayNight.h"
#include "Controller.h"
#include "Texture.h"
#include "RayScene.h"
#include "PhotonMap.h"
#include "Eruption.h"

namespace {

// ---------------------------------------------------------------- world layout
// World space, volcano centered at the origin.
const Vec3 kVolcanoBase(0.0f, 0.0f, 0.0f);
const float kVolcanoBaseRadius = 19.0f;
const float kVolcanoHeight = 27.0f;
const Vec3 kCraterPos(0.0f, kVolcanoHeight * 0.93f, 0.0f);
// The crater point light hovers above the vent (the glow of the lava fountain). Placed exactly at
// the vent it would sit on the cone's tangent plane and light none of the steep slopes.
const Vec3 kCraterLightPos = kCraterPos + Vec3(0.0f, 6.0f, 0.0f);
const Vec3 kPoolPos(-44.0f, 0.1f, 6.0f); // a lava pool on the open plain, clear of every flow, tree and the hut
const float kStormBaseY = 62.0f; // storm clouds (and thunder) live in the sky, not on the volcano
const Vec3 kWindDir = normalize3(Vec3(1.0f, 0.0f, 0.35f)); // shared by ash cloud and storm clouds
const float kTreeLeanDeg = -10.0f; // burnt trees lean away from the wind
// Storms strike only after dark: no lightning bolts and no sky flash once the daylight level reaches this.
const float kNoLightningDaylight = 0.25f;

struct TreePlacement { float angleDeg; float radius; float scale; int seed; };
const TreePlacement kTrees[] = {
    { 111.0f, 30.0f, 1.0f, 1 },      // between the rivulets at 98 and 124 degrees, not in them
    { 142.0f, 36.0f, 0.85f, 2 },
    { 160.0f, 29.0f, 1.15f, 3 },
    { 230.0f, 34.0f, 0.9f, 4 },
    { 260.0f, 40.0f, 1.05f, 5 },
    { 300.0f, 31.0f, 0.8f, 6 },
};
const Vec3 kHutPos(40.0f * std::cos(radians(205.0f)), 0.0f, 40.0f * std::sin(radians(205.0f)));
const Vec3 kWindmillPos(47.0f * std::cos(radians(212.0f)), 0.0f, 47.0f * std::sin(radians(212.0f)));

const Vec3 kHelipadPos(5.9f, 0.0f, -33.5f);   // the helicopter starts here

// Object ids, used for "the object whose camera we are looking through is not drawn".
const int kOrbitDrones = 2;      // autonomous drones, ids 0 and 1
const int kPilotIndex = 2;       // controllable object 1: the pilot drone
const int kRoverIndex = 3;       // controllable object 2: the rover
const int kHeliIndex = 4;        // controllable object 3: the helicopter
const int kBoatIndex = 5;        // controllable object 4: the boat
const float kGimbalPitch = 35.0f; // degrees the drone camera tilts down in "forward" aim

// ---------------------------------------------------------------- app state
enum CamMode { CamOrbit = 0, CamDrone1 = 1, CamDrone2 = 2, CamPilot = 3, CamRover = 4, CamHeli = 5, CamBoat = 6, CamCount = 7 };
// Which object the keyboard flies/drives (Tab cycles): the four controllable objects.
enum Control { CtlNone = 0, CtlDrone = 1, CtlRover = 2, CtlHeli = 3, CtlBoat = 4, CtlCount = 5 };

struct App {
    Camera orbit;
    DayNight sky;
    PilotDrone pilot;
    Rover rover;
    Helicopter heli;
    Boat boat;
    int ctl = CtlNone;         // the controlled object
    int camMode = CamOrbit;
    bool aimForward = false;   // false = crater lock, true = forward with gimbal tilt
    bool paused = false;
    bool autopilot = false;    // debug: hold fixed pilot inputs
    // Phase 2 render settings
    int shadingMode = 2;       // 0 Flat, 1 Gouraud, 2 Phong
    int lightMode = 0;         // 0 both, 1 crater only, 2 sky light only
    int termMode = 3;          // 0 ambient, 1 +diffuse, 2 specular only, 3 full
    bool blinn = false;        // Phong (R.V) vs Blinn-Phong (N.H)
    int tessDelta = 0;         // pending +/- tessellation key presses
    bool useTex = true;        // procedural textures on/off (key T)
    bool showHud = true;       // HUD on/off (key H)
    int rtMode = 0;            // ray tracing: 0 off, 1 shadows, 2 shadows + reflections (key R)
    bool photonsOn = false;    // inverse (light-ray) tracing of the lava glow (key F)
    bool showMap = false;      // light-map minimap (key M)
    bool shake = true;         // camera shake at each eruption burst (key J)
    float photonGain = 3.0f;   // how strongly the light map adds to the diffuse light
    float fps = 60.0f;         // smoothed frames per second
    int drawCalls = 0;         // draw calls in the last frame
    float simTime = 0.0f;      // advances only while not paused
    float savedDistance = 68.0f;
    int width = 1280, height = 720;
};
App g;

// ---------------------------------------------------------------- drone kinematics
struct DroneKin { Vec3 pos; float yaw; float drawTime; };

// The two autonomous drones circle the volcano at different radii, heights,
// speeds and in opposite directions. Heading is the orbit tangent.
DroneKin orbitDrone(int i, float t) {
    float radius = 30.0f + 8.0f * (i % 4);
    float speed = (0.10f + 0.03f * (i % 3)) * (i % 2 == 0 ? 1.0f : -1.0f);
    float ang = radians(360.0f * i / kOrbitDrones) + t * speed;
    float height = 16.0f + 4.0f * (i % 5) + 0.8f * std::sin(t * 0.5f + i);
    float sgn = speed > 0.0f ? 1.0f : -1.0f;
    Vec3 vel(-std::sin(ang) * sgn, 0.0f, std::cos(ang) * sgn);
    DroneKin k;
    k.pos = Vec3(radius * std::cos(ang), height, radius * std::sin(ang));
    k.yaw = std::atan2(vel.x, vel.z);
    k.drawTime = t + i * 0.7f;
    return k;
}

DroneKin pilotDrone(float t) {
    DroneKin k;
    k.pos = g.pilot.pos;
    k.yaw = g.pilot.yaw;
    k.drawTime = t + 2.1f;
    return k;
}

DroneKin droneByIndex(int idx, float t) { return idx == kPilotIndex ? pilotDrone(t) : orbitDrone(idx, t); }

// ---------------------------------------------------------------- lava flows
// The five lava flows, defined once: drawn by renderScene and sampled for photon emitters.
const std::vector<Scene::LavaFlowStyle>& flowStyles() {
    static std::vector<Scene::LavaFlowStyle> styles;
    if (styles.empty()) {
        // 1) Long winding river that crawls far across the plain and ends in an irregular flow front.
        styles.push_back({ 20.0f, 120.0f, 2.2f, 1.6f, 0.16f, 7.0f, 0.0f, 0.90f, 0.30f, 0.04f, true, 0.030f });
        // 2) Braided rivulets: thin streams that split and taper to points partway down.
        for (int i = 0; i < 3; ++i)
            styles.push_back({ 72.0f + 26.0f * i, 48.0f + 16.0f * hashNoise(i * 3 + 1), 0.5f, 0.05f, 0.10f, 11.0f + i, float(i), 0.85f, 0.30f, 0.04f, false, 0.05f });
        // 3) Broad, slow sheet flow: wide, dark-crusted, with a bright lobed front.
        styles.push_back({ 322.0f, 62.0f, 3.0f, 5.5f, 0.05f, 4.0f, 2.0f, 0.62f, 0.14f, 0.03f, true, 0.020f });
    }
    return styles;
}

// ---------------------------------------------------------------- per-frame view
struct Frame {
    Mat4 view, proj;
    Vec3 eye;
    float t = 0.0f;
    SkyState sky;
    float flash = 0.0f;
    int hiddenDrone = -1;   // the object whose camera we are looking through is not drawn
};

const char* camName(int mode) {
    switch (mode) {
        case CamDrone1: return "Drone 1";
        case CamDrone2: return "Drone 2";
        case CamPilot:  return "Pilot drone";
        case CamRover:  return "Rover";
        case CamHeli:   return "Helicopter";
        case CamBoat:   return "Boat";
        default:        return "Free orbit";
    }
}

Frame makeFrame(float aspect) {
    Frame f;
    f.t = g.simTime;
    f.sky = g.sky.at(g.simTime);
    // No thunderstorm by day: the strike schedule only shows through at night and in the twilight.
    f.flash = (f.sky.daylight < kNoLightningDaylight) ? Scene::lightningFlash(g.simTime) : 0.0f;

    if (g.camMode == CamOrbit) {
        f.eye = g.orbit.eyePosition();
        f.view = g.orbit.viewMatrix();
        f.proj = Mat4::perspective(radians(55.0f), aspect, 0.5f, 2500.0f);
        f.hiddenDrone = -1;
    } else if (g.camMode >= CamRover) {
        // Onboard cameras of the ground and water vehicles and the helicopter: from the cab, the nose
        // or the bow, looking ahead and slightly down.
        float yaw, down, eyeUp, eyeFwd;
        Vec3 base;
        int idx;
        if (g.camMode == CamRover)      { idx = kRoverIndex; base = g.rover.pos; yaw = g.rover.yaw; eyeUp = 1.5f;  eyeFwd = 0.9f; down = 6.0f; }
        else if (g.camMode == CamHeli)  { idx = kHeliIndex;  base = g.heli.pos;  yaw = g.heli.yaw;  eyeUp = 1.6f;  eyeFwd = 2.1f; down = 14.0f; }
        else                            { idx = kBoatIndex;  base = g.boat.pos;  yaw = g.boat.yaw;  eyeUp = 1.25f; eyeFwd = 1.4f; down = 4.0f; }
        Vec3 fwd(std::sin(yaw), 0.0f, std::cos(yaw));
        f.eye = base + Vec3(0.0f, eyeUp, 0.0f) + fwd * eyeFwd;
        float p = radians(down);
        Vec3 target = f.eye + Vec3(fwd.x * std::cos(p), -std::sin(p), fwd.z * std::cos(p)) * 10.0f;
        f.view = Mat4::lookAt(f.eye, target, Vec3(0, 1, 0));
        f.proj = Mat4::perspective(radians(70.0f), aspect, 0.3f, 2500.0f);
        f.hiddenDrone = idx;
    } else {
        int idx = (g.camMode == CamDrone1) ? 0 : (g.camMode == CamDrone2) ? 1 : kPilotIndex;
        DroneKin k = droneByIndex(idx, g.simTime);
        float bob = 0.12f * std::sin(k.drawTime * 1.2f);
        Vec3 fwd(std::sin(k.yaw), 0.0f, std::cos(k.yaw));
        // Camera sits at the gimbal: just below and ahead of the body.
        f.eye = k.pos + Vec3(0.0f, bob - 0.2f, 0.0f) + fwd * 0.3f;
        Vec3 target;
        if (g.aimForward) {
            float p = radians(kGimbalPitch);
            target = f.eye + Vec3(fwd.x * std::cos(p), -std::sin(p), fwd.z * std::cos(p));
        } else {
            target = kCraterPos;
        }
        f.view = Mat4::lookAt(f.eye, target, Vec3(0, 1, 0));
        f.proj = Mat4::perspective(radians(70.0f), aspect, 0.3f, 2500.0f);
        f.hiddenDrone = idx;
    }
    return f;
}

// ---------------------------------------------------------------- rendering
// The ray-tracing proxy scene (IMPLEMENTATION_GUIDE.md §11.2), rebuilt each frame from the same
// placement variables the drawing code uses, so the shadows always match what is drawn.
RayScene buildProxyScene(float t) {
    RayScene rs;
    rs.volcanoRadius = kVolcanoBaseRadius;

    RayCone volcano;                       // cones[0]: shadows the sun and moon, not the crater light
    volcano.base = kVolcanoBase;
    volcano.r0 = kVolcanoBaseRadius; volcano.r1 = 0.14f * kVolcanoBaseRadius; volcano.h = kVolcanoHeight;
    volcano.shadowsCrater = false;
    volcano.color = Vec3(0.20f, 0.16f, 0.15f);
    rs.cones.push_back(volcano);

    for (const auto& tp : kTrees) {        // burnt trees: a sharp cone, leaning like the drawn trunk
        RayCone c;
        c.base = Vec3(tp.radius * std::cos(radians(tp.angleDeg)), 0.0f, tp.radius * std::sin(radians(tp.angleDeg)));
        c.r0 = 0.22f * tp.scale; c.r1 = 0.0f; c.h = 3.2f * tp.scale;
        c.tilt = radians(kTreeLeanDeg);
        c.color = Vec3(0.08f, 0.06f, 0.05f);
        rs.cones.push_back(c);
    }
    RayCone tower;                         // windmill tower: a cylinder
    tower.base = kWindmillPos; tower.r0 = tower.r1 = 0.18f; tower.h = 4.5f;
    tower.color = Vec3(0.28f, 0.22f, 0.16f);
    rs.cones.push_back(tower);

    RayBox hut;                            // the hut's walls, rotated like the drawn hut
    hut.present = true;
    hut.c = kHutPos + Vec3(0.0f, 1.0f, 0.0f);
    hut.half = Vec3(1.6f, 1.0f, 1.3f);
    hut.yaw = radians(205.0f + 90.0f);
    hut.color = Vec3(0.32f, 0.24f, 0.16f);
    rs.boxes.push_back(hut);

    RayBox rover;                          // the rover and the boat are boxes that move with their objects
    rover.present = true;
    rover.c = g.rover.pos + Vec3(0.0f, 0.95f, 0.0f); rover.half = Vec3(0.95f, 0.7f, 1.6f);
    rover.yaw = g.rover.yaw; rover.color = Vec3(0.85f, 0.65f, 0.08f);
    rs.boxes.push_back(rover);
    RayBox boat;
    boat.present = true;
    boat.c = g.boat.pos + Vec3(0.0f, 0.45f, 0.0f); boat.half = Vec3(0.85f, 0.5f, 1.7f);
    boat.yaw = g.boat.yaw; boat.color = Vec3(0.88f, 0.9f, 0.93f);
    rs.boxes.push_back(boat);

    for (int i = 0; i < kOrbitDrones; ++i) {   // drones: spheres at their current positions
        DroneKin k = orbitDrone(i, t);
        rs.spheres.push_back({ k.pos + Vec3(0.0f, 0.12f * std::sin(k.drawTime * 1.2f), 0.0f), 1.0f });
    }
    DroneKin pk = pilotDrone(t);
    rs.spheres.push_back({ pk.pos + Vec3(0.0f, 0.12f * std::sin(pk.drawTime * 1.2f), 0.0f), 1.0f });
    rs.spheres.push_back({ g.heli.pos + Vec3(0.0f, 1.8f, 0.0f), 1.6f });   // the helicopter's body
    return rs;
}

// Emitters for the inverse ray tracing: the lava flows and the flying droplets.
std::vector<PhotonEmitter> buildEmitters(float t) {
    std::vector<PhotonEmitter> em;
    std::vector<Scene::FlowSample> samples;
    for (const auto& st : flowStyles()) {
        samples.clear();
        Scene::lavaFlowSamples(st, kVolcanoBaseRadius, kVolcanoHeight, t, 16, samples);
        for (const auto& fs : samples) {
            PhotonEmitter e;
            // A glowing line source a little above the lava, radiating in all directions: a flat
            // emitter lying on the ground would only shine upward and light nothing around it.
            e.pos = fs.pos + fs.normal * 0.6f; e.normal = fs.normal; e.sphere = true;
            e.power = fs.width * fs.length * (0.4f + fs.heat);   // emitting area x brightness
            em.push_back(e);
        }
    }
    Vec3 dp[24]; float db[24];
    int n = Scene::lavaDropletEmitters(dp, db, 24);
    for (int i = 0; i < n; ++i) {
        PhotonEmitter e;
        e.pos = dp[i]; e.sphere = true; e.power = 6.0f * db[i];
        em.push_back(e);
    }
    return em;
}

// Per-frame lighting uniforms (Phase 2): the crater point light, the sky light
// (moon at night / sun by day), the global ambient, and the render modes.
void uploadLighting(const Shader& shader, const Frame& f) {
    SkyLight sl = skyLightFor(f.sky);
    shader.setVec3("uViewPos", f.eye.x, f.eye.y, f.eye.z);
    shader.setVec3("uCraterPos", kCraterLightPos.x, kCraterLightPos.y, kCraterLightPos.z);
    shader.setVec3("uCraterDiffuse", 1.0f, 0.45f, 0.15f);   // warm orange
    shader.setVec3("uCraterSpecular", 1.0f, 0.6f, 0.3f);
    shader.setVec3("uAtten", 1.0f, 0.06f, 0.02f);           // 1 / (a0 + a1 d + a2 d^2)
    shader.setFloat("uCraterBoost", 1.0f + 2.5f * Eruption::intensity(f.t));   // the vent flares during a burst
    shader.setVec3("uSkyL", sl.toLight.x, sl.toLight.y, sl.toLight.z);
    shader.setVec3("uSkyDiffuse", sl.diffuse.x, sl.diffuse.y, sl.diffuse.z);
    shader.setVec3("uSkySpecular", sl.specular.x, sl.specular.y, sl.specular.z);
    shader.setVec3("uAmbient", sl.ambient.x, sl.ambient.y, sl.ambient.z);
    shader.setFloat("uExposure", sl.exposure);
    shader.setInt("uShadingMode", g.shadingMode);
    shader.setInt("uLightMode", g.lightMode);
    shader.setInt("uTermMode", g.termMode);
    shader.setInt("uBlinn", g.blinn ? 1 : 0);
    shader.setInt("uUseTex", g.useTex ? 1 : 0);

    // Ray tracing: mode, the sky used for reflections, and the inverse-tracing light map.
    shader.setInt("uRTMode", g.rtMode);
    shader.setVec3("uHorizon", f.sky.horizon.x, f.sky.horizon.y, f.sky.horizon.z);
    shader.setVec3("uZenith", f.sky.zenith.x, f.sky.zenith.y, f.sky.zenith.z);
    shader.setVec3("uSunDir", f.sky.sunDir.x, f.sky.sunDir.y, f.sky.sunDir.z);
    shader.setVec3("uMoonDir", f.sky.moonDir.x, f.sky.moonDir.y, f.sky.moonDir.z);
    shader.setFloat("uSunVis", smoothstepf(-0.08f, 0.05f, f.sky.sunDir.y));
    shader.setFloat("uMoonVis", smoothstepf(-0.08f, 0.05f, f.sky.moonDir.y) * lerpf(1.0f, 0.25f, f.sky.daylight));
    shader.setInt("uPhotonOn", g.photonsOn ? 1 : 0);
    shader.setFloat("uPhotonGain", g.photonGain);
    shader.setVec2("uMapOrigin", -PhotonMap::kExtent, -PhotonMap::kExtent);
    shader.setFloat("uMapSize", 2.0f * PhotonMap::kExtent);
    shader.setFloat("uRefl", 0.0f);
}

void renderScene(const Shader& shader, const Scene::MeshLibrary& meshes, const Frame& f, const RayScene& proxy) {
    const SkyState& sky = f.sky;
    shader.setMat4("uView", f.view);
    shader.setMat4("uProjection", f.proj);
    uploadLighting(shader, f);
    proxy.upload(shader);
    Scene::setTexture(shader, Scene::TexNone);

    // Select the material and emission strength for the draws that follow.
    auto material = [&](Scene::Material m, float emit = 0.0f, float refl = 0.0f) {
        shader.setInt("uMat", m);
        shader.setFloat("uEmit", emit);
        shader.setFloat("uRefl", refl);   // reflectivity (lava 0.12; the lake sets its own in drawTerrain)
    };

    material(Scene::MatUnlit);
    Scene::drawSky(shader, meshes, f.eye, sky, f.flash);

    material(Scene::MatMatte);
    Scene::drawTerrain(shader, meshes);
    material(Scene::MatRock);
    Scene::setTexture(shader, Scene::TexRock, 10.0f, 3.0f);
    Scene::drawVolcano(shader, meshes, kVolcanoBase, kVolcanoBaseRadius, kVolcanoHeight);
    Scene::setTexture(shader, Scene::TexNone);

    material(Scene::MatLava, 1.0f);
    Scene::drawCraterGlowDisc(shader, meshes, kCraterPos, 1.8f, f.t);   // fits the crater floor (radius 2.0) even at full pulse

    material(Scene::MatRock);
    Scene::setTexture(shader, Scene::TexRock, 2.0f, 2.0f);
    Scene::drawCraterRimRocks(shader, meshes, kCraterPos, 3.1f, 12, kVolcanoBaseRadius, kVolcanoHeight);

    // The lava pool at the foot of the river, and five lava flows, each in its own style.
    Scene::setTexture(shader, Scene::TexNone);
    material(Scene::MatLava, 0.6f, 0.12f);
    Scene::drawLavaPool(shader, meshes, kPoolPos, f.t);
    material(Scene::MatLava, 0.75f, 0.12f);
    const float vr = kVolcanoBaseRadius, vh = kVolcanoHeight;
    for (const auto& st : flowStyles()) Scene::drawLavaFlow(shader, meshes, kCraterPos, vr, vh, st, f.t);
    material(Scene::MatLava, 0.9f, 0.12f);
    Scene::updateAndDrawLavaDroplets(shader, meshes, kCraterPos, vr, vh, f.t);

    material(Scene::MatRock);
    Scene::setTexture(shader, Scene::TexRock, 2.0f, 2.0f);
    Scene::updateAndDrawRockChunks(shader, meshes, kCraterPos, vr, vh, f.t);
    Scene::setTexture(shader, Scene::TexNone);
    material(Scene::MatAsh);
    Scene::updateAndDrawAshPuffs(shader, meshes, kCraterPos, f.t);
    Scene::drawEruptionSmoke(shader, meshes, kCraterPos, kWindDir, f.t);   // the burst plumes
    Scene::drawEruptionEffects(shader, meshes, kCraterPos, vr, vh, f.t);   // shockwave, pyroclastic flows, lake bombs

    // ---- Aftermath and monitoring objects ----
    material(Scene::MatMatte);
    Scene::setTexture(shader, Scene::TexWood, 2.0f, 2.0f);
    for (const auto& tp : kTrees) {
        Vec3 pos(tp.radius * std::cos(radians(tp.angleDeg)), 0.0f, tp.radius * std::sin(radians(tp.angleDeg)));
        Scene::drawBurntTree(shader, meshes, pos, tp.scale, kTreeLeanDeg, tp.seed);
    }
    Scene::drawBurntHut(shader, meshes, kHutPos, 205.0f + 90.0f);
    Scene::drawWindmill(shader, meshes, kWindmillPos, 30.0f, f.t);
    Scene::setTexture(shader, Scene::TexNone);
    Scene::drawHelipad(shader, meshes, kHelipadPos);

    material(Scene::MatAsh);
    Scene::drawAshCloud(shader, meshes, kCraterPos, kWindDir, f.t);

    material(Scene::MatUnlit);
    Scene::drawStormClouds(shader, meshes, kStormBaseY, kWindDir, f.t, f.flash, sky.daylight);
    if (sky.daylight < kNoLightningDaylight)
        Scene::drawVolcanicLightning(shader, meshes, Vec3(0.0f, kStormBaseY, 0.0f), f.t);

    // Two autonomous drones circling the volcano and the pilot drone. The drone
    // we are looking through is skipped so the camera isn't inside its own body.
    material(Scene::MatMatte);
    for (int i = 0; i < kOrbitDrones; ++i) {
        if (f.hiddenDrone == i) continue;
        DroneKin k = orbitDrone(i, f.t);
        Scene::drawResearchDrone(shader, meshes, k.pos, k.drawTime, k.yaw);
    }
    if (f.hiddenDrone != kPilotIndex) {
        DroneKin k = pilotDrone(f.t);
        Scene::drawResearchDrone(shader, meshes, k.pos, k.drawTime, k.yaw, g.pilot.pitchVis, g.pilot.rollVis);
    }

    // The other three controllable objects: rover, helicopter and boat.
    if (f.hiddenDrone != kRoverIndex)
        Scene::drawRover(shader, meshes, g.rover.pos, g.rover.yaw, g.rover.rollVis, g.rover.wheelAngle);
    if (f.hiddenDrone != kHeliIndex)
        Scene::drawHelicopter(shader, meshes, g.heli.pos, g.heli.yaw, g.heli.pitchVis, g.heli.rollVis, g.heli.rotorAngle);
    if (f.hiddenDrone != kBoatIndex)
        Scene::drawBoat(shader, meshes, g.boat.pos, g.boat.yaw, g.boat.speed, f.t);
}

// ---------------------------------------------------------------- input
// Orbit distance when the camera starts following a controlled object (close enough to see it).
float followDistance(int ctl) {
    switch (ctl) {
        case CtlRover: return 18.0f;
        case CtlHeli:  return 30.0f;
        case CtlBoat:  return 24.0f;
        default:       return 26.0f;
    }
}

// Where the orbit camera looks while an object is controlled.
Vec3 controlledTarget() {
    switch (g.ctl) {
        case CtlDrone: return g.pilot.pos;
        case CtlRover: return g.rover.pos + Vec3(0.0f, 1.0f, 0.0f);
        case CtlHeli:  return g.heli.pos + Vec3(0.0f, 1.6f, 0.0f);
        case CtlBoat:  return g.boat.pos + Vec3(0.0f, 0.6f, 0.0f);
        default:       return g.orbit.target;
    }
}

const char* controlName(int ctl) {
    static const char* kNames[] = { "NONE", "PILOT DRONE", "ROVER", "HELICOPTER", "BOAT" };
    return kNames[ctl < 0 || ctl >= CtlCount ? 0 : ctl];
}

// Take control of one of the four objects (or release it with CtlNone).
void setControl(int ctl) {
    bool wasControlling = g.ctl != CtlNone;
    g.ctl = ctl;
    if (ctl != CtlNone) {
        if (!wasControlling) g.savedDistance = g.orbit.distance;
        g.orbit.distance = std::min(g.savedDistance, followDistance(ctl));
    } else if (wasControlling) {
        g.orbit.distance = g.savedDistance;
    }
}

void keyCallback(GLFWwindow*, int key, int, int action, int) {
    if (action != GLFW_PRESS) return; // one tap = one change
    switch (key) {
        case GLFW_KEY_N:   g.sky.cycleMode(); break;
        case GLFW_KEY_C:   g.camMode = (g.camMode + 1) % CamCount; break;
        case GLFW_KEY_V:   g.aimForward = !g.aimForward; break;
        case GLFW_KEY_P:   g.paused = !g.paused; break;
        case GLFW_KEY_TAB: setControl((g.ctl + 1) % CtlCount); break;   // none -> drone -> rover -> helicopter -> boat
        case GLFW_KEY_1:   g.shadingMode = 0; break;   // Flat
        case GLFW_KEY_2:   g.shadingMode = 1; break;   // Gouraud
        case GLFW_KEY_3:   g.shadingMode = 2; break;   // Phong
        case GLFW_KEY_L:   g.lightMode = (g.lightMode + 1) % 3; break;
        case GLFW_KEY_K:   g.termMode = (g.termMode + 1) % 4; break;
        case GLFW_KEY_B:   g.blinn = !g.blinn; break;
        case GLFW_KEY_T:   g.useTex = !g.useTex; break;
        case GLFW_KEY_H:   g.showHud = !g.showHud; break;
        case GLFW_KEY_R:   g.rtMode = (g.rtMode + 1) % 3; break;
        case GLFW_KEY_F:   g.photonsOn = !g.photonsOn; break;
        case GLFW_KEY_M:   g.showMap = !g.showMap; break;
        case GLFW_KEY_E:   Eruption::trigger(g.simTime); break;       // fire a big burst now
        case GLFW_KEY_J:   g.shake = !g.shake; break;
        case GLFW_KEY_EQUAL: case GLFW_KEY_KP_ADD:      g.tessDelta += 1; break;
        case GLFW_KEY_MINUS: case GLFW_KEY_KP_SUBTRACT: g.tessDelta -= 1; break;
        default: break;
    }
}

void scrollCallback(GLFWwindow*, double, double yoff) {
    g.orbit.zoom(static_cast<float>(-yoff) * 3.0f);
}

void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    g.width = width;
    g.height = height;
    glViewport(0, 0, width, height);
}

PilotInput readPilotInput(GLFWwindow* window) {
    PilotInput in;
    if (g.autopilot) { in.fwd = 1.0f; in.turn = 0.6f; in.vert = 0.2f; return in; }
    auto down = [&](int k) { return glfwGetKey(window, k) == GLFW_PRESS; };
    if (down(GLFW_KEY_W)) in.fwd += 1.0f;
    if (down(GLFW_KEY_S)) in.fwd -= 1.0f;
    if (down(GLFW_KEY_A)) in.turn += 1.0f;   // +yaw turns left
    if (down(GLFW_KEY_D)) in.turn -= 1.0f;
    if (down(GLFW_KEY_SPACE)) in.vert += 1.0f;
    if (down(GLFW_KEY_LEFT_SHIFT)) in.vert -= 1.0f;
    return in;
}

void processHeldKeys(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float orbitSpeed = 60.0f * dt;   // degrees/second
    float zoomSpeed = 20.0f * dt;    // units/second

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  g.orbit.orbit(-orbitSpeed, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) g.orbit.orbit(orbitSpeed, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    g.orbit.orbit(0.0f, orbitSpeed);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  g.orbit.orbit(0.0f, -orbitSpeed);
    // Zoom: PgUp/PgDn and the mouse wheel. (+/- now change the tessellation.)
    if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)   g.orbit.zoom(-zoomSpeed);
    if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) g.orbit.zoom(zoomSpeed);
}

// ---------------------------------------------------------------- HUD
std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) {
    char buf[160];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return buf;
}

// Top-left status panel (IMPLEMENTATION_GUIDE.md §9.4 / PRD §10) plus a key legend at the bottom.
void drawHud(Hud& hud, const Frame& f, const Scene::MeshLibrary& meshes, const PhotonMap& photons) {
    static const char* kShade[] = { "FLAT (PER TRIANGLE)", "GOURAUD (PER VERTEX)", "PHONG (PER FRAGMENT)" };
    static const char* kLights[] = { "CRATER + SKY LIGHT", "CRATER ONLY", "SKY LIGHT ONLY" };
    static const char* kTerms[] = { "AMBIENT ONLY", "AMBIENT+DIFFUSE", "SPECULAR ONLY", "AMBIENT+DIFFUSE+SPECULAR" };

    std::vector<std::string> lines;
    lines.push_back(fmt("SHADING: %s   [1/2/3]", kShade[g.shadingMode]));
    lines.push_back(fmt("LIGHTS:  %s   [L]", kLights[g.lightMode]));
    lines.push_back(fmt("TERMS:   %s   [K]", kTerms[g.termMode]));
    lines.push_back(fmt("SPECULAR: %s   [B]", g.blinn ? "BLINN-PHONG (N.H)" : "PHONG (R.V)"));
    lines.push_back(fmt("CONE SEGMENTS: %d   [+/-]", Scene::MeshLibrary::coneSegments(meshes.tessLevel)));
    lines.push_back(fmt("TEXTURES: %s   [T]", g.useTex ? "ON" : "OFF"));
    static const char* kRT[] = { "OFF", "SHADOWS", "SHADOWS + REFLECTIONS" };
    lines.push_back(fmt("RAY TRACING: %s   [R]", kRT[g.rtMode]));
    lines.push_back(fmt("LAVA LIGHT TRACING: %s   [F]", g.photonsOn ? "ON" : "OFF"));
    lines.push_back(fmt("TIME: %s   [N]", g.sky.label(g.simTime).c_str()));
    lines.push_back(fmt("CAMERA: %s%s   [C/V]", camName(g.camMode),
                        g.camMode == CamOrbit ? "" : (g.aimForward ? " (FORWARD)" : " (CRATER LOCK)")));
    lines.push_back("---");
    {
        float burst = Eruption::intensity(f.t);
        if (burst > 0.15f) lines.push_back(fmt("ERUPTION: BURST %d%%   [E] FIRE ONE", (int)(100.0f * std::min(1.0f, burst))));
        else               lines.push_back(fmt("ERUPTION: NEXT BURST IN %.1f S   [E] FIRE ONE", Eruption::timeToNext(f.t)));
    }
    lines.push_back(fmt("ACTIVE DROPLETS:  %d / 24", Scene::activeLavaDroplets()));
    lines.push_back(fmt("ACTIVE ASH PUFFS: %d / 6", Scene::activeAshPuffs()));
    lines.push_back(fmt("LAVA RIVER ADVANCE: %d%%", (int)(100.0f * std::min(1.0f, 0.25f + 0.030f * f.t))));
    lines.push_back(fmt("DISTANCE TO CRATER: %.1f M", length3(f.eye - kCraterPos)));
    if (g.ctl != CtlNone) {
        std::string t = fmt("CONTROL: %s", controlName(g.ctl));
        switch (g.ctl) {
            case CtlDrone: t += fmt("  ALT %.0f  SPD %.0f", g.pilot.pos.y, std::fabs(g.pilot.speed)); break;
            case CtlRover: t += fmt("  SPD %.1f", g.rover.speed); break;
            case CtlHeli:  t += fmt("  ALT %.0f  SPD %.0f  ROTOR %d%%", g.heli.pos.y, std::fabs(g.heli.speed), (int)(100.0f * g.heli.rotorSpeed / 30.0f)); break;
            case CtlBoat:  t += fmt("  SPD %.1f", g.boat.speed); break;
            default: break;
        }
        lines.push_back(t + "  [TAB]");
    } else {
        lines.push_back("CONTROL: NONE   [TAB: DRONE/ROVER/HELI/BOAT]");
    }
    lines.push_back(fmt("FPS: %.0f   DRAW CALLS: %d%s", g.fps, g.drawCalls, g.paused ? "   PAUSED" : ""));

    const float px = 14.0f, lineH = 18.0f, pad = 8.0f;
    float width = 0.0f;
    for (const auto& l : lines) width = std::max(width, Hud::textWidth(l, px));
    hud.begin(g.width, g.height);
    hud.panel(6.0f, 6.0f, width + 2.0f * pad, lines.size() * lineH + 2.0f * pad, 0.0f, 0.0f, 0.0f, 0.55f);
    float y = 6.0f + pad;
    for (const auto& l : lines) {
        bool rule = l == "---";
        hud.text(6.0f + pad, y, l, px, rule ? 0.5f : 1.0f, rule ? 0.5f : 0.95f, rule ? 0.5f : 0.85f, 1.0f);
        y += lineH;
    }
    const char* legend1 = "[1-3] SHADE [L] LIGHT [K] TERMS [B] BLINN [T] TEX [R] RAYS [F] LAVA LIGHT [M] MAP [N] TIME [H] HUD";
    const char* legend2 = "[E] ERUPT [J] SHAKE [C/V] CAMERA [TAB] CONTROL [WASD SPACE SHIFT] DRIVE/FLY [P] PAUSE";
    float lw = std::max(Hud::textWidth(legend1, 11.0f), Hud::textWidth(legend2, 11.0f));
    hud.panel(6.0f, g.height - 42.0f, lw + 12.0f, 36.0f, 0.0f, 0.0f, 0.0f, 0.5f);
    hud.text(12.0f, g.height - 39.0f, legend1, 11.0f, 0.85f, 0.85f, 0.85f, 1.0f);
    hud.text(12.0f, g.height - 22.0f, legend2, 11.0f, 0.85f, 0.85f, 0.85f, 1.0f);
    hud.flush();

    if (g.showMap) {        // the light map as a top-down minimap (x right, z down), volcano at the centre
        float sz = 200.0f, mx = g.width - sz - 14.0f, my = 10.0f;
        hud.begin(g.width, g.height);
        hud.panel(mx - 4.0f, my - 4.0f, sz + 8.0f, sz + 8.0f + 20.0f, 0.0f, 0.0f, 0.0f, 0.7f);
        hud.text(mx, my + sz + 6.0f, fmt("LIGHT MAP  %d HITS", photons.lastHits), 11.0f, 1.0f, 0.8f, 0.5f);
        hud.flush();
        hud.image(photons.texture, mx, my, sz, sz, 40.0f);
    }
}

// ---------------------------------------------------------------- self-test
// Checks the C++ ray-intersection code (RayScene.h) against hand-computed distances. The GLSL in
// shaders/raytrace.glsl uses the same formulas; the render comparisons in the guide cover that side.
bool rayScenePassesSelfTest() {
    bool ok = true;
    auto check = [&](const char* name, float got, float want) {
        bool pass = std::fabs(got - want) < 1e-3f;
        std::printf("  %-34s got %8.4f want %8.4f  %s\n", name, got, want, pass ? "ok" : "FAIL");
        ok = ok && pass;
    };
    Vec3 n;
    RaySphere sp{ Vec3(0, 0, 5), 1.0f };
    check("sphere, centred ray", RayScene::hitSphere(Vec3(0, 0, 0), Vec3(0, 0, 1), sp), 4.0f);

    RayCone cone; cone.base = Vec3(0, 0, 0); cone.r0 = 2.0f; cone.r1 = 0.0f; cone.h = 4.0f;
    check("cone side at y = 1 (radius 1.5)", RayScene::hitCone(Vec3(5, 1, 0), Vec3(-1, 0, 0), cone, n), 3.5f);
    check("cone apex hit from above", RayScene::hitCone(Vec3(0, 10, 0), Vec3(0, -1, 0), cone, n), 6.0f);   // lands on the cone's axis at y=4
    cone.r0 = cone.r1 = 1.0f;
    check("cylinder side", RayScene::hitCone(Vec3(5, 2, 0), Vec3(-1, 0, 0), cone, n), 4.0f);
    cone.tilt = 0.3f;                                         // a tilted cylinder is still hit along its own axis
    float ca = std::cos(0.3f), sa = std::sin(0.3f);
    Vec3 axis(-sa, ca, 0.0f);                                 // Rz(tilt) * (0,1,0)
    check("tilted cylinder, ray along the axis", RayScene::hitCone(axis * 10.0f + Vec3(0, 0, 0), -axis, cone, n), 6.0f);

    RayBox box; box.c = Vec3(0, 1, 0); box.half = Vec3(2.0f, 1.0f, 0.5f); box.yaw = radians(90.0f); box.present = true;
    check("box rotated 90 deg about Y", RayScene::hitBox(Vec3(5, 1, 0), Vec3(-1, 0, 0), box, n), 4.5f);
    std::printf("ray scene self-test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// ---------------------------------------------------------------- debug screenshot
bool writePPM(const char* path, int w, int h) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    FILE* fp = std::fopen(path, "wb");
    if (!fp) return false;
    std::fprintf(fp, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; --y) // OpenGL rows are bottom-up
        std::fwrite(&px[static_cast<size_t>(y) * w * 3], 1, static_cast<size_t>(w) * 3, fp);
    std::fclose(fp);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    // ---- command line (debug options only) ----
    std::string shotPath;
    int shotFrames = 5;
    int optCam = -1, optLock = -1;
    float optStart = 0.0f;
    float benchSeconds = 0.0f;
    std::vector<float> fireAt;   // --fire <t>: fire a viewer burst at this simulated time
    int optTess = -1;
    int optControl = 0;
    bool optOrbit = false, optCtlPos = false;
    Vec3 ctlPos;
    float oYaw = 0, oPitch = 0, oDist = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto more = [&](int n) { return i + n < argc; };
        if (a == "--shot" && more(1)) shotPath = argv[++i];
        else if (a == "--frames" && more(1)) shotFrames = std::atoi(argv[++i]);
        else if (a == "--start" && more(1)) optStart = static_cast<float>(std::atof(argv[++i]));
        else if (a == "--cam" && more(1)) optCam = std::atoi(argv[++i]);
        else if (a == "--lock" && more(1)) optLock = std::atoi(argv[++i]);
        else if (a == "--orbit" && more(3)) {
            optOrbit = true;
            oYaw = static_cast<float>(std::atof(argv[++i]));
            oPitch = static_cast<float>(std::atof(argv[++i]));
            oDist = static_cast<float>(std::atof(argv[++i]));
        }
        else if (a == "--forward") g.aimForward = true;
        else if (a == "--shade" && more(1)) g.shadingMode = std::atoi(argv[++i]);
        else if (a == "--lights" && more(1)) g.lightMode = std::atoi(argv[++i]);
        else if (a == "--terms" && more(1)) g.termMode = std::atoi(argv[++i]);
        else if (a == "--tess" && more(1)) optTess = std::atoi(argv[++i]);
        else if (a == "--blinn") g.blinn = true;
        else if (a == "--selftest") { return rayScenePassesSelfTest() ? 0 : 1; }
        else if (a == "--fire" && more(1)) fireAt.push_back(static_cast<float>(std::atof(argv[++i])));
        else if (a == "--noshake") g.shake = false;
        else if (a == "--notex") g.useTex = false;
        else if (a == "--rt" && more(1)) g.rtMode = std::atoi(argv[++i]);
        else if (a == "--photons") g.photonsOn = true;
        else if (a == "--map") g.showMap = true;
        else if (a == "--gain" && more(1)) g.photonGain = static_cast<float>(std::atof(argv[++i]));
        else if (a == "--bench" && more(1)) benchSeconds = static_cast<float>(std::atof(argv[++i]));
        else if (a == "--nohud") g.showHud = false;
        else if (a == "--pilotpos" && more(3)) {
            g.pilot.pos = Vec3(static_cast<float>(std::atof(argv[i + 1])), static_cast<float>(std::atof(argv[i + 2])), static_cast<float>(std::atof(argv[i + 3])));
            i += 3;
        }
        else if (a == "--pilot") optControl = CtlDrone;
        else if (a == "--control" && more(1)) optControl = std::atoi(argv[++i]);
        else if (a == "--ctlpos" && more(3)) {
            optCtlPos = true;
            ctlPos = Vec3(static_cast<float>(std::atof(argv[i + 1])), static_cast<float>(std::atof(argv[i + 2])), static_cast<float>(std::atof(argv[i + 3])));
            i += 3;
        }
        else if (a == "--autopilot") g.autopilot = true;
    }
    const bool shotMode = !shotPath.empty();

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(g.width, g.height, "VolcanoX", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);
    if (benchSeconds > 0.0f) glfwSwapInterval(0);   // measure the real cost, not the monitor refresh

    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewStatus) << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);   // every generator emits counter-clockwise-outward triangles
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader shader;
    if (!shader.loadFromFiles("shaders/basic.vert", "shaders/basic.frag")) {
        std::cerr << "Shader failed to load. Check that the working directory contains shaders/." << std::endl;
        return -1;
    }

    Scene::MeshLibrary meshes;
    meshes.build();

    // Procedural textures (no image files) on units 0/1/2, and the HUD font atlas.
    Tex::Textures textures;
    textures.build();
    shader.use();
    shader.setInt("uTexRock", 0);
    shader.setInt("uTexAsh", 1);
    shader.setInt("uTexWood", 2);
    Hud hud;
    PhotonMap photons;
    photons.init();
    shader.setInt("uLightMap", 4);
    if (!hud.init()) std::cerr << "HUD disabled (shaders/hud.* failed to load)" << std::endl;

    // ---- apply debug options ----
    g.simTime = optStart;
    for (size_t fi = 0; fi < fireAt.size(); ++fi) Eruption::manualBursts().push_back({ fireAt[fi], 1.3f, 1000 + int(fi), true });
    if (optCam >= 0 && optCam < CamCount) g.camMode = optCam;
    if (optOrbit) { g.orbit.yawDeg = oYaw; g.orbit.pitchDeg = oPitch; g.orbit.distance = oDist; }
    if (optLock >= 0 && optLock <= 2) g.sky.snapLock(optLock);
    if (optTess >= 0) meshes.rebuildTessellation(optTess);
    if (optControl > 0 && optControl < CtlCount) setControl(optControl);
    if (optCtlPos) {
        switch (g.ctl) {
            case CtlRover: g.rover.pos = ctlPos; break;
            case CtlHeli:  g.heli.pos = ctlPos; break;
            case CtlBoat:  g.boat.pos = ctlPos; break;
            default:       g.pilot.pos = ctlPos; break;
        }
    }
    if (optOrbit) g.savedDistance = oDist;

    const Vec3 defaultTarget = g.orbit.target;
    double lastTime = glfwGetTime();
    double lastTitle = 0.0;
    int frameNo = 0;
    long long benchDrawCalls = 0;

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = shotMode ? (1.0f / 60.0f) : std::min(0.1f, static_cast<float>(now - lastTime));
        lastTime = now;

        // Simulated time stops while paused (the day/night clock, droplets, drones...).
        float simDt = g.paused ? 0.0f : dt;
        g.simTime += simDt;
        g.sky.update(dt, g.simTime);
        if (!shotMode && dt > 1e-5f) g.fps += (1.0f / dt - g.fps) * 0.05f;   // smoothed frames per second

        glfwPollEvents();
        processHeldKeys(window, dt);
        if (g.tessDelta != 0) {   // + / - : rebuild the cone and pool meshes, only on a keypress
            meshes.rebuildTessellation(meshes.tessLevel + (g.tessDelta > 0 ? 1 : -1));
            g.tessDelta = 0;
        }

        // The pilot drone integrates live input; with no input it eases to a hover.
        // The four controllable objects integrate live input; whichever is not selected gets no input
        // and eases to a stop (the drone and helicopter hover).
        PilotInput in = g.ctl != CtlNone ? readPilotInput(window) : PilotInput();
        const PilotInput none;
        g.pilot.update(simDt, g.ctl == CtlDrone ? in : none, [](float x, float z) {
            return std::max(2.5f, Scene::terrainHeightAt(x, z, kVolcanoBaseRadius, kVolcanoHeight) + 3.0f);
        });
        g.rover.update(simDt, g.ctl == CtlRover ? in : none);
        g.heli.update(simDt, g.ctl == CtlHeli ? in : none, g.ctl == CtlHeli, [](float x, float z) {
            return Scene::terrainHeightAt(x, z, kVolcanoBaseRadius, kVolcanoHeight);
        });
        g.boat.update(simDt, g.ctl == CtlBoat ? in : none);

        // The orbit camera follows the controlled object, else returns to the volcano.
        Vec3 wantTarget = g.ctl != CtlNone ? controlledTarget() : defaultTarget;
        float k = std::min(1.0f, 6.0f * dt);
        g.orbit.target = g.orbit.target + (wantTarget - g.orbit.target) * k;

        float aspect = g.height > 0 ? float(g.width) / float(g.height) : 1.0f;
        Frame f = makeFrame(aspect);

        // Camera shake: a jolt at the start of every burst that dies away in about a second. A small rotation
        // applied to the view (the eye does not move, so distances and the HUD are unaffected).
        if (g.shake) {
            float a = 0.011f * Eruption::shake(f.t);
            if (a > 1e-5f) {
                float tt = f.t;
                f.view = Mat4::rotateX(a * std::sin(63.0f * tt)) * Mat4::rotateY(a * std::sin(79.0f * tt + 1.1f))
                       * Mat4::rotateZ(0.5f * a * std::sin(51.0f * tt + 2.3f)) * f.view;
            }
        }

        glClearColor(f.sky.horizon.x + 0.30f * f.flash, f.sky.horizon.y + 0.31f * f.flash,
                     f.sky.horizon.z + 0.38f * f.flash, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shader.use();
        textures.bind();
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, photons.texture);
        glActiveTexture(GL_TEXTURE0);
        RayScene proxy = buildProxyScene(f.t);
        Mesh::drawCalls() = 0;
        renderScene(shader, meshes, f, proxy);
        g.drawCalls = Mesh::drawCalls();

        // Inverse ray tracing: fire photons from the lava into the light map for the next frame.
        if (g.photonsOn) photons.update(proxy, buildEmitters(f.t), frameNo);
        if (g.showHud) drawHud(hud, f, meshes, photons);
        (void)lastTitle;

        ++frameNo;
        if (benchSeconds > 0.0f) {
            benchDrawCalls += g.drawCalls;
            if (now >= benchSeconds) {
                std::cout << "bench: " << frameNo << " frames in " << now << " s = " << frameNo / now
                          << " FPS, avg draw calls " << benchDrawCalls / frameNo << std::endl;
                break;
            }
        }
        if (shotMode && frameNo >= shotFrames) {
            GLenum err = glGetError();
            if (err != GL_NO_ERROR) std::cout << "GL error " << err << std::endl;
            bool ok = writePPM(shotPath.c_str(), g.width, g.height);
            std::cout << (ok ? "wrote " : "FAILED to write ") << shotPath << std::endl;
            break;
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
