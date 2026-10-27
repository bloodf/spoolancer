# Changelog

## 0.5.0-alpha.1

Adds five-LED and OLED-plus-five-LED variants using the existing standalone ESP32-S3. Both retain the upright cradle and seated side NFC reader. Physical fit and scale operation remain unvalidated.

- Two DC-input case prototypes with five round openings at 16.667 mm pitch, closed USB walls, separate BOOT access and removable front pods. The builder drills the jack hole and mounts a separate 12 V-to-5 V converter.
- Fifteen LED-only and sixteen OLED STEP/STL pairs, including fit samples, plus editable FreeCAD/assembly STEP. Prior released CAD remains unchanged.
- New firmware profiles show a valid remaining-weight bar, operation feedback and optional OLED status. Missing/stale measurements and ambiguous NFC reads do not produce success indicators.
- GPIO4/5 OLED I²C and GPIO6 LED data, configurable controller/address/color order and 20% default brightness. Hardware identities remain provisional.
- All five firmware profiles compile; native sanitizer tests, 26 web tests and both new asset images pass. New CAD exports reopen and pass modeled clearance checks.
- One ESP32-S3 was backed up, flashed and booted over USB. Serial output confirmed 16 MB flash, approximately 8 MB PSRAM, mounted assets/inventory and setup address. OLED was not detected at `0x3c`; other peripheral and DC-power behavior is untested.

The application identifies itself as `0.5.0-prototype`. See [verification limits](docs/STATUS.md) and [the boot record](docs/DEVICE-BOOT.md).

## 0.4.1-alpha.1

First public prototype release. This is a source and CAD release; physical assembly, weighing accuracy, NFC range and live Spoolman operation still need validation.

- Compact upright cradle for proposed spool widths of 50–100 mm, with the NFC reader on the moving side guide.
- Screenless ESP32-S3 N16R8 and optional ESP-8048S043C-I capacitive-touch variants.
- Individual STEP and STL files for every current printable part and fit sample, plus assembly STEP, native FreeCAD documents and editable generation scripts.
- Local browser interface for weighing, inventory, color/material/brand editing, calibration, Wi-Fi setup, light/dark appearance and optional Spoolman synchronization.
- Onboard BOOT handles setup and Wi-Fi recovery. No separate button or perfboard is required.
- NTAG216 inventory records with read-back verification. Original printer tags can be linked by UID; proprietary decoding and printer-recognized custom tags are not implemented.
- MIT license, project icon, build/printing guides, contribution templates, automated software checks and checksummed release bundles.

The application identifies itself as `0.4.1-prototype`. The release suffix describes the public package's pre-release status. Historical local revisions are preserved under `archive/`; they are not current build instructions.
