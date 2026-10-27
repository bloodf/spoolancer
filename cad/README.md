# Spoolancer enclosure variants · headless v0.4.1

Print the small fit samples before the full enclosure. This editable prototype uses the selected ESP32-S3, PN532 and 80 mm load cell. CAD checks and firmware compilation have passed; physical fit, weighing accuracy and NFC performance remain untested. Build instructions are in [firmware/README.md](../firmware/README.md), with daily operation in the [web-interface guide](../docs/WEB-INTERFACE.md).

The upright spool rests on two hollow rim rails. An open carrier supports both rails, two different sliding guides and the NFC reader. The reader sits in a pod on the right guide and adjusts vertically to face the seated spool's tag. A loose cable loop connects it to the fixed electronics. The intended spool range is 50–100 mm wide and 160–220 mm in diameter, subject to actual flange and tag fit. This is an occasional-weighing scale, without an axle or bearings.

See the [assembled](previews/assembled.png), [exploded](previews/exploded.png), [interior](previews/interior.png), [cradle](previews/cradle.png), [NFC detail](previews/nfc-detail.png) and [load-path](previews/load-path.png) views. The previous upright design is preserved in [archive/v0.2-upright](../archive/v0.2-upright/cad/README.md).

## New DC-powered indicator variants

The additive **[headless + five LEDs](variants/headless-led/NOTES.md)** and **[0.96-inch OLED + five LEDs](variants/oled-led/NOTES.md)** prototypes use five round openings at 16.6667 mm centers. Their fixed front pods share the upright cradle. These variants close the USB wall, provide a separate BOOT access bore, and leave the DC jack opening for the user to drill. A separate 12 V-to-5 V converter is required; no converter mount is assumed.

The new [headless LED](variants/headless-led/exports) and [OLED LED](variants/oled-led/exports) directories contain individual STEP/STL files, full FreeCAD/STEP assemblies, fit samples, validation reports and [previews](variants/oled-led/previews/assembled.png). Existing release files below are preserved. OLED board dimensions, strip width, USB plug and power converter fit remain unverified. Read each variant's notes before printing.

Regenerate with `--variant headless-led` or `--variant oled-led` on `build.py`, `verify_exports.py` and `render.py`. Their parameter files override the shared baseline; `indicators.py` contains the front pod geometry. The new fascia has an elevated integrated mounting bridge and needs local slicer support in its exported face-down pose.

## Choose a variant

- **Headless:** the files in `exports/` and the independent copy under [variants/headless](variants/headless/HEADLESS-NOTES.md). Use the existing N16R8 controller and its onboard BOOT switch; no separate switch, perfboard or printed cap.
- **800 × 480 capacitive touch:** the ESP-8048S043C-I replaces the separate controller. Its fixed front holder and assembly are in [variants/touch-8048](variants/touch-8048/TOUCH-NOTES.md), with [FreeCAD](variants/touch-8048/exports/spoolancer-v0.4-touch-8048.FCStd), [STEP](variants/touch-8048/exports/spoolancer-v0.4-touch-8048-assembly.step) and individual STEP/STL files. It shares the upright carrier, rails and reader.
- **Legacy 2-inch screen:** the previous mechanical package is retained in [archive/v0.3-compact](../archive/v0.3-compact/SUPERSEDED.md). Its screen and three-button arrangement do not belong in the headless base.

The touch holder adds a bezel, stand and flat attachment bridge. It shares the blank cover; its twelve printed instances occupy a 170 × 205.73 mm unloaded footprint. See the [touch assembly notes](variants/touch-8048/TOUCH-NOTES.md) for its fit sample, orientation and fasteners.

The rest of this guide describes the headless prints and shared mechanism. The touch pod must attach only to fixed housing, never to the moving cradle.

## Files to open

| File | Use |
| --- | --- |
| [spoolancer-v0.4.1-headless.FCStd](exports/spoolancer-v0.4.1-headless.FCStd) | Editable assembled FreeCAD document. Printable parts and simplified hardware references are separate groups. |
| [spoolancer-v0.4.1-headless-assembly.step](exports/spoolancer-v0.4.1-headless-assembly.step) | Assembly interchange file. It includes reference bodies; do not send the whole assembly to a slicer. |
| [parameters.json](parameters.json) and [build.py](build.py) | Dimensions, clearances and generator. Values are millimetres. |
| [validation.json](exports/validation.json) and [export-verification.json](exports/export-verification.json) | Modeled geometry checks and reopened-export checks. |
| [clamp-fastener-check.json](exports/clamp-fastener-check.json) and [pod-fastener-check.json](exports/pod-fastener-check.json) | Nominal guide/rail/cover clamp stacks and pod fastener clearances. |
| [HEADLESS-NOTES.md](HEADLESS-NOTES.md) | Headless geometry, onboard BOOT access and comparison with older designs. |

The geometry is original to this project. No third-party CAD model is incorporated. Reference bodies are simplified estimates, not manufacturer models. Published dimensions and unresolved measurements are recorded in [MECHANICAL-SOURCES.md](../docs/MECHANICAL-SOURCES.md).

