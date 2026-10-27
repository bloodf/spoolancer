# Spoolancer build plan

Design and implementation plan, 26 October 2026. v0.3 adds a compact upright mechanism, a side-mounted reader and an ESP32-hosted web interface. Software and CAD checks are separate from physical validation; this build has not been flashed or tested with real spools, tags or printers.

Build an open-source, standalone USB-powered bench scale that identifies a filament spool, shows its remaining filament, reads or writes supported NFC tags, and sends accepted measurements to Spoolman when configured. Use the ESP32-S3 N16R8 and PN532 already owned. The user has both Bambu Lab and Snapmaker U1 printers; the scale must work independently of either printer.

The user confirmed occasional weighing before or after printing and now requires the spool to stand upright, with a maximum spool width of 100 mm. The v0.3 mechanism uses two rim-support rails and sliding side guides, without an axle or bearings. The reader is mounted beside the spool on one of the moving guides. Its proposed guide adjustment covers 50–100 mm spool widths, with an inner gap set to the actual spool width plus 4 mm. Assume up to 1 kg of filament and up to 2 kg gross spool weight for the first version. Diameter references of 200 mm and 220 mm are used for clearance checks, not a claim that every spool of those sizes has been tested. The earlier designs are preserved in the archive. Current CAD and hardware quantities are in [cad/README.md](cad/README.md).

The user confirmed that the Snapmaker U1 will keep its stock firmware. Writable tags are for the scale's inventory. Reading original Snapmaker tags remains a core feature.

Place the spool upright between the guides, align its tag with the side reader, and wait for a stable measurement. The spool stays seated for NFC reading and writing. Reader height and guide spacing are adjustable; rotation in the cradle may still be needed to align a tag. Keep hands clear while measuring. The local web interface edits brand, material, color, spool name, empty-spool weight and starting filament weight. Writing a tag is a separate action with a target-UID preview, confirmation and read-back verification.

**Original Bambu Lab and Snapmaker material decoding remains planned and requires implementation and sample testing.** Detecting a tag UID does not establish that its material data has been decoded. Creating arbitrary tags accepted by stock printers is a separate limitation. Both manufacturers use signed data. Ordinary writable NFC stickers do not bypass that requirement.

| Capability | Planned support | Evidence or condition |
| --- | --- | --- |
| Weigh a spool and show remaining filament | Core feature | Requires calibration and a known empty-spool weight |
| Identify original Bambu Lab tags | Core feature | Published authentication and decoding; validate actual samples |
| Identify original Snapmaker U1 tags | Core feature | Published UID-derived authentication and decoding; validate actual samples |
| Read and write our own inventory tags | Implemented; hardware testing pending | NTAG216 with Spoolancer schema 1 in a MIME NDEF record; OpenSpool interoperability is deferred |
| Send measured remaining filament to Spoolman | First-version feature, optional configuration | Requires a server URL, an explicit Spoolman spool ID mapping and a known empty-spool weight; offline weighing remains available |
| Make custom tags recognized directly by stock Bambu AMS | Unavailable for arbitrary profiles | Modifying signed material/color data invalidates the signature |
| Make custom tags recognized directly by stock U1 | Unavailable for arbitrary profiles | Stock firmware requires Snapmaker's proprietary tags |
| Configure Bambu filament settings through a network integration | Later option | Exact printer, AMS, firmware and supported integration must be checked |

