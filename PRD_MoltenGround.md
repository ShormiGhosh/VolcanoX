# PRD — VolcanoX ("Molten Ground"): A Real-Time Volcanic Eruption Simulation

### Computer Graphics Final Project — Product Requirements Document

**Official project title (KUET CSE 4102 lab proposal):** VolcanoX — A Real-Time Volcanic Eruption Simulation in OpenGL
**Submitted by:** Shormi Ghosh, Roll 2107109 — Submitted to Md. Tajmilur Rahman & Md. Mubtashim Abrar Nihal, Dept. of CSE, KUET
**Version:** 1.4, 2026-10-09 (v1.2: rewritten to match the code; v1.3: day/night, drone views, a pilot drone, ray tracing and inverse ray tracing added to the spec; v1.4: all of it **built**, with as-built numbers; see §0.6)
**Platform:** OpenGL 3.3 Core Profile — GLFW + **GLEW** + a small hand-written math header (see §0.1 for why this differs from the original GLAD/GLM plan)
**Target:** Single student, limited timeframe
**Course decks covered:** `L8_Illumination.pptx`, `L9_shading.pptx`

---

## 0. Amendment — Implementation Status (Current)

This PRD was written around the full graded feature set (lighting, materials,
three shading modes). It was built in phases: objects first, then the
day/night cycle, drone cameras and a pilot drone (Phase 1b), then lighting,
shading, textures and the HUD (Phase 2), then ray tracing (Phase 3). **All of
them are now built** (§0.6). §1 onward is the specification they were built to,
with as-built notes where the code differs.

**v1.2 (2026-10-09).** Between v1.1 and v1.2 the code moved ahead of this
document: five lava flows instead of one channel and a pool, orbiting drones,
a storm-cloud lightning rework, a 36-chunk rock pool, and a whole scenery layer
(`Environment.cpp`). Those changes are **kept**. This PRD was changed to
describe the code as it is, not the other way round. v1.2 also adds three
planned features (§0.5) and brings procedural textures in as a supporting
feature (§3.2). The build, run and extension steps live in
`IMPLEMENTATION_GUIDE.md`.

**v1.3.** Adds a **stretch tier** of ray tracing (§9.10, §9.11): camera rays for shadows and one reflection bounce, and inverse (light-ray) tracing of the lava's glow. It reverses the proposal's "no shadows or reflections" and is built only after the MVP (§16.1) works.

### 0.1 Toolchain substitutions (environment-driven, not a design change)

The lab machine this was built on has GLFW and GLEW available system-wide via
MSYS2/MinGW, but no CMake binary and no GLM, and no package manager to fetch
them:

| Planned (v1.0) | Actually used | Why |
|---|---|---|
| GLAD | **GLEW** | Both are OpenGL function loaders that do the same job (resolve function pointers beyond OpenGL 1.1 on Windows); GLEW was already installed, GLAD was not and there was no package manager to install it with. Functionally interchangeable for this project. |
| GLM | **`src/MathUtils.h`**, a ~120-line hand-written `Vec3`/`Mat4` header (translate/scale/rotate/perspective/lookAt/multiply) | GLM was not installed and there is no network-based package manager on this machine. Writing the handful of functions actually needed is small, has zero external dependency, and — usefully for a lab demo — means every transformation in the scene can be pointed at and explained line-by-line instead of citing a library. |
| CMake build | CMakeLists.txt still provided (for a machine that has CMake) + **`build.ps1`**, a direct g++ invocation | No `cmake.exe` is installed on this machine. `build.ps1` compiles the same sources directly with the MinGW64 g++ toolchain. |

