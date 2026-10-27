# Five LEDs and web controls · DC enclosure fit prototype

This additive variant keeps the upright cradle and adds five separate light openings on a fixed, inclined front pod. It uses the existing ESP32-S3 and PN532. The OLED version is in [../oled-led](../oled-led/NOTES.md). Previous headless and touchscreen release files are preserved.

**This is a fit prototype.** The 60 LEDs/metre pitch is user-confirmed. Strip width, coating, LED package, board dimensions, USB plug, power converter, jack, fastening and all physical behavior remain unverified. Print the fit samples before a full base. Parameters are editable in [parameters.json](parameters.json), the common [generator](../../build.py) and [indicator geometry](../../indicators.py).

## Power and service access

The new base has **no external USB opening and no pre-cut DC jack hole**. The user will drill the jack opening after checking its body and wires inside the enclosure. Keep clear of the fixed load-cell mount, overload posts, electronics, carrier and its motion. A separate 12 V-to-5 V converter is required; do not feed 12 V into the ESP32 or the 5 V strip. No dedicated converter bracket or hole pattern is provided; the owner intends to fix it to a wall.

A translucent 48 × 40 × 22.4 mm reference identifies one unassigned internal clearance volume. It does not identify a selected converter or prove its mounting, airflow, electrical insulation or cable fit. Keep any adhered module and wires out of the scale's moving structure.

For these new variants only, the ESP32 rails move 8 mm inward and 5.5 mm toward the front. A **provisional** 10 × 22 × 12 mm internal USB plug envelope is checked in the model, with 11.35 mm from PCB edge to the inside of the closed wall. Actual right-angle plug fit is not established. The new wiring allowance is 55.8 mm long, centered 1 mm toward USB from the PCB center, and 25 mm high above the PCB. This assumes 22 positions at 2.54 mm pitch: 21 intervals span 53.34 mm, plus 2.46 mm total end allowance. It leaves 1.1 mm to the modeled moving-cell mount. The PCB has 2 mm nominal separation from the load-cell body, but that does not include unknown protruding solder or cable bends. Keep all headers, solder and bends within the checked envelopes and verify the actual assembly. Open the lid for programming and disconnect the converter supply before using computer USB; consult the project's power wiring guide before assembling.

A separate angled **4 mm BOOT access bore** replaces the large service opening. A modeled 2.5 mm blunt nonconductive tool follows the earlier photo-estimated path, updated for the moved board. Position and button operation need physical verification. It is not an external USB opening.

## Front pod

The face is inclined 60 degrees from horizontal and has five 6.4 mm round openings on **1000/60 = 16.6667 mm** centers. Its strip reference is **83.33 × 10 mm**, with five 5 × 5 × 1.5 mm LED packages. These are editable provisional dimensions, except the confirmed pitch. Measure the cut pads and strip coating before cutting or printing.

The removable rear part supports the strip. Use its existing adhesive or a thin insulating adhesive layer; do not load the LED packages against the face. The five LED openings are open; no untested transparent light pipes or sealing claim is made. Light bleed and actual diffusion need a physical check.

The pod attaches to the **fixed front cover posts**. It does not attach to the floating cradle. Its wiring passes through a separate 6 mm hole in the fixed base's front wall at x=40 mm and z=26 mm, away from the load-cell lead allowance. Secure slack to the fixed enclosure so these wires cannot pull on the moving carrier.

## Prints and assembly

Use the individual files in [exports](exports); do not print the complete assembly STEP. STEP/STL pairs share their intended print pose. The complete `spoolancer-v0.6-headless-led.FCStd` and `*-assembly.step` include reference bodies for editing and inspection.

Print the shared carrier, side guide, NFC guide, pod and pod cover once; print the rim rail twice. Print the new base, top cover, indicator fascia and indicator back once. This makes **eleven production pieces**, ten unique production models. Shared cradle contact, sliding and mounting dimensions are retained; exposed edges now have modeled rounds and chamfers.

Compared with the headless fastener list, replace the two front M3×10 closure screws with **two M3×16** screws because the bracket adds 3 mm. Use **four M2.5×10** screws for the indicator rear cover's pilot holes, designed as provisional plastic threads; tighten lightly and test retention in the intended print material. Existing rear closure screws remain M3×10. These modeled lengths still need a real assembly check.

