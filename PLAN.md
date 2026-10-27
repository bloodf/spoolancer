# Spoolancer build plan

Updated 27 October 2026. **v0.4.1 provides screenless and integrated-touch variants with onboard BOOT setup/recovery** of a standalone upright spool scale. Source, CAD, browser checks and compilation are separate from physical validation. No board has been flashed or tested with real spools, tags or printers.

The user has Bambu Lab and Snapmaker U1 printers, an ESP32-S3 N16R8 development board and a PN532. The U1 retains stock firmware. The scale weighs a spool when needed before/after printing, rather than feeding filament during a print. Writable tags serve the scale's inventory.

## Build choices

| Variant | Controller and controls | Mechanical direction |
| --- | --- | --- |
| Screenless | Reuse the N16R8 board and its BOOT button on GPIO0; full browser interface; no separate switch/perfboard | 170 × 160 mm fixed base, no display opening |
| Integrated touch | Confirmed **ESP-8048S043C-I**, integrated N16R8 and 4.3-inch 800 × 480 capacitive panel; onboard BOOT for setup | Shared load mechanism with removable fixed front holder, 170 × 205.73 mm unloaded footprint; physical fit pending |
| Legacy v0.3 | Separate N16R8, Waveshare 2-inch ST7789 and three buttons | Mechanically archived; firmware `spoolancer-s3` retained |

The touch board replaces the separate ESP32 and small screen. Full metadata/configuration editing stays in the browser. Native touch controls cover live weighing, previous/next profile selection, NFC read/write, save, sync, zero and light/dark appearance. Native confirmations recheck the current profile, UID and measurement before action; physical display/touch behavior remains unverified.

