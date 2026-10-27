# v0.3 prototype status

Recorded 26 October 2026. This package provides a compact CAD prototype, a local web interface and an initial ESP32-S3 implementation. Digital checks have passed in the scopes below. The project has not been flashed, assembled, physically measured or connected to a real Spoolman server.

## Checked

| Area | Evidence | Limit |
| --- | --- | --- |
| CAD geometry | 14 single-solid STL exports; closed meshes without detected self-intersections; modeled assembly and clearance checks; 27 spool/reader cases; native FreeCAD and STEP reopened successfully | Simplified envelopes and nominal hardware, not actual fit or strength |
| Fastener geometry | Side-inserted captive nuts with retaining roofs; modeled guide, rail, cover and pod fastener checks | Printed tolerances, nut insertion, thread engagement and PCB fasteners need bench checks |
| Filament estimate | v0.2: 617.96 g; v0.3: 421.80 g; 31.74% less using the same OrcaSlicer/PETG profile | Sliced estimates; fit samples, support additions and printer-specific changes excluded |
| Web logic | 18 tests and JavaScript syntax checks passed | Deterministic local logic/demo, not a real API server |
| Browser workflow | In-app browser: paired, added/selected a spool, saved a simulated measurement, confirmed simulated Spoolman creation and tag read-back; unstable weights blocked save/sync; original tags blocked writes | Explicit localhost demo; no hardware, real tags or remote inventory changed |
| Firmware | PlatformIO build passed; application 937,433 bytes, static RAM 48,312 bytes; 16 MB image header verified | Actual memory, boot and peripherals untested |
| Firmware logic | Native AddressSanitizer/UndefinedBehaviorSanitizer tests passed for measurements, identifiers, NDEF boundaries/truncation and mocked HX711 sampling, sign, calibration and stale windows | Does not exercise the PN532 RF/UART, HTTP server on ESP32, flash durability or real ADC |
| Asset packaging | LittleFS build passed; exactly four web assets, 62,490 source bytes, matching `web/` | No assets or firmware uploaded to a board |

The [material results](../analysis/material-comparison/README.md) include source-STL hashes. CAD reports live in [exports](../cad/exports/validation.json). The [browser screenshot](web-preview.png) shows sample data, not device readings.

## Current behavior

- ESP32 serves the local interface. Pairing gates private inventory and mutations. Inventory, configuration and calibration are persisted locally; assets and inventory occupy separate flash partitions.
- Scale targets 20–2000 g gross operating weight. A 5 kg cell also carries the printed cradle, reader, wiring and fasteners. No accuracy or overload specification is established by that rating.
- Remaining filament requires a known empty-spool weight. A fresh, calibrated, stable reading and matching active profile are required to save or synchronize it.
- Side-mounted NFC is intended to read/write while the spool remains seated. Rotate the spool and adjust reader height to align the antenna with the tag; release it before accepting weight.
- NTAG216 writes use Spoolancer schema 1 and verify read-back. Original Bambu/Snapmaker tags are UID-only and read-only. Link their UID to a manual profile. Proprietary material decoding and OpenSpool interoperability remain unimplemented.
- Spoolman is optional, with HTTP/numeric local IPv4 configuration. Confirm creation or link an existing spool ID; sync absolute remaining weight. Existing remote filament metadata is not updated by local edits. Retry is manual, and an uncertain create result must be reconciled first.

## Next physical checks

1. Measure the received boards, cell holes and thread depths; print the five fit samples. Adjust CAD assumptions before the large parts.
2. Check boot/memory, each peripheral, 3.3 V supply and the complete wiring. Follow the [firmware guide](../firmware/README.md); installation commands are provided but have not been run.
3. Assemble the load path with every spool-contact part on the moving carrier. Check stiffness, upright stability and clearance with the actual 50–100 mm spools.
4. Install a flexible NFC service loop. Compare zero return and repeated known weights at both guide-width extremes and the reader's minimum/maximum heights. Eliminate wire contact or pulling before calibration.
5. Test seated read/write with actual NTAG216 tags and original printer-tag UID detection. Check range through the final plastic and spool flange, tag ambiguity, interrupted writes and display/Wi-Fi activity.
6. Calibrate and test independent masses through the target range, repeated placements, off-center loads and warm-up drift. The plan's ±5 g accuracy target is not yet achieved evidence.
7. Exercise real device persistence, reboot/offline behavior and a disposable Spoolman spool: create, link, absolute update, network failure and ambiguous-response reconciliation.

No publication, purchases, printer jobs, firmware flashing or real external inventory writes were performed. The source is prepared for further development; licensing/public release remains a separate step.
