# Spoolancer device firmware

This is an Arduino/PlatformIO implementation for ESP32-S3 N16R8 hardware. The builds use 16 MB flash and octal PSRAM, read the HX711, communicate with the PN532 over HSU, and serve the local web interface. One `spoolancer-oled-led` board was flashed on 27 October 2026 and reported successful USB startup, detected memory and mounted storage. On 30 October, a connected OLED exposed an initialization stall; the corrected application reached normal startup, and the owner confirmed that the OLED and webpage work. **LED operation, detailed display states, weighing, NFC and real Spoolman behavior remain unvalidated.** See the [boot record](../docs/DEVICE-BOOT.md) and [full evidence record](../docs/STATUS.md).

| Build environment | Controls |
|---|---|
| `spoolancer-headless` | Phone/browser interface and the existing onboard BOOT button; no extra button or display |
| `spoolancer-headless-led` | Same browser/BOOT controls, plus five addressable status LEDs |
| `spoolancer-oled-led` | Same browser/BOOT controls, five LEDs and a 128×64 I2C OLED |
| `spoolancer-s3` | Existing ST7789 screen and Previous/Next/Select buttons |
| `spoolancer-touch-8048` | Experimental ESP-8048S043C-I integrated 800×480 capacitive touchscreen; native actions and browser editor |

The headless build uses the existing N16R8 development board and is the default environment. The touch build uses the ESP32-S3 already integrated into the ESP-8048S043C-I; it does not need a second ESP32. Choose `spoolancer-touch-8048` or the legacy `spoolancer-s3` environment explicitly when using either display board.

## Build and install

From the project root, with PlatformIO Core available:

```sh
pio run -d firmware -e spoolancer-headless
pio run -d firmware -e spoolancer-headless -t buildfs
python3 firmware/tests/run.py
python3 firmware/tests/json_numbers_test.py
```

The pre-build script copies exactly `web/index.html`, `web/styles.css`, `web/app.js`, and `web/logic.js`, and `web/favicon.svg` into the asset image. It does not include the desktop demonstration. The HTTP server serves only these assets; private storage has no file-serving route.

When ready to test the hardware, connect the board's **COM** USB-C port and use:

```sh
pio run -d firmware -e spoolancer-headless -t upload --upload-port YOUR_SERIAL_PORT
pio run -d firmware -e spoolancer-headless -t uploadfs --upload-port YOUR_SERIAL_PORT
pio device monitor -d firmware -e spoolancer-headless --port YOUR_SERIAL_PORT
```

