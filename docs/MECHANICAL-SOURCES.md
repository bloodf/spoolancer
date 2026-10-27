# Mechanical dimensions and assumptions

Research updated 27 October 2026. This document supports the v0.4.1 headless and unchanged v0.4 touch enclosure prototypes. No received module has been measured, fitted to a print or physically tested. Published dimensions, photo estimates and design allowances have different levels of confidence.

## How to use the dimensions

Use dimensions explicitly printed in a source as nominal values. Keep inferred positions and fit allowances editable in CAD. Check the actual parts and small fit samples before accepting the complete enclosure. CAD validation and firmware compilation do not establish physical fit, weighing accuracy or NFC performance. See the [firmware build guide](../firmware/README.md) and [web-interface guide](WEB-INTERFACE.md) for the implemented software and its limits.

The current design is an upright, standalone scale for occasional weighing. Two hollow rim rails replace the full saddle block, and a side-mounted PN532 moves with the right guide. The previous mechanism is preserved in [archive/v0.2-upright](../archive/v0.2-upright/cad/README.md). The target remains up to 2 kg gross spool weight; the 5 kg sensor rating also includes the carrier, rails, guides, reader, pod, moving wiring and fasteners.

## Compact mechanism design assumptions

These dimensions were chosen for the prototype, not measured on hardware. The [parameters](../cad/parameters.json), [validation report](../cad/exports/validation.json) and [headless design notes](../cad/HEADLESS-NOTES.md) are the geometry record.

| Detail | Proposed design value | Physical check still needed |
| --- | --- | --- |
| Base | 170 × 160 × 35 mm; cover upper face at Z = 38 mm | Bed clearance, stiffness and four-foot stability. |
| Open carrier | 140 × 130 mm; upper surfaces at Z = 50 mm | Free movement inside the 144 × 134 mm cover aperture. |
| Rim rails | Two rails covering Y = 34–65 mm and −65 to −34 mm; contact surface `Z = 40 + abs(Y) × tan(30°)` | Actual flange contact, strength, rocking and off-center response. |
| Width guides | Distinct left and right parts; inner gap equals actual width plus 4 mm | Adjustment for 50–100 mm widths without flange squeeze or cable tension. |
| NFC pod | Center height 90–175 mm; 1.2 mm window, 0.8 mm PCB gap, 2 mm gap to spool side | Board/fastener/connector fit and actual read/write range. |
| Spool envelopes | 160, 200 and 220 mm diameter references | Actual flange shapes and embedded tag positions; no universal commercial-spool fit claim. |

For an ideal circular spool on the 30-degree faces, top height is `40 + radius / cos(30°) + radius`, before feet. The 200 mm case reaches about **256.87 mm** including the modeled 1.4 mm net foot contribution; the 220 mm case reaches **278.42 mm**. Loaded depth is 165/200/220 mm for the 160/200/220 mm cases. The unloaded assembly occupies the 170 × 160 mm footprint, with height **185.4–199.8 mm** depending on reader adjustment. The mast makes the empty mechanism taller than v0.2 even though the base and loaded spool are lower.

Guide and rail nuts load sideways beneath 3 mm carrier roofs. Both use M3 × 12 mm screws with nominal 0.5 mm washers. Cover nuts load from inside the housing under 3 mm post roofs and use M3 × 10 mm screws with nominal 0.5 mm washers. The [assembly guide](ASSEMBLY.md) gives entry directions and the complete hardware list. These are designed clamp stacks; printed fit and retention remain untested.

Every spool-contacting part and the reader belongs to the moving assembly. The PN532 cable crosses to the fixed base through a proposed loose silicone loop. Its modeled connector allowance is only 7 × 12 × 8 mm, and the flexible loop is not modeled. Wire force can shift zero without visible contact. Compare readings and zero return at both guide-width limits and reader-height extremes before accepting the routing. No axle or bearings are used.

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

## Legacy Waveshare 2inch LCD Module

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

The v0.3 pod uses these reference dimensions as editable assumptions. Check the owned board's outline, hole locations, switches, components and compact wire exit on the actual pod fit sample. Its provisional 2.8 mm PCB holes, front countersinks and M2.5 nylon hardware are design choices, not dimensions verified for the clone. Keep the antenna facing the 1.2 mm nonmetallic window and use nylon hardware near it. Set the interface switches before closing the removable pod cover.

The pod moves on the right guide to face a seated spool's tag. Its nominal 4 mm antenna-to-spool spacing excludes tag depth within the flange. Actual tag orientation, nearby material, board fit and the moving cable loop require physical tests; a rendered clearance cannot establish read/write performance.

## Owned ESP32-S3 N16R8 board

The [user-supplied front/back image](reference/esp32-s3-n16r8-v6.png) shows a 44-pin development board marked V6.0.0, with two USB-C connectors. No dimensioned manufacturer drawing for this exact board has been verified.

