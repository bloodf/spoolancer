# Spoolancer parts by variant

Selections recorded 26 October 2026. No purchases have been made. Listed prices below are the earlier observed item prices, before shipping/import charges; they have not been refreshed for this variant update.

Choose the controller/display route first:

| Variant | Controller and screen | Setup control |
| --- | --- | --- |
| Screenless | Reuse the owned ESP32-S3 N16R8. No display purchase. | Existing onboard BOOT button, GPIO0, used after firmware starts |
| Touchscreen | [User-selected ESP-8048S043C-I listing](https://pt.aliexpress.com/item/1005013088869895.html): select the **4.3-inch 800 × 480 capacitive** version with integrated ESP32-S3 N16R8. Price and delivered cost unverified. | Onboard BOOT button, used after firmware starts |

The touchscreen board replaces the separate ESP32 and small LCD; it is not an accessory screen for the owned ESP32. Keep the PN532 for either build. The user's confirmed model and supplied screenshots are the basis of the selection. Listing variants can differ, so retain **ESP-8048S043C-I** in the order selection.

The [ESP3D board reference](https://esp3d.io/esp3d-tft/version_1x/hardware/esp32-s3/sunton-43-8048/) documents the 800 × 480 RGB/GT911 family and 16 MB flash/8 MB PSRAM. Mechanical sizing uses the supplied [Guition specification, page 6](https://www.guition.com/icms/upload/fb081940d6fc11f09850077a33e1404f/FTPData/UEditor/file/2026121/1768961092165/ESP32-4827S043%20Specifications-EN.pdf#page=6): 123 × 74 mm board outline. The web reference's different outline is not used for the enclosure. Actual mounting holes, connector access and board revision still need measurement.

## Shared scale parts

| Quantity | Exact selection | Previously listed price | Notes |
| --- | --- | --- | --- |
| 1 | [Bit Maker 5 kg load cell, code 1381](https://www.bitmaker.com.br/sensor-de-peso-c-lula-de-carga-5kg) | R$18.90, or R$17.96 Pix | Four-wire 80 × 12.7 × 12.7 mm bar; seller lists 3–12 V excitation |
| 1 | [Bit Maker HX711, code 0679](https://www.bitmaker.com.br/m-dulo-conversor-hx711-adc-24-bits-para-sensor-de-peso) | R$8.90, or R$8.46 Pix | Listed for 3–5 V supply; use 3.3 V with the ESP32 |
| 5 | [SmartKits SK6052 NTAG216 adhesive tag, 25 mm](https://www.smartkits.com.br/tag-nfc-ntag216-888-bytes) | R$4.90 each | Writable inventory tags; seller specifies 888 usable bytes. Verify received type/capacity |

The cell/HX711 pair previously totaled **R$27.80**, or **R$26.42 Pix**, before delivery. These generic modules are identified by seller code; they are not verified TAL220B or SparkFun parts. Calibration and bench testing must establish actual performance.

## Assembly extras

| Quantity | Selection | Previously listed unit price | Use |
| --- | --- | --- | --- |
| 1 pack to start; second if needed | [SmartKits SK1589, 20 female–female jumpers, 20 cm](https://www.smartkits.com.br/jumpers-femeafemea-20-unidades) | R$7.00 per pack | Bench wiring. Final moving-reader cable needs flexible stranded wire and slack |
| 1–2 strips as needed | [SmartKits SK1935, 1 × 40 male headers, 2.54 mm](https://www.smartkits.com.br/barra-de-pinos-macho-1x40-180-graus) | R$1.50 per strip | Two strips cover a bare 44-pin ESP32 plus modules; fewer if headers are fitted. Check touch-board connectors before buying |

Updated 27 October 2026: both current builds reuse onboard BOOT. **Do not buy a separate setup button or perfboard.** The headless reset cap and its two M2 screws are also removed. Power the two peripheral boards from the available 3V3/GND header connections, or use insulated soldered wire branches; a distribution perfboard is not required. Reusing BOOT requires the v0.4.1 firmware, since v0.4 headless firmware listened on GPIO4.

No combined touchscreen subtotal is stated because its selected-option price, connectors, mounting hardware and delivery remain unverified. The prior three-button/small-display subtotal belongs to the archived v0.3 build.

Reuse a suitable USB data cable and regulated 5 V USB supply. Power PN532 and HX711 at 3.3 V with common ground; verify the controller's rail under load. The integrated screen receives power through its own board USB input. Do not connect a second ESP32 or an additional display supply unless its actual documentation requires it.

The screenless base is 170 × 160 mm. The touchscreen uses a removable fixed front holder, 170 × 205.73 mm overall in CAD. Both retain the proposed 50–100 mm spool-width adjustment, moving reader and common load-cell mechanism. The earlier v0.4 headless estimate was **392.32 g PETG**; the v0.4.1 cover/button changes have not been re-sliced. The unchanged touch estimate is **427.29 g PETG**. Sliced estimates exclude fit samples and printer-specific changes. The touch holder needs four M3 × 10 PCB-clamp screws, two M3 × 12 bridge screws, six standard M3 nuts and matching washers. Replace the two front cover screws with M3 × 16 and add two feet matching the base; see the [touch assembly notes](cad/variants/touch-8048/TOUCH-NOTES.md). Use the [CAD guide](cad/README.md) and [assembly hardware list](docs/ASSEMBLY.md#mounting-hardware) for the selected variant; physical fit samples come before full prints.

The selected cell requires two M4 and two M5 screws. Confirm lengths against actual support/washer thickness and measured thread depth. Flexible reader leads, nylon antenna-area fasteners, strain relief and nonslip feet remain required. Jumper-wire stiffness must not pull on the moving carrier.

Borrow known calibration masses or have a stable object weighed on a trusted reference scale. A nominal 1 kg filament spool or volume-marked bottle is not a known calibration mass. The first firmware accepts 100–2,000 g reference masses and targets at most 2,000 g gross spool weight.

The [Waveshare 2inch LCD Module, SKU 17344](https://www.waveshare.com/2inch-lcd-module.htm), previously US$10.99, is **legacy v0.3 only**. Its separate ST7789 panel and three navigation buttons are not required for either current variant. Its wiring and enclosure are retained in the [archive](archive/v0.3-compact/README.md).