The selected touch model comes from the [user's listing](https://pt.aliexpress.com/item/1005013088869895.html) and supplied images. The [Guition specification, page 6](https://www.guition.com/icms/upload/fb081940d6fc11f09850077a33e1404f/FTPData/UEditor/file/2026121/1768961092165/ESP32-4827S043%20Specifications-EN.pdf#page=6) gives the 123 × 74 mm mechanical outline; its hole-spacing/offset inconsistency remains a fit assumption handled in CAD. The [ESP3D hardware reference](https://esp3d.io/esp3d-tft/version_1x/hardware/esp32-s3/sunton-43-8048/) supplies the board-family signal map; its conflicting outline is not used for mechanical sizing.

Use the [parts list](SHOPPING.md), [variant wiring](WIRING.md), [CAD guide](cad/README.md) and [assembly guide](docs/ASSEMBLY.md) for current quantities. Shared additions are the selected Bit Maker 5 kg load cell code 1381, HX711 code 0679 and writable NTAG216 inventory stickers. Their purchase links and previously observed prices are retained in the shopping list.

The older ESP32/PN532 listings were [1005009519771350](https://pt.aliexpress.com/item/1005009519771350.html) and [1005009330328373](https://pt.aliexpress.com/item/1005009330328373.html). Their online contents could not be inspected. The supplied [ESP32](docs/reference/esp32-s3-n16r8-v6.png) and [PN532](docs/reference/pn532-v3-listing.png) photos support visible board families and pin/switch labels, not clone authenticity or electrical performance.

## Shared mechanism and measurement

The spool stands upright on two rim rails, with a moving carrier and adjustable guides. Proposed width range is **50–100 mm**; set guide spacing to actual width plus 4 mm. The reader is mounted on the moving right guide, with adjustable height and a thin antenna window. No axle or bearings are needed. Diameter references of 160/200/220 mm are clearance cases, not proof that all commercial spools of those sizes fit.

Place the spool, align its tag with the side reader and release it. Rotation while seated may be necessary. All spool-contact parts, guides and the reader must remain on the weighed assembly. Bolt one load-cell end to the fixed base and the other to the carrier. Leave its sensing section free and tune stops from measured deflection; the nominal stop gap is not a validated overload rating.

The 5 kg load cell carries the spool plus the moving cradle, reader, fasteners and cable. Initial firmware targets **20–2,000 g gross spool weight**. A larger nominal filament entry in inventory does not increase that range. Verify mounting-hole dimensions and thread depth; the seller's 6H marking is a thread tolerance, not a screw-engagement depth.

Use flexible four-wire HSU leads with a slack service loop between moving reader and fixed electronics. Cable pull, contact or stiffness can bias weighing. Test zero return and known masses at both width extremes and all reader heights. Re-zero the complete empty mechanism after adjustment. After NFC handling or a network sampling gap, wait for a fresh stable window before accepting weight.

`Remaining filament = measured gross spool weight − known empty-spool weight`

For example, 842 g gross minus 250 g empty gives 592 g remaining. These are examples, not manufacturer spool specifications. A nominal 1,000 g filament label cannot provide empty-spool weight. Unknown tare means gross-only display and blocked net-weight save/sync. Weight alone cannot identify material, brand or spool construction.

The device stores local profiles, one associated tag UID per spool, and accepted measurements. A loaded cradle returning near zero clears selection; an unmatched new tag also clears it. Swapping untagged spools without an observed empty interval requires manual profile selection. Multiple-tag aliases remain deferred.

## Electrical variants

Both use PN532 HSU at 115200 baud with switches **OFF/OFF**, common GND and 3.3 V peripheral signals. On the separate board: PN RX18/TX17, HX711 DT15/CLK16 and onboard BOOT GPIO0. On the touch board: PN RX13/TX11, HX711 DT12/CLK17 and onboard BOOT GPIO0. The touch build disables SD, keeps CS10 high and requires an empty card slot. Do not substitute one variant's peripheral pin map for the other.

Power the integrated touch board through its own USB input. Check actual memory, connector labels and regulator capacity. Some GT911 revisions leave INT18 unconnected, so touch is polled; no solder modification is assumed. The [firmware guide](firmware/README.md) records dependencies, environments and installation commands.

## Setup and interfaces

Screenless and touch variants use a temporary **open** setup AP. Join it and open `http://192.168.4.1`, choose an eight-digit PIN, then hold/release the physical setup button for **2 seconds**. Submit within **60 seconds**. Each physical press authorizes one attempt, including a rejected PIN. Enrollment requires the AP connection, expected Host/Origin and current physical arm; firmware enforces these independently of the browser.

The AP closes after **10 minutes**, or **60 seconds after LAN connection**, whichever occurs first. A configured Wi-Fi failure does not automatically reopen it. An **8-second BOOT hold** resets Wi-Fi and pairing while preserving inventory, calibration and Spoolman settings/links. On both builds the BOOT gesture must happen while firmware is running; holding it at power-up or during RST enters the bootloader. Reusing the onboard button removes the external switch, its wiring, perfboard, mounting screws and printed cap from the screenless assembly.

**Temporary setup network has no password; use it in a trusted location.** PIN and Wi-Fi credentials travel over local HTTP. Physical arming/time limits do not encrypt that traffic. A future label/QR setup secret is a possible improvement, not implemented behavior.

The browser serves all assets from the ESP32 without a CDN, edits brand/material/color/name/tare/starting mass, provides calibration and configures Wi-Fi/Spoolman. Common materials support custom entries; named swatches supplement native color and hex inputs. Light/Dark/System appearance persists locally, while PIN, tokens and Wi-Fi passwords do not. Browser sessions stay in memory and private records require pairing. The explicit desktop demo never contacts hardware or falls back from device failure to fake readings.

## NFC and printer boundaries

| Capability | Initial state | Remaining gate |
| --- | --- | --- |
| Weigh and calculate remaining filament | Implemented with freshness/stability/profile checks | Calibration and physical accuracy testing |
| Identify an original Bambu/Snapmaker tag | UID read and manual local profile link | Proprietary material decoders and actual samples |
| Read/write own inventory tags | NTAG216, Spoolancer schema 1, read-back check | Actual RF range, interruption and locked/unsupported-tag tests |
| Sync accepted weight to Spoolman | Optional explicit create/link/absolute update | Live disposable server record and failure tests |
| Arbitrary tags accepted by stock Bambu/U1 | Not provided | Signed vendor data remains a separate constraint |
| OpenSpool interoperability | Deferred | Format mapping, implementation and test samples |

The first format is a MIME NDEF record, `application/vnd.spoolancer+json`, containing schema 1 and local profile metadata. It does not contain a remaining-weight history, credentials or server settings. Writes require the supported writable tag, confirmed UID and successful matching read-back. A removed tag or failed write can leave incomplete data; preserve the local profile and require a fresh scan. Original vendor tags remain unchanged.

Research references for future decoding: [Bambu RFID guide](https://github.com/queengooborg/Bambu-Lab-RFID-Tag-Guide), [Snapmaker U1 FAQ](https://wiki.snapmaker.com/en/FAQ/u1), [U1 tag protocol](https://github.com/DnG-Crafts/U1-RFID/blob/main/docs/U1%20Tag%20Protocol.md) and [key derivation source](https://github.com/DnG-Crafts/U1-RFID/blob/main/docs/fm175xx_reader.py). A published [ESP32/PN5180 cloning experiment](https://github.com/kaver79/esp32-bambu-rfid-reader) does not establish PN532 cloning or arbitrary signed-profile editing; cloning is outside this build. [OpenSpool's design](https://github.com/paxx12-snapmaker-u1/SnapmakerU1-Extended-Firmware/blob/develop/docs/design/openspool.md) is a future interoperability reference.

## Optional Spoolman

Configure an HTTP numeric-IPv4 server URL and explicit remote spool mapping. Save locally without a server. A configured sync saves the validated measurement and sends **absolute remaining weight**, avoiding repeated consumption subtraction. Create vendor/filament/spool after confirmation or link an existing spool ID. Density and diameter are editable inputs, not measured properties.

Report success only after a server response confirms it. An uncertain create persists a pending marker and blocks blind retries until reconciled. Retry is manual; no automatic queue is implemented. Existing linked records receive weight updates only; local metadata edits do not rewrite remote filament metadata. The implementation and exact routes are documented in [firmware/README.md](firmware/README.md).

## Verification sequence

1. Check actual boards, holes, threads and connector clearances against fit samples. Verify each peripheral and the chosen setup control separately, then together under Wi-Fi/NFC/display activity.
2. Assemble the moving load path; test guide extremes, reader heights, stability and cable clearance. Calibrate with known masses. Proposed acceptance targets remain 1 g display increments, ±5 g over 100–2,000 g and at most 4 g peak-to-peak across ten repeated placements. None is an achieved specification yet.
3. Test seated NTAG216 read/write, unknown/unsupported/multiple tags, interrupted writes and original-tag UID links. Future vendor decoders require independent sample comparison.
4. Exercise real setup arming, invalid/expired attempts, Wi-Fi recovery, power cycles, local persistence, native touch confirmations and offline weighing.
5. Use a disposable Spoolman record to test create/link/update, connection loss, ambiguous outcomes and manual reconciliation. Stock-printer tags remain untouched.

Current evidence is recorded in [STATUS](docs/STATUS.md). Original project references include the [over-engineered spool balance](https://github.com/schnoog/Totally-over-engineered-filament-spool-balance), [SpoolEase Scale](https://www.spoolease.io/docs/build-setup/scale-build) and [SpoolmanScale](https://github.com/Niko11111/SpoolmanScale). Select a license before public release and check licenses before reusing third-party code/CAD. Supplied source and compilation do not establish device readiness.
