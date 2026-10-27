# Mechanical dimensions and assumptions

Research recorded 26 October 2026. This document supports an enclosure CAD prototype. No received module has been measured, fitted to a print or physically tested. Published dimensions, photo estimates and design allowances have different levels of confidence.

## How to use the dimensions

Use dimensions explicitly printed in a source as nominal values. Keep inferred positions and fit allowances editable in CAD. Check the actual parts and print small mounting or bezel samples before accepting the complete enclosure. A successful CAD render does not establish physical fit, weighing accuracy or NFC performance.

The user wants a standalone, open-source scale for occasional weighing, NFC reading/writing and optional configured Spoolman synchronization. The current v0.2 requirement is an upright spool with a maximum width of 100 mm. A V-shaped saddle and two sliding side guides replace the superseded flat-spool design in [archive/v0.1-flat](../archive/v0.1-flat/SUPERSEDED.md). The first-version target remains up to 2 kg gross spool weight; the 5 kg sensor rating also includes the moving carrier, saddle, guides and their fasteners.

## Upright mechanism design assumptions

These are dimensions chosen for the v0.2 prototype, not measured hardware specifications. The generator and its validation report are the current geometry record.

| Detail | Proposed design value | Physical check still needed |
| --- | --- | --- |
| Base | 190 × 230 × 35 mm; top-cover upper face at Z = 38 mm | Print-bed clearance, stiffness and stable support on the feet |
| Moving carrier | 154 × 138 mm; upper face at Z = 50 mm | Free movement inside the 158 × 142 mm cover aperture |
| V-shaped saddle | 148 × 130 mm; bottom at Z = 50 mm, apex at Z = 55 mm; slopes at 30 degrees | Contact with actual spool flanges, stiffness and retention without rocking |
| Side guides | Two identical sliding parts, reaching Z = 120 mm | Adjustment for 50–100 mm actual spool widths; set inner gap to width plus 4 mm |
| Spool envelope references | 200 mm diameter for the main reference; 220 mm for a larger clearance case | Actual flange shapes, stability and clearance; these references are not tested compatibility claims |

For an ideal circular spool resting on both 30-degree faces, its calculated top height is `55 + radius / cos(30°) + radius`, before feet. A 200 mm spool therefore reaches about 271.87 mm including the modeled 1.4 mm net foot height; the corresponding 220 mm case reaches about 293.42 mm. These loaded heights exceed the printed mechanism's nominal 121.4 mm height with feet. Allow about 245 mm total depth for the 200 mm case and 255 mm for the 220 mm case, compared with the 230 mm base depth.

Adjust the guides with hex-key fasteners. No axle or bearings are part of this occasional-weighing design. Every spool-contacting part belongs to the moving assembly so it cannot bypass the load cell through the fixed cover. Guide range, upright stability, screw retention and load-cell moment sensitivity all require physical checks.

## Selected 5 kg load cell

