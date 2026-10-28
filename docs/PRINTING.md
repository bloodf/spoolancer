# Print the six-part v0.8 enclosure

The current **LED-only and OLED-plus-LED builds each need six pieces, one of each file**. Their ramps and fixed left fence are part of one broad cradle. The tower, cassette and cover all print on broad faces; there are no separate rails, sliding width guides, lid hooks, thin retaining fingers or edge-standing covers.

This revision replaces the v0.7 LED/OLED mechanism. Use one complete v0.8 set. **Actual print fit, adhesive strength and load-bearing behavior remain untested.** Original `headless` and `touch-8048` CAD are preserved legacy designs with different parts and hardware.

## Current print files

Individual STEP and STL files are already placed in their intended print orientations, at millimetre scale with the bed face at Z=0. Import at **100%** and preserve that orientation. Assembly STEP and FreeCAD files also include electronics, hardware and a spool; do not print the entire assembly.

| Part | Qty | Bed face | Print bounds, mm | LED files | OLED files |
| --- | ---: | --- | --- | --- | --- |
| Base | 1 | Broad flat underside | 170 × 170 × 32 | [STEP](../cad/variants/headless-led/exports/base.step) · [STL](../cad/variants/headless-led/exports/base.stl) | [STEP](../cad/variants/oled-led/exports/base.step) · [STL](../cad/variants/oled-led/exports/base.stl) |
| Integral cradle | 1 | Flat 8 mm floor | 155.4 × 140 × 58 | [STEP](../cad/variants/headless-led/exports/cradle.step) · [STL](../cad/variants/headless-led/exports/cradle.stl) | [STEP](../cad/variants/oled-led/exports/cradle.step) · [STL](../cad/variants/oled-led/exports/cradle.stl) |
| Moving-cell spacer | 1 | Broad flat bottom | 28 × 27 × 15.8 | [STEP](../cad/variants/headless-led/exports/cell_spacer.step) · [STL](../cad/variants/headless-led/exports/cell_spacer.stl) | [STEP](../cad/variants/oled-led/exports/cell_spacer.step) · [STL](../cad/variants/oled-led/exports/cell_spacer.stl) |
| NFC tower | 1 | Front panel down, tray upward | 153.3 × 100 × 30 | [STEP](../cad/variants/headless-led/exports/nfc_tower.step) · [STL](../cad/variants/headless-led/exports/nfc_tower.stl) | [STEP](../cad/variants/oled-led/exports/nfc_tower.step) · [STL](../cad/variants/oled-led/exports/nfc_tower.stl) |
| NFC cassette | 1 | Closed back down, antenna opening upward | 47.2 × 82 × 11.4 | [STEP](../cad/variants/headless-led/exports/nfc_cassette.step) · [STL](../cad/variants/headless-led/exports/nfc_cassette.stl) | [STEP](../cad/variants/oled-led/exports/nfc_cassette.step) · [STL](../cad/variants/oled-led/exports/nfc_cassette.stl) |
| NFC rear cover | 1 | Entire flat face | 147.3 × 100 × 3 | [STEP](../cad/variants/headless-led/exports/nfc_cover.step) · [STL](../cad/variants/headless-led/exports/nfc_cover.stl) | [STEP](../cad/variants/oled-led/exports/nfc_cover.step) · [STL](../cad/variants/oled-led/exports/nfc_cover.stl) |

The base is the only part that differs between these two current variants. Its optional OLED window is integrated into the vertical front wall. Five LED holes are 7.4 mm diameter at 16.667 mm pitch. No USB or DC-jack hole is supplied; the jack hole is drilled after checking actual hardware clearance.

## Slicing and fit

The broad bed faces and sloping cover-boss supports are intended to avoid external supports. This is a design intent, not proof that every material/printer combination will print cleanly. Inspect the actual layer preview, especially front-wall openings, load-cell counterbores, stop nut seats and the tower's small nut-loading passages. Do not add supports throughout the open trays by default.

| Setting | Starting point |
| --- | --- |
| Material | Ordinary unfilled PETG, including the antenna panel |
| Layer height | 0.2 mm |
| Perimeters | 4 |
| Top/bottom layers | 5 |
| Infill | 20–25% |
| Bed area | At least 170 × 170 mm usable area, plus brim or slicer clearance |
| Scale | 100% |

These settings are not a qualified print profile. Check cooling, first-layer expansion, bridge extrusion and bed adhesion in your own slicer. The ramps are solid CAD bodies so the slicer can apply its infill; CAD solid volume is not filament mass. Any slicer estimate must name its profile and must not be reported as a weighed print.

Before final bonding, dry-fit the six pieces and actual electronics. Check the cassette slides freely over the full 85–170 mm reader-center range, both nylon clamp heads sit flush, the cover nuts load from the sides, and the tower seats in its glue nest without rocking. Check the real board's BOOT button against the 6 mm top access hole. The generic PCB, glass, connector and strip-width envelopes are provisional.

Do not use the previous LED/OLED fit samples to qualify this redesigned mechanism. Current production files are the fit basis; printing a cropped section in your slicer can reduce the cost of checking a particular interface. A section does not establish full-part strength or assembled behavior.

## Assembly and editable files

Use the [12-screw bill of materials](BOM.md#current-ledoled-fasteners), [assembly sequence](ASSEMBLY.md) and [single glue-joint guide](GLUE-ASSEMBLY.md). The [3D and exploded views](ASSEMBLY-VIEW.md) show placement, not printer-bed orientation.

| Build | Complete assembly STEP | Editable FreeCAD |
| --- | --- | --- |
| LED-only | [STEP](../cad/variants/headless-led/exports/spoolancer-v0.8-headless-led-assembly.step) | [FreeCAD](../cad/variants/headless-led/exports/spoolancer-v0.8-headless-led.FCStd) |
| OLED + LEDs | [STEP](../cad/variants/oled-led/exports/spoolancer-v0.8-oled-led-assembly.step) | [FreeCAD](../cad/variants/oled-led/exports/spoolancer-v0.8-oled-led.FCStd) |

## Legacy designs

The preserved [original headless exports](../cad/variants/headless/exports/) and [integrated-touch exports](../cad/variants/touch-8048/exports/) retain their earlier mechanisms. Their [v0.7-era print manifest](https://github.com/bloodf/spoolancer/blob/v0.7.0-alpha.1/docs/PRINTING.md#shared-production-parts) describes those unchanged parts. They are not six-part v0.8 builds and must not be mixed with them. The prior LED/OLED v0.7 files remain available from their tagged release for existing builders.

## Offline slice comparison

The [recorded P1S/U1 diagnostic](../analysis/printability-v0.8/README.md) successfully sliced all six parts on their exported bed faces with automatic normal supports enabled. Cradle, spacer, cassette and cover generated no supports. Base and tower require local support cleanup around windows, counterbores, slots and nut pockets. Do not describe the entire build as support-free.

The new set estimates 511–521 g of PETG with these profiles: about 15–16% more than v0.7, with 83–86% less support extrusion. The thicker construction prioritizes fewer, sturdier parts; physical strength still needs testing. The bundled P1S PETG profile reports a bed-temperature warning. Inspect and use temperatures appropriate for your actual filament rather than treating the diagnostic profile as printer-ready G-code.
