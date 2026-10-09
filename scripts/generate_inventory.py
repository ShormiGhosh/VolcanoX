"""
Generates object_inventory.xlsx from the object list below.

This is the maintained source of truth for VolcanoX's scene object roster
(see PRD_MoltenGround.md Section 6). To add or change an object, edit the
`OBJECTS` list below and re-run:

    "/c/Users/Shormi/AppData/Local/Programs/Python/Python313/python.exe" scripts/generate_inventory.py

from the project root. Do not hand-edit the .xlsx file directly -- it will
just get overwritten the next time this script runs.
"""
import openpyxl
from openpyxl.styles import Font, Alignment, PatternFill
from openpyxl.utils import get_column_letter

COLUMNS = [
    "ID", "Object", "Category", "Real-World Purpose", "OpenGL Geometry",
    "Position / Placement", "Transformation Used", "Animation Later?",
    "Status", "Notes",
]

OBJECTS = [
    (1, "Volcano", "Volcanic",
     "Main eruption source; the mountain itself",
     "Cone (wide base, small open top = crater mouth)",
     "World origin (0,0,0), base radius 10, height 14",
     "Translation, Scaling", "No (static mountain)", "Existing",
     "drawVolcano() in SceneObjects.cpp"),

    (2, "Crater", "Volcanic",
     "The vent opening at the volcano's summit",
     "Flat disc (glow surface) + open cone top",
     "Volcano apex, (0, ~13, 0)",
     "Translation, Scaling (breathing pulse)", "Yes (scale pulse, implemented)", "Existing",
     "drawCraterGlowDisc(); pulse is a pure transform, not emission/lighting"),

    (3, "Crater / Rim Rocks", "Volcanic",
     "Rock debris ringing the crater mouth",
     "10x irregular jittered-sphere rocks",
     "Ring of radius 1.7 around the crater",
     "Translation, Rotation (ring placement), Scaling (per-rock jitter)",
     "Optional (instancing later)", "Existing",
     "drawCraterRimRocks(); non-instanced loop by design for now (see PRD Section 0.2)"),

    (4, "Lava Pool", "Volcanic",
     "Molten lava accumulating at the volcano's base",
     "Flat disc",
     "(14, 0.1, 7), down-slope from the crater",
     "Translation, Scaling (slow growth over time)", "Yes (growth, implemented)", "Existing",
     "drawLavaPool(); saturating growth curve, no emission/glow shading yet"),

    (5, "Lava Flow Channel", "Volcanic",
     "Lava creeping downhill from the crater to the pool",
     "6x boxes bridging crater -> pool along a straight interpolated path",
     "Crater to lava pool",
     "Translation, Rotation (segment facing), Scaling (segment length)",
     "Yes (travelling brightness pulse) -- deferred to lighting phase", "Existing",
     "drawLavaFlowChannel(); geometry only for now, no shading-based glow"),

    (6, "Lava Droplets", "Volcanic",
     "Molten fragments ejected from the crater on ballistic arcs",
     "Sphere (pooled, 8 slots)",
     "Spawn at crater, fly outward/down",
     "Translation (parabolic p(t)=p0+v0t+0.5gt^2), Scaling (cooling shrink)",
     "Yes, implemented", "Existing",
     "updateAndDrawLavaDroplets(); color lerp hot-yellow to orange is a plain uniform color change, not lighting"),

    (7, "Flying Rock Chunks", "Volcanic",
     "Solid debris thrown out alongside the lava",
     "Irregular jittered-sphere rock (pooled, 6 slots)",
     "Spawn at crater, fly outward/down",
     "Translation (parabolic), Rotation (continuous tumble)",
     "Yes, implemented", "Existing",
     "updateAndDrawRockChunks()"),

    (8, "Ash / Smoke Puffs", "Volcanic",
     "Gas and pulverized rock rising from the vent between fountain bursts",
     "Sphere (pooled, 6 slots)",
     "Spawn at crater, rise upward",
     "Translation (rise + drift), Scaling (grow), Alpha blend (fade)",
     "Yes, implemented", "Existing",
     "updateAndDrawAshPuffs()"),

    (9, "Burnt Tree", "Damage/Aftermath",
     "Vegetation killed and charred by heat/ashfall near an active volcano (documented 'ghost forest' effect)",
     "Tapered cone trunk + cylinder branches + irregular-rock debris",
     "6 placed in a ring, radius 16-22, lower/mid slope (never inside the crater)",
     "Translation, Rotation (wind lean + facing), Scaling (per-tree size variety)",
     "Optional (wind-sway)", "Added",
     "drawBurntTree() in NewObjects.cpp"),

    (10, "Burnt / Damaged Hut", "Settlement/Damage",
     "Rural dwelling near the volcano damaged by ashfall/heat (real hazard e.g. around Sinabung, Merapi)",
     "Boxes for walls (one collapsed, one charred) + 2 angled box roof panels (one slumped) + rock debris",
     "Lower environment, away from the lava-flow corridor",
     "Translation, Rotation, Scaling", "No", "Added",
     "drawBurntHut() in NewObjects.cpp"),

    (11, "Windmill", "Optional",
     "Not a volcanic phenomenon; included because the burnt hut establishes a small rural homestead, and a homestead windmill is a natural real-world pairing. Shown storm/blast-damaged to match the aftermath theme.",
     "Cylinder tower + box hub + 4 box blades",
     "Next to the burnt hut",
     "Translation, Rotation", "Optional (blade spin)", "Added",
     "drawWindmill() in NewObjects.cpp; judged to fit, unlike the aircraft (#16)"),

    (12, "Ash / Smoke Cloud", "Weather/Volcanic",
     "Eruption column spreading into a wind-bent umbrella/mushroom shape -- the classic volcano-eruption-ash-cloud silhouette",
     "Stacked rising-stem spheres + wider cauliflower head cluster of spheres, offset along the wind vector",
     "Directly above the crater",
     "Translation, Scaling", "Yes (drift/billow)", "Added",
     "drawAshCloud() in NewObjects.cpp; shares windDir with trees and wind indicators"),

    (13, "Volcanic Lightning", "Weather/Volcanic",
     "'Dirty thunderstorm' phenomenon: static discharge from colliding ash/rock particles in a real eruption plume",
     "Fractal midpoint-displacement jagged line segments (GL_LINES), 3 bolts",
     "Inside/around the ash cloud only",
     "Translation, Scaling", "Yes (flicker) -- deferred", "Added",
     "drawVolcanicLightning(); static/visible only for this phase, per task scope"),

    (14, "Wind Indicators", "Weather",
     "Diagram convention for showing wind direction (not a physical object)",
     "4x curved GL_LINE_STRIP streaks",
     "Off to the side, away from the volcano, so it reads as an annotation",
     "Translation (slow drift loop), Rotation (face wind direction)",
     "Yes, implemented (drift)", "Added",
     "drawWindIndicators(); same windDir vector as the ash cloud lean and tree lean"),

    (15, "Research / Camera Drone", "Monitoring",
     "Quadcopter drones are the real-world standard for close-range volcanic monitoring (thermal/gas/visual survey) without risking a person",
     "Box body + 4 box arms + cylinder rotor hubs + thin cylinder propeller discs + angled box camera/sensor",
     "Hovering near the volcano, clear of the ash plume",
     "Translation (hover bob), Rotation (arm fan-out, camera tilt)",
     "Optional (flight path later)", "Added",
     "drawResearchDrone(); static hover position + a gentle bob only, no flight AI"),

    (16, "Aircraft", "Optional",
     "Considered and not implemented: the drone already covers the aerial-monitoring role, and a generic jet was explicitly out of scope as pure decoration",
     "--", "--", "--", "--",
     "Not implemented / optional",
     "Documented per task instructions; revisit only if a specific research-aircraft need arises"),

    (17, "Ground Plane", "Environment",
     "Surrounding terrain the whole scene sits on",
     "Flattened box",
     "Centered at the origin, 90x90 units",
     "Translation, Scaling", "No", "Existing",
     "drawGroundPlane()"),
]

