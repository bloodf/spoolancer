# Print files and quantities

Choose a variant and print its fit samples first. **The enclosure has CAD checks but no physical print or fit validation yet.** Use the [hardware report](HARDWARE-VALIDATION.md) to record your build.

Individual `.step` files contain editable solid parts for CAD tools and slicers that support STEP. Individual `.stl` files are the mesh alternative supported by most slicers. Each part uses its intended print orientation and millimetre dimensions. Import at 100% scale and check the bounds before slicing.

Assembly STEP files include simplified electronics and reference bodies. Use the individual parts below for printing. No printer-specific G-code is supplied; slice for your printer, nozzle, material and build surface.

## Choose your complete set

| Build | Production files | Pieces to print | Fit samples | Folder |
| --- | ---: | ---: | ---: | --- |
| Headless v0.4.1 | 8 STEP/STL pairs | 9, including two rim rails | 3 pairs | [headless exports](../cad/variants/headless/exports/) |
| ESP-8048S043C-I touch | 11 STEP/STL pairs | 12, including two rim rails | 3 pairs | [touch exports](../cad/variants/touch-8048/exports/) |

Download a complete [headless print pack](https://github.com/bloodf/spoolancer/releases/download/v0.4.1-alpha.1/spoolancer-v0.4.1-alpha.1-headless-print.zip) or [touch print pack](https://github.com/bloodf/spoolancer/releases/download/v0.4.1-alpha.1/spoolancer-v0.4.1-alpha.1-touch-8048-print.zip). The [release page](https://github.com/bloodf/spoolancer/releases/tag/v0.4.1-alpha.1) also supplies the full source package and checksums.

Fit samples are additional to the production count, except a successful `fit_nfc` can be reused as the production `nfc_pod`. The headless model uses onboard BOOT and has no printed button cap. Historical `archive/` designs are separate revisions.

## Shared production parts

Use the files from your chosen variant's column. Quantities apply to either build.

| Part | Qty | Print pose | Bounds, mm | Headless files | Touch files |
| --- | ---: | --- | --- | --- | --- |
| Base | 1 | Floor down | 170 × 160 × 35 | [STEP](../cad/variants/headless/exports/base.step) · [STL](../cad/variants/headless/exports/base.stl) | [STEP](../cad/variants/touch-8048/exports/base.step) · [STL](../cad/variants/touch-8048/exports/base.stl) |
| Top cover | 1 | Exterior face down | 170 × 160 × 3 | [STEP](../cad/variants/headless/exports/top_cover.step) · [STL](../cad/variants/headless/exports/top_cover.stl) | [STEP](../cad/variants/touch-8048/exports/top_cover.step) · [STL](../cad/variants/touch-8048/exports/top_cover.stl) |
| Carrier | 1 | Upper face down | 140 × 130 × 25.3 | [STEP](../cad/variants/headless/exports/carrier.step) · [STL](../cad/variants/headless/exports/carrier.stl) | [STEP](../cad/variants/touch-8048/exports/carrier.step) · [STL](../cad/variants/touch-8048/exports/carrier.stl) |
| Rim rail | 2 | Outer end face down | 140 × 27.53 × 31 each | [STEP](../cad/variants/headless/exports/rim_rail.step) · [STL](../cad/variants/headless/exports/rim_rail.stl) | [STEP](../cad/variants/touch-8048/exports/rim_rail.step) · [STL](../cad/variants/touch-8048/exports/rim_rail.stl) |
| Plain side guide | 1 | Inner spool-facing face down | 52 × 130 × 16 | [STEP](../cad/variants/headless/exports/side_guide.step) · [STL](../cad/variants/headless/exports/side_guide.stl) | [STEP](../cad/variants/touch-8048/exports/side_guide.step) · [STL](../cad/variants/touch-8048/exports/side_guide.stl) |
| NFC side guide | 1 | Inner spool-facing face down | 134 × 130 × 16 | [STEP](../cad/variants/headless/exports/nfc_guide.step) · [STL](../cad/variants/headless/exports/nfc_guide.stl) | [STEP](../cad/variants/touch-8048/exports/nfc_guide.step) · [STL](../cad/variants/touch-8048/exports/nfc_guide.stl) |
| NFC pod | 1 | Antenna window down | 46.8 × 71 × 13.6 | [STEP](../cad/variants/headless/exports/nfc_pod.step) · [STL](../cad/variants/headless/exports/nfc_pod.stl) | [STEP](../cad/variants/touch-8048/exports/nfc_pod.step) · [STL](../cad/variants/touch-8048/exports/nfc_pod.stl) |
| NFC pod cover | 1 | Flat face down | 46.8 × 49.1 × 1.6 | [STEP](../cad/variants/headless/exports/nfc_pod_cover.step) · [STL](../cad/variants/headless/exports/nfc_pod_cover.stl) | [STEP](../cad/variants/touch-8048/exports/nfc_pod_cover.step) · [STL](../cad/variants/touch-8048/exports/nfc_pod_cover.stl) |

The guides are different parts. Print two copies of the same rim rail, then rotate the second one during assembly so the slopes face each other.

## Additional touch parts

The touch build adds these parts to the shared nine pieces. Its controller mounts to the fixed base, separate from the weighing cradle.

| Part | Qty | Print pose | Bounds, mm | Files |
| --- | ---: | --- | --- | --- |
| Touch bezel | 1 | Outer face down | 139 × 82 × 5.8 | [STEP](../cad/variants/touch-8048/exports/touch_bezel.step) · [STL](../cad/variants/touch-8048/exports/touch_bezel.stl) |
| Touch stand | 1 | Connected feet down | 140 × 49 × 77.12 | [STEP](../cad/variants/touch-8048/exports/touch_stand.step) · [STL](../cad/variants/touch-8048/exports/touch_stand.stl) |
| Touch bridge | 1 | Flat | 164 × 23 × 3 | [STEP](../cad/variants/touch-8048/exports/touch_bridge.step) · [STL](../cad/variants/touch-8048/exports/touch_bridge.stl) |

## Fit samples

Print one of each sample for your chosen build before the full set.

| Sample | Headless files | Touch files | Check |
| --- | --- | --- | --- |
| Controller mount | [STEP](../cad/variants/headless/exports/fit_esp32.step) · [STL](../cad/variants/headless/exports/fit_esp32.stl) | [STEP](../cad/variants/touch-8048/exports/fit_touch_mount.step) · [STL](../cad/variants/touch-8048/exports/fit_touch_mount.stl) | Headless board rails and underside; touch board-hole pitch, nut seats and PCB/glass stack. Actual USB and BOOT access also need the full enclosure. |
| NFC pod | [STEP](../cad/variants/headless/exports/fit_nfc.step) · [STL](../cad/variants/headless/exports/fit_nfc.stl) | [STEP](../cad/variants/touch-8048/exports/fit_nfc.step) · [STL](../cad/variants/touch-8048/exports/fit_nfc.stl) | Board holes, nylon fasteners, components, wire exit and tag operation through the window. Reuse as the production pod if successful. |
| Load-cell mount | [STEP](../cad/variants/headless/exports/fit_cell.step) · [STL](../cad/variants/headless/exports/fit_cell.stl) | [STEP](../cad/variants/touch-8048/exports/fit_cell.step) · [STL](../cad/variants/touch-8048/exports/fit_cell.stl) | Hole positions and screw clearance. Measure thread depths separately; this does not test load-bearing behavior. |

## Slicer starting point

| Setting | Initial choice |
| --- | --- |
| Material | Ordinary unfilled PETG, including the antenna window |
| Layer height | 0.2 mm |
| Perimeters | 4 |
| Top/bottom layers | 5 |
| Infill | 20–25% |
| Scale and orientation | 100%, keep exported pose |
| Bed space | At least 170 × 160 mm usable area, plus brim or slicer clearance |

These are starting settings, not a qualified print profile. Inspect short nut-pocket bridges, holes, the touch stand's tilted pads and connector reliefs. Support-free printing has not been established. Select support and brim settings after inspecting your slicer's preview.

The old v0.4 headless estimate of 392.32 g includes removed button parts and is not the current v0.4.1 mass. The unchanged touch geometry has a historical 427.29 g PETG estimate. Both are slicer estimates, not weighed prints; fit samples and printer-specific changes are excluded. See the [material comparison](../analysis/material-comparison/README.md) for its recorded profile.

## Editable assemblies

| Build | Assembly STEP | Native FreeCAD |
| --- | --- | --- |
| Headless | [spoolancer-v0.4.1-headless-assembly.step](../cad/variants/headless/exports/spoolancer-v0.4.1-headless-assembly.step) | [spoolancer-v0.4.1-headless.FCStd](../cad/variants/headless/exports/spoolancer-v0.4.1-headless.FCStd) |
| Touch | [spoolancer-v0.4-touch-8048-assembly.step](../cad/variants/touch-8048/exports/spoolancer-v0.4-touch-8048-assembly.step) | [spoolancer-v0.4-touch-8048.FCStd](../cad/variants/touch-8048/exports/spoolancer-v0.4-touch-8048.FCStd) |

The [CAD guide](../cad/README.md#edit-and-regenerate) explains regeneration from parameters. Record changes to clearances or fasteners with your print results so another builder can reproduce them.
