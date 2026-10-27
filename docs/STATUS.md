# Spoolancer verification status

## v0.6 rounded CAD and assembly viewer — 27 October 2026

The LED-only and OLED-plus-LED CAD now include real rounds and chamfers, with matching interactive and static assembly views. **Physical fit, stiffness, tipping stability, cable forces and weighing accuracy remain unvalidated.** Firmware and the previous device-flash record are unchanged.

| Area | Evidence | Limit |
| --- | --- | --- |
| CAD edge treatment | 17 explicit operations per variant; all ten unique production models changed. Comparisons find no added volume and no removed material in 24 protected regions for LED-only and 26 for OLED | Digital shape comparisons do not establish strength or print quality |
| STEP/STL/native exports | 15 LED-only and 16 OLED single-solid STEP/STL pairs reopened; native FreeCAD and complete STEP assemblies reopened; modeled collision, 27 spool cases, nine reader placements, BOOT approach and front clamp checks pass | Delivered component dimensions and real assembly remain provisional |
| Earlier CAD | 121 original headless/touch CAD/export/preview artifacts unchanged | Those models retain their earlier validation limits |
| Assembly images | Seven views per new variant, rendered from final scene meshes; numbered leaders and metadata shared with viewer; scene/metadata/renderer/image hashes recorded | Wiring, converter, jack and some fasteners are omitted; exploded offsets are illustrative |
| Interactive viewer | Exact regenerated mesh bytes checked against both CAD scenes; 28-group transform/visibility tests, JavaScript syntax and bundled library hashes pass | WebGL2 required; assets are for a computer/phone browser, not the ESP32 filesystem |
| Browser behavior | In-app browser loaded final CAD; rotation, zoom, assembled/exploded states, spool hiding, transparent housing, part selection/isolation, light/dark themes and both variant selectors checked | Desktop browser observation; no physical hardware or mobile-device result |

The viewer has eleven selectable printed instances per variant, 26 total labeled groups for LED-only and 28 for OLED. Twelve nominal M3 clamp sets and four stops are shown as simplified references. The converter volume is unassigned space; no jack hole or converter mounting location is implied. Parts 01–11 identify the printed assembly, including two copies of the same rim rail.

Sources: [edge and protected-region comparison](../cad/edge-finish-verification.json), [LED export checks](../cad/variants/headless-led/exports/export-verification.json), [OLED export checks](../cad/variants/oled-led/exports/export-verification.json), [LED render manifest](../cad/variants/headless-led/previews/render-manifest.json), [OLED render manifest](../cad/variants/oled-led/previews/render-manifest.json), [viewer tests and reproduction](../viewer/README.md) and [assembly guide](ASSEMBLY-VIEW.md).

No device flash, print job, NFC operation or inventory write was performed for this update. No sliced material or print-time estimate is claimed for v0.6. The application still identifies itself as `0.5.0-prototype`.

## Historical v0.5 LED/OLED update — 27 October 2026

Both new cases and firmware profiles are implemented. One owned ESP32-S3 has been flashed and reached its ready state over USB. **Physical enclosure fit, weighing, peripheral operation and real Spoolman integration remain unvalidated.** The older v0.4.1 record below is retained as historical evidence; its no-flash statements describe that earlier checkpoint.

| Area | Current evidence | Limit |
| --- | --- | --- |
| LED/OLED CAD | 15 and 16 STEP/STL pairs reopened; editable FreeCAD/assembly STEP reopened; valid solids/meshes; modeled clearances, 27 spool cases, nine reader placements, BOOT approach and two new front clamps pass | Provisional OLED/strip/plug dimensions; no real print, fit, strength or power assessment |
| Existing CAD | 121 earlier CAD/export/preview artifacts unchanged | Earlier physical-fit limits remain |
| Firmware | All five environments compile; alternate SH1106/GRBW build compiles; native ASan/UBSan tests pass | Compilation and simulated logic are not peripheral validation |
| Web | 26 logic tests pass; OLED variant uses the same physical open-AP enrollment path | No real-device browser session checked |
| Asset images | Both new LittleFS images unpack to the current five web files and pass partition guards | Does not establish upgrade retention from an existing inventory |
| Physical controller | Application and assets flashed with verified upload hashes; USB boot reports 16 MB flash, approximately 8 MB PSRAM, ready assets/inventory and setup AP/address | One board and short serial captures only; no HTTP, button gesture or Wi-Fi association test |
| OLED | No acknowledgement at configured `0x3c` during startup | Controller, address, wiring and image remain unverified |
| DC supply and other peripherals | Wiring and assembly documented; no hardware result claimed | 12 V conversion, LEDs, HX711, PN532 and Spoolman remain untested |

