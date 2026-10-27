# OLED and five-LED variants

Two additional builds use the existing standalone ESP32-S3 N16R8, the upright cradle and the seated side-facing PN532. Both accept the same proposed 50–100 mm spool widths. The local browser remains the editor for inventory, color, material, brand, calibration and Spoolman settings.

| Build | Firmware environment | Printed footprint | STEP/STL pairs | Production pieces |
| --- | --- | --- | ---: | ---: |
| [Five LEDs](../cad/variants/headless-led/NOTES.md) | `spoolancer-headless-led` | 170 × 186.48 mm | 15, including five fit samples | 11 |
| [OLED + five LEDs](../cad/variants/oled-led/NOTES.md) | `spoolancer-oled-led` | 170 × 201.48 mm | 16, including six fit samples | 11 |

The main base is still 170 × 160 mm; the inclined front pod adds depth. A nominal 200 mm spool gives a loaded depth of 211.48 mm or 226.48 mm respectively. These are CAD dimensions, not physical fit measurements. The original headless and integrated-touch CAD files are preserved.

![OLED and LED CAD prototype](../cad/variants/oled-led/previews/assembled.png)

## Rounded cases and 3D guide

The v0.6 STEP, STL and FreeCAD files include rounded guide tops, rail ends, NFC pod corners and front-panel edges. Smaller rounds and chamfers soften the main housing while preserving the checked mounting seats, sliding surfaces, antenna window and opening throats. Exact treatments and protected-region checks are in each variant's notes and [the geometry report](../cad/edge-finish-verification.json).

Use the **[interactive 3D viewer](https://bloodf.github.io/spoolancer/)** to rotate the assembly, select or hide individual parts, see inside the housing, and spread the parts apart. The [numbered exploded image](../cad/variants/oled-led/previews/exploded.png) and [component-location image](../cad/variants/oled-led/previews/component-locations.png) use the same part numbers. These are geometry guides; physical fit remains untested.

## Printing and assembly

The new cases have closed USB walls, a dedicated 4 mm BOOT tool bore, no pre-cut jack hole and no converter mount. The builder drills the jack opening and fixes an insulated converter assembly to a fixed wall. **12 V must pass through a regulated 5 V buck converter** before reaching the ESP32 or strip. Follow the [DC wiring and programming guide](DC-POWER.md); open the main cover for programming.

The removable front fascia and rear carrier mount on the fixed cover posts. Their wires pass through the fixed front wall and must stay clear of the moving load path. Five 6.4 mm light openings use the confirmed 60 LEDs/metre pitch: **16.667 mm between centres**, 66.667 mm from first to fifth centre.

Use the complete exports from the selected variant, especially its new base and top cover. Replace the two front M3×10 closure screws with **two M3×16** and add **four M2.5×10** rear-cover screws. Test the printed pilot threads and tighten lightly. Retain the strip with insulating adhesive and the OLED at its PCB edges with thin foam; do not clamp the glass. The fascia prints face-down and needs local support beneath its raised mounting bridge.

Print `fit_led_face`, `fit_led_carrier`, `fit_esp32`, `fit_cell` and `fit_nfc` first; the OLED version adds `fit_oled_face`. The complete rear cover is also needed to check the OLED's pins and wire exit. See the [print manifest](PRINTING.md) and each variant's notes for quantities, mounting and exact provisional clearances.

## Component assumptions and wiring

The owner confirmed a **5 V three-wire addressable strip at 60 LEDs/metre** and a **0.96-inch 128×64 four-pin OLED**. The screen photo labels GND, VDD, SCK and SDA; it does not identify the controller or supply circuit.

| Item | Current implementation | Check on the actual part |
| --- | --- | --- |
| LED strip | Five WS2812-style GRB/800 kHz pixels, brightness 51/255 (20%); nominal 10 mm strip width and 5050 packages | Chip, color order, cut pads, coating, package height and wiring |
| OLED | Provisional SSD1306, I²C `0x3c`; nominal 27.3 × 27.3 mm PCB with edge retention | Controller, address, supply rating, glass offset, headers and overall dimensions |
| OLED connection | SDA GPIO4, SCK/SCL GPIO5 | 3.3 V-compatible supply and pull-ups |
| LED connection | GPIO6 through a suitable 3.3 V-to-5 V buffer, then a 330–470 ohm DIN resistor | Actual signal/power behavior and common ground |

GPIO4/5/6 are confined to the two new standalone profiles; they are not the integrated-touch pin map. Firmware options support SH1106, a different I²C address, and RGB/RGBW order. See the [firmware options](../firmware/README.md#five-leds-and-optional-096-inch-oled). An I²C acknowledgement alone does not identify the OLED controller. Brightness limiting is not an electrical current limiter.

## Indicator behavior

Five cyan segments show remaining filament relative to the selected spool's configured starting filament mass. Each full segment is 20%; the next segment dims proportionally. Only a fresh, calibrated, stable measurement with known empty-spool weight and valid starting mass can display the bar. Invalid or unavailable measurements never appear as an empty or full spool.

| Event | Signal |
| --- | --- |
| Wi-Fi setup | Slow blue pulse |
| Measurement unavailable or waiting | Amber lights |
| Explicit NFC operation | Steady purple lights |
| Save or Spoolman operation in progress | Steady blue lights |
| Operation succeeds | Green lights for 2.5 seconds |
| Operation fails | Red lights for 2.5 seconds |

The OLED shows valid remaining grams, gross grams, selected material, status and local address. When net weight is unavailable, it labels gross weight and explains the missing action. Busy operations hide remaining grams. Setup directs the user to the browser; BOOT retains the two-second enrollment and eight-second Wi-Fi recovery gestures.

Routine background NFC scans do not flash the lights. Explicit NFC success requires exactly one target without an error. Pending Spoolman creation is an unresolved outcome requiring reconciliation, not a background sync queue. Text in the OLED/browser supplies meaning alongside color.

## Verification and remaining checks

On 27 October 2026, all five firmware environments compiled, native sanitizer tests and 26 web tests passed, and the two new web asset images matched the current source files. Both CAD variants passed solid/mesh and clearance checks, 27 spool cases, nine reader placements, BOOT approach and export reopening. See [the evidence record](STATUS.md).

The `oled-led` firmware and web assets were flashed to one ESP32-S3. USB startup reported 16 MB flash, approximately 8 MB PSRAM, mounted assets/inventory and a setup AP/address. **The OLED was not detected at `0x3c`.** This establishes controller startup, not working LEDs, a display image, physical BOOT gestures, weighing, NFC, DC power or Spoolman operation. See [the boot record](DEVICE-BOOT.md).

Before relying on measurements, check received dimensions, print fit samples, verify the converter and combined power draw, test the actual display/LEDs and BOOT access, and compare repeated known weights with indicators on and off. Physical fit, stiffness, tipping stability and accuracy remain unvalidated.
