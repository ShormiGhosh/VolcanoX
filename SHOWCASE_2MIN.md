# VolcanoX — 2-Minute Feature Showcase (silent screen recording)

**Written for:** you, as the person pressing the keys while the screen is recorded. No narration is needed.
Every key press and time below was played into the real program in one continuous run, and the window was captured
at the key moments to check that the feature really was on screen.

## How it works

Use **one launch with no options**: `.\build\VolcanoX.exe` (from `D:\openGL`).

The program's own day/night clock is the timeline. It starts at 20:00 (night) and a full day takes 120 seconds, so
**sunrise arrives at about 0:50 and it is full day by 1:20.** That is why night features come first and day features last.
Press the keys at the times below; the HUD on the left shows the name of each mode so the viewer can read it.

- Start recording, **then** launch the program, and click once on its window so it has keyboard focus.
- Count from the moment the window appears (`0:00`).
- Do not press `P` (pause) or `N` more than the table says: the day/night clock is the timeline.

## The sequence

| Time | Keys | What the viewer sees | Feature shown |
|---|---|---|---|
| **0:00** | hold `→` for 5 s | A full orbit of the night scene: volcano, lava flows, pool, trees, hut, windmill, drones, lightning | The scene; camera orbit; lightning (night only) |
| **0:08** | hold `PgUp` for 1.5 s | Close to the crater | Zoom |
| **0:14** | tap `E` | Fire flash, smoke plume, shockwave rings, shaking view, flying rocks with glowing trails | Eruption bursts; bombs with trails; camera shake; transformations of the flying rocks |
| **0:20** | hold `PgDn` for 1 s | Pull back a little | |
| **0:21.5** | tap `K` at 21.5, 23, 24.5 and 26 | Ambient only → ambient + diffuse → specular only → full | Ambient, diffuse and specular terms |
| **0:27.5** | tap `L` at 27.5, 29 and 30.5 | Crater light only → sky light only → both | Two lights; point-light attenuation |
| **0:32** | tap `F`, then `M` at 33.5 | Orange glow on the ground beside the lava, and the light-map minimap in the corner | Inverse ray tracing |
| **0:38.5** | tap `M`, then `F` | Both off | |
| **0:41.5** | tap `Tab` four times, 0.25 s apart | The camera moves to the boat on the lake (`CONTROL: BOAT`) | Controllable boat |
| **0:42.5** | tap `R` twice | `RAY TRACING: SHADOWS + REFLECTIONS` | Ray-traced reflections |
| **0:43.5** | tap `E` | About 5 s later, bombs splash into the lake with ripples, spray and steam | Lake splash and ripples |
| **0:51** | tap `R` | Ray tracing off | |
| **0:51.5** | tap `Tab` | Release control; the camera returns to the volcano. The sun is rising | Automatic day and night |
| **0:54** | tap `R` | `RAY TRACING: SHADOWS`: long shadows of the volcano, trees, hut, windmill and vehicles | Ray-traced shadows |
| **0:55** | hold `X` for 3 s | Orbit to show the shadow | |
| **1:01** | tap `R` twice | Ray tracing off | |
| **1:05** | tap `N` once | `TIME: DAY (LOCKED)` | Time of day lock |
| **1:06** | tap `T` | `TEXTURES: OFF` | Texture switch |
| **1:06.5** | tap `-` three times | `CONE SEGMENTS: 8` | Tessellation |
| **1:08** | tap `1` | `SHADING: FLAT`: visible facets | Flat shading, Mach banding |
| **1:10.5** | tap `2` | `SHADING: GOURAUD` | Gouraud shading |
| **1:13** | tap `3` | `SHADING: PHONG`: smooth | Phong shading |
| **1:15.5** | tap `+` three times | `CONE SEGMENTS: 28` | |
| **1:17** | tap `T` | `TEXTURES: ON`: rock, ash and wood textures | Procedural textures |
| **1:19** | tap `Tab` | `CONTROL: PILOT DRONE` | Controllable drone |
| **1:19.5** | hold `↑` for 2 s, then `Space` for 1 s | The drone flies forward and climbs, leaning, rotors spinning | |
| **1:23.5** | tap `Tab` | `CONTROL: ROVER` | Controllable rover |
| **1:24** | hold `↑` for 3 s, then `↓` for 1.2 s | The rover drives into the volcano's base and bounces back: `BUMPS 1 (VOLCANO)` | Rover collision and bump |
| **1:29** | tap `Tab` | `CONTROL: HELICOPTER` | Controllable helicopter |
| **1:29.5** | hold `Space` for 4 s, then `↑` for 2 s | The rotor spins up, then it lifts off and flies | |
| **1:36.5** | tap `Tab` | `CONTROL: BOAT` | |
| **1:37** | hold `↑` for 3 s | The boat sails, with wake and radar | |
| **1:41** | tap `Tab` | `CONTROL: NONE` | |
| **1:42** | tap `C` at 1:42, 1:44, 1:46, 1:48, 1:50, 1:52 and 1:53.5 | Looking through Drone 1, Drone 2, Pilot drone, Rover, Helicopter, Boat, then back to free orbit | Onboard cameras (viewing transformation) |
| **1:54** | tap `E` | Finale: a big burst | |
| **1:55** | hold `PgDn` for 1.5 s | Pull back to a wide view of the eruption | |
| **2:00** | stop recording | | |