Sources: [CAD delivery report](../cad/indicator-delivery-verification.json), [LED export checks](../cad/variants/headless-led/exports/export-verification.json), [OLED export checks](../cad/variants/oled-led/exports/export-verification.json), [USB boot record and artifact hashes](DEVICE-BOOT.md), [firmware checks](../firmware/README.md) and [variant instructions](OLED-RGB-VARIANT.md).

The full pre-flash backup remains private outside the repository. No purchases, print jobs, Wi-Fi credentials or real external inventory writes were performed. The new cases have no external USB cutouts or converter mount; the builder drills the jack opening. USB startup does not validate the proposed 12 V-to-5 V power path.

## Historical v0.4.1 onboard BOOT record

Recorded 27 October 2026. The package contains a compact screenless CAD prototype, a provisional integrated-touch enclosure, a local web interface and ESP32-S3 firmware. No board has been flashed, assembled, physically measured or connected to a real Spoolman server.

### Checked evidence

| Area | Evidence | Limit |
| --- | --- | --- |
| Headless CAD | 11 matching single-solid STEP/STL pairs including three fit samples; nine production instances; closed meshes; 27 spool/reader cases and nine pod positions; native FreeCAD/STEP reopened | Nominal envelopes and hardware, not real fit, strength or tipping resistance |
| Clamping geometry | Modeled guide/rail/cover/pod screws and side-inserted nuts; nominal 2.5 mm BOOT tool path clears modeled geometry through the existing USB opening | BOOT location is photo-derived; actual button, USB plug, wiring and tool clearance need bench checks |
| Headless material | Earlier v0.4 estimate: 392.32 g PETG versus 421.80 g v0.3 and 617.96 g v0.2 | Historical estimate includes the removed button cap and old cover; v0.4.1 has not been re-sliced. Fit samples, supports and printer-specific changes excluded |
| Touch hardware basis | User confirmed ESP-8048S043C-I; supplied Guition page-6 outline and board-family pin reference reviewed | Exact received revision, touch operation, connectors and enclosure fit untested |
| Touch CAD | 14 matching single-solid STEP/STL pairs including three fit samples; 27 spool/reader cases, nine pod placements, eight touch fastener checks and reopened FreeCAD/STEP pass | Geometry only; 12 printed instances with an open-back holder; actual board/glass/connector fit remains provisional |
| Touch material | Unchanged touch geometry: 427.29 g PETG, 30.85% below v0.2 | Same slicing settings; shared STL hashes match headless; no supports or fit samples in estimate |
| Web logic | 26 dependency-free tests and JavaScript syntax checks passed | Local logic and deterministic preview adapter; not real hardware |
| Browser workflows | Updated in-app demo displays the BOOT runtime instructions and keeps enrollment disabled before the simulated press. Earlier demo checks covered dark appearance, custom material/color save/reopen, swatch/hex/picker agreement, enrollment, Settings, save/sync/NFC and rejections | Localhost simulation only; no real Wi-Fi credentials entered, tags written or remote inventory changed |
| Firmware compilation | All three environments compiled; latest reported sizes below | Compilation cannot establish boot, display, touch, UART, ADC or Wi-Fi behavior |
| Firmware logic | Native ASan/UBSan tests cover measurements, identifiers, NDEF boundaries/truncation, mocked HX711 sampling, BOOT startup-held suppression, 2/8-second boundaries, one event per hold, rollover and one-use enrollment/Host/Origin checks | No RF, physical button, on-device HTTP, flash endurance or real server exercised |
| Web asset source | Five device files, 82,977 bytes raw; 23,945 bytes when individually gzipped | Gzip size is a size comparison, not a claim that firmware serves compressed files |
| Filesystem packaging | All three asset images unpack to the five current web files. Binary partition tables and 7,274,496-byte images checked; build/upload guard verifies assets at `0x810000` and excludes inventory at `0xf00000` | No flashing occurred; this verifies built artifacts and tool selection, not physical flash retention |

Final compiled application/static-RAM sizes:

