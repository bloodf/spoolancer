# 0.96-inch OLED and five LEDs · DC enclosure fit prototype

This additive variant keeps the upright cradle and adds five separate light openings on a fixed, inclined front pod. It uses the existing ESP32-S3 and PN532. The screenless LED version is in [../headless-led](../headless-led/NOTES.md). Previous headless and touchscreen release files are preserved.

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

## OLED fit assumptions

The user confirmed a 0.96-inch 128×64 display, but has not confirmed its controller, PCB dimensions, glass position, hole pattern or headers. This prototype uses a **27.3 × 27.3 × 1.6 mm PCB**, 26 × 15 × 3 mm glass envelope, 24 × 14 mm viewing window and 21.74 × 11.18 mm active-area reference. The PCB is supported at its outer top/bottom edges, with light rear foam shims; there is **no assumed PCB screw-hole pattern**. Do not clamp the glass. Rear pads include a 0.3 mm nominal gap for a compressible pad; check the stack before tightening.

These are design assumptions, not measurements of the user's display. [Winstar's WEA012864D-03](https://www.winstar.com.tw/products/oled-module/graphic-oled-display/4-pin-oled.html) publishes a 27.3 mm square PCB, while [Joy-IT's SBC-OLED01](https://www.joy-it.net/en/products/SBC-OLED01) publishes 27 × 27 × 11 mm. Neither identifies the received module. These differing primary sources are why the CAD remains adjustable. Controller and pin order must be checked separately before powering it.

Print `fit_oled_face` to check the glass opening and edge pads, then verify PCB placement, the rear pins, wire exit and gently retained fit with the complete `indicator_back`. A passing front coupon alone does not prove the rear/header fit. The firmware choice must match the actual controller.

## Prints and assembly

Use the individual files in [exports](exports); do not print the complete assembly STEP. STEP/STL pairs share their intended print pose. The complete `spoolancer-v0.5-oled-led.FCStd` and `*-assembly.step` include reference bodies for editing and inspection.

Print the shared carrier, side guide, NFC guide, pod and pod cover once; print the rim rail twice. Print the new base, top cover, indicator fascia and indicator back once. This makes **eleven production pieces**, ten unique production models. Shared cradle geometry is unchanged.

Compared with the headless fastener list, replace the two front M3×10 closure screws with **two M3×16** screws because the bracket adds 3 mm. Use **four M2.5×10** screws for the indicator rear cover's pilot holes, designed as provisional plastic threads; tighten lightly and test retention in the intended print material. Existing rear closure screws remain M3×10. These modeled lengths still need a real assembly check.

Fit samples reuse the actual production geometry:

- `fit_oled_face`: provisional OLED viewing opening and edge supports.
- `fit_led_face`: all five holes and their spacing.
- `fit_led_carrier`: rear strip support, width and coating clearance.
- `fit_esp32`: shifted rails and underside clearance; this small coupon does not reproduce the complete closed USB wall.
- `fit_cell` and `fit_nfc`: shared sensor and reader fit samples.

The fascia is exported face-down. Its integrated mounting bridge is elevated in this orientation and **requires local slicer support**. The back is flat-down; inspect bridges and strip supports in the slicer. Ordinary PETG is a starting material, not a tested print profile. No current filament mass estimate is published.

## Generated geometry results

The unloaded printed assembly occupies **170 × 201.48 × 184 mm**, before feet and reader-height adjustment. The main base remains 170 × 160 mm. A modeled 200 mm spool brings depth to **226.48 mm**. These are CAD envelopes, not verified hardware fit or tipping stability.

The directory contains **16 individual STEP/STL pairs**: ten unique production parts and 6 fit samples. The current digital checks pass for valid single solids, closed meshes, no detected mesh self-intersections, no unintended printed/component collisions, 27 spool/reader combinations, nine reader placements and the dedicated BOOT tool path. The export verifier separately checks the two new front M3×16 clamp assemblies. Read the linked JSON reports for exact values; physical validation remains false.

## Reproduce and inspect

Use a Python interpreter compatible with FreeCAD, as described in [the CAD guide](../../README.md):

```sh
python3 cad/build.py --variant oled-led
python3 cad/verify_exports.py --variant oled-led
python3 cad/render.py --variant oled-led
```

[validation.json](exports/validation.json) records the actual envelope, intersections and 27 modeled spool/reader combinations. [export-verification.json](exports/export-verification.json) reopens the individual STEP/STL files and assembled FreeCAD/STEP files. Digital geometry checks do not prove physical fit, stiffness, tipping stability, accuracy, electronics or electrical safety.
