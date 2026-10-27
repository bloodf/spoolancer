# Spoolancer v0.3 compact enclosure

Print the small fit samples before the full enclosure. This editable prototype uses the selected ESP32-S3, PN532, Waveshare display and 80 mm load cell. CAD checks and firmware compilation have passed; physical fit, weighing accuracy and NFC performance remain untested. Build instructions are in [firmware/README.md](../firmware/README.md), with daily operation in the [web-interface guide](../docs/WEB-INTERFACE.md).

The upright spool rests on two hollow rim rails. An open carrier supports both rails, two different sliding guides and the NFC reader. The reader sits in a pod on the right guide and adjusts vertically to face the seated spool's tag. A loose cable loop connects it to the fixed electronics. The intended spool range is 50–100 mm wide and 160–220 mm in diameter, subject to actual flange and tag fit. This is an occasional-weighing scale, without an axle or bearings.

See the [assembled](previews/assembled.png), [exploded](previews/exploded.png), [interior](previews/interior.png), [cradle](previews/cradle.png), [NFC detail](previews/nfc-detail.png) and [load-path](previews/load-path.png) views. The previous upright design is preserved in [archive/v0.2-upright](../archive/v0.2-upright/cad/README.md).

## Files to open

| File | Use |
| --- | --- |
| [spoolancer-v0.3.FCStd](exports/spoolancer-v0.3.FCStd) | Editable assembled FreeCAD document. Printable parts and simplified hardware references are separate groups. |
| [spoolancer-v0.3-assembly.step](exports/spoolancer-v0.3-assembly.step) | Assembly interchange file. It includes reference bodies; do not send the whole assembly to a slicer. |
| [parameters.json](parameters.json) and [build.py](build.py) | Dimensions, clearances and generator. Values are millimetres. |
| [validation.json](exports/validation.json) and [export-verification.json](exports/export-verification.json) | Modeled geometry checks and reopened-export checks. |
| [clamp-fastener-check.json](exports/clamp-fastener-check.json) and [pod-fastener-check.json](exports/pod-fastener-check.json) | Nominal guide/rail/cover clamp stacks and pod fastener clearances. |
| [COMPACT-NOTES.md](COMPACT-NOTES.md) | Geometry, fastener changes and comparison with v0.2. |

The geometry is original to this project. No third-party CAD model is incorporated. Reference bodies are simplified estimates, not manufacturer models. Published dimensions and unresolved measurements are recorded in [MECHANICAL-SOURCES.md](../docs/MECHANICAL-SOURCES.md).

## Fit samples first

| Sample | Check against the actual part |
| --- | --- |
| [fit_esp32.stl](exports/fit_esp32.stl) | Board rails and underside clearance. Check the actual USB plugs and wiring again in the full base. |
| [fit_display.stl](exports/fit_display.stl) | Screen holes, bezel alignment, glass height and rear connector. |
| [fit_nfc.stl](exports/fit_nfc.stl) | Actual pod geometry: matching PCB holes, nylon screw seats, components, compact wire exit and tag performance through the window. A successful fit can serve as the pod. |
| [fit_buttons.stl](exports/fit_buttons.stl) | Button strip mounts, actuator alignment, full release and optional caps. |
| [fit_cell.stl](exports/fit_cell.stl) | Four cell-hole positions and screw clearance; check thread depth separately. This sample does not validate the load path. |

Change the relevant parameters and repeat the affected sample before the full prints. The [assembly guide](../docs/ASSEMBLY.md) lists intended fasteners and fit checks.

## Main prints

There are **nine main pieces**, plus three optional button caps. The two guides are different parts. Each STL is already in its intended print orientation, with its lowest point at Z = 0.