| Environment | Application bytes | Static RAM bytes |
| --- | ---: | ---: |
| `spoolancer-headless` | 924,025 | 48,056 |
| `spoolancer-touch-8048` | 976,673 | 48,844 |
| `spoolancer-s3` (legacy) | 937,929 | 48,312 |

Sources: [per-part STEP delivery checks](../cad/step-delivery-verification.json), [touch CAD checks](../cad/variants/touch-8048/exports/validation.json), [touch reopen/fastener report](../cad/variants/touch-8048/exports/export-verification.json), [touch slicing](../analysis/material-comparison/v0.4-touch.json), [headless CAD checks](../cad/variants/headless/exports/validation.json), [export reopen report](../cad/variants/headless/exports/export-verification.json), [headless slicing report](../analysis/material-comparison/v0.4-headless.json) and [firmware build guide](../firmware/README.md). Browser screenshots show [dark sample data](web-preview-dark.png), [the editor](web-editor-dark.png) and [simulated setup](web-setup.png), not device readings.

### Behavior and boundaries at that revision

- Screenless uses the existing N16R8 board; touch uses the integrated ESP32 on the ESP-8048S043C-I. Both now use onboard BOOT on GPIO0. The headless external switch, perfboard, button wiring, cap and two M2 screws are removed. The old ST7789 build remains a separate firmware environment and archived enclosure.
- Both current builds use an open setup AP, chosen eight-digit PIN and physical 2-second arm/60-second one-attempt enrollment. The AP closes after ten minutes or 60 seconds after LAN connection. A configured Wi-Fi failure does not open it automatically. While firmware is running, an 8-second BOOT hold clears Wi-Fi/pairing and reopens setup without rebooting; inventory, calibration and Spoolman settings/links remain. Release BOOT before another gesture. Holding BOOT at power-on or during RST enters the programming bootloader instead; release BOOT and tap RST to run normally.
- PIN and Wi-Fi credentials use local HTTP. Setup must happen in a trusted location; physical/time restrictions are not encryption. Browser storage contains only appearance preference. Native theme storage is separate.
- Native touchscreen actions cover weighing, profile selection, read/write, save/sync/zero and light/dark appearance. Invalid net weights fall back to gross; expired setup addresses are hidden; ambiguous multi-tag scans clear selection. Full metadata, calibration-reference and connection editing stay in the browser. Native hardware operation is unverified.
- Scale operating target is 20–2,000 g gross. The 5 kg cell also carries the printed mechanism, reader, wiring and fasteners. Accuracy and overload protection have not been established.
- Remaining filament requires known empty-spool weight, a fresh calibrated stable reading and the matching active profile. Unknown tare never becomes a guessed remaining amount.
- The shared moving side reader supports proposed seated NFC alignment across 50–100 mm spool widths. Cable forces and actual read range require physical checks.
- NTAG216 writes use Spoolancer schema 1 and read-back verification. Original Bambu/Snapmaker tags are UID-only/read-only; proprietary decoding and OpenSpool interoperability remain unimplemented.
- Spoolman uses optional HTTP/numeric-IPv4 setup and explicit absolute-weight updates. Uncertain creates must be reconciled; no automatic retry queue exists. Local metadata edits do not rewrite remote filament records.

### Remaining physical checks at that revision

1. Measure the received boards, holes, glass/connector projections and cell thread depths. Print the selected variant's fit samples before its full enclosure.
2. Verify controller memory, 3.3 V power, raw HX711 readings, PN532 HSU and actual BOOT timing and press/release access with cables fitted. For touch, check RGB timing, GT911 coordinates and BOOT access; leave SD empty.
3. Assemble the moving load path and test guide extremes, spool stability, stop adjustment and reader heights. Eliminate cable contact/pull before zero and calibration.
4. Test actual open-AP enrollment, invalid/expired attempts, LAN reconnection/grace, 8-second recovery, retained records and power-loss behavior.
5. Check seated NFC range through final plastic and spool flanges; test unsupported/locked tags, interrupted writes and original-printer UID links.
6. Calibrate against independent known masses, repeated placements, off-center loads and warm-up drift. The ±5 g target is not achieved evidence.
7. Exercise native confirmations and real browser/API behavior, then a disposable Spoolman create/link/update with deliberate network failures and uncertain-response reconciliation.

No purchases, print jobs, firmware flashes or real external inventory writes were performed. The source, CAD and project artwork now carry MIT; third-party reference images and dependencies retain their own rights. The first public package is marked as a pre-release.
