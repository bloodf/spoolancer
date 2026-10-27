# v0.4.1 headless upright prototype · onboard BOOT

The headless build uses the existing ESP32-S3 N16R8, PN532, HX711 and 80 mm load cell. The ESP32's onboard BOOT switch handles recovery. There is no separate switch, perfboard, switch wiring, printed cap or button-mount sample. The web interface provides normal controls; firmware behavior is documented separately.

The v0.4 delivery ZIP and the historical v0.3 package remain unchanged. This revision replaces the current headless exports only. The touch variant keeps its existing geometry.

## Dimensions and comparison

| Quantity | v0.3 compact | v0.4.1 headless |
|---|---:|---:|
| Fixed base footprint | 170 × 190 mm | 170 × 160 mm |
| Footprint area | 32,300 mm² | 27,200 mm² |
| Quantity-inclusive CAD solid volume | 444,606.27 mm³ | 415,788.54 mm³ |
| Reference loaded depth, 200 mm spool | 215 mm | 200 mm |
| Reference loaded height including nominal feet | 256.87 mm | 256.87 mm |

The base footprint is 15.79% smaller than v0.3 and 37.76% smaller than v0.2. Quantity-inclusive analytic CAD volume is 6.48% below v0.3 and 53.05% below v0.2. These are solid volumes, not filament weights. The total includes nine printed instances and excludes fit samples.

The earlier 392.32 g PETG estimate belongs to v0.4's external-switch design. It is not a current v0.4.1 slicing result; this revision has not been re-sliced.

All printed parts fit within 170 × 160 × 184 mm at the nominal/lowest reader positions; fully raising the reader reaches 198.4 mm. Nominal feet raise these heights to 185.4 and 199.8 mm. The fixed mast does not telescope. The 160 / 200 / 220 mm spool cases occupy 165 / 200 / 220 mm total depth. Spool width target remains 50–100 mm.

## Prints and changes

There are eight unique production STLs and nine printed pieces: base, blank top cover, carrier, two copies of the rim rail, two different guides, reader pod and pod cover. The three fit samples are `fit_esp32.stl`, `fit_nfc.stl` and `fit_cell.stl`, for eleven unique STL exports in total. Every STL has a matching individual `.step` containing one analytic solid in the same print orientation; use STEP for CAD edits or a compatible slicer, and STL for general slicer compatibility.

Only `top_cover.stl` changes relative to the previous headless package. Its external-switch hole and underside switch mounts are removed; the blank cover is 3 mm high in print orientation. The old `button_cap.stl` and `fit_button.stl` are removed from current outputs. The base, carrier, rails, guides and reader parts retain their geometry. The six shared mechanism files remain byte-for-byte identical to completed v0.3.

The assembly exports are `spoolancer-v0.4.1-headless.FCStd` and `spoolancer-v0.4.1-headless-assembly.step`.

## BOOT access through the USB opening

The existing left-side USB opening is 36 mm wide along Y and 29 mm high, from Z = 7 to 36 mm. Use it to approach the onboard BOOT switch above the USB connectors with a thin, blunt nonconductive tool. No extra enclosure hole or plunger is needed. See `previews/boot-access.png`; the translucent housing in that image reveals the tool approach, not a transparent printed part.

The nominal approach uses a 2.5 mm diameter tool from X/Y/Z = −94/−18/33 mm toward −51/−18/15.6 mm. BOOT's XY position and switch height are photo estimates, not received-board measurements. The approach clears modeled components and all printed parts, including the carrier, and does not require spool removal in the nominal model.

**Before printing the full enclosure, install the actual ESP32 and plugged-in USB cable in the board fit sample and check the BOOT position and safe approach.** Then verify with the complete enclosure and wiring. The USB cable overmould, headers and individual leads are not modeled. Keep that channel free; route leads to the sides. Do not force a tool against connectors or use a sharp metal object. If a different board places BOOT elsewhere, adjust the board/service opening before printing.

The correct button is BOOT, not RST. Use it after normal power-up; holding BOOT during power-up or reset can select the chip's download mode. The firmware README defines hold/release behavior. Mechanical validation does not demonstrate a working reset.

## Base layout and remaining hardware

The fixed base spans X = −85…85 mm and Y = −85…75 mm. Front feet are at Y = −72 mm and rear feet at Y = 62 mm. The nominal load center remains inside the support polygon; real tipping resistance has not been tested.

There is 13 mm between the cell's wired front end and the inner front wall. A modeled 18 × 12 × 10 mm lead allowance leaves 1 mm to that wall. The ESP32 wiring envelope retains 2.4 mm below carrier ribs and 1.0 mm below the relieved cover roof. The free BOOT approach is within the board's general wiring allowance: keep actual wires out of that local route.

Retained clamping joints:

- Guides and rails: four M3×12 screws each, nominal 0.5 mm washers and nuts bearing under 3 mm retaining roofs.
- Cover: four M3×10 screws, nominal 0.5 mm washers and inward-facing captive nuts. Install nuts before the cover.
- Cell: two M5×12 screws with nominal 1 mm washers at the fixed end and two M4×25 with nominal 0.8 mm washers at the moving end. Check actual thread depth.
- Reader pod and stops: unchanged; use the current assembly guide. No M2 switch-board screws are required.

The loose reader cable loop remains essential: the moving assembly must contact the fixed housing only through the load cell during weighing.

## Validation and next physical check

The current headless model passes eleven valid single-solid/closed-mesh checks, no detected mesh self-intersections, assembly/component clearance checks, 27 spool/pod cases and nine reader-position cases. The nominal BOOT approach has no positive-volume intersections with modeled components or printed parts. All eleven STL/individual STEP pairs, the FreeCAD document and complete STEP assembly are reopened for verification; dimensions and volumes are compared with explicit tessellation tolerances. Twenty guide/rail/cover fastener settings and nine reader-pod settings clear their intended parts.

`validation.json`, `export-verification.json`, `clamp-fastener-check.json` and `pod-fastener-check.json` record the checks. Physical fit, BOOT operation with the actual cable, printing, stiffness, loaded stability, NFC range, overload settings and cable-induced weighing bias remain untested.

## Optional touchscreen

The ESP-8048S043C-I variant remains in `cad/variants/touch-8048/` with its existing bezel, stand, bridge, exports and 170 × 205.73 mm unloaded footprint. It shares the blank top cover and upright mechanism; its integrated controller replaces the standalone board. Its own BOOT control is reached through the open-back holder. Use its TOUCH-NOTES.md and fit sample; received-board fit remains unverified.