| Quantity | STL | Orientation already applied | Print bounds, mm |
| --- | --- | --- | --- |
| 1 | [base.stl](exports/base.stl) | Floor down | 170 × 190 × 35 |
| 1 | [top_cover.stl](exports/top_cover.stl) | Exterior face down | 170 × 190 × 6.5 |
| 1 | [carrier.stl](exports/carrier.stl) | Upper face down | 140 × 130 × 25.3 |
| 2 | [rim_rail.stl](exports/rim_rail.stl) | Outer end face down | 140 × 27.53 × 31 each |
| 1 | [side_guide.stl](exports/side_guide.stl) | Inner spool-facing face down | 52 × 130 × 16 |
| 1 | [nfc_guide.stl](exports/nfc_guide.stl) | Inner spool-facing face down | 134 × 130 × 16 |
| 1 | [nfc_pod.stl](exports/nfc_pod.stl) | Antenna window down | 46.8 × 71 × 13.6 |
| 1 | [nfc_pod_cover.stl](exports/nfc_pod_cover.stl) | Flat face down | 46.8 × 49.1 × 1.6 |
| 3, optional | [button_cap.stl](exports/button_cap.stl) | Round head down | 10 × 10 × 3.3 each |

The base and cover need a usable print area of at least **170 × 190 mm**, plus the slicer's required clearance or brim. The unloaded assembly stays within that footprint, but its adjustable reader makes it **185.4–199.8 mm high** including the specified feet. A modeled 200 mm spool gives overall dimensions of about **170 × 215 × 256.87 mm**; the 220 mm case reaches **225 mm depth and 278.42 mm height**. These are geometry envelopes, not tested spool compatibility or stability.

Start with ordinary PETG, 0.2 mm layers, four perimeters, five top/bottom layers and 20–25% infill. This is an unvalidated starting profile. Use unfilled filament around the NFC antenna. The rail orientation avoids a long roof bridge; small nut channels and holes still require short bridging. Inspect the slicer preview before printing. The [material comparison](../analysis/material-comparison/README.md) distinguishes CAD solid volume from sliced filament estimates under its stated profile.

## Edit and regenerate

From the project root on this Mac:

```sh
/Applications/FreeCAD.app/Contents/Resources/bin/python cad/build.py
```

The generator replaces its named exports. Keep manual CAD edits in a separate file if they must survive regeneration. The FreeCAD `Dimensions` spreadsheet records build values; regeneration reads `parameters.json`. Some nut entries, rail details and mounting offsets are defined in `build.py`.

The [preview renderer](render.py) reads `previews/scene.json` and requires VTK, NumPy and Matplotlib. Refresh previews after rebuilding geometry.

| Parameter group | What to measure or adjust |
| --- | --- |
| `cell_*`, `fixed_*`, `moving_*` | Cell body, hole positions and fastener seats; choose lengths from measured thread depths. |
| `carrier_*`, `rim_*`, `saddle_*`, `guide_*`, `spool_*` | Rail contact, carrier clearance and guide range. `saddle_*` retains shared slope/attachment parameters; there is no full saddle block. |
| `nfc_*` | Pod, actual board holes, connector envelope, antenna spacing and vertical travel. |
| `screen_*`, `esp_*`, `hx_*` | Board fit, connectors and wiring; screen offset and ESP32 outline include estimates. |
| `button_*`, `stop_*` | Button fit and stop settings; tune the stops from physical deflection. |

## Current verification and physical gate

The current validation covers fourteen single-solid exports with closed meshes and no detected mesh self-intersections. Checked printed parts and simplified component envelopes have no unintended positive-volume intersections. The report also checks 27 spool/pod combinations and nine reader placements. Separate fastener reports check twenty guide/rail/cover clamp settings and nine pod settings. Reopening the STLs, FreeCAD file and STEP file is recorded separately in the export-verification report.

Nominal clearances include 3 mm under the carrier ribs, 2 mm around the carrier in the cover, and 1 mm above the modeled ESP32 wiring envelope in the cover's underside pocket. The pod window is 1.2 mm thick, with a 0.8 mm antenna-side gap and a 2 mm spool-side gap. These yield 4 mm nominal antenna-to-spool separation, before any embedded-tag depth or actual board variation.

`physical_validation` remains `false`. Fit, fastener retention, stiffness, upright stability, off-center accuracy, NFC range and the cable loop's effect on zero need actual hardware. Most fasteners and the flexible loop are not modeled. The stops begin at an untuned 1 mm gap and provide no demonstrated overload protection. Follow the [assembly guide](../docs/ASSEMBLY.md) and compare repeated weights at both reader heights and guide-width limits before accepting the build.
