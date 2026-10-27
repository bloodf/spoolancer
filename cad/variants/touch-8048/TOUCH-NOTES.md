# v0.4 integrated touch option

This is a physical-fit prototype for the **ESP32-8048S043C-I, 800 × 480 capacitive touch board**. The touchscreen's ESP32 replaces the standalone ESP32-S3 board. The 170 × 160 mm base and entire floating cradle are shared with the headless option. The unused ESP32 shelves and USB opening remain in this shared base; the touch version uses a blank fixed cover.

The holder has three removable printed pieces: bezel, stand and flat bridge. It attaches only to the fixed cover posts, so touching it does not directly load the weighing cradle. Pressing the screen can still disturb the table or base: wait for a stable reading after interaction.

## Drawing evidence and limits

The [manufacturer's family drawing](https://www.guition.com/icms/upload/fb081940d6fc11f09850077a33e1404f/FTPData/UEditor/file/2026121/1768961092165/ESP32-4827S043%20Specifications-EN.pdf), shows a 123 × 74 mm board and 3.2 mm holes. The 3.5 mm end offsets imply 116 mm horizontal pitch. Vertical dimensions disagree by 0.2 mm: 67.2 mm is marked, but two 3.5 mm offsets imply 67.0 mm. The design therefore uses **116 × 67.1 mm pitch with 3.4 × 4.4 mm vertical slots** and sliding nut pockets.

This drawing is labeled for the 4827 family. The selected 8048 variant's overall size agrees with the supplied listing, but its received PCB, glass height, connector placement and mounting stack remain unverified. The 105.5 × 67.2 × 3 mm glass and 1.6 mm PCB thickness are clearance assumptions. A 107 × 64 mm bezel opening leaves the 95.04 × 53.86 mm active area clear in this centered model. Check the capacitive edges on the real board before printing the full holder.

The open rear and reliefs reserve room for side controls and USB. The BOOT button must remain reachable for the firmware's recovery gesture. Connector and cable envelopes are approximate, not manufacturer models; the exports do not prove a particular USB plug, harness or finger fits.

## Regeneration

Use a Python environment containing FreeCAD's modules as described in the [CAD regeneration guide](../../README.md#edit-and-regenerate). Run from the repository root:

```sh
python3 cad/build.py --variant touch-8048
python3 cad/verify_exports.py --variant touch-8048
python3 cad/render.py --variant touch-8048
```

The build and verifier require FreeCAD, Part, Mesh and MeshPart. Rendering additionally requires VTK, NumPy and Matplotlib. Replace `python3` with the matching FreeCAD Python interpreter when necessary; `FREECAD_LIB` can point to its module directory.

The shared sources are `cad/build.py`, `cad/render.py` and `cad/verify_exports.py`. `cad/parameters.json` contains the common headless/cradle dimensions, and this folder's `parameters.json` overrides only the touch-specific dimensions. Outputs remain in this folder's `exports/` and `previews/`. Replace `touch-8048` with `headless` to rebuild the separate headless variant. Omitting `--variant` writes the default headless outputs to `cad/exports` and `cad/previews`.

## Parts and orientation

Print the shared base, carrier, two identical rim rails, left guide, NFC guide, NFC pod and pod cover. Use this folder's **blank `top_cover.stl`**, plus one each of `touch_bezel.stl`, `touch_stand.stl` and `touch_bridge.stl`. There are **12 main printed instances, 11 unique main STLs**. The three fit samples are `fit_nfc`, `fit_cell` and `fit_touch_mount`, for **14 STL files and 14 matching individual STEP files**. Fit samples are additional, not counted in the material total. Every individual STEP contains one analytic solid in the same print orientation as its STL. The complete assembly STEP includes hardware references and must not be printed as one object.

The exported poses place individual solids and meshes at Z=0. STEP preserves analytic curves for CAD editing and STEP-capable slicers; STL is the triangulated fallback. Reopened files are checked for single solids, dimensions and volume with stated tessellation tolerances in `exports/export-verification.json`. The bezel prints outer face down; its small nut-pocket roofs need short bridges. The stand prints on its connected feet; its bosses have sloping supports and its two attachment tabs are a separate flat bridge. Inspect the stand's small tilted PCB-pad overhangs and connector reliefs in the slicer. No support material is modeled or included in the CAD volume, and support-free printing is not physically proven. The blank cover and bridge print flat. Start with `fit_touch_mount.stl` to verify hole pitch, nut fit and the PCB/glass stack.

## Fasteners and assembly

Additional touch hardware:

| Purpose | Quantity | Hardware |
|---|---:|---|
| Four corner PCB clamps | 4 | M3 × 10 socket screws, 0.5 mm washers and standard M3 nuts, approximately 2.4 mm thick |
| Bridge to stand | 2 | M3 × 12 socket screws, 0.5 mm washers and standard M3 nuts |
| Front cover posts through bridge | 2 | M3 × 16 socket screws and 0.5 mm washers, replacing the headless option's two front M3 × 10 screws; reuse the base's captive nuts |
| Stand feet | 2 | 12 mm diameter × 2 mm rubber feet, matching the base feet |

The two rear cover screws remain M3 × 10. Check screw head and washer dimensions: the model uses a 5.5 mm socket-head diameter and 6.4 mm washer outside diameter. PCB clamps may use nylon hardware of matching dimensions. Tighten gently against the PCB margins; do not clamp the glass.

1. Fit the two M3 bridge nuts into the stand's outward-facing side channels while the stand is loose. Their retaining roof carries the clamp load. Add the two rubber feet to the stand's 0.6 mm recesses.
2. Place four M3 nuts into the bezel's front pockets. Their narrow flats face the slot walls; small vertical movement accommodates the drawing's inconsistent pitch.
3. Put the PCB between the bezel's four front standoffs and the stand's four rear pads. Insert the four M3 × 10 screws and washers from the open rear. The nominal stack is: rear washer seat −7.4 mm, 2 mm rear pad, 1.6 mm PCB, 3.8 mm front support, then the nut pocket. The screw enters the nut from behind and protrudes about 0.3 mm beyond the seated nut. Verify that the real glass remains clear of the bezel.
4. Attach the flat bridge to the stand using two M3 × 12 screws at X = ±65 mm, Y = −90 mm. These lie outside the PCB edges for driver access. Nominal under-head seat is Z = 41 mm, washer 0.5 mm, captive nuts Z = 30.8–33.2 mm.
5. Seat the bridge over the base's front cover holes at X = ±78 mm, Y = −76 mm. Use M3 × 16 screws through the bridge and blank cover into the existing base nuts. The stand and base rubber feet must sit evenly on the same surface.
6. Connect the screen/controller to the fixed HX711 and PN532 harness as documented in the wiring guide. Keep the screen harness clear of the floating carrier. Preserve the flexible NFC service loop: its force bias remains untested.

A 1.6 mm PCB, 3 mm glass and these exact fastener dimensions are assumptions. Print the fit sample, measure the received hardware and adjust parameters before treating these screw lengths as final.

## Size and material comparison

The empty printed assembly is **170 × 205.73 × 184 mm** at the reference reader height; maximum reader height increases the total to 198.4 mm. Rubber feet add 1.4 mm after their recess. With a 200 mm spool, depth is 230.73 mm and overall height including feet is 256.87 mm. A 220 mm spool increases depth to 240.73 mm. These are full assembly bounds; the base alone remains 170 × 160 mm.

| Printed part | Quantity | Exported print bounds, mm | CAD solid volume, mm³ each |
|---|---:|---|---:|
| Blank touch cover | 1 | 170 × 160 × 3 | 22,782.14 |
| Touch bezel | 1 | 139 × 82 × 5.8 | 9,582.05 |
| Touch stand | 1 | 140 × 49 × 77.12 | 16,690.65 |
| Touch bridge | 1 | 164 × 23 × 3 | 5,447.05 |

Including every main printed quantity, this option totals **447,508.29 mm³ of CAD solid volume**, versus **415,788.54 mm³** for the v0.4.1 headless revision: 31,719.75 mm³ more, or 7.63%. This is analytic geometry volume, **not grams or sliced filament**. The consistent OrcaSlicer PETG profile estimates **427.29 g** for touch. The earlier **392.32 g** headless comparison and **34.97 g** difference apply to the old v0.4 external-switch revision; v0.4.1 has not been re-sliced. Touch remains **30.85% below** the v0.2 upright design's 617.96 g estimate. These slicer estimates exclude hardware and fit samples; actual printing remains untested. The settings and final STL hashes are recorded in `analysis/material-comparison/v0.4-touch.json`.

Eight shared unique production STLs—base, blank top cover, carrier, rim rail, side guide, NFC guide, pod and pod cover—match the headless v0.4.1 hashes exactly. `comparison.json` records that check, print quantities and full bounds. Both current variants use the blank cover and their onboard BOOT switches.

## What has been checked

`exports/validation.json` records solid validity, mesh closure, print bounds, component clearances and all 27 combinations of three spool diameters, three spool widths and three NFC heights. `exports/export-verification.json` records reopening all fourteen STL/individual STEP pairs, the native FreeCAD file and complete assembly STEP, plus eight touch fastener envelope checks. STEP/design dimensions and volume and STEP/STL tessellation differences are checked and recorded for every part.

These checks establish geometry consistency only. Actual part fit, capacitive behavior, print strength, tipping resistance, NFC read/write range, wire-force bias and calibrated weighing accuracy remain untested.
