# Bill of materials

Choose one controller and the shared parts below. The headless build reuses the project owner's ESP32-S3 N16R8 and PN532. It needs **no separate button, perfboard, display, axle or bearings**.

These selections match the current CAD assumptions. They are not bench-qualified substitutes for checking received dimensions and electrical behavior. Purchase links come from the existing [shopping notes](../SHOPPING.md), recorded 26 October 2026; prices, stock, variants and delivery have not been refreshed for this release.

## Electronics

| Quantity | Part and specification | Build / status | Link or fit detail |
| ---: | --- | --- | --- |
| 1 | ESP32-S3 N16R8 development board, dual USB-C, documented V6.0.0 44-position layout | Headless only; already owned | [Board reference photo](reference/esp32-s3-n16r8-v6.png). A different board may need adjusted mounts or USB access. |
| 1 | ESP-8048S043C-I, integrated ESP32-S3 N16R8, 4.3-inch 800 × 480 capacitive touch | Touch only; alternative to the separate ESP32 | [Selected AliExpress listing](https://pt.aliexpress.com/item/1005013088869895.html). Select that exact model; received revision and fit remain unverified. |
| 1 | PN532 NFC MODULE V3, red Elechouse-style board, UART/HSU capable | Both; already owned | [Board reference photo](reference/pn532-v3-listing.png). Confirm actual clone layout. |
| 1 | 5 kg four-wire bar load cell, 80 × 12.7 × 12.7 mm, seller code 1381 | Both; required | [Bit Maker load cell](https://www.bitmaker.com.br/sensor-de-peso-c-lula-de-carga-5kg). CAD assumes M5 fixed-end and M4 moving-end threads; measure the received part. |
| 1 | HX711 module, seller code 0679, listed for 3–5 V supply | Both; required | [Bit Maker HX711](https://www.bitmaker.com.br/m-dulo-conversor-hx711-adc-24-bits-para-sensor-de-peso). This build uses 3.3 V. |
| 1 per spool to tag; 5 suggested initially | Writable NTAG216 adhesive tags, 25 mm, SK6052 | Optional inventory writing | [SmartKits NTAG216](https://www.smartkits.com.br/tag-nfc-ntag216-888-bytes). Verify received type/capacity; these do not replace stock-printer authenticated tags. |
| 1 | Regulated 5 V USB supply | Both; reuse if suitable | Check complete-board operation with Wi-Fi, reader and optional screen active. Supply capacity and controller rail margin remain unmeasured. |
| 1 | USB cable with data conductors and connector matching the board's USB-C input | Both; reuse if suitable | Headless uses the COM port. Check the actual plug overmould in the enclosure. |

The 5 kg rating is the cell rating. The current firmware targets at most **2 kg gross spool weight**, and the cell also carries the printed cradle, reader and moving hardware.

## Wiring and consumables

| Quantity | Item | Use / link |
| --- | --- | --- |
| 1 pack initially | 20 female–female jumpers, 20 cm, SK1589 | [SmartKits jumpers](https://www.smartkits.com.br/jumpers-femeafemea-20-unidades), for bench wiring where the actual connectors fit. |
| Up to 2 strips | 1 × 40 male headers, 2.54 mm, SK1935 | [SmartKits headers](https://www.smartkits.com.br/barra-de-pinos-macho-1x40-180-graus), only where headers are absent. Touch connectors need separate verification. |
| Enough for the measured route and slack loop | Flexible stranded wire, solder, heat-shrink insulation | Final PN532 wiring must allow both guide width and reader height adjustment without pulling the cradle. Large Dupont housings are not qualified for the pod exit. |
| 3 headless; 1 touch, plus routing ties as needed | Small cable ties, approximately 2.5 mm wide | Two retain the separate ESP32 and one retains HX711. |
| 4 headless; 6 touch | Adhesive rubber feet, approximately 12 mm diameter × 2 mm thick | Four base feet; the touch stand adds two matching feet. |
| Slicer-dependent | Ordinary unfilled PETG | Use the [print manifest and starting settings](PRINTING.md). Current headless filament mass has not been re-sliced. |
| 1 or more known masses | Calibration and independent check masses | Borrow masses or use stable objects weighed on a trusted reference scale. A nominal 1 kg filament spool is not a calibration mass. |

Use the available 3V3/GND headers or insulated soldered branches to power PN532 and HX711. No distribution perfboard is required. Keep connections and fixed wiring clear of the moving assembly.

## Headless fasteners

Quantities cover one complete headless build. Screw lengths assume the modeled stack; confirm actual head, nut and washer sizes and usable load-cell thread depth. The [assembly guide](ASSEMBLY.md#mounting-hardware) identifies every joint.

| Quantity | Hardware | Location |
| ---: | --- | --- |
| 4 | M3 × 10 mm cover screws, heads fitting the modeled seats | Top cover |
| 8 | M3 × 12 mm socket-head screws | Four rail screws and four guide screws |
| 12 | Standard M3 nuts | Cover, rails and guides |
| 12 | M3 washers, approximately 0.5 mm thick | Cover, rails and guides |
| 4 | M3 × 20 mm socket-head screws | Adjustable stops |
| 4 | M3 nylon-insert locknuts, at most 4.2 mm high | Stop pockets |
| 2 | M5 × 12 mm socket-head screws and approximately 1 mm washers | Fixed cell end |
| 2 | M4 × 25 mm socket-head screws and approximately 0.8 mm washers | Moving cell end |
| 2 | M3 × 8 mm nylon screws, nylon nuts and approximately 0.5 mm nylon washers | Reader height adjustment |
| 2 | M2.5 × 8 mm flat-head nylon screws and matching nylon nuts | PN532 PCB; provisional hole/head fit |
| 2 | M2.5 × 6 mm nylon screws | Pod cover; printed pilots |

Keep the antenna-area fasteners nonmetallic. The cell drawing's `6H` is a thread tolerance class, not a thread-depth measurement. Confirm that cell screws clamp the stack without bottoming out.

## Changes for the touchscreen

Use the headless fastener list with these changes. The [touch holder notes](../cad/variants/touch-8048/TOUCH-NOTES.md#fasteners-and-assembly) show the full stack.

| Change | Quantity | Purpose |
| --- | ---: | --- |
| Replace M3 × 10 cover screws with M3 × 16 screws | 2 | Front cover positions, through the bridge. Keep the two rear M3 × 10 screws. |
| Add M3 × 10 socket screws, washers and standard M3 nuts | 4 sets | PCB corner clamps. Check clearance to the actual glass and components. |
| Add M3 × 12 socket screws, washers and standard M3 nuts | 2 sets | Bridge to stand |
| Add matching rubber feet | 2 | Stand support; included in the six-foot total above |

The touch model uses approximately 0.5 mm thick, 6.4 mm outside-diameter washers and 5.5 mm socket heads. Nylon PCB-clamp hardware can be used if its dimensions fit. Do not buy the old external setup switch, its two M2 screws, or the legacy ST7789 screen and navigation buttons for either current build.

## LED and OLED additions

For the `headless-led` and `oled-led` variants, reuse the shared scale electronics above. The owner has a 5 V three-wire addressable RGB strip at 60 LEDs/metre and a 0.96-inch OLED. Use five pixels, nominal pitch 16.667 mm. The exact LED chip/color order and OLED controller/PCB dimensions remain unverified.

The owner chose a 12 V DC jack and glued wall mounting for the converter. Add a regulated 12 V-to-5 V converter rated for at least 2 A continuous, a suitable 3.3 V-to-5 V LED data buffer such as a preassembled 74AHCT125 module, a 330–470 ohm data resistor and a 500–1000 µF capacitor rated at least 6.3 V on the **5 V** rail. The buffer also needs local bypass capacitance. No perfboard or external reset button is required if using wired modules. Check actual module dimensions and wiring before glue or drilling. See [DC power and programming isolation](DC-POWER.md).

For either new front pod, replace two front M3×10 closure screws with **two M3×16** and add **four M2.5×10** rear-cover screws. The latter use printed plastic pilot threads; test retention and tighten lightly. Add thin insulating adhesive for the strip and compressible foam for OLED PCB edge retention. Use [the selected variant's fit notes](OLED-RGB-VARIANT.md#printing-and-assembly) before committing to a full print.

These cases omit USB cutouts and dedicated converter mounts. The jack opening is user-drilled. The original USB-powered variants and their parts remain listed above.
