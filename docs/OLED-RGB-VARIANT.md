# OLED and five-LED variants

Two additional builds use the existing standalone ESP32-S3 N16R8, the upright cradle and the seated side-facing PN532. Both accept the same proposed 50–100 mm spool widths. The local browser remains the editor for inventory, color, material, brand, calibration and Spoolman settings.

| Build | Firmware environment | Printed footprint | STEP/STL pairs | Production pieces |
| --- | --- | --- | ---: | ---: |
| [Five LEDs](../cad/variants/headless-led/NOTES.md) | `spoolancer-headless-led` | 170 × 160 mm | 14, including five fit samples | 10 |
| [OLED + five LEDs](../cad/variants/oled-led/NOTES.md) | `spoolancer-oled-led` | 170 × 160 mm | 15, including six fit samples | 10 |

Both controls fit within the 170 × 160 mm base. The nominal printed assembly is 187 mm high before feet; the highest reader setting reaches 198.4 mm. A nominal 200 mm spool gives a loaded depth of 200 mm. These are CAD dimensions, not physical fit measurements. The original headless and integrated-touch CAD files are preserved.

![OLED and LED CAD prototype](../cad/variants/oled-led/previews/assembled.png)

## Chamfered enclosure and 3D guide

The v0.7 STEP, STL and FreeCAD files use real chamfers on exposed corners and edges. The OLED sits vertically on the left of the front wall, with five LEDs to its right; the LED-only case uses the same front-wall approach. Their supports are integrated into the base and accessed through the main lid. Both builds use 20 screws in three sizes, with separate metal and nylon M3 × 12 purchasing lines. The main lid uses rigid rear hooks and two front screws; each rail uses locating keys and one central screw. The PN532 has PCB-edge retention, and each NFC cover uses a rigid tongue and one screw.

A covered cable channel behind the NFC reader stores slack as its height changes. Its body is part of the moving guide, with one removable printed cover. The remaining flexible route into the base must stay clear of the fixed housing and load-cell flexure. Exact cable allowances, fasteners and installation steps are recorded in each variant's notes.

Use the **[interactive 3D viewer](https://bloodf.github.io/spoolancer/)** to rotate the assembly, select or hide individual parts, see inside the housing, and spread the parts apart. The [numbered exploded image](../cad/variants/oled-led/previews/exploded.png) and [component-location image](../cad/variants/oled-led/previews/component-locations.png) use the same part numbers. These are geometry guides; physical fit remains untested.

## Printing and assembly

The cases have closed USB walls, a dedicated BOOT tool bore, no pre-cut jack hole and no converter mount. Drill the jack opening and fix an insulated converter assembly to a fixed wall after checking the real parts. **12 V must pass through a regulated 5 V buck converter** before reaching the ESP32 or strip. Follow the [DC wiring and programming guide](DC-POWER.md); open the main cover for programming.

The OLED and strip belong to the fixed front wall. Their leads stay inside the base and clear of the moving load path. Five light openings use the confirmed 60 LEDs/metre pitch: **16.667 mm between centres**, 66.667 mm from first to fifth centre.

Use the complete v0.7 export set and [20-screw fastener list](BOM.md#current-ledoled-fasteners). Do not mix the revised joints with v0.6 parts. Retain the strip with insulating adhesive and the OLED at its PCB edges with thin foam; do not clamp the glass. Follow the [print manifest](PRINTING.md) and each variant's notes for quantities, print orientation and the fit samples to check before printing the full enclosure.

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

The retained v0.5 software record from 27 October 2026 covers successful compilation of all five firmware environments, native sanitizer tests, 26 web tests and matching web asset images. Firmware and web behavior are unchanged by the v0.7 mechanical revision. See [the current evidence record](STATUS.md) for the revised CAD, export and assembly-view checks.

The `oled-led` firmware and web assets were flashed to one ESP32-S3. USB startup reported 16 MB flash, approximately 8 MB PSRAM, mounted assets/inventory and a setup AP/address. **The OLED was not detected at `0x3c`.** This establishes controller startup, not working LEDs, a display image, physical BOOT gestures, weighing, NFC, DC power or Spoolman operation. See [the boot record](DEVICE-BOOT.md).

Before relying on measurements, check received dimensions, print fit samples, verify the converter and combined power draw, test the actual display/LEDs and BOOT access, and compare repeated known weights with indicators on and off. Physical fit, stiffness, tipping stability and accuracy remain unvalidated.
