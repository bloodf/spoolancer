# Spoolancer enclosure prototype

Print the small fit samples before the full enclosure. This is an editable CAD prototype for the selected ESP32-S3, PN532, Waveshare display and 80 mm load cell. No physical fit, weighing accuracy or NFC performance has been verified, and no firmware has been implemented.

The enclosure has three main prints: a fixed base, a floating tray and a removable front panel. The front panel forms a curved deck around the front of the tray, with a nominal 2 mm radial gap. Three push-on button caps are optional. Read the [assembly guide](../docs/ASSEMBLY.md) before buying fasteners or printing the complete parts.

View the CAD previews: [assembled](previews/assembled.png), [exploded](previews/exploded.png), [interior](previews/interior.png) and [load path](previews/load-path.png).

## Files to open or print

| File | Use |
| --- | --- |
| [spoolancer-v0.1.FCStd](exports/spoolancer-v0.1.FCStd) | Open the assembled design in FreeCAD. `PrintedParts` contains printable solids; `ReferenceEnvelopes` contains approximate hardware bodies. |
| [spoolancer-v0.1-assembly.step](exports/spoolancer-v0.1-assembly.step) | Import the assembly into another CAD tool. It includes hardware reference bodies, so do not send the whole assembly to a slicer. |
| [parameters.json](parameters.json) | Edit dimensions, clearances and component positions, then regenerate. All dimensions are millimetres. |
| [build.py](build.py) | Generates the FreeCAD document, STEP assembly, individual STLs and validation report. |
| [validation.json](exports/validation.json) | Records geometry checks and print bounds for the current exports. |

The geometry is original to this project. No third-party CAD model is incorporated. The reference bodies are simplified clearance estimates, not manufacturer models. Their sources and unresolved measurements are in [MECHANICAL-SOURCES.md](../docs/MECHANICAL-SOURCES.md).

### Fit samples first

| Sample | Check against the actual part |
| --- | --- |
| [fit_esp32.stl](exports/fit_esp32.stl) | Board rails, underside clearance, USB openings and a connected cable |
| [fit_display.stl](exports/fit_display.stl) | Screen holes, bezel alignment, glass height and rear connector |
| [fit_nfc.stl](exports/fit_nfc.stl) | PN532 hole pattern, nylon fasteners, header clearance and tag reading through the scan face |
| [fit_buttons.stl](exports/fit_buttons.stl) | Button strip mounts, actuator alignment, full release and optional caps |
| [fit_cell.stl](exports/fit_cell.stl) | Four load-cell hole positions and screw clearance; this flat sample does not validate the complete load path |

Change the relevant parameters and repeat only the affected fit sample before printing the complete enclosure.

### Main prints

The STL files already have the intended printing orientation, with their lowest point at Z = 0.

| Quantity | STL | Orientation already applied | Reported print bounds, mm |
| --- | --- | --- | --- |
| 1 | [base.stl](exports/base.stl) | Base floor on the bed | 200 × 248 × 35 |
| 1 | [tray.stl](exports/tray.stl) | Spool-facing top surface down; ribs upward | 196 × 195.92 × 25.3 |
| 1 | [front_panel.stl](exports/front_panel.stl) | Exterior top face down; bosses upward | 200 × 122 × 6.5 |
| 3, optional | [button_cap.stl](exports/button_cap.stl) | Round head down | 10 × 10 × 3.3 each |

The nominal tray diameter is 196 mm, intended for a centered spool of about 200 mm diameter. The assembled printed body occupies **200 × 248 × 50 mm**. The nominal height with the specified feet is **51.4 mm**. The base print alone is 35 mm high.

The display opening is 43 × 34 mm at the underside and flares at 45 degrees to 43 × 40 mm at the top face for viewing the recessed screen. Check bezel alignment against the actual display with the fit sample.

The printer needs at least the base's 200 × 248 mm footprint plus clearance for its usable-bed limits, skirt or brim. Check the actual printer's usable bed and slicer profile.

Start with ordinary PETG, 0.2 mm layers, four perimeters, five top and bottom layers and about 20–25% infill. These are starting settings, not a tested print profile. Use unfilled filament for the NFC panel and avoid carbon-fiber-filled material for this first build. Inspect the slicer preview and use the fit samples to assess holes, bridging and dimensional error before the larger prints.

## Edit and regenerate

From the project root on this Mac, run:

```sh
/Applications/FreeCAD.app/Contents/Resources/bin/python cad/build.py
```

The command uses the installed FreeCAD Python runtime and replaces the named files in `cad/exports`. Keep manual CAD edits in a separate file if they must survive regeneration. The FreeCAD document's `Dimensions` spreadsheet records the parameter values used for that build; geometry regeneration reads `parameters.json`.

The [preview renderer](render.py) reads the generated `previews/scene.json`. It requires a Python environment with VTK, NumPy and Matplotlib; run it after a geometry rebuild to refresh the PNG previews.

| Parameter group | What to measure or adjust |
| --- | --- |
| `cell_*`, `fixed_*`, `moving_*` | Cell body, hole positions and fastener seats; choose screw lengths from the actual thread depths |
| `screen_*` | PCB size, hole spacing, window dimensions and window offset; the current 2 mm horizontal offset is a photo estimate |
| `nfc_*` | Owned PN532 size and hole spacing, scan-face thickness, antenna gap and wire routing |
| `esp_*`, `hx_*` | Board envelope, rail fit, USB position, underside components and wire height |
| `button_*`, `stop_*` | Button strip, actuator/cap fit and adjustable stops; tune stops from measured deflection |

Some construction details, including mounting-hole offsets and rail geometry, are defined in `build.py`. Editing a component envelope alone does not prove its connectors or wires will fit.

## Current verification

The current [validation report](exports/validation.json) records nine valid, single-solid exports with closed meshes and no detected mesh self-intersections. The checked printed-part pairs, load cell and simplified hardware envelopes have no positive-volume intersections. Nominal clearances include 3 mm below the tray ribs, 2 mm between tray and front panel in plan, and 2.4 mm above the estimated ESP32 wiring allowance.

These checks cover modeled geometry only. The report explicitly sets `physical_validation` to `false`. Hole fit, screw engagement, unsupported wire or connector details, tray motion, NFC range and scale accuracy still require the actual hardware. The stop gap starts at 1 mm and is untuned; it is not evidence of overload protection.

Next, use the [assembly guide](../docs/ASSEMBLY.md) to fit the samples and record the actual measurements before the full prints.