Fit samples reuse the actual production geometry:

- `fit_led_face`: all five holes and their spacing.
- `fit_led_carrier`: rear strip support, width and coating clearance.
- `fit_esp32`: shifted rails and underside clearance; this small coupon does not reproduce the complete closed USB wall.
- `fit_cell` and `fit_nfc`: shared sensor and reader fit samples.

The fascia is exported face-down. Its integrated mounting bridge is elevated in this orientation and **requires local slicer support**. The back is flat-down; inspect bridges and strip supports in the slicer. Ordinary PETG is a starting material, not a tested print profile. No current filament mass estimate is published.

## Rounded geometry · v0.6

The softer edges are real analytic CAD geometry in the STEP, FreeCAD and STL exports. No render-only smoothing is used as a substitute. All treatments remove material; they do not expand the old collision envelope. The generator selects edges by explicit geometric bounds and fails if a requested treatment is missing or invalid. The validation file records each selected edge, radius/chamfer and before/after volume.

| Location | Treatment |
| --- | --- |
| Base lower / upper outer edges | R1.0 / R0.65 mm |
| Cover upper outside edge / carrier aperture | R0.8 mm / 0.45 mm chamfer |
| Carrier lower exposed edges | R0.6 mm |
| Rim rail outer end perimeters | R1.2 mm, outside the 100 mm spool-width envelope |
| Plain guide upper outside edge | R1.2 mm |
| NFC mast outer long corners / top edges | R1.5 / R0.6 mm |
| NFC pod and back cover outside corners | R2.2 mm |
| NFC rear cover outside rim | R0.45 mm |
| Indicator face outside corners / front rim | R3.0 / R0.7 mm |
| LED and OLED opening front edges | 0.4 mm chamfer; original throat size retained |
| Indicator back outside corners / rear rim | R1.0 / R0.4 mm |

Load-cell bearing pads, bolt seats, nut pockets, sliding feet and the central 1.2 mm NFC antenna face remain unchanged. The [before/after geometry report](../../edge-finish-verification.json) confirms zero removed material in 24 protected regions for this variant, including the full PCB-sized antenna window, load-cell supports, sliding feet, clamp seats and indicator opening throats. Rail contact slopes remain unchanged within the intended spool width. Structural web edges, inner corners, screw bores and narrow wire passages deliberately remain sharp where rounding would weaken walls or alter fit. OLED support pads and the glass seat remain unchanged. Print profiles and physical stiffness are still untested.

Twelve simplified M3 clamp sets are now visible in the assembly: four cover joints, four rim-rail joints and four side-guide joints. They use the previously checked head/shaft/washer/nut dimensions; threads and screw drives are omitted. The four existing stop references remain. This is **partial hardware visualization**: load-cell, NFC, indicator rear fasteners, cable ties and adhesives are not included.

## Generated geometry results

The unloaded printed assembly occupies **170 × 186.48 × 184 mm**, before feet and reader-height adjustment. The main base remains 170 × 160 mm. A modeled 200 mm spool brings depth to **211.48 mm**. These are CAD envelopes, not verified hardware fit or tipping stability.

The directory contains **15 individual STEP/STL pairs**: ten unique production parts and 5 fit samples. The current digital checks pass for valid single solids, closed meshes, no detected mesh self-intersections, no unintended printed/component collisions, 27 spool/reader combinations, nine reader placements and the dedicated BOOT tool path. The export verifier separately checks the two new front M3×16 clamp assemblies. Read the linked JSON reports for exact values; physical validation remains false.

The total analytic solid volume including print quantities changed from **438,966.93 mm³** to **438,009.46 mm³**. This measures geometry only, not sliced filament or mass.

## Reproduce and inspect

Use a Python interpreter compatible with FreeCAD, as described in [the CAD guide](../../README.md):

```sh
python3 cad/build.py --variant headless-led
python3 cad/verify_exports.py --variant headless-led
python3 cad/render.py --variant headless-led
```

[validation.json](exports/validation.json) records the actual envelope, intersections and 27 modeled spool/reader combinations. [export-verification.json](exports/export-verification.json) reopens the individual STEP/STL files and assembled FreeCAD/STEP files. Digital geometry checks do not prove physical fit, stiffness, tipping stability, accuracy, electronics or electrical safety.
