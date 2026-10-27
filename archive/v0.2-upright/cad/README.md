# Spoolancer enclosure prototype

Print the small fit samples before the full enclosure. This is an editable CAD prototype for the selected ESP32-S3, PN532, Waveshare display and 80 mm load cell. No physical fit, weighing accuracy or NFC performance has been verified, and no firmware has been implemented.

The v0.2 enclosure holds the spool upright in a V-shaped saddle. It has six main printed pieces: a fixed base, a full top cover, a moving carrier, a saddle and two identical sliding side guides. The guide range is intended for spool widths from 50 to 100 mm, with an inner gap set to the measured width plus 4 mm. Adjustment uses hex-key fasteners; there is no axle or bearing. Three push-on button caps are optional. Read the [assembly guide](../docs/ASSEMBLY.md) before buying fasteners or printing the complete parts.

View the CAD previews: [assembled](previews/assembled.png), [exploded](previews/exploded.png), [interior](previews/interior.png), [upright cradle](previews/cradle.png) and [load path](previews/load-path.png). The earlier flat-spool prototype is [archived and superseded](../archive/v0.1-flat/SUPERSEDED.md).

## Files to open or print

| File | Use |
| --- | --- |
| [spoolancer-v0.2.FCStd](exports/spoolancer-v0.2.FCStd) | Open the assembled design in FreeCAD. `PrintedParts` contains printable solids; `ReferenceEnvelopes` contains approximate hardware and spool bodies. |
| [spoolancer-v0.2-assembly.step](exports/spoolancer-v0.2-assembly.step) | Import the assembly into another CAD tool. It includes reference bodies, so do not send the whole assembly to a slicer. |
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
| 1 | [base.stl](exports/base.stl) | Base floor on the bed | 190 × 230 × 35 |
| 1 | [top_cover.stl](exports/top_cover.stl) | Exterior top face down; bosses upward | 190 × 230 × 6.5 |
| 1 | [carrier.stl](exports/carrier.stl) | Upper mounting face down; ribs upward | 154 × 138 × 25.3 |
| 1 | [saddle.stl](exports/saddle.stl) | Flat underside down; V-shaped seat upward | 148 × 130 × 42.53 |
| 2 | [side_guide.stl](exports/side_guide.stl) | Inner spool-facing surface down | 65 × 130 × 16 each |
| 3, optional | [button_cap.stl](exports/button_cap.stl) | Round head down | 10 × 10 × 3.3 each |

The assembled printed mechanism occupies **190 × 230 × 120 mm**, or **121.4 mm high** with the specified feet. The carrier moves inside the cover's 158 × 142 mm opening, leaving a nominal 2 mm gap on each side. Every spool-contacting part is carried by the load cell.

The upright spool extends above and behind the printed mechanism. The modeled 200 mm diameter case occupies about **271.87 mm height × 245 mm depth**, including the feet; the 220 mm case reaches **293.42 mm height × 255 mm depth**. These are ideal circular-spool envelope calculations. Actual flange shape, stability and weighing performance require physical checks. The 100 mm limit is spool width, not enclosure width.

The display opening is 43 × 34 mm at the underside and flares at 45 degrees to 43 × 40 mm at the top face for viewing the recessed screen. Check bezel alignment against the actual display with the fit sample.

The printer needs at least the base and cover's 190 × 230 mm footprint plus clearance for its usable-bed limits, skirt or brim. Check the actual printer's usable bed and slicer profile.

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
| `carrier_*`, `saddle_*`, `guide_*`, `spool_*` | Moving assembly, guide adjustment and reference spool envelopes; verify actual flange contact and upright stability |
| `screen_*` | PCB size, hole spacing, window dimensions and window offset; the current 2 mm horizontal offset is a photo estimate |
| `nfc_*` | Owned PN532 size and hole spacing, scan-face thickness, antenna gap and wire routing |
| `esp_*`, `hx_*` | Board envelope, rail fit, USB position, underside components and wire height |
| `button_*`, `stop_*` | Button strip, actuator/cap fit and adjustable stops; tune stops from measured deflection |

Some construction details, including mounting-hole offsets and rail geometry, are defined in `build.py`. Editing a component envelope alone does not prove its connectors or wires will fit.

## Current verification

The current [validation report](exports/validation.json) records eleven valid, single-solid exports with closed meshes and no detected mesh self-intersections. The checked printed-part pairs, load cell and simplified hardware envelopes have no positive-volume intersections. It also checks nine ideal spool envelopes, combining diameters of 160, 200 and 220 mm with widths of 50, 80 and 100 mm, without positive-volume intersections. Each guide setting leaves 2 mm nominal clearance per side.

Nominal mechanism clearances include 3 mm below the carrier ribs, 2 mm between carrier and cover in plan, and 2.4 mm between the carrier ribs and estimated ESP32 wiring envelope. A local underside pocket in the cover leaves 1 mm above the modeled 25 mm wiring allowance and a 1.4 mm roof. Check actual jumper housings and wire bends against that pocket before closing the cover.

These checks cover modeled geometry only. The report explicitly sets `physical_validation` to `false`. Hole fit, screw engagement, unsupported wire or connector details, carrier motion, guide retention, upright stability, NFC range and scale accuracy still require the actual hardware. The stop gap starts at 1 mm and is untuned; it is not evidence of overload protection.

Next, use the [assembly guide](../docs/ASSEMBLY.md) to fit the samples and record the actual measurements before the full prints.
