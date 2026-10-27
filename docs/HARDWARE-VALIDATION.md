# Hardware validation report

Use this report for an actual Spoolancer build. Current [project evidence](STATUS.md) covers CAD, export reopening, software tests and compilation. **All physical checks below start as not run.** Completing a browser demonstration or rebuilding firmware does not complete a hardware check.

Copy this document into your build notes or a GitHub issue. Record measured values and photos where useful, without Wi-Fi credentials, pairing PINs or session tokens.

## Build record

| Field | Your result |
| --- | --- |
| Date, project version and commit | Not recorded |
| Variant and firmware environment | Not recorded |
| Controller markings, board revision, detected flash and PSRAM | Not recorded |
| Load-cell and HX711 model or seller code | Not recorded |
| PN532 board revision and tag type | Not recorded |
| Printer, nozzle, material and slicer profile | Not recorded |
| CAD parameter changes and fastener substitutions | Not recorded |
| USB supply/cable and reference scale or masses | Not recorded |

## Fit and assembly

| Check | What to record | Result |
| --- | --- | --- |
| Fit samples | Board dimensions, cell holes/threads, actual screw/washer/nut fit, PN532 exit clearance; touch PCB/glass fit if used | Not run |
| USB and BOOT access | Cable plugged in, cover fitted, button pressed and fully released through its intended access; no contact with surrounding electronics | Not run |
| Clamps and adjustment | Rail/guide clamping, actual 50 mm and 100 mm spool cases, reader height travel and flange clearance | Not run |
| Load path | Empty and loaded carrier clears the cover, base, stops and fixed wiring; cell is the only intended load path | Not run |
| Stability and feet | Even support on a flat surface, upright spool retention and stability at guide/reader extremes | Not run |
| Stop setting | Measured loaded deflection and chosen gap. Record separately from any overload claim | Not run |

Follow the [assembly guide](ASSEMBLY.md) for the modeled hardware stack. Do not use an overload test as the first fit check. The 1 mm initial stop gap and 5 kg cell label do not establish printed strength or a safe overload limit.

## Power, setup and recovery

| Check | Expected behavior / evidence to record | Result |
| --- | --- | --- |
| Power and boot | Confirm 16 MB flash and 8 MB PSRAM; record 3.3 V rail measurements with Wi-Fi, NFC and optional screen active; note resets or brownouts | Not run |
| Peripherals | HX711 raw reading changes with load; PN532 responds over HSU with switches 1/2 OFF | Not run |
| First enrollment | Setup AP and manual address work; enrollment requires runtime BOOT hold/release for two seconds; one attempt within 60 seconds | Not run |
| Enrollment expiry | Invalid attempt consumes the window; expired window rejects enrollment until a new physical press | Not run |
| Wi-Fi handover | LAN address works; setup AP closes after ten minutes or 60 seconds after connection; configured Wi-Fi failure does not reopen it automatically | Not run |
| Recovery and retention | Runtime BOOT hold for eight seconds resets Wi-Fi/pairing once; old session stops working; inventory, calibration and Spoolman settings remain | Not run |
| Normal restart | Short BOOT press does not reset Wi-Fi; RST restarts normally; retained records return after power cycling | Not run |
| Touch build only | Image, touch coordinates, edge controls and native confirmations work; microSD slot stays empty | Not run |

Run setup in a trusted location. The temporary AP has no password and transmits local HTTP. Keep a copy of test metadata before recovery tests so retained fields can be compared.

## Weighing

Calibrate with the complete moving assembly installed, including reader, pod, fasteners and flexible cable loop. Zero the empty stable cradle, then use a known mass in the firmware's 100–2,000 g calibration range. Use independent masses for the checks below; repeating the calibration mass alone does not establish accuracy.

| Check | Procedure | Result |
| --- | --- | --- |
| Independent masses | Check several known masses across the intended 20–2,000 g gross range. Record reference mass, indicated mass and signed error | Not run |
| Repeatability | Remove and replace the same known load ten times. Record all readings and the maximum spread | Not run |
| Zero return | Record unloaded readings before and after those placements | Not run |
| Position sensitivity | Repeat with the load centered and at normal off-center spool positions; keep it supported within the rails | Not run |
| Reader/cable force | Repeat empty and loaded readings at low/high reader positions and narrow/wide guide settings. Record zero shifts before re-zeroing | Not run |
| Warm-up drift | Record zero and a fixed reference load at startup and after 5, 15 and 30 minutes | Not run |
| Measurement rejection | Unstable, stale, uncalibrated or over-range readings do not save/sync; unknown empty-spool weight does not become a guessed net weight | Not run |

The design's **±5 g accuracy target is a proposed acceptance target**, not a measured specification. Firmware's 5 g stability-window threshold does not prove ±5 g accuracy. Record any failed mass or configuration instead of reporting only an average. A changed reader position that changes zero requires cable-routing work and rechecking before trusting saved weights.

Suggested measurement table:

| Reference mass, g | Placement / reader setting | Indicated gross, g | Error, g | Empty return, g | Notes |
| ---: | --- | ---: | ---: | ---: | --- |
| | | | | | |

## NFC and local records

| Check | Expected behavior / evidence to record | Result |
| --- | --- | --- |
| Seated alignment | Reads at intended spool widths and reader heights through the printed window; record tag position and actual gap | Not run |
| NTAG216 write/read-back | A spare supported blank tag accepts a Spoolancer record; rescan returns the intended metadata and UID | Not run |
| Refused writes | Wrong type, locked/protected tag, foreign content or changed UID produces refusal rather than success | Not run |
| Interrupted write | On a spare tag, movement or interruption reports failure; rescan shows the resulting tag state before retrying | Not run |
| Original printer tags | Bambu/Snapmaker tags identify only by UID and remain read-only; a local UID link selects the expected manually entered profile | Not run |
| Record persistence | Brand, material, color, empty-spool weight and saved measurement survive normal restart; selecting another spool does not overwrite the first | Not run |

Use spare tags for interrupted-write tests. NTAG216 writes are not transactional. Native touch and browser actions must use the same active spool and confirmation rules; test both when building the touch version.

## Optional Spoolman checks

Use a disposable test record on a Spoolman server you control. Configure its numeric local IPv4 HTTP address. Record remote IDs and outcomes, not credentials.

| Check | Expected behavior / evidence to record | Result |
| --- | --- | --- |
| Connection and link | Test connection works; linking an existing spool ID selects the intended server record | Not run |
| Create | One explicit sync creates the expected vendor/filament/spool and saves its returned ID locally | Not run |
| Weight update | A known measurement updates absolute `remaining_weight`; repeating it does not subtract consumption twice | Not run |
| Failure and reconciliation | Network failure reports failure/unknown outcome. Inspect the server before retrying any uncertain create and link the existing record | Not run |
| Local metadata change | Editing a linked local spool does not silently rewrite remote filament metadata | Not run |

## Publish the result

Report the exact files and hardware used, measured errors, failed checks and modifications. Distinguish a successful single build from compatibility with other board revisions or spool types. A useful first report includes fit photos, the measurement table and whether NFC worked with the spool seated.