## Individual STEP and STL files

Every current printable part and fit sample has both a `.step` and a `.stl` file with the same basename. STEP contains one editable analytic solid; STL contains its triangulated surface. Both use millimetres, the same intended print rotation and an actual surface bottom at Z = 0. STEP positioning and verification use tight analytic bounds, so trimmed curved surfaces do not inherit Open CASCADE's sometimes oversized conservative bounding box. A slicer may tessellate STEP differently, so inspect the imported orientation, scale and thin features before printing.

- **Headless:** [11 individual STEP files](variants/headless/exports) and 11 STLs: eight unique production parts plus three fit samples. Print the rim rail twice, for nine production pieces. `cad/exports` is a convenience copy of this variant.
- **Touch:** [14 individual STEP files](variants/touch-8048/exports) and 14 STLs: eleven unique production parts plus three fit samples. Print the rim rail twice, for twelve production pieces.
- **Full assemblies:** the named `*-assembly.step` and `.FCStd` files retain all assembled parts and simplified component references. Use these for editing or understanding fit; do not print the entire assembly.

The [export reports](exports/export-verification.json) reopen both formats, compare bounds and volume, and record STEP checksums. [The delivery report](step-delivery-verification.json) records that adding STEP export did not change any existing STL. This is digital geometry validation only; physical fit, strength and scale accuracy are untested.

## Fit samples first

| Sample | Check against the actual part |
| --- | --- |
| [fit_esp32.stl](exports/fit_esp32.stl) · [STEP](exports/fit_esp32.step) | Board rails and underside clearance. Check the actual USB plugs and wiring again in the full base. |
| [fit_nfc.stl](exports/fit_nfc.stl) · [STEP](exports/fit_nfc.step) | Actual pod geometry: matching PCB holes, nylon screw seats, components, compact wire exit and tag performance through the window. A successful fit can serve as the pod. |
| [fit_cell.stl](exports/fit_cell.stl) · [STEP](exports/fit_cell.step) | Four cell-hole positions and screw clearance; check thread depth separately. This sample does not validate the load path. |

Change the relevant parameters and repeat the affected sample before the full prints. The [assembly guide](../docs/ASSEMBLY.md) lists intended fasteners and fit checks.

## Main prints

There are **nine printed pieces**. The two guides are different parts. Each STL is already in its intended print orientation, with its lowest point at Z = 0.

| Quantity | STL / STEP | Orientation already applied | Print bounds, mm |
| --- | --- | --- | --- |
| 1 | [base.stl](exports/base.stl) · [STEP](exports/base.step) | Floor down | 170 × 160 × 35 |
| 1 | [top_cover.stl](exports/top_cover.stl) · [STEP](exports/top_cover.step) | Exterior face down | 170 × 160 × 3 |
| 1 | [carrier.stl](exports/carrier.stl) · [STEP](exports/carrier.step) | Upper face down | 140 × 130 × 25.3 |
| 2 | [rim_rail.stl](exports/rim_rail.stl) · [STEP](exports/rim_rail.step) | Outer end face down | 140 × 27.53 × 31 each |
| 1 | [side_guide.stl](exports/side_guide.stl) · [STEP](exports/side_guide.step) | Inner spool-facing face down | 52 × 130 × 16 |
| 1 | [nfc_guide.stl](exports/nfc_guide.stl) · [STEP](exports/nfc_guide.step) | Inner spool-facing face down | 134 × 130 × 16 |
| 1 | [nfc_pod.stl](exports/nfc_pod.stl) · [STEP](exports/nfc_pod.step) | Antenna window down | 46.8 × 71 × 13.6 |
| 1 | [nfc_pod_cover.stl](exports/nfc_pod_cover.stl) · [STEP](exports/nfc_pod_cover.step) | Flat face down | 46.8 × 49.1 × 1.6 |

The base and cover need a usable print area of at least **170 × 160 mm**, plus the slicer's required clearance or brim. The unloaded assembly stays within that footprint, but its adjustable reader makes it **185.4–199.8 mm high** including the specified feet. A modeled 200 mm spool gives overall dimensions of about **170 × 200 × 256.87 mm**; the 220 mm case reaches **220 mm depth and 278.42 mm height**. These are geometry envelopes, not tested spool compatibility or stability.

Start with ordinary PETG, 0.2 mm layers, four perimeters, five top/bottom layers and 20–25% infill. This is an unvalidated starting profile. Use unfilled filament around the NFC antenna. The rail orientation avoids a long roof bridge; small nut channels and holes still require short bridging. Inspect the slicer preview before printing. The [material comparison](../analysis/material-comparison/README.md) records earlier sliced estimates. Its 392.32 g figure belongs to the old v0.4 headless switch/cap design, not this v0.4.1 revision; no new slicing run has been performed.

## Edit and regenerate

Run from the repository root with **Python supplied by, or ABI-compatible with, FreeCAD**. The build and verifier require `FreeCAD`, `Part`, `Mesh` and `MeshPart`; they do not require the renderer's packages. The release geometry was generated and checked with FreeCAD 1.1.4 / Open CASCADE 7.8. Installing a package named `FreeCAD` from PyPI is not a substitute for the desktop application's modules.

