# Spoolancer shopping list

Recommended selections, checked 26 October 2026. Keep the existing ESP32-S3 N16R8 and PN532. Prices are listed item prices, before shipping and any import charges; this is not a delivered-cost quote. Nothing has been ordered.

**Buy these three main parts.**

| Quantity | Exact selection and purchase link | Listed price | Why this part |
| --- | --- | --- | --- |
| 1 | [Waveshare 2inch LCD Module, SKU 17344](https://www.waveshare.com/2inch-lcd-module.htm) | US$10.99 | Non-touch 240 × 320 IPS screen, ST7789, SPI, 3.3 V operation. Includes the PH2.0 eight-wire 20 cm cable. Matches the planned display connections. |
| 1 | [Bit Maker 5 kg load cell, code 1381](https://www.bitmaker.com.br/sensor-de-peso-c-lula-de-carga-5kg) | R$18.90, or R$17.96 Pix | Four-wire bar cell, 80 × 12.7 × 12.7 mm; seller lists 3–12 V excitation. Local purchase. |
| 1 | [Bit Maker HX711, code 0679](https://www.bitmaker.com.br/m-dulo-conversor-hx711-adc-24-bits-para-sensor-de-peso) | R$8.90, or R$8.46 Pix | Seller explicitly lists 3–5 V operation. Use 3.3 V supply with the ESP32. Local purchase from the same store as the cell. |

The load cell and HX711 were listed with immediate availability. Waveshare's direct store displayed its price and an enabled purchase control; shipping availability and charges for the delivery address still need checking. The Brazilian modules are generic products identified by the seller's codes, not a verified TAL220B/SparkFun part number. Their published specifications support the selection; calibration and a bench test will establish actual performance.

**Add these inexpensive assembly and inventory parts if they are not already available.**

| Quantity | Exact selection and purchase link | Listed unit price | Purpose |
| --- | --- | --- | --- |
| 3 | [SmartKits SK1798, 6 × 6 × 7 mm momentary pushbutton](https://www.smartkits.com.br/push-button-180-graus) | R$0.30 | Previous, next and select controls |
| 2 packs | [SmartKits SK1589, 20 female–female jumpers, 20 cm](https://www.smartkits.com.br/jumpers-femeafemea-20-unidades) | R$7.00 per pack | Forty leads give room for the prototype, power distribution and spares; existing module cables can reduce the quantity |
| 2 strips | [SmartKits SK1935, 1 × 40 male headers, 2.54 mm](https://www.smartkits.com.br/barra-de-pinos-macho-1x40-180-graus) | R$1.50 per strip | Two strips cover the ESP32's 44 positions if it has bare holes, plus other modules; one is enough if its headers are already fitted |
| 5 | [SmartKits SK6052, NTAG216 adhesive tag, 25 mm](https://www.smartkits.com.br/tag-nfc-ntag216-888-bytes) | R$4.90 | Writable tags for the scale's inventory; seller specifies 888 usable bytes |
| 1 | [SmartKits SK1941, 7 × 9 cm solder perfboard](https://www.smartkits.com.br/placa-fenolite-perfurada-7x9cm) | R$4.90 | Recommended for mounting the buttons and making common power/ground connections |

These quantities cost R$47.30 at the listed prices. The buttons, jumpers, headers and perfboard showed immediate availability. The tag page offered purchase without an explicit stock quantity. Confirm the received tags' type and writable capacity during reader setup.

**Listed subtotal: US$10.99 + R$75.10**, including the main pair and every small-part quantity above at regular prices. This excludes shipping, import charges, printed parts, mounting hardware, power accessories and calibration equipment. Pix pricing on the Bit Maker pair reduces the BRL subtotal to R$73.72. Compare shipping before ordering low-cost extras separately, or buy equivalent connectors/buttons locally.

Reuse a suitable USB data cable and regulated 5 V USB supply. The v0.3 compact design uses an open carrier, rim rails, adjustable guides and a side-mounted PN532 pod. The current CAD guide lists the exact printable parts and quantities. It supports a proposed 50–100 mm spool-width range, with no axle or bearings. Check fit samples against the delivered parts before final printing. The [assembly guide](docs/ASSEMBLY.md#mounting-hardware) records the intended fasteners, including the revised rim-rail, guide and reader-pod hardware. The selected cell requires two M4 and two M5 mounting screws; confirm lengths against actual support thickness and thread depths. Nonslip feet and matching washers can come from a local hardware shop. Mounting hardware remains excluded from the subtotal above.

For the first calibration, borrow known reference masses or have a stable object weighed on a trusted reference scale. A nominal 1 kg filament spool or a bottle filled to a volume mark is not a reliable calibration mass. A costly certified weight is unnecessary for the initial prototype.

**The selection resolves the remaining electrical choices.** Power the Waveshare screen, PN532 and selected HX711 from 3.3 V, with common ground. The screen's BL pin can be tied to 3V3 for constant backlight; its [schematic](https://www.waveshare.net/w/upload/e/ee/2inch_LCD_Module_SchDoc.pdf) includes the backlight transistor. Use the supplied display cable and the signal assignments in [WIRING.md](WIRING.md). Confirm the assembled board's supply remains stable with all peripherals operating.

Design the mounting around the selected cell's actual 80 mm body, mounting holes and measured dimensions. Its seller specifies red to E+, black to E−, green to A+ and white to A−. Follow those colors only for this selected cell, and verify the received labeling before assembly.

The screen's [manufacturer documentation](https://docs.waveshare.net/2inch_LCD_Module/) gives its 58 × 35 mm board dimensions and interface. The exact SKU is the separate display module, without an ESP32 or touch controller. The three tactile buttons provide navigation. The enclosure CAD prototype uses these published dimensions and the estimates recorded in [MECHANICAL-SOURCES.md](docs/MECHANICAL-SOURCES.md). Actual hole diameters, thread depths, connector clearance and physical fit remain to be checked. Writable inventory tags do not change the decision to retain stock Snapmaker U1 firmware.

For the side reader, allow four flexible stranded leads and strain relief for a slack service loop. The existing jumper leads can establish electrical operation, but their stiffness may bias a load cell if they pull on the moving cradle. Final cable choice and routing need a zero/repeatability check. Use nylon fasteners near the antenna; consult the v0.3 assembly guide before buying the revised guide and pod fasteners.