See `IMPLEMENTATION_GUIDE.md` for exact build/run steps, including a real
toolchain gotcha hit during development (an older MinGW install earlier in
`PATH` silently crashing the compiler — fixed by building with MSYS2's
`mingw64\bin` prioritized in `PATH`, and by static-linking the runtime so the
shipped `.exe` doesn't depend on `PATH` at all at demo time).

### 0.2 Scope for this phase

Phase 1 implemented every object in §6's inventory, placed in the scene and
driven only by **transformations** (translate/rotate/scale) with flat
colours. Everything the original objects-only scope left out is now built (§0.6).
Still **not** implemented: rim-rock instancing (rim rocks are drawn in a plain
non-instanced loop, which §14 recommends as the first step anyway), the GPU-side
lava pulse, windmill blade spin, the ground rover, the picture-in-picture drone
feed, soft shadows, and the ore-vein detail. Shadow *mapping* and cube-map
reflections remain non-goals (§3.2).

This does **not** contradict §3.2's rejection of a "destructible environment
(falling trees, crushed huts)": that line rejects a **runtime physics/collision
system** that destroys objects during simulation. The burnt trees and burnt hut
are static, pre-authored "already-damaged" geometry: no collision detection, no
simulated destruction, the same cost as any other static mesh.

### 0.3 Objects added in Phase 1 (implemented state)

Each was checked against "is this realistic around an active volcano, and what
does it represent?" before being added. The Status column is the code's state.

| # | Object | Real-world basis | Geometry | Status |
|---|---|---|---|---|
| N1 | Burnt trees (×6) | Heat/ashfall kill and char nearby vegetation (documented "ghost forest" effect, e.g. Mount St. Helens) | Tapered cone trunk + cylinder branches + irregular-rock debris at the base | Implemented. Radius 29–40, clear of every lava flow; lean is a fixed -10° (not wind-driven) |
| N2 | Burnt/damaged hut | Rural dwellings near active volcanoes are routinely damaged by ashfall and heat (e.g. Sinabung, Merapi) | Boxes for walls (one collapsed/tilted, one charred), two angled box roof panels (one slumped), rock-debris blocks | Implemented. Radius 40, angle 205° |
| N3 | Windmill | A simple homestead windmill is a natural pairing with the hut; storm/blast-damaged (one drooping blade) to match the aftermath theme | Footing, tower, nacelle with tail vane, spinning rotor of 4 box blades | Implemented. Radius 47, angle 212°. The rotor spins |
| N4 | Ash/smoke cloud | The eruption column spreads into an umbrella/mushroom shape as it loses momentum, bent downwind | Stacked rising-stem spheres + a wider cluster of spheres at the head, offset along the shared wind vector | Implemented. Lobes bob gently |
| N5 | Volcanic lightning | "Dirty thunderstorms" are real, but the scene now uses a **storm-cloud layer** (N9) as the source of the bolts, which also brightens the sky | Fractal midpoint-displacement bolts, drawn as thin boxes built from the bolt polylines | Implemented, **reworked**: bolts hang from the storm layer at y = 62, one strike every 2.2 s, two bolts per strike, with a sky flash |
| N6 | Wind indicators | A diagram convention for showing wind direction | Curved line strips | **Removed.** `windDir` still drives the ash cloud and the storm clouds |
| N7 | Research/camera drones | Quadcopters are the real-world standard tool for close-range volcanic monitoring without risking a person near an active vent | Box body + 4 box arms + cylinder rotor hubs + translucent propeller disc per arm + angled box camera/sensor underneath | Implemented as **two** drones **orbiting** the volcano (radius 30 and 38, height 16 and 20, opposite directions, with a bob). They gain headings, cameras and a piloted sibling in §0.5 |
| N8 | Aircraft | Considered and **not implemented**: a generic jet is decoration, and the drones already cover "aerial volcanic monitoring platform" | — | **Not implemented** |
| N9 | Storm clouds | Source of the lightning and part of the setting | Two layers of flattened spheres (46 + 34 puffs) drifting along `windDir`, faded at the layer edges | Implemented |
| N10 | Environment | Gives the volcano a place: sky dome with 220 stars and a moon, terrain discs, a lake, 16 hills, up to ~90 healthy trees (contrast with the burnt ones), two mountain ranges | Spheres, cones, cylinders, discs, a large box | Implemented (`Environment.cpp`) |

Shared wind vector: the ash cloud and the storm clouds read the same `windDir`
value from `main.cpp` (`normalize3(1, 0, 0.35)`), so changing it once updates
both. The burnt trees' lean does **not** use it.

### 0.4 File map (current)

```
src/MathUtils.h        Vec3/Mat4 (translate/rotate/scale/lookAt/perspective) -- GLM replacement
src/Geometry.h/.cpp     Procedural primitive generators (cone, sphere, box, disc, rock, line, bolt)
src/Mesh.h              One VAO/VBO wrapper, reused by every object
src/Shader.h/.cpp       GLSL compile/link + uniform setters
src/Camera.h            Orbit camera (arrows, wheel, PgUp/PgDn; no mouse capture, demo-safe)
src/DayNight.h          Day/night clock, SkyState and the blended sky light (header-only)
src/Controller.h        The pilot drone's input integration and limits (header-only)
src/RayScene.h          Ray-tracing proxy scene and C++ intersections (header-only)
src/PhotonMap.h         Inverse ray tracing: photons from the lava into a light map (header-only)
src/Texture.h/.cpp      Procedural textures, the bitmap-font HUD and the minimap
src/SceneObjects.h      Shared MeshLibrary + every draw*() function's declaration
src/SceneObjects.cpp    Volcano, crater glow disc, rim rocks, lava flows, particle pools
                        (also drawLavaPool / drawLavaFlowChannel / drawGroundPlane, no longer called)
src/NewObjects.cpp      drawBurntTree, drawBurntHut, drawWindmill, drawAshCloud,
                        drawVolcanicLightning (+ lightningFlash), drawResearchDrone
src/Environment.cpp     drawSky, drawTerrain, drawStormClouds
src/main.cpp            Window/GLEW setup, scene layout (coordinates for every object instance), render loop
shaders/basic.vert/frag Gouraud / Flat / Phong lighting, textures, ray-traced shadows and reflections,
                        light-map glow, emission, and the sky-dome gradient
shaders/lighting.glsl   The Phong illumination equation (shared, via #include)
shaders/raytrace.glsl   Ray-shape intersections, shadow rays, one reflection bounce
shaders/hud.vert/frag   Screen-space text, panels and the minimap
```

Full build/run instructions, the object-inventory spreadsheet, and the
implemented-state tables live in `IMPLEMENTATION_GUIDE.md` and
`object_inventory.xlsx`.

### 0.5 Added in v1.2 and v1.3 (all built in v1.4)

| Feature | What it is | Specified in |
|---|---|---|
| **Day/night cycle** | A 24-hour clock that advances automatically; sky, stars, sun, moon, clouds and the directional light follow it. `N` locks day or night for the demos | §5.1, §9.7, §11.1; Guide §10.1 |
| **Drone camera views** | The screen can show the view from either orbiting drone (or the piloted one), aimed at the crater or forward | §9.8, §10; Guide §10.2 |
| **Controllable objects** | A third drone the user flies with the keyboard; optionally a ground rover | §9.9, §10; Guide §10.3 |
| **Procedural textures** | Rock, ash and wood patterns generated by code at startup, modulating diffuse/ambient colour only; `T` turns them off | §3.2, §14; Guide §9.3 |
| **Ray tracing (stretch)** | Fragment-shader shadows and one reflection bounce against a small analytic proxy scene; `R` cycles Off / Shadows / Shadows + Reflections | §9.10; Guide §11.3 |
| **Inverse ray tracing (stretch)** | Photons fired from the lava flows and droplets, traced on the CPU into a top-down light map that adds the lava's glow to the surroundings; `F` toggles it | §9.11; Guide §11.4 |

### 0.6 As built (v1.4)

Details and measurements are in `IMPLEMENTATION_GUIDE.md` §8 (what was verified) and the
"As built" lists at the top of its §9, §10 and §11. The numbers that matter for this PRD:

| Item | As built |
|---|---|
| Lava pool | **Re-enabled** as the basin at the **end of the broad sheet flow** (about `(52, 0.1, -41)`). It does not exist until the flow reaches it (about 25 s), then fills with a hot core, a pulsing inflow glow and drifting crust plates (a free-standing disc read as a flat orange circle). It is needed for the Gouraud-vs-Phong highlight demo (§12.2): the crater disc cannot show a highlight, because the crater light lies in its plane |
| Draw order | Opaque pass first (terrain, volcano, lava, objects and every vehicle), then the translucent pass back to front. The storm-cloud layer is drawn first from below and last from above, so no vehicle shows through it from above |
| Crater light position | Hovers 6 units above the vent. At the vent it sits on the cone's tangent plane and lights none of the steep slopes |
| Exposure and ambient | The albedos are the original dark night colours, so the lit result is multiplied by an exposure of **9 at night, 4 by day**. Night ambient is `(0.08, 0.09, 0.14)`, day `(0.55, 0.62, 0.78)` (§11.1) |
| Sun and moon | Not exactly opposite: both sweep the camera-facing side, the moon in front of the default view, the sun slightly behind it so the scene is lit by day |
| Sphere and rock meshes | Two original bugs fixed: holes at the sphere's poles, and cracks in the rocks |
| Back-face culling | On (every mesh wound counter-clockwise from outside) |
| Draw calls | **About 800 per frame by day, 1000–1300 at night** (measured) |
| Frame rate (1280×720, vsync off) | **242 FPS** with ray tracing off; 99 with shadows; 97 with reflections; 75 with lava-light tracing as well; 71 of those at night |
| Ray tracing | `R` cycles shadows / reflections; `F` toggles the lava glow; `M` shows the light-map minimap. Gouraud is per-fragment while it is on |
| Inverse tracing emitters | Lava flows and droplets radiate in all directions from just above the lava (a flat emitter would light nothing); the crater disc is not an emitter |
| Reflections | The lake (Fresnel) and the lava (constant 0.12); the reflected scene is the proxy shapes, the ground and the sky |
| Crater | The cone's top is **open** and a crater bowl mesh forms the inside, with the glow disc as its floor (the first build had a solid lid, so there was no crater). Rock chunks landing inside the mouth rest on the floor |
| Not confirmed visually | A visible reflection of the volcano, trees or the moon in the lake |

---

## 1. One-Line Pitch

A volcano continuously ejects glowing lava fragments on true parabolic arcs while lava flows creep down its slopes and out across the plain — a scene that runs through day and night (night is the demo setting), built to put dull volcanic rock and glowing molten lava side by side under one light, to make the entire range of the Phong material model visible in a single frame.

---

## 2. Framing (Read This First — It Determines Your Grade)

Your brief says: *"The project should demonstrate concepts rather than just look visually impressive."*

This project is presented, in the report and in the viva, as:

> A demonstration of dynamically-computed projectile motion, radial light attenuation, and material-property contrast under the Phong illumination model — using a volcanic eruption as the carrier scene because it naturally produces two physically opposite materials (rough dark rock vs. glossy glowing lava) under one dominant, strongly-attenuating point light source.

Every feature below exists to demonstrate a taught concept from `L8` or `L9`. If a feature doesn't map to a row in Section 4, it doesn't get built.

The v1.2 additions are justified the same way (all four are rows in §4): the
**day/night cycle** is a directional light whose direction, colour and
intensity change continuously (L8); the **drone views** are the viewing
transformation applied to a moving, rotating object; the **controllable drone**
is a user-driven transform hierarchy (composite + hierarchical); **textures**
are a supporting feature with a switch to turn them off, so they cannot hide
the shading model. **Ray tracing** (v1.3, stretch tier) is the global-illumination counterpart to the local Phong model (L8 slides 10–11): shadows and a reflection bounce traced from the camera, and light traced from the lava.

---

## 3. Goals and Non-Goals

### 3.1 Goals
- Demonstrate all five required transformation categories, load-bearing rather than decorative.
- Implement the full `L8` Phong illumination model, including radial attenuation and two summed light sources.
- Implement all three `L9` surface rendering methods (Flat, Gouraud, Phong) as a live, switchable comparison.
- Compute all scene motion inside the render loop from `glfwGetTime()`. No baked keyframes, no animation tables. **One deliberate exception:** a user-controlled object (§9.9) is integrated each frame from live input, because its position depends on past key presses. It is not pre-recorded.
- Run an automatic day/night cycle (§9.7) that changes the sky and the directional light.
- Let the viewer look through the drones' cameras (§9.8) and fly one drone (§9.9).
- Stretch tier, after the MVP: hybrid ray tracing (shadows, one reflection bounce) and inverse ray tracing of lava light (§9.10, §9.11), switchable and always off by default.
- Ship a demonstrable vertical slice early, then layer features.

### 3.2 Explicit Non-Goals (Scope Protection)

| Rejected feature | Why |
|---|---|
| Real fluid-dynamics lava simulation | Research-grade simulation, not a simple scene |
| Deformable/erodable terrain | Real-time mesh deformation at this scale is a separate project |
| Multiple volcanoes / multiple craters | Duplicate objects, no new concept demonstrated |
| Destructible environment (falling trees, crushed huts) | Physics/collision system, zero graphics concepts gained |
| Collision detection or physics for the controllable objects | Altitude and distance are clamped; there is no collision system |
| Image-file textures, normal maps, bloom/HDR post-processing | Hides the shading model you're being graded on. (Procedural textures generated by code are allowed since v1.2, with an off switch; see §14) |
| Shadow mapping, environment/cube-map reflections | Ray tracing (§9.10) is the one route to shadows and reflections; a second technique would add cost and no new concept |
| Full path tracing, global illumination, a BVH, hardware ray tracing (DXR, Vulkan RT, OpenGL 4.3 compute) | The program is OpenGL 3.3 core. Ray tracing is hybrid, depth 1, against about a dozen analytic shapes |
| Particle-count realism (thousands of embers) | A handful of pooled, well-lit embers demonstrates the concept just as well and is far cheaper |

> **Changed in v1.3:** "Shadows" and "Reflections" are no longer blanket non-goals. The submitted proposal says *"No textures, shadows, or reflections"*, so the ray-tracing stretch tier **reverses it deliberately**, and the report must say so. Shadow *mapping* stays rejected.
>
> **Changed in v1.2:** "Full day-night cycle" was removed from this table. It
> is now a goal (§3.1), and the directional light carries it, so the scene
> still has two lights. "Textures" was narrowed to image-file textures; the
> original proposal said "no textures", so the report must state that these
> are generated procedurally with no image files or pre-made assets.

### 3.3 Scope Ceiling (Hard Limits)
- **1** volcano, **1** crater, **5** lava flows (one long river, three rivulets, one broad sheet flow), and **1** lava pool on the open plain.
- **2** light sources: the crater point light, and one directional sky light that is the moon at night and the sun by day (lighting phase; not yet wired up, see §0).
- Pooled particle counts fixed at startup: **24** lava droplets (8 before the eruption bursts), **36** tumbling rock chunks, **6** ash puffs, plus the closed-form burst plumes (22 puffs per burst, no pool).
- **3** drones: two autonomous, orbiting; one user-controlled. **Four user-controlled objects in all**: the pilot drone, a rover, a helicopter and a boat (§9.9).
- Ray tracing (stretch): a proxy scene of about 16 shapes or fewer, depth 1 (one shadow ray per light, one reflection ray), and 4096 photons per frame into a 128×128 light map.

> **Updated in v1.2:** the original "≤ 7 unique meshes / ≤ 15 draw calls" ceiling
> was sized for the volcanic core alone. The code now builds **16 GPU meshes**
> (cylinder, cone, sphere, box, disc, 4 rock variants, volcano cone, line, 3
> lightning-bolt variants) and issues **several hundred draw calls per frame**
> (an estimate from reading the code, not a measurement). The 220 stars, ~90
> forest trees and ~80 storm-cloud puffs dominate. Mesh *reuse*, the
> optimization principle §14 cares about, is preserved: no object introduces a
> one-off mesh. The old draw-call budget is replaced by a frame-rate target in
> §14.

---

## 4. Requirement Traceability Matrix

| # | Requirement | Feature that satisfies it | Verify by |
|---|---|---|---|
| 1 | Simple 3D scene | 14 meshes reused by every object; lava flows, pools of fragments, a handful of multi-part objects | Object count in report |
| 2 | OpenGL only | GLFW/GLEW + a hand-written math header only; no engine, no physics lib | Dependency list |
| 3 | Runs continuously in render loop | All state advances from `glfwGetTime()` each frame | Leave it running 5 min |
| 4 | Visible motion/animation | Lava fountain, tumbling rock, advancing and pulsing lava flows, pulsing crater, rising ash, orbiting drones, drifting clouds, day/night | Visual |
| 5 | Clear indications | HUD: shading mode, lights, terms, time of day, camera, eruption countdown, distance to the crater, controlled object, frame rate | Screenshot |
| 6a | Translation | Droplet/chunk flight, ash rise, drone orbit, piloted drone | Section 8 |
| 6b | Rotation | Rock-chunk tumble, ring placement of rim rocks, drone heading and bank | Section 8 |
| 6c | Scaling | Ash growth-fade, droplet cooling shrink, crater pulse, lava flow advance | Section 8 |
| 6d | Composite | `T(p(t))·R(spin·t)·S(size(t))` on every ejected fragment | Section 8 |
| 6e | Hierarchical | Crater → ring of rim rocks; drone body → arms → rotors → camera | Section 7 |
| 7 | Lighting maintained | 2 lights summed every frame, on every object, every mode | Visual |
| 8 | Specular mandatory | Glossy lava (`n_s`≈140) beside matte rock (`n_s`≈4) | Visual |
| 9 | Phong preferred | Per-fragment Phong is default; Flat/Gouraud are comparison modes | `1`/`2`/`3` keys |
| 10 | Concepts over looks | Material-contrast framing, attenuation demo, HUD physics readout | Demo script §16 |
| 11 | Optimized | Pooling, instancing, GPU-side flow animation, mesh reuse table | Section 14 |
| 12 | No pre-computed animation | Every formula in Section 9 is a closed form of `t`, except the user-controlled drone, which is integrated from live input | Code review |
| 13 | Time of day (v1.2) | Automatic day/night clock driving the sky and the directional light | Leave it running 2 min; `N` key |
| 14 | Multiple viewpoints (v1.2) | Views through the drones' cameras: the viewing transformation on a moving object | `C` / `V` keys |
| 15 | User interaction (v1.2) | **Four** controllable objects (drone, rover, helicopter, boat), each a user-driven transform hierarchy with an onboard camera | `Tab` + arrow keys |
| 16 | Texture (v1.2, supporting) | Procedural rock/ash/wood textures, diffuse/ambient only, with a switch | `T` key |
| 17 | Ray tracing (v1.3, stretch) | Fragment-shader shadow and reflection rays against an analytic proxy scene | `R` key; shadows swing with the clock; the lake mirrors the sky |
| 18 | Inverse ray tracing (v1.3, stretch) | Photons from the lava traced into a light map that glows on the ground | `F` and `M` keys; orange glow beside the flows at night |

### 4.1 Lecture Content Traceability

| Deck topic | Where it appears |
|---|---|
| L8: Ambient / Diffuse / Specular terms | Fragment shader, three separate summed terms |
| L8: Point light + `1/(a₀+a₁d+a₂d²)` attenuation | Crater light, the scene's dominant, strongly-attenuating source |
| L8: Directional light | The sky light: dim moonlight at night, strong sunlight by day, direction following the sun and moon (§9.7) |
| L8: Material coefficient table (slide 60) | Optional ore-vein detail on the rock face, cited verbatim |
| L8: Specular exponent `n_s` range (shiny > 100, rough ≈ 1) | Lava (`n_s`≈140) vs. rock (`n_s`≈4) in one frame |
| L8: Emission | Lava's `I_e` term; crater glow |
| L8: Multiple lights summed | Shader loops over 2 lights |
| L8: Local vs Global illumination | The raster Phong path is the local model; the ray-tracing stretch tier (§9.10, §9.11) adds shadows, one reflection bounce and light transport from the lava. Report note on the difference |
| L9: Flat surface rendering | Mode 1, `flat`/`dFdx-dFdy` derived face normal |
| L9: Gouraud (intensity interpolation) | Mode 2, lighting evaluated in the vertex shader |
| L9: Phong (normal interpolation) | Mode 3, lighting evaluated in the fragment shader |
| L9: Vertex normal averaging `N_v=ΣN_i/‖ΣN_i‖` | Mesh generator for the volcano cone, documented |
| L9: Mach banding | Mode 1 on the low-tessellation volcano cone |
| L9: "Gouraud misses highlights" | Mode 2 on the crater glow disc at low tessellation: highlight vanishes (§12.2) |
| Not in L8/L9: viewing transformation, texture mapping | Drone camera views (§9.8); procedural textures (§14). Supporting features, not taught concepts from these two decks |

---

## 5. Core Concept and Scene Description

### 5.1 Setting
A single volcanic cone rises from a rocky plain ringed by hills, a lake, a healthy forest and distant mountains, under a sky that **cycles through day and night on its own** (§9.7). The demo setting is **night**, where the volcano's own glow is the primary source of light and a faint, cool moonlight provides minimal fill so nothing is pure black. By day the sun is the dominant light and the crater glow is much less visible. The scene opens at night, and the `N` key can lock either state so the lighting demos don't depend on the clock.

### 5.2 The Story the Scene Tells
Pressure inside the volcano continuously ejects molten fragments from the crater. Each one leaves the vent, arcs under gravity, cools slightly as it flies (a subtle colour/scale shift from bright yellow to duller orange), and lands back on the slope or the plain. Meanwhile lava flows creep steadily downhill and out across the ground: one long winding river, thin braided rivulets, and a broad slow sheet. Ash rises from the vent in intermittent puffs and spreads into a wind-bent cloud, a storm gathers overhead and throws lightning, and research drones circle the volcano to watch it. A third drone can be flown by the viewer. Apart from that drone nothing is triggered by the user: this is a continuous natural process, not a single event.

### 5.3 Why This Is Meaningful
The scene visualises two things worth understanding on their own: **how a light's intensity falls off with distance** (the crater's glow lighting nearby rock brightly and distant rock barely at all — real, physical, and directly checkable by moving the camera), and **how the same lighting equation produces wildly different appearances** depending only on material coefficients (identical rock geometry looks completely different depending on whether `k_s` and `n_s` say "dull" or "molten").