The Bambu limitations and cloning distinction are documented in the [Bambu RFID research guide](https://github.com/queengooborg/Bambu-Lab-RFID-Tag-Guide). Snapmaker's stock behavior is documented in its [U1 FAQ](https://wiki.snapmaker.com/en/FAQ/u1). The [Snapmaker tag protocol](https://github.com/DnG-Crafts/U1-RFID/blob/main/docs/U1%20Tag%20Protocol.md) and [published key derivation](https://github.com/DnG-Crafts/U1-RFID/blob/main/docs/fm175xx_reader.py) provide the standalone reader path. Neither reader requires printer credentials to derive the documented tag authentication keys.

Exact copies of genuine Bambu tags are a narrower possibility. They preserve the original signed profile and require suitable clone media. An [ESP32 RFID workbench](https://github.com/kaver79/esp32-bambu-rfid-reader) reports a PN5180/FUID clone accepted by an A1 Combo with AMS Lite. That does not establish cloning with the owned PN532 or arbitrary material editing. Cloning is outside the first build.

**Use the existing electronics and add these parts.**

| Part | Quantity | Selection |
| --- | --- | --- |
| ESP32-S3 N16R8 development board | 1, owned | Supplied photo shows a 44-pin board marked V6.0.0; proposed GPIO assignments are in WIRING.md |
| PN532 reader/writer | 1, owned | Supplied image shows a red Elechouse-style NFC MODULE V3; use HSU with both switches OFF |
| Color display | 1 | [Waveshare 2inch LCD Module, SKU 17344](https://www.waveshare.com/2inch-lcd-module.htm), non-touch ST7789 SPI, 240 × 320 |
| Load cell | 1 | [Bit Maker code 1381](https://www.bitmaker.com.br/sensor-de-peso-c-lula-de-carga-5kg), four-wire 5 kg bar cell, 80 × 12.7 × 12.7 mm |
| Load-cell amplifier | 1 | [Bit Maker HX711 code 0679](https://www.bitmaker.com.br/m-dulo-conversor-hx711-adc-24-bits-para-sensor-de-peso), documented 3–5 V supply; use 3.3 V |
| Push buttons | 3 | SmartKits SK1798, momentary 6 × 6 × 7 mm; previous/next select a saved spool; long select zeros the empty cradle |
| Writable tags | 5 | SmartKits SK6052 NTAG216 stickers, for local inventory |
| Assembly materials | As needed | Printed base, top cover, open carrier, rim rails, side guides and reader pod; matching fasteners, wire and nonslip feet |
| Power and calibration | 1 set | USB supply/cable and known reference masses |

The user supplied these component listings on 26 October 2026: [AliExpress listing 1005009519771350](https://pt.aliexpress.com/item/1005009519771350.html) and [AliExpress listing 1005009330328373](https://pt.aliexpress.com/item/1005009330328373.html). Their contents could not be inspected because text retrieval failed and browser access was blocked. The user subsequently supplied an [ESP32 board image](docs/reference/esp32-s3-n16r8-v6.png) and a [PN532 product image](docs/reference/pn532-v3-listing.png). Those images support the board families, visible pin labels and PN532 switch settings used in [WIRING.md](WIRING.md). They do not establish the delivered units' electrical performance or manufacturer authenticity. The ESP32 image shows separate COM and USB connectors and an ESP32-S3-N16R8 module marking.

Use the selected non-touch color display so filament color, material and weight fit on one screen. [SHOPPING.md](SHOPPING.md) records exact purchase links, quantities, observed prices, included cables and remaining assembly materials. Selections are recommendations; parts have not been purchased or physically tested.

```mermaid
flowchart LR
    Spool[Upright spool in rim-support rails] --> Carrier[Moving carrier and side guides]
    Carrier --> Cell[5 kg load cell]
    Cell --> HX[HX711]
    HX --> ESP[Owned ESP32-S3]
    Tag[Spool NFC tag] <--> PN[Owned PN532]
    PN <--> ESP
    Buttons[Three buttons] --> ESP
    ESP --> Screen[2-inch color screen]
    ESP <--> Records[Saved spool profiles and measurements]
    Records -. Configured Wi-Fi sync .-> Spoolman[Optional Spoolman server]
```

Give the screen its SPI connection. Use PN532 UART/HSU at 115200 baud, with ESP32 TX on GPIO17 connected to PN532 RXD and ESP32 RX on GPIO18 connected to PN532 TXD. Set PN532 switches 1 and 2 OFF with power disconnected. HX711 uses GPIO15 for DOUT and GPIO16 for clock. The full proposed pin assignment is in [WIRING.md](WIRING.md). Prepare the enclosure CAD prototype now using the facts and assumptions in [MECHANICAL-SOURCES.md](docs/MECHANICAL-SOURCES.md). Verify electrical operation, actual dimensions and physical fit before treating that design as ready for final printing.

Keep all ESP32 signals at 3.3 V logic. Some HX711 boards expose separate analog and digital supplies; others do not. Follow the selected board's schematic rather than assuming its supply and output voltages. [SparkFun's HX711 guide](https://learn.sparkfun.com/tutorials/load-cell-amplifier-hx711-breakout-hookup-guide/all?print=1) explains its separate-rail arrangement. On applicable S3 boards with octal memory, GPIO35–37 are occupied; preserve USB and boot pins as described in the [Espressif board guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html).

**The upright mechanism uses a fixed base and a moving carrier.** Bolt one end of the bar cell to the base and the other to the carrier, following the cell's mounting orientation. Attach the rim-support rails and both sliding side guides to the carrier so all spool contact stays on the weighed assembly. Set the guides with their hex-key fasteners; no thumb wheels, axle or bearings are required. The carrier, rim rails, guides, reader, fasteners and wires must not transfer weight directly to the housing or top cover. Allow the cell to flex and add adjustable stops. Cell capacity includes the complete moving assembly and the spool. Measure the selected cell's mounting holes and thread depths before finalizing supports and screw lengths.

The PN532 antenna faces the spool flange from an adjustable side pod. Use a thin unfilled-plastic window, nylon fasteners and a small air gap. Tag position, orientation, flange thickness and nearby material affect reading, so no universal seated-read compatibility is claimed before testing. Use a flexible four-wire service loop from the moving guide to the fixed electronics. Strain relief belongs at both ends, with slack between them. Cable force can bias the load cell; check zero and repeatability at both guide-width extremes and all reader heights. Re-zero the empty mechanism after adjustment. Pause accepted measurements during NFC operations and after handling the spool, then wait for stability again.

**The device distinguishes scale zero, empty-spool weight and filament weight.**

`Remaining filament = measured gross spool weight − empty-spool weight`

For example, 842 g gross minus a 250 g empty spool gives 592 g of filament. Those are example values, not a Bambu or Snapmaker spool specification.

Scale zero removes the complete moving assembly, reader and installed wiring from the reading. Empty-spool weight comes from a measured empty spool, a confirmed saved profile, or manual entry. A nominal 1,000 g filament value on a tag cannot supply empty-spool weight. When the empty weight is unknown, show gross weight and “Set empty spool weight” instead of a guessed remaining amount. Mark estimates when the empty weight comes from an unverified profile. If gross weight is below the configured empty weight beyond measurement tolerance, ask the user to check the profile.

Material identification comes from decoded tag data or a user-assigned profile. Weight alone cannot distinguish PLA, PETG, ABS, manufacturer, or spool construction.

Store a local spool record containing its internal ID, tag identifiers, material, color, brand, empty-spool weight and its source, nominal starting filament amount, and latest measured gross weight. The initial implementation stores one associated tag UID per spool; multiple-tag aliases remain a later extension. Refresh the active spool identity for each weighing session so a previous spool's empty weight is not silently reused.

**The ESP32 hosts the complete local web interface.** Phone and desktop browsers load assets from the device without a CDN or internet service. The interface provides live weighing, local spool records, NFC operations, calibration, Wi-Fi and optional Spoolman setup. Local records and calibration survive restart. Device pairing uses a PIN shown by the device, with an in-memory browser session token. Stored Wi-Fi passwords are never returned by the configuration API. The separate desktop demonstration is explicitly labeled and does not communicate with a scale.

**Spoolman synchronization is part of the first version when configured.** Save the server URL and an explicit mapping from each local spool record to its Spoolman spool ID. Persist accepted measurements locally and show whether synchronization is unconfigured, pending, successful or failed. The initial implementation uses explicit manual synchronization and retry; an automatic background queue is deferred. Its server address must use HTTP and a numeric IPv4 address on the trusted local network. Existing linked Spoolman records receive remaining-weight updates; editing local brand/material/color does not automatically rewrite remote filament metadata. Report success only after the server confirms the update. Network calls are bounded but can briefly pause sampling in this first firmware. After a gap, the device must collect a fresh stable window before accepting another measurement. Local records remain available without Spoolman.

Send a measured remaining-filament amount only when the gross measurement is stable and the empty-spool weight is known. If the empty-spool weight is missing, show the live gross weight and request the missing value; the initial firmware accepts a saved remaining-weight measurement only with a known tare. An unlinked spool can be created in Spoolman after confirmation, or linked by entering its existing ID. Do not guess consumption from a nominal full-spool weight or from unrelated readings. Use absolute remaining-weight updates, not repeated consumption deltas. Creation and synchronization must report success only after the server confirms them. An uncertain create response requires reconciliation before another create attempt, to avoid duplicate records. The firmware documentation records the current API assumptions and remaining live-server checks.

The first firmware writes an NDEF MIME record named `application/vnd.spoolancer+json`, with a documented schema version and local spool metadata. This is a Spoolancer inventory format. OpenSpool import/export remains a later interoperability task; it is not implemented or implied by using NDEF. The [OpenSpool design](https://github.com/paxx12-snapmaker-u1/SnapmakerU1-Extended-Firmware/blob/develop/docs/design/openspool.md) remains a reference for that work. Neither format creates signed profiles for stock printers.

On a write, show the target tag and intended profile, check tag family and available space, write only supported user-owned inventory tags, and reread the record before reporting success. Preserve the local profile if a tag is removed or writing fails. Original vendor tags remain read-only in the first version.

**Build and verify in five bounded steps.**

1. **Prepare the CAD and prototype the electronics.** Model the selected parts with documented dimensions and editable assumptions. When the parts are available, measure them and check each peripheral independently. Completion means stable boot, working screen/buttons, raw load samples and repeatable tag detection on the chosen wiring. CAD can precede these hardware checks; final fit acceptance cannot.
2. **Build the upright weighing mechanism.** Check small mounting and bezel test prints against the actual parts before printing the complete assembly. Check guide adjustment at 50 mm and 100 mm spool widths, free carrier movement and stability with an upright spool. Assemble the load path, calibrate zero and span, and check several known masses and repeated spool placements. Proposed acceptance targets are 1 g display increments, error within ±5 g over 100–2,000 g, and no more than 4 g peak-to-peak variation over ten placements of the same test mass. These are targets, not achieved specifications. Check drift after warm-up and with NFC/display activity.
3. **Add spool identification.** Decode real Bambu and Snapmaker samples, compare fields with their labels, save empty-spool profiles, and calculate remaining filament. Completion includes unknown, unsupported, unreadable and multiple-tag cases, plus correct behavior after changing spools and rebooting.
4. **Add inventory tag writing.** Encode saved profiles onto NTAG216, verify by rereading, and provide profile editing. Completion includes insufficient capacity, locked tags, tag removal during writing, and recovery after power loss. A second reader should also decode the generated NDEF record.
5. **Validate offline use and configured Spoolman sync.** Scan and weigh several spools in succession, write and reread inventory tags, and repeat after restarting with Wi-Fi unavailable. Confirm that each spool retains its own profile and measurements. With Spoolman configured, check the correct spool receives the measured remaining amount, manual synchronization after disconnection, visible errors and safe retries without duplicate consumption. Original Snapmaker tags stay unchanged for use with the stock U1; custom inventory tags are used by the scale.

The U1 firmware decision is settled: retain stock firmware. Spoolancer schema 1 is the initial inventory-tag format, without claiming recognition by the stock U1. Any later Bambu network integration needs the exact printer/AMS model and supported firmware path checked separately. Printer profile setting and measured-weight synchronization remain separate capabilities.

Useful references include the [original four-spool balance](https://github.com/schnoog/Totally-over-engineered-filament-spool-balance), [SpoolEase Scale assembly](https://www.spoolease.io/docs/build-setup/scale-build), and [SpoolmanScale](https://github.com/Niko11111/SpoolmanScale). SpoolEase's scale expects its separate Console, and SpoolmanScale expects an inventory server. Their mechanics and workflows are useful references; Spoolancer stores its own records and adds optional Spoolman synchronization. Choose an open-source license for this project and check licenses before reusing code or CAD, particularly SpoolEase's custom restrictions.

The supplied images and selected modules are sufficient for the proposed wiring and an upright CAD prototype. Power the selected display and HX711 at 3.3 V and tie the Waveshare BL control to 3V3 for constant backlight. The next physical gate is to measure the delivered modules, verify operation and check the printed mounts, moving carrier and adjustable guides. Before any optional direct Bambu printer integration, identify the printer/AMS model. Spoolman synchronization is a separate first-version feature.

## Local web interface

See [the web interface guide](docs/WEB-INTERFACE.md), [web implementation](web/README.md) and [firmware build guide](firmware/README.md). The local demonstration uses sample data only. Firmware compilation does not verify the connected load cell, NFC reader, display, Wi-Fi or a real Spoolman server.