The preliminary **63.3 × 27.6 mm overall envelope, with approximately ±1 mm uncertainty**, comes from scaling the photograph. The estimate assumes a standard 2.54 mm header pitch; 21 intervals span about 743 image pixels. The length includes the module's antenna overhang and must not be substituted for the carrier PCB length. The two USB connector centers appear about 12 mm apart, also a photo estimate.

Keep the board holder and USB openings adjustable. Check the actual PCB edges, underside components, fitted header height, connector protrusion and cable-plug envelope. Support the carrier PCB without loading the antenna overhang. These estimates do not prove identity with any official Espressif or YD board. The current cover has a local underside pocket for an assumed 25 mm wire/header allowance above the PCB, leaving 1 mm nominal clearance and a 1.4 mm roof. Actual connector housings and wire bends must be checked in place.

## HX711 and onboard BOOT access

The selected [Bit Maker HX711, code 0679](https://www.bitmaker.com.br/m-dulo-conversor-hx711-adc-24-bits-para-sensor-de-peso) remains a generic breakout. Confirm its outline, mounting provisions, component height and soldered-wire clearance on receipt. Do not use a different supplier's HX711 board dimensions as verified geometry for this module.

Both current variants reuse their controller's BOOT button, GPIO0. The screenless v0.4.1 removes the external 6 × 6 × 7 mm switch, trimmed perfboard, printed cap and two M2 mounting screws. Its existing left-side USB service opening provides proposed access with a blunt nonconductive tool. BOOT's position is photo-derived, not a measured manufacturer coordinate; the CAD access check cannot establish real access or reliable actuation. Identify BOOT on the actual board and test full press/release with the USB plug and peripheral wiring in place. RST is a separate adjacent button and must not be confused with BOOT.

The touch variant keeps its existing open-back holder and onboard BOOT access. The three-button strip belongs to archived v0.3. BOOT gestures happen after normal startup; holding GPIO0 low during power-on/reset selects the ESP32 bootloader.

## ESP-8048S043C-I capacitive touch option

The user confirmed this exact selected model. The supplied listing pictures show the broader ESP32-4827S043 family and cannot by themselves prove the 800 × 480 touch option. The [manufacturer family specification](https://www.guition.com/icms/upload/fb081940d6fc11f09850077a33e1404f/FTPData/UEditor/file/2026121/1768961092165/ESP32-4827S043%20Specifications-EN.pdf), page 6, is the drawing used for the provisional enclosure. The full vendor PDF is linked rather than redistributed in the source package.

| Detail | Nominal value / evidence |
| --- | --- |
| PCB outline | 123 × 74 mm, page 6 and listing size image |
| Mounting holes | Diameter 3.2 mm; 3.5 mm horizontal edge offset gives 116 mm pitch |
| Vertical hole pitch | Drawing states 67.2 mm while 3.5 mm edge offsets imply 67.0 mm; use short slots and confirm the board |
| Display/glass body width | Drawing states 105.5 mm; this is not the PCB width |
| Active area | Listing states 95.04 × 53.86 mm; active center and glass offset remain to be measured |
| Thickness, connectors and capacitive overlay | No complete verified dimensioned stack; provisional clearances require actual fit checks |

The [primary ESP3D board reference](https://esp3d.io/esp3d-tft/version_1x/hardware/esp32-s3/sunton-43-8048/) supports the 800 × 480, ST7262, GT911 and exposed-pin configuration. Its 181 × 108 mm size entry conflicts with the manufacturer drawing and is not used for CAD. The family PDF also has conflicting table dimensions; page 6's explicit outline takes precedence for this prototype. These documents are references, not a physical qualification of the user's exact revision.

Use a fixed front pod with open rear/connector access, rather than loading the moving carrier with a large screen. Check glass pressure, PCB seating, cable bends, USB insertion and BOOT access. Native firmware must be tested for correct RGB mapping, touch coordinates and simultaneous PN532/HX711 operation on the received board.

## Physical checks before accepting the enclosure

1. Measure the delivered boards, load-cell holes and usable thread depths. Update the editable CAD assumptions and choose fastener lengths.
2. Check the chosen variant’s ESP32/display, load-cell and NFC-pod fit samples, followed by onboard BOOT access. Check the complete base's USB openings with actual plugs before final acceptance.
3. Confirm the carrier moves freely, the side-loaded nuts clamp both rails and guides, and upright spools remain stable. Check the pod's full height range and cable loop at both width limits without contact or tension.
4. Test seated NFC with actual Bambu/Snapmaker tags and supported writable NTAG216 inventory tags, with screen and Wi-Fi operating. The implemented original-tag support is UID-only and read-only; proprietary material decoding is not implied.
5. Calibrate with the entire moving assembly and wiring installed. Compare zero return and repeated weights after moving the pod and guides; investigate any wire-induced bias. CAD dimensions and a compiled firmware binary cannot establish the proposed ±5 g accuracy target.
