# Spoolancer verification status

## OLED startup repair, 30 October 2026

A connected OLED acknowledged at `0x3c` but stalled during display initialization, preventing the main loop from serving HTTP or updating the LED status. Removing duplicate U8g2 pin configuration while retaining `Wire.begin(4, 5)` cleared that stall on the attached board. The application-only upload passed flash hash verification; post-fix serial output reached OLED initialization, status UI readiness and application readiness with both filesystems mounted. All five firmware profiles compiled. The owner confirmed that the OLED and webpage work; LED output, detailed UI actions and the soldered power circuit still need separate confirmation. See the [device record](DEVICE-BOOT.md#oled-initialization-repair-30-october-2026).

## v0.8 six-piece print redesign, 28 October 2026

The current LED and OLED/LED mechanical sets have **six printed pieces and twelve screws**. The fixed-width cradle replaces separate rails, thin frames, sliding width guides and the main lid. One broad glued joint attaches the reader tower; the cassette retains height adjustment. The original headless and integrated-touch CAD and all firmware are unchanged.

| Check | Evidence | Limit |
| --- | --- | --- |
| Printable files | Six single-solid STEP/STL pairs per variant; reopened meshes are closed, have no detected self-intersections, rest at Z=0 and agree with STEP dimensions/volumes | Mesh/solid checks do not prove a successful print |
| Assembly | Native FreeCAD and assembly STEP reopen; printed parts and component references have no detected positive-volume collisions | Board, connector and cell envelopes are provisional |
| Spool and reader fit | Nine full-cylinder spool cases (160/200/220 mm diameters × 50/80/100 mm widths), seven reader heights and the provisional BOOT probe clear modeled parts | Physical tag position, stability, cable forces and off-center scale accuracy are untested |
| Service | Five sampled vertical lifts clear modeled fixed parts after removing the spool and moving-end screws and disconnecting the reader lead | The loose spacer must be supported; real connector handling is untested |
| Print poses | All six parts have broad flat undersides; the rear cover prints flat, with about 14,692 mm² bed contact | First-layer adhesion and surface finish remain untested |
| Slicing | 60 successful offline slices: 36 v0.7 baseline and 24 v0.8, across LED/OLED and P1S/U1 profiles. Four new parts generated no supports; base and tower retain localized supports | No job was sent to a printer; P1S profile temperature warning retained |
| Material and supports | New complete builds estimate 511–521 g PETG; support volume falls 83.4–86.3%, while filament rises 15.2–16.1% | Same diagnostic settings within each printer; these are slicer estimates, not weighed parts or strength results |
| Views | Nine CAD-derived images per variant, including assembled, exploded, component locations and actual print poses; source/image hashes recorded | Rendered electronics and cable routes are simplified references |
| Viewer | Exact asset checks pass; LED has 20 labeled groups, OLED has 21, both with six printed instances. Transform/visibility tests pass | Browser presentation and physical operation are separate checks |

Sources: [printability report](../analysis/printability-v0.8/README.md), [LED geometry](../cad/variants/headless-led/exports/validation.json), [OLED geometry](../cad/variants/oled-led/exports/validation.json), [reopened LED files](../cad/variants/headless-led/exports/export-verification.json), [reopened OLED files](../cad/variants/oled-led/exports/export-verification.json) and [assembly guide](ASSEMBLY.md).

**Physical printing, fit, bond strength, weighing accuracy, seated NFC and stability remain unvalidated.** The 1 mm initial stop gap is untuned. The CAD controls describe supported JSON inputs separately from dimensions fixed in the editable generator. This update performs no device flash.

## Historical glued-rail assembly option, 27 October 2026

The [glue assembly guide](https://github.com/bloodf/spoolancer/blob/b4534d792bfe316cf2ef9fed4b145b440af3957d/docs/GLUE-ASSEMBLY.md) documents an 18-screw option for the existing v0.7 LED/OLED print sets. It omits two rail screws, two ordinary nuts and two washers; the other joints remain mechanically fastened. The 20-screw CAD reference assemblies and their existing validation reports are unchanged. Published v0.7.0 ZIPs predate this supplementary guide.

Read-only inspection of the shared rail/carrier geometry confirmed flat mating faces and two 30 × 6 mm placement regions per rail, clear of the key pockets and holes. Permanent rails preserve guide-nut loading, carrier-to-cell screw access and the existing lid service path. Individual rail removal is no longer a service operation. This is geometric evidence only: **no glue, printed bond, cure time, joint strength or weighing performance has been physically tested**. No CAD geometry, firmware or device state changed for this documentation update.

## Historical v0.7 simplified fasteners, 27 October 2026

The v0.7 LED-only and OLED-plus-LED models reduce the screw count from 28 to 20. They use three dimensional sizes and four purchasing lines: 12 metal M3 × 12, four nylon M3 × 12, two M4 × 25 and two M5 × 12. The matching lists contain 16 nuts and 16 washers. The original headless/touch models keep their previous hardware.

The lid uses rigid rear hooks and two front screws. Each keyed rim rail has one central screw; guides retain two each. Raised stop posts use the common M3 × 12 length. PCB-edge retention removes the PN532 mounting screws, and each NFC cover uses a rigid tongue with one nylon screw. The [v0.7 BOM](https://github.com/bloodf/spoolancer/blob/v0.7.0-alpha.1/docs/BOM.md) and [assembly guide](https://github.com/bloodf/spoolancer/blob/v0.7.0-alpha.1/docs/ASSEMBLY.md) describe these joints.

Both complete CAD builds, reopened exports and regenerated assembly assets pass the local checks below. These results do not establish hosted-browser, mobile-device or physical-build behavior.

| Area | Current evidence | Limit |
| --- | --- | --- |
| Printable geometry | 14 LED-only and 15 OLED parts, including fit samples, are valid single solids with closed meshes and no detected mesh self-intersections; each build has ten production pieces | Digital topology does not establish print quality or strength |
| Reopened exports | All 14 LED-only and 15 OLED individual STEP/STL pairs reopen and match the design checks. Native FreeCAD assemblies contain 54 and 59 valid shapes, including ten printed instances; complete STEP assemblies reopen with 84 and 89 valid solids. Recorded source hashes match the generator, parameters, metadata and render scripts | Simplified reference bodies are included in complete assemblies; print individual files only |
| Chamfers and assembled clearance | 27 explicit chamfer operations per variant; assembled component checks, 27 spool cases, nine reader placements and BOOT approach pass | Current results are in each variant's validation report; the shared edge-finish report remains historical v0.6 evidence |
| Lid, rails and stops | Conservative lid release/lift envelopes clear the installed mechanism at the 50 mm guide setting; rails clear a 60 mm removal lift with both guides removed; twelve modeled stop settings clear surrounding parts | Removal preconditions are required. The 1 mm initial stop gap is untuned and has no demonstrated overload rating |
| NFC joints and service | Actual modeled hardware at nine width/height settings, tool access, sampled nut loading, sampled whole-pod/cover/PCB removal and geometric nut-retention checks pass | Sampling and geometric obstruction do not measure retention force, fatigue, shim pressure or print tolerance |
| Reader wiring | Discrete cable routes and conservative continuous height/width envelopes pass; the removable cable cover's hardware, tool and sampled removal checks pass | Cable shape is a nominal envelope. Cable forces, fatigue and real wire bending remain untested |
| Assembly images | Eight regenerated views per variant have matching scene, metadata, renderer and image hashes in their manifests | Rendered placement references do not establish physical assembly or cable forces |
| Interactive viewer | Exact generated model bytes match both final CAD scenes; model/visibility behavior tests pass. LED-only has 29 labeled groups and OLED has 31, each with ten printed instances | Local asset and logic checks; hosted-browser and mobile-device behavior are separate checks |
| Unchanged components | Firmware, web interface and original headless/touch CAD have no changes; all 17 original preview images retain their hashes | Earlier hardware and software evidence keeps its original scope |

Sources: [LED validation](https://github.com/bloodf/spoolancer/blob/v0.7.0-alpha.1/cad/variants/headless-led/exports/validation.json), [OLED validation](https://github.com/bloodf/spoolancer/blob/v0.7.0-alpha.1/cad/variants/oled-led/exports/validation.json), [LED export checks](https://github.com/bloodf/spoolancer/blob/v0.7.0-alpha.1/cad/variants/headless-led/exports/export-verification.json), [OLED export checks](https://github.com/bloodf/spoolancer/blob/v0.7.0-alpha.1/cad/variants/oled-led/exports/export-verification.json), [LED render manifest](https://github.com/bloodf/spoolancer/blob/v0.7.0-alpha.1/cad/variants/headless-led/previews/render-manifest.json), [OLED render manifest](https://github.com/bloodf/spoolancer/blob/v0.7.0-alpha.1/cad/variants/oled-led/previews/render-manifest.json), [viewer checks](../viewer/README.md), [current hardware schedule](BOM.md#current-ledoled-fasteners) and [v0.7 service instructions](https://github.com/bloodf/spoolancer/blob/v0.7.0-alpha.1/docs/ASSEMBLY.md#service-the-current-ledoled-reader).

The reduced screw count uses reinforced printed posts and locating keys. Production CAD solid volume is **445,142.83 mm³ for LED-only** and **445,834.12 mm³ for OLED**, compared with 431,404.83 and 432,096.12 mm³ in v0.6, about **3.2% more solid geometry**. There are still ten printed pieces. These are CAD volume totals with production quantities, not sliced filament mass, support usage, print-time estimates or physical measurements.

**Physical fit, joint retention, strength, tipping stability, cable forces and weighing accuracy remain unvalidated.** The historical evidence below applies to its named revisions and does not establish v0.7 results. Firmware remains `0.5.0-prototype`; this update performs no flash or peripheral operation.

## Historical v0.6 chamfered CAD and assembly viewer, 27 October 2026

The LED-only and OLED-plus-LED cases use integrated vertical front controls, a covered adjustable-reader cable route and actual exterior chamfers. Each has ten printed instances from nine production models. **Physical fit, stiffness, tipping stability, cable forces and weighing accuracy remain unvalidated.** Firmware and the previous device-flash record are unchanged.

| Area | Evidence | Limit |
| --- | --- | --- |
| CAD edge treatment | 20 explicit chamfer operations per variant; each removes material only. Eighteen protected functional regions per variant remain unchanged. The report separates edge treatment from added/removed redesign geometry | Digital shape comparisons do not establish strength or print quality |
| STEP/STL/native exports | 14 LED-only and 15 OLED single-solid STEP/STL pairs reopened, including fit samples; native FreeCAD and complete STEP assemblies reopened. Modeled collision, 27 spool cases, nine reader placements, BOOT approach, clamp checks and a continuous 40 mm OLED insertion sweep with the main lid removed pass | Delivered component dimensions and real assembly remain provisional |
| Covered NFC cable route | Nine actual path cases per variant plus conservative continuous height, full-width tail/outlet and clamp-tool clearance checks pass. Moving reader route has a constant modeled centerline length and R10 bends | The internal base-tail drawing has an illustrative R5 arc and changing drawn lengths. Neither it nor the swept envelope proves real cable behavior, force or bend suitability |
| Earlier CAD | 121 original headless/touch CAD/export/preview artifacts unchanged | Those models retain their earlier validation limits |
| Assembly images | Eight views per new variant, rendered from final scene meshes; numbered leaders and metadata shared with viewer; scene/metadata/renderer/image hashes recorded | Cable paths and electrical component bodies are nominal references; jack, converter, terminations and some fasteners are omitted |
| Interactive viewer | Exact regenerated mesh bytes checked against both CAD scenes; 30-group transform/visibility tests, JavaScript syntax and bundled library hashes pass | WebGL2 required; assets are for a computer/phone browser, not the ESP32 filesystem |
| Browser behavior | In-app desktop browser loaded final CAD; rotation, zoom, assembled/exploded states, spool hiding, transparent housing, part selection/isolation/focus, light/dark themes and both variant selectors checked. Alternate-height cable routes remain hidden in ordinary assembly views | Desktop browser observation; no physical hardware or mobile-device result |

The viewer has ten selectable printed instances per variant, 28 total labeled groups for LED-only and 30 for OLED. Printed parts 01–09 include two copies of the rim rail; part 29 is the new cable cover. Retired parts 10/11 and converter-space reference 20 are removed. Twelve nominal M3 clamp sets and four stops are simplified references. No jack hole or reserved converter mounting volume is implied.

The printed footprint is 170 × 160 mm for both versions. Solid CAD volume, including production quantities, changes from 438,009.46 to 431,404.83 mm³ for LED-only and from 451,819.89 to 432,096.12 mm³ for OLED. These figures describe geometry; they do not predict sliced filament mass, support usage or print time.

Sources are retained in the [v0.6.0-alpha.1 release](https://github.com/bloodf/spoolancer/releases/tag/v0.6.0-alpha.1) and its [CAD reports](https://github.com/bloodf/spoolancer/tree/v0.6.0-alpha.1/cad). The current working-tree reports describe the newer revision.

No device flash, print job, NFC operation or inventory write was performed for this update. The application still identifies itself as `0.5.0-prototype`.

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