## Everything the video covers

| Feature | At |
|---|---|
| Scene of simple shapes, camera orbit | 0:00 |
| Translation, rotation, scaling and composite motion (flying rocks, droplets, ash) | 0:14 |
| Hierarchical motion (rim rocks, drone parts, rover wheels, rotors) | 0:08, 1:19, 1:24, 1:29 |
| Eruption bursts, shockwave, camera shake, bombs with trails | 0:14 |
| Ambient, diffuse, specular terms | 0:21 |
| Two lights, attenuation | 0:27 |
| Material contrast (glossy lava, matte rock) | 0:21 (specular only) |
| Inverse ray tracing and light-map minimap | 0:32 |
| Lake splash, ripples, steam | 0:43 |
| Reflections | 0:42 |
| Day and night, sunrise | 0:51 onward |
| Ray-traced shadows | 0:54 |
| Flat, Gouraud, Phong; Mach banding; tessellation | 1:05 – 1:15 |
| Procedural textures and the off switch | 1:06, 1:17 |
| Four controllable objects, onboard cameras, collision bump | 1:19 – 1:54 |

## If you fall behind

- **More than 5 s behind before 1:05:** skip the lake scene (0:41.5 – 0:51.5) and the second `L` presses.
- **Shading section (1:05 – 1:17):** this is the most valuable part. Do not cut it. If time is short, skip `2`.
- **Vehicles:** keep the rover bump and the helicopter lift-off; drop the drone and boat first.
- **If the HUD still says `NIGHT` after you tap `N` at 1:05:** the clock was already locked. Tap `N` until it says `DAY (LOCKED)`.

## Optional: a 10-second clip for the Gouraud-versus-Phong highlight

The lava-pool highlight is the clearest picture of the difference, but it needs a different camera, so it is a second launch.
Record it separately and cut it in after 1:15:

```
.\build\VolcanoX.exe --lock 1 --start 50 --control 1 --ctlpos 52 3 -41 --orbit -77 58 22 --tess 1 --lights 2
```

Wait 3 s for the camera to settle over the pool, then tap `2` (hard-edged diamond highlight), wait 4 s, tap `3` (smooth round
highlight), wait 4 s. If you add it, cut the drone and boat from the vehicles section to keep the video at two minutes.

## Optional: on-screen captions to add in a video editor

Since there is no narration, short captions help. Suggested text, one per segment:

`Time-driven motion · Eruption bursts · Ambient / diffuse / specular · Two lights, attenuation · Inverse ray tracing ·
Reflections · Day and night · Ray-traced shadows · Flat / Gouraud / Phong · Procedural textures · Four controllable objects ·
Collision and bump · Onboard cameras`

## What was and was not checked

- **Checked:** the whole sequence was played into a real window three times with real key presses (no crash; the rover
  bump registered; the final camera angle matched). Window captures of the lava-light map, lake splash, sunrise shadows, flat
  shading, and helicopter flight were looked at and match the table.
- **Not looked at in the final run:** the Gouraud and Phong frames, the boat and the onboard camera frames. Those worked in
  earlier checks.
- **Real-time variation:** bursts and particles are not scripted, so every take looks a little different. The clock, the
  keys and the HUD text are the same each time.
- **Frame rate:** it dips to about 70 FPS while ray tracing and lava-light tracing are on, then recovers.
