# Changelog

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
