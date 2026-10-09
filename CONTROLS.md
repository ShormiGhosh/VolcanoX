# VolcanoX — Keyboard Controls

Run the program from the project root: `./build/VolcanoX.exe`

## The four controllable objects

Press **`Tab`** to cycle the controlled object:
**none → pilot drone → rover → helicopter → boat → none.**
The same keys drive whichever one is selected.

| Key | Action | Applies to |
|---|---|---|
| `W` / `S` | Forward / back | all four |
| `A` / `D` | Turn left / right | all four |
| `Space` / `Left Shift` | Up / down | pilot drone and helicopter only |

| Object | Notes |
|---|---|
| **Pilot drone** | Leans into movement. Altitude is limited (above the ground and cone, below the storm clouds). |
| **Rover** | Steers only while moving; reversing mirrors the steering. Stays outside the volcano's base. |
| **Helicopter** | Starts on its helipad. Press `Space` to lift off: the rotor must spin up first, and it cannot move or turn until it is airborne. |
| **Boat** | Stays inside the lake. Slow to speed up and to stop. |

While an object is selected, the orbit camera follows it. The objects that are not selected come to rest (the drone and helicopter hover).

## Camera

| Key | Action |
|---|---|
| Arrow keys | Orbit the camera |
| Mouse wheel / `PgUp` / `PgDn` | Zoom in / out |
| `C` | Camera: free orbit → Drone 1 → Drone 2 → Pilot drone → Rover → Helicopter → Boat |
| `V` | Drone camera aim: lock on the crater / look forward |

## Eruption and world

| Key | Action |
|---|---|
| `E` | Fire a big eruption burst now |
| `J` | Camera shake on / off |
| `N` | Time of day: automatic → lock day → lock night |
| `P` | Pause |

## Rendering

| Key | Action |
|---|---|
| `1` / `2` / `3` | Flat / Gouraud / Phong shading |
| `L` | Lights: both / crater only / sky light only |
| `K` | Terms: ambient / ambient + diffuse / specular only / full |
| `B` | Phong ↔ Blinn-Phong specular |
| `+` / `-` | Increase / decrease cone and pool tessellation |
| `T` | Procedural textures on / off |
| `R` | Ray tracing: off / shadows / shadows + reflections |
| `F` | Lava-light tracing on / off |
| `M` | Light-map minimap on / off |
| `H` | HUD on / off |
| `Esc` | Quit |

## Notes

- Toggle keys act once per tap; held keys (arrows, `W` `A` `S` `D`, `Space`, `Left Shift`, `PgUp`, `PgDn`) act while held.
- Ray tracing (`R`) evaluates Gouraud shading per pixel, so it looks like Phong. Turn ray tracing off to compare the shading modes.
- Lightning only strikes at night.
- For the full specification, see `IMPLEMENTATION_GUIDE.md` (§4 has the key map and the command-line debug options).
