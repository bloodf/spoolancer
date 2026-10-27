# Third-party notices and provenance

The root MIT license applies to original Spoolancer code, CAD, documentation and project art. It does not relicense dependencies, manufacturer documentation, product photos, trademarks or external projects.

## Runtime and development dependencies

Dependencies are downloaded by the build tools; their source and toolchain caches are not included in this repository or the source release. Preserve their upstream notices when redistributing a build. The direct dependency versions are in [firmware/platformio.ini](firmware/platformio.ini); this is not a complete transitive lockfile.

| Component | Upstream and notice |
| --- | --- |
| ArduinoJson 7.4.2 | [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson), MIT |
| Adafruit NeoPixel 1.15.5, five-LED variants | [adafruit/Adafruit_NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel), retain its LGPL and architecture-specific source notices |
| U8g2 2.36.15, OLED variant | [olikraus/u8g2](https://github.com/olikraus/u8g2), retain its BSD and individual font notices |
| Adafruit ST7735/ST7789 library 1.11.0, legacy display | [adafruit/Adafruit-ST7735-Library](https://github.com/adafruit/Adafruit-ST7735-Library), retain its license and attribution |
| Adafruit GFX / BusIO and other transitives | [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) carries BSD notices; [BusIO](https://github.com/adafruit/Adafruit_BusIO) carries MIT notices; inspect the resolved packages |
| Arduino_GFX 1.4.9, touch display | [moononournation/Arduino_GFX](https://github.com/moononournation/Arduino_GFX); the installed package lacks a top-level license file and cites reused sources including Adafruit GFX and LovyanGFX. Do not assume the project MIT license covers it |
| Arduino ESP32, ESP-IDF and compiler/toolchain | [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32), [ESP-IDF](https://github.com/espressif/esp-idf) and their bundled components retain their own notices |
| PlatformIO, FreeCAD, rendering and slicing tools | Build-time tools with their own licenses; not vendored |

This release distributes firmware source, not precompiled application/toolchain binaries. A redistributed binary needs the applicable dependency notices. The lack of a bundled license in a dependency is not a grant of new rights.

## Hardware reference material

Images under `docs/reference/` and historical `archive/*/docs/reference/` are supplied seller/manufacturer reference images used to identify the specific boards and record provisional dimensions. Their artwork and visible manufacturer marks belong to their respective owners and are excluded from Spoolancer's MIT grant. They are not project logos or proof of a tested assembly.

The original project discussion supplied these product pages:

- [AliExpress component listing 1005009519771350](https://pt.aliexpress.com/item/1005009519771350.html)
- [AliExpress component listing 1005009330328373](https://pt.aliexpress.com/item/1005009330328373.html)
- [AliExpress touch-board listing 1005013088869895](https://pt.aliexpress.com/item/1005013088869895.html)

The [mechanical sources](docs/MECHANICAL-SOURCES.md) link the Elechouse PN532 manuals, cell drawing, display references and Guition outline. Full downloaded vendor PDFs and the vendor V4 CAD package are not distributed. The electronics in Spoolancer's CAD are simplified reference envelopes, not redistributed manufacturer CAD models.

## Project identity and inspiration

The raster project icon was created using image generation; the SVG companion and banner are authored project graphics. Their [asset notes](assets/brand/README.md) record the method and prompt. They are covered by the project MIT license.

The [Totally over-engineered filament spool balance](https://github.com/schnoog/Totally-over-engineered-filament-spool-balance) prompted the original project idea. It is referenced as inspiration, not vendored or claimed as Spoolancer's source. Bambu Lab, Snapmaker, ESP32, Spoolman and other names identify the referenced products/projects; their names and marks remain with their respective owners.