CATEGORY_COLORS = {
    "Volcanic": "FDE9D9",
    "Damage/Aftermath": "3B3B3B",
    "Settlement/Damage": "5A4632",
    "Optional": "D9D9D9",
    "Weather/Volcanic": "D9E1F2",
    "Weather": "D9E1F2",
    "Monitoring": "E2EFDA",
    "Environment": "EDEDED",
}

def build():
    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = "Object Inventory"

    header_fill = PatternFill(start_color="4472C4", end_color="4472C4", fill_type="solid")
    header_font = Font(bold=True, color="FFFFFF")
    wrap = Alignment(wrap_text=True, vertical="top")

    for col_idx, name in enumerate(COLUMNS, start=1):
        cell = ws.cell(row=1, column=col_idx, value=name)
        cell.fill = header_fill
        cell.font = header_font
        cell.alignment = Alignment(wrap_text=True, vertical="center", horizontal="center")

    for row_idx, row in enumerate(OBJECTS, start=2):
        for col_idx, value in enumerate(row, start=1):
            cell = ws.cell(row=row_idx, column=col_idx, value=value)
            cell.alignment = wrap
        category = row[2]
        fill_color = CATEGORY_COLORS.get(category)
        if fill_color:
            ws.cell(row=row_idx, column=3).fill = PatternFill(
                start_color=fill_color, end_color=fill_color, fill_type="solid")

    widths = [5, 22, 18, 42, 38, 32, 30, 22, 16, 42]
    for col_idx, width in enumerate(widths, start=1):
        ws.column_dimensions[get_column_letter(col_idx)].width = width

    ws.freeze_panes = "A2"
    ws.auto_filter.ref = f"A1:{get_column_letter(len(COLUMNS))}{len(OBJECTS) + 1}"

    # Second sheet: a short legend so this stays easy to update later.
    legend = wb.create_sheet("Legend")
    legend_rows = [
        ("Status values", ""),
        ("Existing", "Object was part of the core volcanic scene (proposal's baseline)."),
        ("Added", "Object added in this task's objects-only implementation phase."),
        ("Not implemented / optional", "Considered, deliberately left out; reason is in the Notes column."),
        ("", ""),
        ("How to update this sheet", ""),
        ("1.", "Edit the OBJECTS list in scripts/generate_inventory.py"),
        ("2.", "Re-run: python scripts/generate_inventory.py"),
        ("3.", "This file is fully regenerated each run -- do not hand-edit the Object Inventory sheet directly."),
    ]
    for r, (a, b) in enumerate(legend_rows, start=1):
        legend.cell(row=r, column=1, value=a).font = Font(bold=(b == "" and a != ""))
        legend.cell(row=r, column=2, value=b)
    legend.column_dimensions["A"].width = 28
    legend.column_dimensions["B"].width = 70

    wb.save("object_inventory.xlsx")
    print(f"Wrote object_inventory.xlsx with {len(OBJECTS)} objects.")

if __name__ == "__main__":
    build()