If these modules are already on your interpreter's search path:

```sh
python3 cad/build.py --variant headless
python3 cad/verify_exports.py --variant headless
python3 cad/build.py --variant touch-8048
python3 cad/verify_exports.py --variant touch-8048
```

If the FreeCAD library directory is elsewhere, set `FREECAD_LIB` to the folder containing its `FreeCAD` module before using the matching Python interpreter. This only adds a module search path; Python and FreeCAD still need compatible versions and native libraries. A standard macOS FreeCAD installation was verified with:

```sh
export FREECAD_PYTHON="/Applications/FreeCAD.app/Contents/Resources/bin/python"
export FREECAD_LIB="/Applications/FreeCAD.app/Contents/Resources/lib"
"$FREECAD_PYTHON" cad/build.py --variant headless
"$FREECAD_PYTHON" cad/verify_exports.py --variant headless
"$FREECAD_PYTHON" cad/build.py --variant touch-8048
"$FREECAD_PYTHON" cad/verify_exports.py --variant touch-8048
```

Use the Python interpreter and module directory provided by your FreeCAD installation on Linux or Windows; those environments have not been tested for this release. No user home path is required in the source. Omitting `--variant` regenerates the default headless convenience copy in `cad/exports` and `cad/previews`.

Each build emits individual STEP/STL parts, complete STEP/FCStd assemblies, a dimension/quantity validation manifest and the preview scene. The verifier reopens every printable export and the complete assemblies. For matching FreeCAD/OCC versions, geometry and orientations are deterministic; STEP timestamps and FCStd metadata can change the binary hash between runs. The STL-preservation delivery report is a record of this release, not a command that rewrites itself on future builds.

Use `--variant headless` or `--variant touch-8048` with `render.py` to refresh that variant's previews. Rendering additionally needs VTK, NumPy and Matplotlib in the chosen Python environment. Touch dimensions override the shared parameters from `variants/touch-8048/parameters.json`.

The generator replaces its named exports. Keep manual CAD edits in a separate file if they must survive regeneration. The FreeCAD `Dimensions` spreadsheet records build values; regeneration reads `parameters.json`. Some nut entries, rail details and mounting offsets are defined in `build.py`.

The existing USB opening doubles as access to BOOT with a thin, blunt nonconductive tool. See [the access view](previews/boot-access.png). The exact board outline, button location, plugged-in cable and wiring must be checked before printing; do not force a tool against headers. No new top opening or spring-loaded plunger is required.

The [preview renderer](render.py) reads `previews/scene.json` and requires VTK, NumPy and Matplotlib. Refresh previews after rebuilding geometry.

| Parameter group | What to measure or adjust |
| --- | --- |
| `cell_*`, `fixed_*`, `moving_*` | Cell body, hole positions and fastener seats; choose lengths from measured thread depths. |
| `carrier_*`, `rim_*`, `saddle_*`, `guide_*`, `spool_*` | Rail contact, carrier clearance and guide range. `saddle_*` retains shared slope/attachment parameters; there is no full saddle block. |
| `nfc_*` | Pod, actual board holes, connector envelope, antenna spacing and vertical travel. |
| `screen_*`, `esp_*`, `hx_*` | Board fit, connectors and wiring; screen offset and ESP32 outline include estimates. |
| `boot_*`, `stop_*` | Photo-derived BOOT position and tool path; verify physical access and tune stops from physical deflection. |

## Current verification and physical gate

The current validation covers eleven single-solid exports with closed meshes and no detected mesh self-intersections. Checked printed parts and simplified component envelopes have no unintended positive-volume intersections. The report also checks 27 spool/pod combinations and nine reader placements. The nominal BOOT tool approach clears modeled components, printed parts and the 27 spool/pod configurations. Separate fastener reports check twenty guide/rail/cover clamp settings and nine pod settings. Reopening all eleven headless STL/individual STEP pairs, the FreeCAD file and the complete STEP assembly is recorded separately in the export-verification report. The touch report checks all fourteen corresponding pairs. STEP/design bounds agree within 0.001 mm and volume within 0.02 mm³; STEP/STL bounds allow 0.2 mm and relative volume allows 0.5% for tessellation differences. Actual observed differences are recorded per part.

Nominal clearances include 3 mm under the carrier ribs, 2 mm around the carrier in the cover, and 1 mm above the modeled ESP32 wiring envelope in the cover's underside pocket. The pod window is 1.2 mm thick, with a 0.8 mm antenna-side gap and a 2 mm spool-side gap. These yield 4 mm nominal antenna-to-spool separation, before any embedded-tag depth or actual board variation.

`physical_validation` remains `false`. Fit, fastener retention, stiffness, upright stability, off-center accuracy, NFC range and the cable loop's effect on zero need actual hardware. Most fasteners and the flexible loop are not modeled. The stops begin at an untuned 1 mm gap and provide no demonstrated overload protection. Follow the [assembly guide](../docs/ASSEMBLY.md) and compare repeated weights at both reader heights and guide-width limits before accepting the build.
