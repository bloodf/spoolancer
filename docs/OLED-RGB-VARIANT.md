# OLED and five-LED variants

The current v0.8 builds reuse the standalone ESP32-S3 N16R8 and side-facing PN532. They replace the earlier LED/OLED mechanics with **six broad printed pieces and 12 screws**. Inventory, color, material, brand, calibration and Spoolman settings remain available in the local browser.

| Build | Firmware environment | Base footprint | Production pieces |
| --- | --- | --- | ---: |
| [Five LEDs](../cad/variants/headless-led/NOTES.md) | `spoolancer-headless-led` | 170 × 170 mm | 6 |
| [OLED + five LEDs](../cad/variants/oled-led/NOTES.md) | `spoolancer-oled-led` | 170 × 170 mm | 6 |

The printed tower is 202 mm above the base underside, or 204 mm above the nominal feet. Cover screw heads bring the hardware footprint to approximately 173.7 × 170 mm. A loaded spool extends beyond that envelope. These are CAD dimensions, not tested fit or stability. Original headless and integrated-touch models are retained as legacy designs.

![OLED and LED CAD prototype](../cad/variants/oled-led/previews/assembled.png)

## Chamfered enclosure and 3D guide

The cradle has an 8 mm floor, integral solid ramps and a fixed left fence. A broad glued tower establishes the right side of the 104 mm bay. The only remaining sliding mechanism is the NFC cassette, adjustable through 85–170 mm reader-center height. The tower has a 3 mm front panel and an open rear tray; one flat, screw-retained cover hides the reader-wire slack.

The OLED window and five LED holes are integrated into the vertical front wall. There is no separate fascia or main lid. Base service uses the two moving-end cell screws, leaving the fixed cell attachment in place. The broad print faces and chamfered corners replace the previous thin frame, rails, slotted width guides, hooks and retaining fingers.

Use the [interactive 3D viewer](https://bloodf.github.io/spoolancer/?model=oled-led), [exploded view](../cad/variants/oled-led/previews/exploded.png) and [assembly guide](ASSEMBLY.md) to see every placement. Geometry views do not establish physical fit or strength.

## Printing and assembly

Print one each of `base`, `cradle`, `cell_spacer`, `nfc_tower`, `nfc_cassette` and `nfc_cover`, preserving the supplied broad-face-down poses. Use the complete v0.8 set; old v0.7 parts and fit samples do not qualify these interfaces. The [print manifest](PRINTING.md) lists STEP/STL links, dimensions and orientation.

The [current parts list](BOM.md#current-ledoled-fasteners) has **4 metal M3 × 16, 4 nylon M3 × 16, 2 M4 × 25 and 2 M5 × 12**, plus **8 nuts and 10 washers**. Bond only the tower-to-cradle nest using the [glue guide](GLUE-ASSEMBLY.md); test the fit before permanent assembly. The reader and cover remain removable.

The five light openings are **7.4 mm diameter**, with **16.667 mm center spacing**. The strip attaches with insulating adhesive; use removable foam at safe OLED PCB edges and keep pressure off the glass. All front-control wires stay in the fixed base. The moving reader lead needs a flexible slack loop, tested for zero shift throughout travel.

These cases have no USB cutouts, DC-jack hole or prescribed converter mount. Check the actual parts before drilling and attaching an insulated converter to a fixed wall. **Convert 12 V to regulated 5 V before the ESP32 or LED strip**, following the [DC wiring and programming guide](DC-POWER.md). The 6 mm top BOOT access hole is aligned to an estimated button position; confirm it on the actual board, with the spool removed.

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

The retained v0.5 software record from 27 October 2026 covers successful compilation of all five firmware environments, native sanitizer tests, 26 web tests and matching web asset images. Firmware and web behavior are unchanged by the v0.8 mechanical revision. See [the current evidence record](STATUS.md) for the revised CAD, export and assembly-view checks.

The `oled-led` firmware and web assets were flashed to one ESP32-S3. USB startup reported 16 MB flash, approximately 8 MB PSRAM, mounted assets/inventory and a setup AP/address. **The OLED was not detected at `0x3c`.** This establishes controller startup, not working LEDs, a display image, physical BOOT gestures, weighing, NFC, DC power or Spoolman operation. See [the boot record](DEVICE-BOOT.md).

Before relying on measurements, check received dimensions and the current six production parts, verify the converter and combined power draw, test the actual display/LEDs and BOOT access, and compare repeated known weights with indicators on and off. Physical fit, stiffness, tipping stability and accuracy remain unvalidated.