Both upload commands were run successfully for `spoolancer-oled-led`; other profiles have compilation evidence only. Replace `spoolancer-headless` with the environment for your hardware. For the current DC-powered cases, use the [two-screw cradle service access](../docs/ASSEMBLY.md#6-service-and-acceptance) and follow the [J1/J2 power-isolation sequence](../WIRING.md#10-usb-programming-and-service) before attaching computer USB. Verify 16 MB flash / 8 MB PSRAM on your actual module before installation. PlatformIO's inherited board description still says N8; `platformio.ini` overrides its flash size, memory type and partition table. The generated bootloader header is 16 MB. The firmware reports detected flash and PSRAM on the serial console. USB CDC is disabled so the programming bridge supplies serial output.

For the integrated touchscreen, use `-e spoolancer-touch-8048` for build, filesystem and upload commands. It pins Arduino_GFX 1.4.9 and uses the reference RGB panel/GT911 layout described below. Treat it as experimental until the exact purchased board revision has been checked and tested.

The `spiffs` partition contains LittleFS web assets at `0x810000`, size `0x6f0000` (7,274,496 bytes). A separate 1 MB `inventory` partition at `0xf00000` stores local spools and the remote-operation journal. Its CSV subtype is written numerically as `0x82`, the same ESP32 SPIFFS subtype, so PlatformIO 6.12 does not mistake the last private filesystem for the asset-upload target. A post-build configuration guard refuses filesystem build/upload targets with the wrong address or size. Configuration, PIN and calibration use NVS. Uploading assets leaves inventory separate; erasing the whole flash destroys all three. Inventory initializes once; a later mount or parsing failure disables writes and preserves data for recovery.

After building firmware and filesystems, `python3 firmware/tests/check_filesystem.py --env spoolancer-headless` checks the generated binary partition table, image size, and unpacked contents against the five current web files. Omit `--env` to inspect all five built variants. It does not upload anything.

## Five LEDs and optional 0.96-inch OLED

Use `-e spoolancer-headless-led` or `-e spoolancer-oled-led` in the build, asset and upload commands. These profiles use the separate ESP32-S3 N16R8 and keep the headless BOOT recovery gesture and browser editor. They do not use the ST7789 buttons or integrated touchscreen pin map. Connect OLED SDA to GPIO4, OLED SCK/SCL to GPIO5 and the LED data signal to GPIO6 through the buffer in the [complete wiring guide](../WIRING.md#6-connect-the-five-leds-and-logic-buffer).

The LED driver defaults to five 5 V WS2812-style **GRB / 800 kHz** pixels at brightness **51/255 (20%)**. The user's strip chip and color order are not yet confirmed. `SPOOLANCER_LED_ORDER=NEO_RGB` changes the order; `SPOOLANCER_LED_ORDER=NEO_GRBW` supports compatible RGBW strips while leaving white off. Add these definitions to the selected environment's `build_flags` with `-D`; `SPOOLANCER_LED_BRIGHTNESS` is also configurable from 0 to 255. A firmware brightness limit is not an electrical current limiter.

| LED appearance | Meaning |
|---|---|
| Cyan completed segments | Each is 20% of initial filament, after subtracting empty-spool weight |
| Partial segment: red, orange, yellow, green | Four 5% bands inside the next segment; 85/90/95% gives four cyan LEDs plus red/orange/yellow, 96–99% green, 100% all cyan |
| Low weight / empty | At 10% or less, the partial segment pulses; at zero, the first LED pulses red |
| Blue breathing / chase / double flash | Setup AP / connecting without a selected spool / offline without a selected spool |
| White center breathing / cyan breathing | Empty cradle / select a spool |
| Amber bounce / double pulse / outer LEDs | Settling / calibration needed / unknown empty-spool weight |
| Red breathing / alternating | Stale sensor / invalid or out-of-range weight |
| Purple / blue chase | Explicit NFC / save or Spoolman communication |
| Green fill / red double flash for 2.5 seconds | Action succeeded / failed, then normal status returns |

A selected spool can still show its remaining bar while offline; the last LED briefly flashes blue every five seconds. Routine background tag scans do not flash the strip. NFC and cooperative HTTP waits refresh the scale and status animations. Invalid or stale measurements never produce a remaining-weight bar.

In **Settings → Hardware tests**, **Test LEDs** runs a nonblocking 12-second sequence: red, green, blue, one white pixel at a time, then a colored fill bar. It returns to normal automatically. The API reports that the output pattern started, not that the LEDs physically lit. Verify five pixels and the color order visually. Another explicit device operation can interrupt this test.

**Test NFC** performs a new PN532 handshake and reads a single identifier without writing the tag or changing a spool profile. A responding reader with no tag is distinct from a missing reader or multiple tags. Reprobing clears cached availability/content before the handshake, so unplugging/reconnecting cannot retain an old successful result. **Test scale** reports sample freshness, stability, calibration, raw mean and gross grams when available; it neither zeroes nor calibrates. Use known independent weights to verify accuracy.

The OLED shows material, valid remaining grams, gross grams, status and IP address. Unknown empty weight, stale/unstable data, missing calibration, out-of-range gross or impossible remaining weights suppress remaining grams. Busy operations hide remaining grams so the operation status stays prominent. The OLED is an indicator; editing and confirmation stay in the browser.

**SSD1306 at I2C address 0x3c is provisional**, not identified from screen size alone. Add `-DSPOOLANCER_OLED_SH1106=1` if the delivered controller is SH1106; use `-DSPOOLANCER_OLED_ADDRESS=0x3d` if its address differs. U8g2 supports both controllers; acknowledging an I2C address does not identify the controller. An absent OLED is skipped, and a later I2C failure disables display updates without stopping the browser. Repair the wiring and restart to enable it again. LED signaling, OLED rendering and fit still need checks on the actual hardware.

## Headless first use

Use the development board’s existing **BOOT button on GPIO0**. No separate setup button, button wiring, or button perfboard is needed. The firmware enables GPIO0’s pull-up. Keep HX711 DOUT15/CLK16 and PN532 RX18/TX17 as specified in [WIRING.md](../WIRING.md), with both PN532 switches OFF. Leave the screen and Previous/Next/Select controls unconnected; this variant does not use them.

Use BOOT **after the firmware has started**, with the button initially released. Holding BOOT while powering on or pressing RST selects the ESP32 download boot mode, not Wi-Fi recovery. If that happens, release BOOT and press RST once, then use BOOT while the scale is running. The gesture handler ignores a button already held when it first starts polling until it sees a release. RST alone only restarts the board.

1. On first boot, join `Spoolancer-xxxx` from your phone. It is a temporary network **without a password**. Open `http://192.168.4.1` manually; no serial console or display is needed.
2. The first page is setup. Select a nearby 2.4 GHz Wi-Fi network and enter its password. Hidden networks can be entered manually. Choose an eight-digit PIN, or uncheck PIN protection for access by anyone on the same local network. Finish setup in the browser without pressing BOOT. Wi-Fi and the access choice are saved in one configuration write.
3. After the scale confirms a Wi-Fi connection, note the LAN address shown in the browser, reconnect your phone to that network and open the scale there. `http://spoolancer.local` may also work on networks supporting mDNS. With PIN protection enabled, pair whenever this tab reloads or its two-hour session expires. Without PIN protection, the page obtains a local session automatically.
4. The open setup AP closes after ten minutes, or 60 seconds after a successful LAN connection, whichever comes first. A failed connection on an already configured headless device does **not** automatically open an AP. Hold BOOT eight seconds while running to recover, then choose Wi-Fi and PIN protection again.
5. Zero the empty stable cradle, calibrate with a known 100–2000 g reference, and verify independent known weights. Add/select a spool with known empty-spool weight before saving a measurement or syncing it.

**Temporary setup network has no password; use it in a trusted location.** Pairing and Wi-Fi credentials travel over local HTTP on this open network and can be observed by someone nearby. The setup AP's time limit and unconfigured-device restriction limit enrollment access; they do not encrypt this traffic. No universal private password is supplied.

Initial setup is accepted only while the device is explicitly unconfigured, through the AP's local destination address and subnet, while its station connection is disconnected, with the expected setup Host/Origin, and before the setup AP expires. Attempts are throttled for 1.5 seconds. The resulting random browser session token is separate from the chosen PIN. An eight-second reset removes Wi-Fi credentials and pairing access, closes existing sessions, and preserves inventory, Spoolman settings/links and calibration. It does not erase the entire device.

An upgrade preserves a valid legacy PIN, even if Wi-Fi was never configured. Pair with that PIN and choose a network in Settings, or use the eight-second physical reset to run the new setup wizard. Explicit no-PIN mode is persisted separately from an empty PIN; malformed stored access settings remain locked until physical recovery. The versioned configuration takes precedence over any legacy PIN key.

## Screen version first use

1. Follow [WIRING.md](../WIRING.md). The fixed pin map is in `include/Pins.h`; PN532 switches are both OFF for HSU.
2. Read the eight-digit device PIN on the screen or serial console. Join `Spoolancer-xxxx` using that PIN as its setup Wi-Fi password, then open `http://192.168.4.1`.
3. Pair in the browser with the PIN. Configure the home Wi-Fi network if desired. A failed connection keeps the setup AP available. Browser sessions last two hours and are held only in memory.
4. With the cradle empty and still, zero it. Place a known 100–2000 g reference, enter its weight and calibrate. Check independent known weights before relying on measurements.
5. Add a spool with its brand, material, color, initial filament weight, and known empty-spool weight. Select it or scan a linked tag, place it in the cradle and wait for a stable reading. Save the measurement locally, or explicitly sync it to Spoolman.

## ESP-8048S043C-I touchscreen variant

This environment follows the [ESP32-8048S043C reference board definition](https://github.com/rzeldent/platformio-espressif32-sunton/blob/main/esp32-8048S043C.json): 800×480 RGB display, backlight GPIO2, GT911 SDA19/SCL20, reset38 and interrupt18. The exact physical board revision and connector orientation remain unverified. Use signal labels and the board schematic, not assumed connector positions.

The exposed peripheral-header reference is [ESP3D's Sunton 4.3-inch 8048 page](https://esp3d.io/esp3d-tft/version_1x/hardware/esp32-s3/sunton-43-8048/). Its reported board outline is not used for the enclosure; check the physical revision before wiring.

| Peripheral signal | Integrated board GPIO |
|---|---:|
| PN532 TXD → ESP RX | 13 |
| PN532 RXD ← ESP TX | 11 |
| HX711 DOUT | 12 |
| HX711 SCK | 17 |
| Physical Wi-Fi/access recovery | Onboard BOOT, GPIO0 |

PN532/HX711 power and logic remain 3.3 V with common ground. The peripheral pins differ from the separate N16R8 development board. GPIO11/12/13 are reused from the SD interface: SD is disabled, CS10 is held high, and the card slot must remain empty. Do not connect the PN532 to GPIO18 or the HX711 to GPIO15/16 on this board; those pins belong to touch/display functions.

The touchscreen uses the same temporary open-AP onboarding and optional browser PIN as the headless build. Its screen shows setup instructions and the address/PIN. Initial setup needs no physical button. Use the onboard BOOT button **only after firmware is running**, holding eight seconds to clear Wi-Fi/pairing. Holding BOOT during power-up/reset enters the ESP32 bootloader instead of performing a Wi-Fi reset.

Native controls select spools, read tags, write inventory tags, save measurements, sync Spoolman, and zero the cradle. Writes, sync and zero require native confirmation; the handlers recheck the current spool selection, tag UID and measurement conditions before acting. Local storage and NFC/Spoolman operations are shared with the browser paths. Brand/material/color/tare editing, calibration-reference entry and network/server configuration remain browser actions. The native theme is stored separately from browser theme preferences.

The configured measurement range is 20–2000 g **gross**, including the empty spool. This remains a limit even though inventory can describe an initial filament weight up to 5000 g. Unknown empty-spool weight blocks net-weight saving and synchronization. Calibration and zero are separate from the empty-spool weight.

The HX711 is sampled only when ready. Weighing uses a median of three samples, responsive smoothing and time-based stability: four observations spanning at least 500 ms, with a 2 g entry band and 3 g exit band around a fixed anchor. Weight keeps updating while stable. A 600 ms sampling gap invalidates the reading. Tare and span calibration retain their separate 20-sample raw-data checks. A synthetic 10 SPS step test becomes stable at 700 ms, compared with the previous 2,000 ms window; this does not establish physical settling time or accuracy.

Firmware clears the selection after a loaded cradle returns near zero; wait for this before placing the next spool. A newly presented unrecognized tag also clears selection; an explicit read without a match keeps the current manual selection. The sensor cannot identify a different untagged spool swapped without a detected empty interval: select the correct record manually.

## Buttons

The headless and integrated-touch builds use onboard **BOOT / GPIO0** after startup: hold eight seconds to reset Wi-Fi/pairing and reopen setup. Short presses do nothing, and initial setup has no BOOT step. Reset fires once per hold and does not reboot the ESP32. Release BOOT before starting another gesture. Inventory, calibration and Spoolman settings/links are kept. In the headless build, tare, calibration and spool selection are browser actions.

The existing screen build retains these controls:

| Action | Result |
|---|---|
| Previous / Next, short press | Select a local spool |
| Previous, hold 3 seconds | Show the pairing PIN for one minute |
| Select, hold 2 seconds | Zero the empty, stable cradle and save calibration |
| All three, hold 8 seconds | Reset Wi-Fi and pairing PIN; keep inventory and calibration |

## NFC format and limits

Original Bambu Lab, Snapmaker and other ISO14443A tags are **read-only UID identification**. Their proprietary filament fields are not decoded. Linking a scanned identifier to an existing spool only changes the scale's saved association. It does not change printer data. To make a tag recognizable to a stock printer, write one of the printer formats below.

Identification polls NFC-A first, then NFC-B, FeliCa 212/424 and Jewel, subject to the PN532's reported firmware capabilities. Legacy NFC-A UIDs remain uppercase hexadecimal (4/7/10 bytes). Other identifiers are namespaced: `B:` + 4-byte PUPI, `F:` + 8-byte IDm, `J:` + 4-byte ID. Protocol parsers reject malformed/truncated responses and reported multiple targets. A single attempt waits at most 350 ms per family/rate, yielding to scale/status work; the absent-tag path can total 1,750 ms. Polling stops at the first family found, and Jewel cannot report multiple targets, so place only one physical tag in the field. Randomized identifiers are unsuitable for persistent binding. ISO15693/Type 5 is not supported by this reader. No memory commands are sent to B/F/J tags.

Identity scanning for linking is independent of tag-memory decoding. Valid single-target IDs can be associated even when the contents are protected or unreadable. A fresh scan must match the user's confirmation, and an ID already assigned to another stored profile is rejected. Explicit UID association takes priority over an older NDEF profile on a reused tag.

Writes require one detected, unlocked NTAG213/215/216 with an exact supported version response and unprotected read/write capability container. Accepted NDEF capacities are respectively 144/496/872 bytes (physical user memory is 144/504/888). The complete serialized record must fit before any write; the current profile normally exceeds NTAG213 capacity. NTAG216 remains the recommended writable sticker. Only supported blank NDEF or Spoolancer-owned MIME records are accepted. Factory lock-control layouts not supported by the writer remain read-only. Other tag types, locked/protected tags, and foreign content are refused. UID is checked against the confirmation, reselected during each 16-byte write group, and checked on final read-back. Reserved lock/configuration/credential pages are never written. Tag movement, interruption or power loss can leave an incomplete record; the UI must report failure and the tag must be rescanned. This is not a transactional NFC device.

The actual format is a single NDEF MIME record, `application/vnd.spoolancer+json`, in a Type 2 extended-length NDEF TLV. JSON payload limit is 650 bytes. It contains:

```json
{
  "schema": 1,
  "spool_id": "0123456789abcdef",
  "name": "PLA black",
  "brand": "Generic",
  "material": "PLA",
  "color": "#111111",
  "empty_weight_g": 250,
  "initial_weight_g": 1000,
  "density_g_cm3": 1.24,
  "diameter_mm": 1.75
}
```

The Spoolancer inventory record itself is not a printer format; use the printer formats below for that. No credentials, server URLs or remaining-weight measurements are written to tags. Foreign Spoolancer records are displayed after validation but are not automatically imported. Explicitly bound identifiers select the saved profile; an unbound record requires linking, even when its embedded spool ID matches. Tag authenticity is not cryptographically verified; the safeguards prevent accidental writes, not a deliberate tag impersonation.

### Printer tag formats

Stock printer firmware reads its own tag layout, not the Spoolancer record. The writer reproduces the layouts of open-source tools that those printers already accept. After a verified write the tag UID is linked to the spool, so the scale identifies it while the printer reads material and color from the tag.

| Format | Tag | Written data | Source layout |
| --- | --- | --- | --- |
| Creality CFS (K2 Plus/Pro, K1 + CFS, Hi) | MIFARE Classic 1K, 4-byte UID | Sector 1: AES-128-ECB encrypted 48-character record (vendor 0276, material ID, color, length in metres). Sector 1 Key A/B become the UID-derived key; access bits must be the transport default `FF 07 80` | [DnG-Crafts/K2-RFID](https://github.com/DnG-Crafts/K2-RFID) |
| Qidi Box (Plus4, Q2, Max4) | MIFARE Classic 1K | Block 4: material code, nearest of 24 Qidi palette colors, vendor Generic. Keys unchanged | [DnG-Crafts/QIDI-RFID](https://github.com/DnG-Crafts/QIDI-RFID), QIDI_Q2 `officiall_filas_list.cfg` |
| Anycubic ACE (Kobra 3, Kobra S1) | NTAG213/215/216 | Pages 4–31: SKU, brand, type, ABGR color, nozzle/bed range, diameter, length, weight | [DnG-Crafts/ACE-RFID](https://github.com/DnG-Crafts/ACE-RFID) |
| Elegoo Canvas (Centauri Carbon) | NTAG213/215/216 | Pages 4–24: material, family/subtype, RGB, nozzle range, diameter, weight. The printer shows every brand as ELEGOO | [DnG-Crafts/ELG-RFID](https://github.com/DnG-Crafts/ELG-RFID), [Savion/elegoo-rfid-editor](https://github.com/Savion/elegoo-rfid-editor) |
| OpenSpool | NTAG215/216 | NDEF `application/json` OpenSpool record. Read by OpenSpool readers and the community Snapmaker U1 Extended firmware, not stock Snapmaker firmware | [spuder/OpenSpool](https://github.com/spuder/OpenSpool) |

Material text maps to PLA, PLA+, PLA Silk, PLA Matte, PLA-CF, PETG, PETG-CF, ABS, ASA, TPU, PC, PA, PA-CF or PVA. Unknown materials are refused instead of guessed, because a wrong family makes the printer use wrong temperatures. A brand without a profile for that family (for example Anycubic and PA) is also refused. Creality and Qidi printers take temperatures from their own material databases; Anycubic, Elegoo and OpenSpool records carry the family's default range. Length uses the last saved remaining weight when available, otherwise the initial filament weight, at the reference tools' 330 m per kg for 1.75 mm PLA, scaled by the spool's density and diameter. Weight fields carry the nominal spool size, as the reference tools write. Writing Qidi onto a tag previously keyed for Creality restores the default sector keys. A tag that holds a printer format cannot be rewritten as a Spoolancer inventory record; write another printer format or link its UID instead.

### Reading and cloning Bambu Lab / Snapmaker tags

Genuine Bambu Lab and Snapmaker U1 tags are MIFARE Classic 1K with per-tag sector keys derived from the UID (Bambu by HKDF-SHA256, Snapmaker by HMAC-SHA256). The firmware derives those keys and **reads** the tag to show its brand, material and color as a read-only preview (`POST /api/nfc/read` returns `metadata.source:"printer_tag"`). These tags carry an RSA-2048 signature only the vendor can produce, so new or edited tags are impossible.

A Bambu tag can be **cloned** byte-for-byte onto a Gen2/CUID or FUID magic card: its data blocks are read with the derived keys and its sector trailers are reconstructed (Key A = derived, access bits `87 87 87 69`, Key B = 0), exactly as genuine tags store them. `POST /api/nfc/clone {step:"read"}` reads the source; `{step:"write",confirm:true}` writes the magic card. The write writes block 0 first and verifies the UID changed, so a non-magic card is refused without any sector being rekeyed. The clone keeps the original material and color — it cannot change them. **Snapmaker cannot be cloned**: its signature covers the sector keys, so a clone fails verification. For custom Snapmaker tags, use the OpenSpool format with the community U1 Extended firmware.

Printer formats overwrite any unlocked tag after confirmation, including foreign content. Locked or password-protected NTAGs and Classic sectors with unknown keys are refused before writing. Not possible: **Bambu Lab** AMS and **stock Snapmaker U1** tags carry RSA signatures that only the manufacturer can produce; **Prusa OpenPrintTag** uses ISO15693, which the PN532 cannot reach. **Sovol** printers have no filament tag reader. Formats were reproduced from source and checked against published byte vectors; they have not been tested on each printer.

## Spoolman

Settings offers discovery on the connected LAN and manual configuration. Discovery probes TCP port 7912 on at most 254 addresses in the ESP32's own /24, clipped to the actual subnet. Four nonblocking sockets use 180 ms probe deadlines, with a 45-second search bound. Optional mDNS HTTP services and `spoolman.local` are also checked. Candidates must pass Spoolman's `/info` and spool-list response checks; discovery never changes configuration automatically. Other ports or subnets require manual entry unless an eligible service is advertised.

Use a numeric IPv4 HTTP base address such as `http://192.168.1.20:7912`; a port and base path are optional. Manual DNS names, HTTPS, and authenticated reverse proxies are not implemented. Use it on a trusted local network: device pairing and Spoolman requests use HTTP, and flash storage is not encrypted.

When configured, Spoolman is the authoritative inventory. Complete paginated snapshots replace the visible cache atomically, up to 200 active remote spools. Local records and unfinished registrations are kept separately and never substituted for remote inventory on an error. Cached remote data is marked stale after 30 seconds or a failed request; refresh attempts are throttled for ten seconds. A remote profile is read-only here, except for a local tag binding. Unknown server weights remain unknown. Persisted caches are stale after reboot and are written only with explicit record operations, not every poll.

Every sync saves the current validated measurement locally first. An existing link uses `PATCH /api/v1/spool/{id}` with the **absolute** `remaining_weight`, avoiding duplicate consumption subtraction. A new link finds an exact vendor name or creates the vendor, then creates a filament and spool. The density and diameter fields are editable; the defaults are not measurements. Existing linked filament metadata is not updated automatically.

Before every create request, the device persists a pending marker. Timeout, invalid success response, or server error can mean creation happened without a received confirmation. That state blocks automatic creation retries. Check Spoolman and enter the correct existing spool ID; if only a vendor/filament was created, finish the spool manually and link it. There is no background sync queue. A pending marker is not a scheduled retry.

Removing a linked spool archives it with an idempotent PATCH and verifies its state with a separate GET. An archive journal is persisted before any remote mutation. Unknown outcomes keep that journal and surface a recovery action even after a refresh hides the remote entry. Local deletion and RFID changes roll back in memory if persistence fails. Hidden remote profiles with a retained binding appear in `drafts` with `inactive_tag_link:true`; explicit unlink matches the exact stored ID, including legacy formats, without requiring the physical tag. A clean unlinked draft can be discarded, but unresolved creation journals are protected.

If a spool was externally deleted during an uncertain archive, retry can finish the existing archive journal only after an exact ID-specific Spoolman 404, fresh server-info validation and fresh ordered inventory checks including archived records prove absence. Paging is bounded to 200 records with a six-second start deadline; incomplete/error checks retain the journal. That outcome returns `archived:false,removed_missing:true` and does not claim to restore deleted server history. It is not a generic 404 cleanup path.

Changing the configured server is blocked while any links or unresolved operations remain, so numeric IDs cannot silently target a different database. Remote profiles are read-only in the spool editor. Recovery links a saved draft to a confirmed server ID; when that record is already cached, reconciliation preserves the draft identity and rejects conflicting tag bindings.

HTTP requests use a 150 ms connection timeout, cooperative socket writes/header/body reads with a two-second deadline, and a 20 KB response cap. Scale sampling continues during cooperative waits. HTTP/1.0 is requested; redirects, compressed bodies and transfer encodings are rejected. The browser waits up to 30 seconds for sync and does not retry automatically. A browser timeout is an unknown outcome, not proof that the operation was canceled.

## API

`GET /api/status` is public and contains hardware status, active spool ID and connection state. `POST /api/session` accepts `{ "pin": "your-eight-digit-PIN" }` and returns a bearer token. Fresh headless/OLED/touch setup uses `POST /api/setup` with `{wifi_ssid,wifi_password,wifi_open,pin_required,pin?}` under the AP-only first-run restrictions above. It returns a bearer token after saving. Explicitly configured no-PIN devices accept `POST /api/session {}` from their own local IP or `spoolancer.local` origin; all private routes still require the returned bearer token. `POST /api/wifi/scan` starts asynchronous discovery and `GET /api/wifi/scan` polls `state` (`scanning`, `ready`, `failed`) plus bounded network results. Scan access requires a session or the trusted initial setup AP. Initial setup does not require BOOT. All routes below require `Authorization: Bearer TOKEN`, including the two private GET routes. JSON request bodies are limited to 4096 bytes; errors are `{ "error": "message" }`.

Status identifies `device.variant` as `headless`, `oled-led`, `touch-8048`, or `st7789`; `device.headless` is true only for the screenless N16R8 variant. Status also includes `auth:{pin_required,invalid}` and explicit `setup.required`/`setup.complete`. Headless, OLED and integrated-touch status include `setup:{ap_active,open_network,via_ap,physical_confirmation_required:false,can_submit,ap_seconds,warning}`. `via_ap` describes the current HTTP connection. These fields contain no PIN, Wi-Fi password or session token. The browser uses `can_submit` to enable initial setup through the temporary AP. Scale status includes `tare_ready` and `calibration_ready` separately from ordinary weighing stability, plus `sample_count`. Status also exposes `diagnostics:{leds_supported,led_test_remaining_ms}` so unsupported profiles and an active visual test are explicit.

| Route | Payload / result |
|---|---|
| `GET /api/spools` | `{spools:[...],inventory:{source,configured,stale,cached,error,limit},drafts:[...]}`; up to 100 local records or 200 active remote records; `?refresh=1` requests refresh |
| `POST /api/spools` | Create/update local metadata or explicitly create in Spoolman with `confirm:true,spool_condition`; optional `infer_empty_from_measurement:true` rechecks a full-spool tare estimate on-device. Remote metadata is read-only except tag binding. Failed creates return the saved draft for recovery |
| `POST /api/spools/tag` | `{id,expected_uid,confirm:true}`; freshly verify the presented identifier and bind it to an existing spool without writing the tag. Optional `unlink:true` instead clears the exact saved identifier without a physical scan; unresolved journals refuse either action |
| `POST /api/spools/remove` | `{id,confirm:true}`; transactional local deletion or verified Spoolman archive. Uncertain remote outcomes retain `removal_pending:true` and `recovery_required:true` for an explicit retry |
| `POST /api/active-spool` | `{id}` or `{id:null}` |
| `POST /api/measurements` | `{spool_id,confirm:true}`; save fresh stable net weight locally |
| `POST /api/diagnostics` | `{component:"nfc"\|"leds"\|"scale"}`; authenticated signal test. Returns `state:pass/warning/fail/observe` and observations; LEDs return a bounded duration, never automatic visual success |
| `POST /api/scale/tare` | `{}`; zero empty stable cradle |
| `POST /api/scale/calibrate` | `{reference_g}` |
| `POST /api/nfc/read` | `{}`; return `nfc`, validated metadata if readable and matched `spool_id`; a readable single identifier can succeed even when tag contents cannot be decoded |
| `POST /api/nfc/write` | `{spool_id,expected_uid,confirm:true,format?}`; `format` is `spoolancer` (default), `creality`, `qidi`, `anycubic`, `elegoo` or `openspool`. Success only after read-back; the UID is then linked to the spool |
| `POST /api/nfc/clone` | `{step:"read"}` reads a genuine Bambu tag; `{step:"write",confirm:true}` clones it onto a Gen2/FUID magic card. Read-only Bambu only; Snapmaker and non-magic cards are refused |
| `GET /api/config` | Device name, Wi-Fi SSID, Spoolman URL, `has_wifi_password`; no password/PIN/token |
| `POST /api/config` | Partial `device_name`, `wifi_ssid`, `wifi_password`, `wifi_open`, legacy `clear_wifi_password`, `spoolman_url` |
| `POST /api/spoolman/test` | `{}`; verify the configured server and refresh its inventory |
| `POST /api/spoolman/discover`, `GET /api/spoolman/discover` | Start or poll bounded local discovery; proposed server URLs never replace configuration automatically |
| `POST /api/spoolman/sync` | `{spool_id,confirm:true}`; local save then create or absolute update |
| `GET /api/update` | Check GitHub for the latest release. Returns `{current,variant,latest,update_available,notes_url,has_filesystem}`, or `{current,variant,error}` when no release is reachable |
| `POST /api/update` | `{confirm:true}`; download and flash the release image for this board, then reboot. Refused when already current or offline |

Config save returns `reconnect_required`, `ap_ssid`, `ip` and a human-readable `message`. Reconnect occurs after the response; firmware does not reboot. An omitted or blank Wi-Fi password keeps it only when the SSID is unchanged. A changed secured SSID requires a fresh password; `wifi_open:true` deliberately selects an open network. Passphrases accept 8–63 UTF-8 bytes or a 64-digit hexadecimal key. SSIDs preserve exact spaces and are bounded to 32 UTF-8 bytes. The old `clear_wifi_password` field remains compatible for the unchanged network.

Text limits are UTF-8 **bytes** on the device: name64, brand60, material30, device name48, SSID32. Browser character limits can accept fewer-byte ASCII and more-byte international text differently; the device reports validation failure when its byte limit is exceeded.

## Over-the-air updates

Settings → Hardware tests → **Firmware update** checks the project's GitHub releases and, when a newer one exists, installs it over Wi-Fi. The device asks `https://api.github.com/repos/bloodf/spoolancer/releases/latest`, downloads the two assets named for its own board variant — `spoolancer-<variant>-littlefs.bin` (web UI and assets) then `spoolancer-<variant>-firmware.bin` (the app) — flashes them to the inactive OTA slot and the filesystem partition, and reboots. The variant string matches `device.variant` (`headless`, `headless-led`, `oled-led`, `touch-8048`, `st7789`). The running version is reported as `device.firmware` and is set at build time from the release tag (`0.0.0-dev` for local builds).

Recovery: the two app partitions (`ota_0`/`ota_1`) mean a bad app image leaves the previous one in place; a failed filesystem write only costs the web assets, which a repeated update or a USB `uploadfs` restores. The transport is HTTPS but the certificate chain is not pinned and the image is not signed (`WiFiClientSecure::setInsecure`); the recovery paths above bound the risk. Pin a CA bundle and sign images before trusting this over an untrusted network.

To cut a release, push a `vX.Y.Z` tag (or run the **Release firmware** workflow with that tag). CI builds every variant's firmware and filesystem image, injects the tag as the firmware version, and publishes them as the release assets the OTA expects. See [the release workflow](../.github/workflows/release.yml).

## Evidence and remaining verification

The real-library numeric JSON test runs after PlatformIO installs ArduinoJson. It verifies that new fields are created through their member proxies, that finite weights and explicit unknown/null values appear in responses, and that spool weights survive serialization/reload. CI runs it after compiling the firmware. This catches a failure that pure sensor stubs cannot detect: converting an absent member to a bound JsonVariant before assignment can discard the numeric field.


The native tests exercise measurement rejection rules, identity/color validation, documented PN532 A/B/F/J response fixtures, malformed/truncated/multi-target responses, NTAG213/215/216 capacity and protection, reserved-page exclusion, NDEF boundaries, cache/refresh and bounded timeout yielding. They also cover removal/unlink policies, uncertain archive state, HX711 calibration/freshness using mocked GPIO/time, BOOT timing/rollover, startup-held suppression, one-reset-per-hold behavior, AP expiry, setup throttling, no-button first-run setup and hostile Host/Origin rejection under AddressSanitizer/UndefinedBehaviorSanitizer. All five build variants and filesystem packaging validate build integration. They do not test a physical button, sensor, UART, NFC RF placement, display, Wi-Fi/AP onboarding, or a running Spoolman server. Those bench checks remain necessary, especially with the spool seated and the movable reader wiring connected.

Primary references: [Espressif Arduino core](https://github.com/espressif/arduino-esp32), [PlatformIO ESP32-S3 board definition](https://github.com/platformio/platform-espressif32/blob/v6.12.0/boards/esp32-s3-devkitc-1.json), [PN532 user manual](https://www.nxp.com/docs/en/user-guide/141520.pdf), [NTAG213/215/216 datasheet](https://www.nxp.com/docs/en/data-sheet/NTAG213_215_216.pdf), and Spoolman [spool](https://github.com/Donkie/Spoolman/blob/master/spoolman/api/v1/spool.py), [filament](https://github.com/Donkie/Spoolman/blob/master/spoolman/api/v1/filament.py) and [vendor](https://github.com/Donkie/Spoolman/blob/master/spoolman/api/v1/vendor.py) API source.