---

## 6. Object Inventory

**Base reusable meshes: 16** (see `src/SceneObjects.h`'s `MeshLibrary`). Every object is assembled from transformed copies of these (the pool disc is a 16th, added in Phase 2).

| Mesh | Primitive | Used for | Instances |
|---|---|---|---|
| Volcano cone (wide base, open top) | cone (topRadius≈0.14×base, 28 segments, no top cap) | The mountain | 1 |
| Crater bowl | inverted frustum (rim 0.14, floor 0.105, depth 0.07 of the cone's height), normals inward | The inside of the crater | 1 |
| Cylinder | cylinder (16 segments) | Tree branches, windmill tower/hub, drone arms/rotor hubs/propellers, forest trunks | many |
| Cone (sharp point) | cone (topRadius=0, 16 segments) | Tree trunks, forest foliage, mountains | many |
| Unit sphere | sphere (10 × 16) | Lava droplets, ash puffs, ash cloud lobes, storm clouds, stars, moon, sky dome, hills | many |
| Unit box | box | Hut walls/roof/debris, windmill footing/nacelle/blades, drone body/arms/camera, lightning bolts, terrain base | many |
| Flat disc | disc (24 segments) | Crater glow surface, terrain discs, lake | several |
| Irregular rock (×4 seeded variants) | jittered low-res sphere | Rim rocks, tumbling rock chunks, burnt-tree/hut debris | 12 (ring) + 36 (pool) + ~12 (debris) |
| Line segment | `GL_LINES` | Building block for the bolt variants | — |
| Lightning bolt (×3 seeded variants) | fractal midpoint-displacement polyline | Bolt shapes; drawn as thin boxes along the polyline | 2 per strike |

(The old ground-plane mesh and the wind-streak mesh no longer exist as separate meshes.)

**Total drawn objects per frame:** several hundred draw calls (an estimate from reading the code; measure it). The scenery dominates: 220 stars, up to ~90 forest trees at 3 parts each, ~80 storm-cloud puffs. The multi-part scene objects are small by comparison (each burnt tree ≈ 7 draws, the hut ≈ 10, the windmill ≈ 6, each drone ≈ 13). See §3.3 and §14.

### 6.1 Object table

| # | Object | Category | Real-World Purpose | OpenGL Geometry | Placement | Transform Used | Animation | Status |
|---|---|---|---|---|---|---|---|---|
| 9 | Burnt Tree (×6) | Damage/Aftermath | Heat/ashfall-killed vegetation | Cone trunk + cylinder branches + rock debris | Radius 29–40, around the lower slope | Translation, Rotation (lean + facing), Scaling | None (wind-sway optional) | Implemented |
| 10 | Burnt/Damaged Hut | Settlement/Damage | Rural dwelling damaged by eruption aftermath | Boxes (walls, 2 roof panels) + rock debris | Radius 40, angle 205° | Translation, Rotation, Scaling | No | Implemented |
| 11 | Windmill | Settlement | Homestead pairing with the hut | Cylinder tower + box hub + 4 box blades | Radius 47, angle 212° | Translation, Rotation | None (blade spin optional) | Implemented |
| 12 | Ash/Smoke Cloud | Volcanic/Weather | Eruption column spreading into a wind-bent umbrella | Stacked + clustered spheres, offset along wind vector | Above the crater | Translation, Scaling | Lobes bob | Implemented |
| 13 | Volcanic Lightning | Weather | Static discharge in a storm | Fractal-displacement bolts drawn as thin boxes | From the storm layer (y = 62) to the ground, one strike every 2.2 s | Translation, Rotation, Scaling | Yes (strike schedule + sky flash) | Implemented |
| 14 | Wind Indicators | Weather (diagram) | Show wind direction | Curved line strips | — | — | — | **Removed** |
| 15 | Research Drones (×2) | Monitoring | Close-range volcanic monitoring | Box body + box arms + cylinder hubs + translucent propeller discs + box camera | Orbit at radius 30 / 38, height 16 / 20, opposite directions | Translation, Rotation | Yes (orbit + bob) | Implemented. Heading and camera views planned (§9.8) |
| 16 | Aircraft | Monitoring (optional) | Considered; the drones already cover the role | — | — | — | — | **Not implemented** |
| 17 | Storm Clouds | Weather | Source of lightning; part of the setting | Two layers of flattened spheres (46 + 34) | y ≈ 62, drifting along `windDir` | Translation, Scaling | Yes (drift + flash) | Implemented |
| 18 | Sky, stars, moon | Setting | The sky | Large sphere (gradient shader), 220 small spheres, a moon sphere + halo | Centred on the camera | Translation, Scaling | Yes (day/night) | Implemented. The moon and sun follow the clock (§9.7) |
| 19 | Terrain, lake, hills, forest, mountains | Setting | A place for the volcano | Boxes, discs, flattened spheres, cones, cylinders | Radius ~78 to 540 | Translation, Scaling | No | Implemented |
| 20 | Lava Flows (×5) | Volcanic | Lava creeping down the slopes | Strips following a meandering path over the cone surface | Azimuth 20°, 72°, 98°, 124°, 322° | Translation, Scaling | Yes (advance + pulse) | Implemented |
| 21 | Pilot Drone | Controllable | A drone the viewer flies | Same model as the research drones | Starts hovering near (17, 14, 29) | Translation, Rotation (heading, lean), hierarchical parts | Yes (user input) | Implemented |
| 22 | Rover | Controllable | Ground research vehicle | Box chassis + cabin + 4 spinning cylinder wheels + sensor mast | Starts at (-28, 0.1, -2); kept outside the cone | Translation, Rotation (wheels spin, body leans) | Yes (user input) | Implemented |
| 25 | Helicopter + helipad | Controllable | Aerial survey and rescue | Ellipsoid body, canopy, tail boom, skids, spinning main and tail rotors; a disc pad with an H | Pad at (5.9, 0, -33.5) | Translation, Rotation (heading, lean, rotors) | Yes (user input) | Implemented |
| 26 | Boat | Controllable | Research boat on the lake | Hull boxes, cabin, mast, rotating radar bar, wake strips | On the lake, kept inside its ellipse | Translation, Rotation (heading, waves, radar) | Yes (user input) | Implemented |
| 27 | Eruption smoke | Volcanic | Burst plumes from the vent | 22 sphere puffs per burst + two fireball shells | At the crater | Translation, Scaling | Yes (burst schedule, §9.12) | Implemented |
| 28 | Shockwave | Volcanic | The pressure wave of a burst | Two expanding flat rings | Ground, from the cone's base outward | Scaling | Yes (§9.13) | Implemented |
| 30 | Volcanic bombs, lake splash | Volcanic | Glowing rocks with trails; long-range bombs with a splash, ripples and steam at the lake | Rocks, spheres, rings | Flight paths solved to land on the lake | Translation, Rotation, Scaling | Yes (§9.13) | Implemented |
| 23 | Sun | Setting | Daytime light source | Sphere disc at `sunDir × 800` | Orbits with the clock | Translation | Yes (day/night) | Implemented |
| 24 | Lava Pool | Volcanic | The basin the sheet flow pours into | Flat discs (rebuildable tessellation) + 10 drifting crust plates | End of the sheet flow, about `(52, 0.1, -41)` | Scaling (fills to radius 7.5 once the flow arrives), Rotation (plates) | Yes | Implemented (re-enabled, §0.6) |

Full column detail (exact positions, notes) is maintained in
`object_inventory.xlsx`, generated by `scripts/generate_inventory.py`. It was
last generated before the Sep 30 code changes, so **regenerate it after
updating the script's object list** (the script still lists the removed wind
indicators).

---

## 7. Scene Graph (Hierarchy Specification)

```
WORLD
├── SKY LIGHT (directional, no geometry)  <- moonlight at night, sunlight by day (§9.7)
├── Sky dome, stars, moon, sun                      (centred on the active camera)
├── Terrain: base box, ash discs, lake, hills, forest, mountains
├── VOLCANO                                          [volcano cone mesh]
│   └── CRATER (fixed point at the cone's apex)
│       ├── Crater glow disc            [disc, emissive, pulsing scale]
│       ├── Crater light (point light, attenuated) — Light 0
│       └── RIM ROCK RING (children of the crater; 12 rocks)
│           └── rock_i = T(crater) · R_y(i · 360°/N) · T(radius,0,0) · S(size_i)
│               (plain loop now; one instanced draw call is the target)
│
├── LAVA FLOWS (×5)                                  (travelling pulse; advance along the path)
│
├── PARTICLE POOLS (world-space, spawned near the crater)
│   ├── Lava droplet pool   [sphere ×8]   — parabolic flight + cooling shrink
│   ├── Rock chunk pool     [rock ×36]    — parabolic flight + tumble
│   └── Ash puff pool       [sphere ×6]   — rise + grow + fade
│
├── AFTERMATH: burnt trees ×6, burnt hut, windmill
├── WEATHER: ash cloud, storm clouds, lightning
├── RAY-TRACING PROXY SCENE (never drawn; rebuilt each frame): volcano cone, 6 tree cones, hut box,
│   windmill cylinder, 3 drone spheres, ground plane, lake (§9.10)
└── DRONES
    ├── Research drone 1, 2        (autonomous orbit)
    └── Pilot drone                (user-controlled)
        └── body  T · R_y(yaw) · R_x(lean) · R_z(bank)
            ├── arm_i            (child of body)
            │   └── rotor hub_i  (child of arm)
            │       └── propeller_i   R_y(ω·t), child of hub
            └── camera gimbal    (child of body; its eye is the drone view, §9.8)
```

**Critical hierarchy property:** the rim rocks are genuine children of the crater — each one's transform is `T(crater_pos) · R_y(angle_i) · T(radius,0,0) · S(size_i)`, so if the crater's position ever changes (e.g., as an optional camera-shake or "breathing" effect), every rim rock inherits it without any per-rock code change. They are drawn in a plain loop today; the single instanced call (§14) is the target. The drone is the second hierarchy: moving or turning the body carries every arm, rotor, propeller and the camera with it.

---

## 8. Model Transformation Specification

| Category | Object | Transform | Purpose |
|---|---|---|---|
| **Translation** | Lava droplet | `T(p(t))` from ballistic equation | Fountain arc |
| | Rock chunk | `T(p(t))` from ballistic equation | Ejected debris |
| | Ash puff | `T(origin + v·age)` | Rising smoke |
| | Research drone | `T(r·cos a, h + bob, r·sin a)`, `a = a₀ + speed·t` | Orbit around the volcano |
| | Pilot drone | `T(pos)`, integrated from input | User flight |
| | Sun / moon | `T(eye + dir(t) · 800)` | Day/night motion |
| **Rotation** | Rock chunk | `R(axis, ω_spin · age)` | Tumble while airborne |
| | Rim rocks | `R_y(i · 360°/N)` | Placement around the crater ring |
| | Drone | `R_y(yaw)` (heading), plus lean/bank on the pilot drone | Faces where it flies |
| | Drone propellers, rover wheels | `R(ω·t)` relative to the parent | Spinning child parts |
| **Scaling** | Lava droplet | `S(s₀ · (1 − k·age))` | Cooling/shrinking as it flies |
| | Ash puff | `S(s₀ + k·age)`, alpha `1 − age/life` | Expansion + fade |
| | Lava flow | advance `min(1, 0.25 + k·t)` of its path | Slow, saturating growth over time |
| | Crater glow disc | `S(1 + 0.08·sin(3t))` | Breathing pulse |
| **Composite** | Ejected fragment | `T(p(t)) · R(spin·t) · S(size(t))` | Full TRS per fragment |
| | Rim rock | `T(crater) · R_y(angle) · T(radius,0,0) · S(size)` | Ring placement |
| | Pilot drone | `T(pos) · R_y(yaw) · R_x(lean) · R_z(bank)` | User-driven TRS |
| **Hierarchical** | Rim rocks ← crater | 2-level parent/child | Crater changes propagate to all rim rocks |
| | Drone parts ← body | body → arm → hub → propeller; body → camera | Moving the body moves every part |
| **Viewing** | Drone camera | `lookAt(gimbalEye, target, up)` rebuilt each frame | The view from a moving object (§9.8) |

---

## 9. Motion Specification — What Moves, How, Why

Every formula is a closed-form function of time or of an `age` counter that only increments; nothing is a stored keyframe. The one exception is the piloted drone (§9.9), which is integrated from live input.

### 9.1 Lava Fountain (Droplets)
**What:** up to 8 pooled droplets.
**How:** every ~0.2 s (a fixed spawn interval, checked each frame against elapsed time), the next inactive pool slot is activated at the crater with an initial velocity drawn from a small fixed set of angles/speeds (deterministic — indexed by spawn count, not stored randomness):
```
p(τ) = p₀ + v₀·τ + ½·g·τ²        τ = now − spawnTime
scale(τ) = s₀ · max(0.3, 1 − 0.15·τ)      // cools and shrinks slightly
color(τ) = lerp(hotYellow, coolerOrange, clamp(τ/1.5, 0, 1))
```
When `p.y` falls back to the slope or ground height, the slot deactivates and becomes available again.
**Why:** magmatic pressure ejects molten rock; gravity brings it back down; it visibly cools in flight.

### 9.2 Rock Chunks
**What:** up to 36 pooled larger fragments.
**How:** same ballistic equation as 9.1, plus continuous tumble: `angle(τ) = ω_spin · τ` about a fixed per-chunk axis.
**Why:** solid debris is thrown out alongside the lava; unlike lava it doesn't glow or cool, it just tumbles — a deliberate visual contrast to the droplets.

### 9.3 Lava Flows
**What:** five flows leaving the crater and running down and across the plain: a long winding river (azimuth 20°, reaching radius 120), three braided rivulets that taper to points (72°, 98°, 124°), and a broad sheet flow with a lobed front (322°).
**How (implemented):** each flow is a strip following a meandering path (`azimuth + amplitude · sin(frequency · u + phase)`) over the cone surface, with width tapering from `widthStart` to `widthEnd`. The visible length grows with time (§9.4), and a bright pulse travels down it, computed on the CPU as a colour term:
```
heat(u, t) = 0.10 · pulse + front        pulse = 0.5 + 0.5 · sin(26·u − 3·t)      u = position along the path
```
**Target (lighting phase):** move the pulse into a GPU-side emission term, `glow(s, t) = 0.4 + 0.6 · pulse(fract(s/L − flowSpeed·t))`, so brightness surges appear to travel downhill with zero CPU cost.
**Why:** suggests continuous flow without simulating fluid, a cheap and honest stand-in for it (state this in the report).

### 9.4 Lava Flow Advance
**What:** how far each flow has crept down its path.
**How:** `advance(t) = min(1, 0.25 + growSpeed · t)`, the fraction of the path covered. It grows steadily, then saturates once the flow reaches its end. `growSpeed` differs per flow (0.020 to 0.050 per second).
**Why:** lava keeps advancing until it cools and stops. This is the saturating-growth scaling that the old base-pool growth (`scale = min(S_max, k·t)`) provided. `drawLavaPool` still exists in the code but is not drawn; if the pool is re-enabled it grows by that formula.

### 9.5 Crater Glow Pulse
**What:** the glowing disc inside the crater.
**How:** `scale(t) = 1 + 0.08·sin(3t)`, `emission(t) = I_e · (0.85 + 0.15·sin(3t))`.
**Why:** represents the roiling, unsteady surface of the magma near the vent; also keeps the crater light's implied source visibly "alive."

### 9.6 Ash Cloud
**What:** up to 6 pooled puffs.
**How:** identical stateless recycling pattern to 9.1 — `age` increments, `position = origin + (0,1,0)·rise·age`, `scale = s₀ + k·age`, `alpha = 1 − age/life`; when `age > life`, deactivate and respawn from the crater with a small time offset so puffs don't all appear synchronised.
**Why:** gas and pulverised rock rise with the eruption; a slow, continuous visual cue that the volcano is active even between fountain bursts.

### 9.7 Day/Night Cycle
**What:** a 24-hour clock that advances automatically; the sky, stars, sun, moon, storm clouds and the directional light all follow it.
**How:** a closed-form function of simulated time (it freezes with `P`):
```
hour(t)  = fmod(startHour + 24 · t / dayLength, 24)         dayLength = 120 s, startHour = 20
theta    = 2π · (hour − 6) / 24          sunElev = sin(theta)
sunDir   = normalize(cos theta, sin theta, 0.3)               moonDir = −sunDir
daylight = smoothstep(−0.12, 0.25, sunElev)                   twilight = clamp(1 − |sunElev|/0.25, 0, 1)
```
`N` cycles Auto → lock Day → lock Night; the locks ease toward their target over about 1.5 s. Sky colours, star alpha, cloud shade and the lighting values are blends between a night and a day setting by `daylight`, with a warm tint by `twilight` (values in `IMPLEMENTATION_GUIDE.md` §10.1 and §11.1 below).
**Why:** continuously changing a directional light's direction, colour and intensity is the L8 directional-light content in motion, and it needs no pre-recorded animation.

### 9.8 Drone Orbit and Drone Views
**What:** two research drones circle the volcano; the viewer can look through either (or the pilot drone's) camera.
**How:** drone `i` is at `(r·cos a, h + 0.8·sin(0.5t + i), r·sin a)` with `r = 30 + 8·(i mod 4)`, `a = 2π·i/N + speed·t`, `speed = ±(0.10 + 0.03·(i mod 3))` (alternating sign), `h = 16 + 4·(i mod 5)`. Its heading is the orbit tangent, `yaw = atan2(vel.x, vel.z)`. The drone view is `lookAt(eye, target, up)` with `eye` at the gimbal (just below and ahead of the body) and `target` either the crater (default) or a point along the heading tilted down 35° (key `V`), with a 70° field of view. The drone you are looking through is not drawn.
**Why:** the view matrix is rebuilt every frame from a moving, rotating object; this is the viewing transformation applied to a hierarchy.

### 9.9 Controllable Objects
**What:** four objects the viewer controls: the pilot drone, a rover, a helicopter and a boat. `Tab` cycles which one the keyboard drives; the others come to rest.
**How:** simple kinematics integrated each frame with `dt`: speed eases toward the input (`speed += (input − drag·speed)·accel·dt`), position advances along `R_y(yaw)·(0, climb, speed)`, and the body leans and banks into the motion. Altitude is clamped between a floor (the larger of the ground and the cone surface plus a margin) and 55, below the storm layer, and the distance from the volcano is clamped. These are limits, not a collision system (§3.2). `Tab` takes and releases control; on release the drone hovers where it was.
**As built:** the rover steers only while moving (reversing mirrors it), its wheels turn by `distance / radius`, and it **collides** with trees, rocks, the hut, the windmill, the parked helicopter, the volcano and the lake (circle colliders; a hit bounces it back with restitution 0.35, dives the nose, hops the body and jolts the view); the helicopter can move only once airborne and climbs only once the rotor is at speed; the boat stays inside the lake's ellipse and bobs on the waves. Each has an onboard camera.
**Why:** each is a user-driven composite and hierarchical transform. They are the objects whose motion is not a closed form of time, which §3.1 states.

### 9.10 Ray Tracing (stretch tier)
**What:** hard shadows and one reflection bounce, traced from the camera's fragments.
**How:** in the fragment shader, against a **proxy scene** of closed-form shapes built each frame from the scene's own placement data: the volcano (truncated cone, base 19, top 2.66, height 27), six tree cones, the hut (oriented box), the windmill tower (cylinder), three drone spheres, the ground plane, and the lake. The crater light is not blocked by the volcano proxy, because it sits inside the cone's mouth.
```
shadow ray:   origin P + 0.03·N, direction L (sun/moon constant; crater = toward craterPos); any hit -> that light's diffuse + specular x 0
reflection:   R = reflect(-V, N); nearest proxy hit shaded cheaply, or the analytic sky colour sky(R) on a miss
lake:         F = F0 + (1 - F0)(1 - N.V)^5, F0 = 0.02;    lava: constant 0.12;    final = mix(lit, reflected, F)
```
`R` cycles Off, Shadows, Shadows + Reflections. When on, Gouraud is evaluated per fragment (it looks like Phong), so the shading demos run with it off.
**Why:** ray tracing is how the shadows and inter-reflection that the local model leaves out are produced. The sun-lit day shows shadows best; at night both lights are dim, so shadows are subtle, while the lake still mirrors the moon and sky.

### 9.11 Inverse Ray Tracing of Lava Light (stretch tier)
**What:** the lava lights its surroundings. Rays are fired from the light sources, the reverse of camera rays.
**How:** every frame, on the CPU, 4096 photons are emitted from the five lava flows (16 sample points each along the advanced path, cosine-weighted hemisphere, power `width x (0.4 + heat)`) and the eight droplets (uniform over the sphere, power by brightness x scale). They are traced through the same proxy scene. A photon that hits the ground, lake or volcano slope is deposited into a 128x128 top-down **light map** (±160 units, `RGB16F` texture):
```
deposit(cell) += photonPower . n_y / A_cell        map = (1 - a)·map + a·thisFrame,  a ~ 0.15
fragment:  colour += albedo . texel . texture(lightMap, (P.xz - origin)/size + 0.5) . gain
```
A photon that hits a tree, hut, windmill or drone is absorbed, which leaves shadow-shaped gaps. The crater disc is not an emitter (its rays leave upward). Directions come from a hash of `(frame x N + i)`, so there is no stored random state. `F` toggles it; `M` shows the map as a minimap.
**Why:** light density falls off as 1/d² on its own, so the attenuation falls out of the geometry. It lights the ground from the lava, which the Phong model does not.

### 9.12 Eruption Bursts
**What:** the volcano erupts in bursts instead of smouldering evenly.
**How:** a closed-form schedule (`src/Eruption.h`): burst `b` starts at `t_b = 1.5 + 9·b` s, with `envelope(τ) = τ/0.35` up to the peak and `exp(−(τ−0.35)/2.2)` after it; every third burst is 30% bigger. Each burst throws 22 smoke puffs up from the vent (direction within ~25° of vertical, speed 14–26, drag `e^{−0.75τ}`, buoyancy and wind drift, swelling to radius ~5.6, cooling from glowing orange to grey, fading over 10 s) behind a two-shell fireball flash. The ash umbrella swells and climbs after a burst and thins before the next, the crater light is multiplied by `1 + 2.5·intensity`, and the droplet and rock fountains spawn up to 4× faster and harder.
**Why:** a visible, repeating eruption makes the scene read as an eruption rather than a static smoke column, and it is entirely a function of time (no stored animation).

### 9.13 Eruption Effects
**What:** additions to the eruption bursts of §9.12: a shockwave with camera shake, glowing bombs with trails, a trigger key, and a splash with ripples where bombs land in the lake.
**How:** all hang off one list of burst start times (scheduled every 9 s, plus any fired with `E`), and every effect is a closed-form function of time and that list.
- *Shockwave:* two rings at `19 + 52τ`, thickness 14% of the radius, fading over 3.2 s. *Camera shake:* a view rotation of up to 0.011·amp rad decaying as `e^{−τ/0.55}`.
- *Bombs:* the larger rock chunks, white-hot (`e^{−age/2.2}`) with a trail made of their own earlier ballistic positions.
- *Lake bombs and splash* (big bursts): the lake is 143 units away and ordinary ejecta reach about 60, so two bombs per big burst are thrown at the lake with the launch speed solved in closed form (`T = √(2(h₀ − h_w + D tan θ)/g)`, `v = D/(T cos θ)`). Each lands with a flash, four ripples, a spray of droplets (each making its own ripple) and steam.
- *`E` key:* adds a big burst start time at the current time; the schedule stays closed-form.
**Why:** a visible, repeating and interactive eruption, built entirely from time, shows the scene's transformations and scaling in motion with no stored animation.

---

## 10. Interaction and On-Screen Indications

| Key | Action | Why it earns marks |
|---|---|---|
| `1` / `2` / `3` | Flat / Gouraud / Phong shading | **The L9 demonstration** |
| `+` / `−` | Increase/decrease cone & crater-disc tessellation | Shows highlight quality vs. polygon count |
| `L` | Cycle: both lights / crater only / sky light only | Isolates each light's contribution |
| `K` | Cycle: ambient only / +diffuse / +specular / full | Recreates L8 slide 54 live |
| `B` | Toggle Phong `(R·V)^n` ↔ Blinn-Phong `(N·H)^n` | L8 slide 49–50 bonus |
| `T` | Procedural textures on / off | Keeps the shading model demonstrable |
| `N` | Time of day: Auto → lock Day → lock Night | Demos need a dark scene; the clock runs on its own otherwise |
| `C` | Camera: free orbit → Drone 1 → Drone 2 → Pilot drone → Rover → Helicopter → Boat | Viewing transformation on a moving object |
| `V` | Drone-camera aim: crater lock ↔ forward | Two views from the same camera |
| `Tab` | Cycle the controlled object: none → pilot drone → rover → helicopter → boat | Four user-driven transform hierarchies |
| `E` | Fire a big eruption burst now | Interaction with the closed-form eruption |
| `J` | Camera shake on / off | |
| `↑` / `↓`, `←` / `→`, `Space` / `Shift` | While controlling an object: forward/back, turn, up/down (arrow keys, so no letter key is needed) | |
| `R` | Ray tracing: Off / Shadows / Shadows + Reflections (stretch) | Global effects on demand |
| `F` | Inverse (light-ray) tracing of lava glow on / off (stretch) | Light traced from the source |
| `M` | Light-map minimap (stretch) | Evidence for the report |
| `P` | Pause time (freeze `t`, including the day/night clock) | Lets grader study a single frame |
| Arrow keys (when nothing is controlled), `Z` / `X`, `Home` / `End`, left mouse drag; mouse wheel / `PgUp` / `PgDn` | Camera orbit, zoom (the orbit follows the controlled object) | Lets grader walk toward/away from the crater to see attenuation change |

Camera zoom moved off `+`/`−` (which are needed for tessellation) and
the arrow keys drive the controlled object. Toggle keys are handled by a key-press
callback so a tap changes a mode exactly once.

### HUD Text (top-left, always visible)
```
SHADING: Phong (per fragment)      [1/2/3]
LIGHTS:  Crater + sky light        [L]
TERMS:   Ambient+Diffuse+Specular  [K]
SPECULAR: Phong (R.V)              [B]
CONE SEGMENTS: 28                  [+/-]
TEXTURES: On                       [T]
RAY TRACING: Off                   [R]
LAVA LIGHT TRACING: Off            [F]
TIME: 02:30 NIGHT (auto)           [N]
CAMERA: Drone 2 (crater lock)      [C/V]
---
ERUPTION: NEXT BURST IN 5.2 S      [E] fire one
DISTANCE TO CRATER: 18.4 M         (from the active camera)
CONTROL: ROVER  SPD 7.8  BUMPS 1 (WINDMILL)  [TAB]
FPS: 60   DRAW CALLS: 1205
```
Each line carries the key that changes it, and a two-line legend along the bottom of the window lists all the keys. (An earlier
version also showed the number of active droplets and ash puffs and the lava river's advance; they were removed as clutter.)
The distance readout is the deliberate, checkable evidence for attenuation: as the camera moves away, the HUD number climbs while the rock visibly darkens.

---

## 11. Lighting Design (per `L8_Illumination.pptx`)

### 11.1 Light Sources — 2 total, summed

**Light 0 — Crater Glow (Point, dominant)**
- Type: positional, at the crater.
- **Radial attenuation:** `f(d) = 1/(a₀ + a₁·d + a₂·d²)` with `a₀=1.0, a₁=0.06, a₂=0.02` — tuned so nearby rock is bright and rock more than ~25 units away is nearly unlit by it.
- `I_diffuse = (1.0, 0.45, 0.15)` warm orange
- `I_specular = (1.0, 0.6, 0.3)`

**Light 1 — Sky Light (Directional; moon at night, sun by day)**
- Type: directional, no attenuation (L8 slide 19). Its direction, colour and intensity follow the day/night clock (§9.7), blended by `daylight`.
- Night (moonlight): `L = −normalize((−0.3, −0.6, 0.4))`, `I_diffuse = (0.12, 0.14, 0.20)` cool and dim, `I_specular = (0.15, 0.16, 0.18)`.
- Day (sunlight): `L` from `sunDir`, `I_diffuse = (1.00, 0.95, 0.82)`, `I_specular = (0.60, 0.60, 0.55)`.
- Blend the direction too, as `normalize(sunDir · daylight + moonDir · (1 − daylight) · 0.5)`, so it never snaps across the sky when the sun and moon swap. At sunrise and sunset, tint the colour toward `(0.95, 0.50, 0.25)` by `twilight`.

**Global ambient (L8 slide 57):** the specified night value is `(0.04, 0.04, 0.06)`, very low, because the crater does the real work of lighting anything nearby; day `(0.28, 0.32, 0.40)`; blended by `daylight`. **As built:** night `(0.08, 0.09, 0.14)` and day `(0.55, 0.62, 0.78)`, with an exposure of 9 at night and 4 by day (§0.6). The scene has exactly two lights at every moment.

### 11.2 Material Properties

| Object | `k_a` | `k_d` | `k_s` | `n_s` | Notes |
|---|---|---|---|---|---|
| **Volcanic rock** | (0.05, 0.04, 0.04) | (0.25, 0.20, 0.18) | (0.05, 0.05, 0.05) | **4** | Rough, near-Lambertian — L8's "rough surface" case |
| **Rim rocks** | same as rock | same | same | 4 | Reuses the rock material |
| **Molten lava (droplets/flows)** | (0.30, 0.05, 0.00) | (0.90, 0.25, 0.05) | (0.95, 0.85, 0.60) | **140** | Wet, glassy — L8's "shiny surface" case; `I_e = (1.0, 0.4, 0.05)` emission, scaled by cooling factor from §9.1 |
| **Ash** | (0.08, 0.08, 0.09) | (0.35, 0.34, 0.32) | (0.05, 0.05, 0.05) | 2 | Near-Lambertian grey |
| **(Optional) ore vein detail** | (0.231, 0.231, 0.231) | (0.278, 0.278, 0.278) | (0.774, 0.774, 0.774) | **89.6** | L8 slide 60 "Polished Silver," cited verbatim, for a small embedded mineral highlight on the rock face |

**Everything else.** The table covers the three materials the demo is about. For the rest of the scene (trees, hut, windmill, drones, terrain, hills, mountains): lit and matte, `k_d` = the object's existing colour, `k_a` = 0.2 × that colour, `k_s` = 0.05, `n_s` = 4. Sky dome, stars, sun, moon, lightning and storm clouds are unlit and keep their flat colour. The crater glow disc and lava are emissive. Ash puffs and the ash cloud are lit and keep their alpha.

**Design intent:** the frame deliberately spans `n_s` = **2 → 140**, a 70× range, on objects standing directly next to each other (rock beside lava beside ash) — reproducing L8 slide 46's shininess comparison with a real scene instead of test spheres.

### 11.3 The Illumination Equation (implemented in GLSL)

```
I = I_e                                             // emission (L8 s55) — lava only
  + k_a · I_a_global                                // ambient  (L8 s26)
  + Σ_lights  f_att(d) · [ k_d · I_d · max(0, N·L)  // diffuse  (L8 s32)
                         + k_s · I_s · max(0, R·V)^n_s ]  // specular (L8 s44)

where R = reflect(−L, N) = 2(N·L)N − L              (L8 s47-48)
   or, in Blinn-Phong mode:  (N·H)^n_s, H = normalize(L+V)   (L8 s49)
   and f_att = 1/(a₀ + a₁d + a₂d²) for the crater light, 1.0 for moonlight
```

With the ray-tracing stretch tier on, each light's bracketed diffuse + specular term is multiplied by a shadow factor `S ∈ {0, 1}` (§9.10), and a traced-glow term from the lava (§9.11) is added to the diffuse side.

---

## 12. Shading Model Design (per `L9_shading.pptx`)

### 12.1 The Three Modes — One Shader Program

Single shader program, `uniform int uShadingMode`, exactly as in the L9 slides.

| Mode | Key | Implementation | L9 reference |
|---|---|---|---|
| **0 — Flat** | `1` | Face normal via `normalize(cross(dFdx(FragPos), dFdy(FragPos)))`, lit once per face | Slides 9–16 |
| **1 — Gouraud** | `2` | Vertex shader evaluates full illumination, outputs interpolated colour | Slides 17–29 |
| **2 — Phong** | `3` | Vertex shader outputs interpolated normal; fragment shader lights per-pixel | Slides 30–37 |

### 12.2 The Money Shot — Two Guaranteed Demonstrations

**Demo A — "Gouraud misses the highlight" on the lava pool.**
The pool is a flat, high-`n_s` (140) lava surface. At low tessellation (`−` a few times, which rebuilds the pool disc with fewer segments), with the camera near the sun's mirror angle, **Gouraud** mode shows the specular highlight as a diamond of linear interpolation between the vertices, or loses it between them. **Phong** mode, on identical geometry, keeps a smooth, bright glint. This is L9 slide 28 reproduced on your own scene. The highlight is a *point* on the plane, so orbit until it lands on the pool; by day it is brightest. (The crater glow disc was the first choice, but its light lies in its plane, so no highlight forms on it.)

**Demo B — Mach banding on the volcano cone.**
Set the cone to low segment count (`−`) and switch to **Flat**. The facets and perceived brightness bands are exactly L9 slides 13–15's description. Photograph it for your report.

### 12.3 Vertex Normal Averaging

The cone and irregular rock-chunk meshes use the L9 slide 20 formula at generation time:
```
N_v = (Σ N_i) / ‖Σ N_i‖
```
Document this function in your report (see the Broadside-style implementation pattern: accumulate face normals into each shared vertex, then normalize).

---

## 13. Specular Reflection Design (Requirement 8 — Mandatory)

| Surface | Effect | Why it's noticeable |
|---|---|---|
| **Lava droplets/flows** | Bright, wet-looking glints (`n_s`=140) | The scene's dominant specular source; visible from any angle near the crater |
| **Rock** | Almost none (`n_s`=4, `k_s`=0.05) | Deliberate contrast case — proves what "rough" looks like next to "glassy" |
| **Ash** | None | Contrast case — proves near-Lambertian appearance |
| **(Optional) ore vein** | Very tight silver pinpoint (`n_s`=89.6) | A third, distinct `n_s` value for comparison if added |

**Guaranteed visibility check:** at any camera position near the volcano, the glowing lava's highlight is on screen; orbiting to see the rock beside it in the same frame is a one-second camera move, making the contrast trivial to demonstrate live.

---

## 14. Optimization Strategy (Requirement 11)

| Technique | Implementation | Saving |
|---|---|---|
| **Mesh reuse** | 16 GPU meshes (the 14 plus the pool disc and the crater bowl) serve every object plus all pooled fragments | Minimal VBO count |
| **Single shader program** | One program, `uShadingMode` branch | No state-change cost on mode switch |
| **GPU-side flow animation** (not built) | Lava-flow glow computed per-fragment/vertex from `s` and `t`; today it is a CPU-computed colour pulse | Zero CPU cost, zero buffer uploads |
| **Instanced rim rocks** (not built) | One `glDrawArraysInstanced` call, per-instance angle/size from `gl_InstanceID`; today a plain loop of 12 | One draw call regardless of ring density |
| **Stateless particle pools** | Fixed-size arrays (8 droplets, 36 chunks, 6 puffs); all fields pure functions of `age` | Zero allocation after startup |
| **Conditional draws** | Inactive pool slots are skipped entirely; the lightning is drawn only during a strike | Fewer draw calls most frames |
| **Back-face culling + depth test** | `glEnable(GL_CULL_FACE)`; every generator now emits counter-clockwise-outward triangles and the sky dome turns culling off locally | Standard fragment/overdraw savings |
| **Uniform hoisting** | View/projection/light uniforms set once per frame | Fewer `glUniform` calls |
| **Tessellation on demand** | Cone/disc meshes rebuilt only on `+`/`−` keypress | No per-frame geometry regeneration |
| **Normal matrix on CPU** | `(M⁻¹)ᵀ` computed once per object per frame | Avoids per-vertex inverse |
| **Ray tracing kept small** (stretch) | Analytic proxy scene of about a dozen shapes, depth 1, early-outs (`N·L ≤ 0`, distance cut-off, off by default); photon tracing on the CPU at 4096 photons | Tens of millions of tests per frame at most; shadows-only is the cheaper mode |
| **Procedural textures only** | Three 256×256 textures and a font atlas generated by code once at startup; no image files, no loader library | No asset loading; textures can be switched off with `T` |
| **No post-processing** | Deliberate omission | Keeps focus on the shading model being graded |
| **Scenery batching** (if needed) | Stars and the healthy forest are the cheapest to batch or instance | Cuts the largest share of draw calls |

**Performance budget.** Target 60 FPS at 1280×720. The earlier "≤ 16 draw calls, ≤ 20k triangles" budget assumed no environment layer and no longer applies. **Measured on the lab machine** (vsync off): about 800 draw calls per frame by day (1000–1300 at night) and **242 FPS** with ray tracing off, 99 with shadows, 97 with reflections, 75 with the lava-light tracing as well. The 60 FPS target is met with every feature on. It was not measured on integrated graphics. A second render pass (a picture-in-picture drone feed) would double the cost, so it is optional and not built.

**Report note:** state explicitly that the raster path is a **local illumination** model (L8 slides 10–11) — the crater's glow does not bounce onto surrounding rock via inter-reflection, and there are no shadows cast by the ejected fragments. That is the correct, intentional limitation of a raster Phong pipeline, not an oversight. The optional ray-tracing mode adds shadows, one reflection bounce and lava light transport on top, and **reverses the proposal's "no shadows or reflections"**; say so. State also that the textures are generated by code with no image files, and that the piloted drone is the only object not driven by a closed form of time.

---

## 15. Risks and Mitigations

| Risk | Severity | Mitigation |
|---|---|---|
| Scope creep toward a "real" lava simulation | **High** | Section 3.2 is a contract; the flow channel's glow is explicitly a stand-in, not a fluid sim |
| Attenuation constants tuned wrong (whole scene too dark or too flat/bright everywhere) | Medium | Tune `a₀,a₁,a₂` with the HUD distance readout visible; adjust until nearby rock is clearly brighter than rock 20+ units away |
| Gouraud-vs-Phong demo doesn't show a visible difference | Medium | Requires **low tessellation + high `n_s`** on a large-enough surface (the crater glow disc or the sheet flow, not a droplet) — verify this specific combination early; fall back to re-enabling `drawLavaPool` |
| Lava flows advance too fast or too slowly | Low | `growSpeed` is a per-flow constant; test at 4× normal speed first, then slow it down |
| Rim-rock instancing bug (all rocks in the same spot) | Medium | Verify `gl_InstanceID`-derived angle/radius on a **non-instanced** loop first, then convert to instanced draw once positions are confirmed correct |
| Day/night makes the lighting demos hard to see (the crater glow is weak against the sun) | Medium | Run the demos on the `N` night lock; the scene opens at night |
| Several hundred draw calls miss 60 FPS once lighting is added | Medium | Measure first; batch stars and forest; keep the picture-in-picture feed optional |
| A drone camera sits inside its own body, or the sky dome follows the wrong eye | Medium | Hide the drone being looked through; pass the active camera's eye to the sky, `uViewPos` and the HUD |
| The piloted drone flies into the volcano or off the map | Low | Clamp altitude (floor and 55) and distance; no collision system |
| Textures hide the shading model | Medium | Diffuse/ambient only, never specular; `T` switch; screenshot the banding demo with textures off |
| Ray tracing eats the schedule or the frame rate | **High** | It is a stretch tier: start only after §16.1 works. Test the C++ and GLSL intersections against each other first. Keep shadows-only as the fallback mode |
| Shadows look weak at night | Medium | Expected (both lights are dim). Demo shadows by day (`N` day lock); demo the lava glow and the lake at night |
| Ray-traced terms break the Gouraud/Flat comparison | Medium | Run Demo A and Demo B with `R` off; document that Gouraud is per-fragment when ray tracing is on |
| Proxy scene and drawn scene drift apart (shadow of something that isn't there) | Low | Build the proxy from the same placement variables `main.cpp` uses |
| Time overrun | Medium | MVP (Section 16.1) is complete before the shading-comparison and HUD polish phases |

---

## 16. Acceptance Criteria & Demo Script

### 16.1 Minimum Viable Product (must exist to submit)
- Volcano cone with correct lighting
- Crater point light with visible attenuation as camera moves away
- Lava fountain: droplets on true parabolic arcs, cooling/shrinking in flight
- Lava flows with a travelling glow, advancing along their paths
- Ash puffs rising and fading
- Full Phong illumination: 2 lights, ambient/diffuse/specular, attenuation, ≥ 3 distinct materials (rock, lava, ash)
- Three shading modes switchable live
- HUD showing shading mode, time of day, camera, distance to crater, and active particle counts
- Automatic day/night cycle, with `N` to lock day or night
- Views through the drones' cameras (`C`)
- Four keyboard-controlled objects (`Tab` + arrow keys)
- Procedural textures with the `T` switch (supporting, last to be built)

### 16.2 The 3-Minute Demo Script
1. **(0:00)** Scene running. "Everything here — the fountain, the flows, the ash, the drones, the sky — is computed from elapsed time in the render loop. Nothing is pre-recorded."
2. **(0:15)** Press `N` to lock day, then back to night. "The clock runs a full day in two minutes on its own; the sky light changes direction, colour and intensity. That is the L8 directional light, moving."
3. **(0:35)** Press `K` through its cycle. "Ambient, then diffuse, then specular, then full — L8 slide 54, live, on real rock and lava."
4. **(0:55)** Orbit the camera away from the crater while watching the HUD's distance readout and the rock's brightness. "That's the radial attenuation formula — `1/(a₀+a₁d+a₂d²)` — not a fade I painted in."
5. **(1:15)** Point at the rock and the lava in the same shot. "Same equation, same light, two completely different `n_s` and `k_s` values — 4 versus 140."
6. **(1:35)** Press `−` a few times on the crater disc, then `2` then `3`. "Gouraud loses the highlight at low tessellation. Phong keeps it. That's L9 slide 28." Then press `1` on the low-tessellation cone with `T` off. "Mach banding."
7. **(2:05)** Point out a rock chunk tumbling mid-flight. "Translation and rotation composed on the same object — the parabola from gravity, the spin independent of it."
8. **(2:20)** Press `C` to look through a drone, then `Tab` and fly the pilot drone. "That view matrix is rebuilt every frame from a moving, rotating object. The drone is a transform hierarchy: move the body and every arm, rotor and propeller follows."
9. **(2:45)** Point at the rim-rock ring. "Each rock is a child of the crater: change the crater and the ring moves with it."

(Trim steps to fit. Once the rim rocks are instanced, say so here: one draw call, positioned entirely by `gl_InstanceID`.)

### 16.3 Optional (only if time remains)
- Blinn-Phong toggle (`B`)
- The ore-vein detail citing L8's Polished Silver coefficients
- Windmill blade spin; spinning drone propellers
- The ground rover as a second controllable object
- **Ray tracing and inverse ray tracing** (§9.10, §9.11): **built.** Demo it by pressing `R` near sunrise (07:30) for the volcano's long shadow, then `F` and `M` at night for the lava glow
- A picture-in-picture drone feed (doubles the draw calls, so only after batching)
- Occasional larger "boom" fountain burst at a longer interval, layered on top of the steady stream

---

## 17. Deliverables

1. Source code (`/src`, `/shaders`, `CMakeLists.txt`)
2. Compiled executable
3. Report containing: the L8/L9 traceability matrix (Section 4.1), the illumination equation as implemented, the material table with cited slide-60 values (if the ore vein is included), screenshots of the Flat/Gouraud/Phong comparison on the crater glow disc, a Mach-banding screenshot of the cone (textures off), the day/night clock formulas, the drone view transform, a note that textures are procedural and that the piloted drone is the one object not driven by a closed form of time, ray-tracing screenshots (`R` off and on, the light-map minimap) with a statement that the proposal's "no shadows or reflections" was deliberately reversed, and the optimization table (Section 14)
4. 3-minute demo (Section 16.2)