The selected part is [Bit Maker code 1381](https://www.bitmaker.com.br/sensor-de-peso-c-lula-de-carga-5kg). Its [seller-hosted dimension drawing](https://cdn.awsli.com.br/2500x2500/573/573703/produto/272363005/2a708ca7df3169d7045e4c1bc0fa0071-lc4nawzthd.jpeg) is the mechanical reference for this generic cell. It is not evidence that the cell is a named TAL220B revision.

| Detail | Value | Basis and limit |
| --- | --- | --- |
| Body | 80 × 12.7 × 12.7 mm | Seller dimensions |
| Mounting threads | Two M4 and two M5 holes | Seller drawing; verify which end has each size on receipt |
| Longitudinal hole spacing | 15, 40, 15 mm between successive centers | Dimension chain in the drawing |
| Outer hole offset from each end | 5 mm | Inferred from an 80 mm body, a 70 mm hole-center span and a symmetric arrangement |
| Proposed longitudinal hole coordinates | 5, 20, 60 and 75 mm from one end | Derived from that symmetry assumption; check actual cell before final printing |
| Thread depth | Unknown | The drawing's `6H` denotes an internal-thread tolerance class, not a 6 mm hole depth |

Measure usable thread depth and mounting stack thickness before choosing screw lengths. A screw must not bottom out or clamp the wrong part of the cell. Fix one end to the base and the other to the moving carrier, leaving the central sensing section free to flex. Spacers, cable routing and overload stops must preserve that load path.

## Waveshare 2inch LCD Module

Use the separate non-touch [2inch LCD Module, SKU 17344](https://www.waveshare.com/2inch-lcd-module.htm). The [manufacturer documentation](https://docs.waveshare.net/2inch_LCD_Module/) identifies the SKU and active area. Its [outline drawing](https://www.waveshare.com/img/devkit/LCD/2inch-LCD-Module/2inch-LCD-Module-size_960.jpg) and [rear photograph](https://www.waveshare.com/img/devkit/LCD/2inch-LCD-Module/2inch-LCD-Module-3_880.jpg) provide the mounting reference.

| Detail | Value | Basis and limit |
| --- | --- | --- |
| PCB outline | 58 × 35 mm | Manufacturer drawing |
| PCB corner radius | 1.50 mm | `R1.50` in the drawing describes the PCB corner, not a mounting hole |
| Bottom-left hole center | 2.50 mm from each adjacent edge | Explicitly dimensioned |
| Proposed four-hole pattern | 53 × 30 mm center spacing | Assumes the other three corners repeat the dimensioned 2.50 mm offsets |
| Proposed hole centers | X = 2.5 and 55.5 mm; Y = 2.5 and 32.5 mm | Coordinates from the lower-left PCB corner, derived from the symmetric pattern |
| Hole diameter and screw thread | Unknown | Screws cover the holes in the drawing; no diameter or thread callout found |
| Active area in landscape orientation | 40.80 mm wide × 30.60 mm high | With the 58 mm PCB side horizontal; software can use 320 × 240 orientation |
| Active-area offset relative to PCB | Not dimensioned | The front photo suggests the active center is about 2 mm away from the flex/pin edge relative to PCB center; this is a visual estimate only |
| Total thickness and rear cable clearance | Unknown | No dimensioned side view found |

The rear photograph shows a PH2.0 connector and four brass standoffs. Their appearance does not establish standoff dimensions, screw thread or package inclusion. The [electrical schematic](https://www.waveshare.net/w/upload/e/ee/2inch_LCD_Module_SchDoc.pdf) provides no mechanical thickness or mounting-hole dimensions.

Mount the PCB on a removable carrier or lid bosses. Keep support and fasteners clear of the glass, flex cable and rear components. Keep the bezel offset, screw holes and support height adjustable. Measure the active-area position and connector envelope before accepting the bezel and rear clearance.

## Owned PN532 module

The [user-supplied listing image](reference/pn532-v3-listing.png) shows an Elechouse-style NFC MODULE V3 board. The [original Elechouse V3 manual](https://www.elechouse.com/elechouse/images/product/PN532_module_V3/PN532_%20Manual_V3.pdf), page 1, gives nominal module dimensions of **42.7 × 40.4 × 4 mm**. These describe the reference design, not a measured clone with headers and wires attached.

The official [Elechouse V4 product](https://www.elechouse.com/product/pn532-nfc-rfid-module-v4/) links a [V4 3D model](https://www.elechouse.com/wp-content/uploads/2022/04/ELECHOUSE_PN532_V4_3D.zip). Inspection of that model yielded a 27.8 × 25.2 mm mounting-center separation and approximately 3.048 mm hole diameter. Those dimensions belong to the V4 reference model. They do not confirm the mounting geometry of the owned V3-style board and must not be treated as its fixed hole pattern.

Use an adjustable carrier until the owned board's outline, hole locations, switches and connector clearances have been measured. Leave access to the interface switches. Keep the antenna near a thin nonmetallic scan face, away from the load cell and metal fasteners where practical. The finished NFC read range and enclosure-wall thickness require tests with the actual spool tags.

## Owned ESP32-S3 N16R8 board

The [user-supplied front/back image](reference/esp32-s3-n16r8-v6.png) shows a 44-pin development board marked V6.0.0, with two USB-C connectors. No dimensioned manufacturer drawing for this exact board has been verified.

The preliminary **63.3 × 27.6 mm overall envelope, with approximately ±1 mm uncertainty**, comes from scaling the photograph. The estimate assumes a standard 2.54 mm header pitch; 21 intervals span about 743 image pixels. The length includes the module's antenna overhang and must not be substituted for the carrier PCB length. The two USB connector centers appear about 12 mm apart, also a photo estimate.

Keep the board holder and USB openings adjustable. Check the actual PCB edges, underside components, fitted header height, connector protrusion and cable-plug envelope. Support the carrier PCB without loading the antenna overhang. These estimates do not prove identity with any official Espressif or YD board. The current cover has a local underside pocket for an assumed 25 mm wire/header allowance above the PCB, leaving 1 mm nominal clearance and a 1.4 mm roof. Actual connector housings and wire bends must be checked in place.

## HX711 and three tactile buttons

The selected [Bit Maker HX711, code 0679](https://www.bitmaker.com.br/m-dulo-conversor-hx711-adc-24-bits-para-sensor-de-peso) remains a generic breakout. Confirm its outline, mounting provisions, component height and soldered-wire clearance on receipt. Do not use a different supplier's HX711 board dimensions as verified geometry for this module.

The selected [SmartKits SK1798 buttons](https://www.smartkits.com.br/push-button-180-graus) have a nominal 6 × 6 × 7 mm size. That is a body/height description, not a verified lead-pitch or actuator-travel specification.

Mount the three switches on one trimmed piece of the [selected perfboard](https://www.smartkits.com.br/placa-fenolite-perfurada-7x9cm). Confirm lead fit before soldering. A shared ground plus one signal per button needs four wires to the controller. Support the strip near the switches so it does not bend when pressed. Optional printed caps need their bore, height and travel-stop gap checked against the actual actuator; every switch must release fully at rest.

## Physical checks before accepting the enclosure

1. Measure the delivered boards, load-cell holes and usable thread depths. Update the editable CAD assumptions and choose fastener lengths.
2. Fit small printed samples for the display bezel, USB openings, button strip and load-cell mounts before printing the complete enclosure.
3. Confirm the complete upright carrier moves freely, both guides adjust through the intended width range and the spool remains stable. The sensing section must flex without cables, fixed cover or stops carrying normal weighing load.
4. Test NFC with actual Bambu, Snapmaker and writable inventory tags at the final scan face. Check the reader with the screen and Wi-Fi operating.
5. Calibrate and test weighing only after the assembled mechanism passes the fit checks. CAD dimensions alone cannot establish the proposed ±5 g accuracy target.
