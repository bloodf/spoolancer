# Spoolancer device firmware

This is an Arduino/PlatformIO implementation for ESP32-S3 N16R8 hardware. The builds use 16 MB flash and octal PSRAM, read the HX711, communicate with the PN532 over HSU, and serve the local web interface. One `spoolancer-oled-led` board was flashed on 27 October 2026 and reported successful USB startup, detected memory and mounted storage. **OLED/LED operation, weighing, NFC and real Spoolman behavior remain unvalidated.** See the [boot record](../docs/DEVICE-BOOT.md) and [full evidence record](../docs/STATUS.md).

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
```

The pre-build script copies exactly `web/index.html`, `web/styles.css`, `web/app.js`, and `web/logic.js`, and `web/favicon.svg` into the asset image. It does not include the desktop demonstration. The HTTP server serves only these assets; private storage has no file-serving route.

When ready to test the hardware, connect the board's **COM** USB-C port and use:

```sh
pio run -d firmware -e spoolancer-headless -t upload --upload-port YOUR_SERIAL_PORT
pio run -d firmware -e spoolancer-headless -t uploadfs --upload-port YOUR_SERIAL_PORT
pio device monitor -d firmware -e spoolancer-headless --port YOUR_SERIAL_PORT
```

Both upload commands were run successfully for `spoolancer-oled-led`; other profiles have compilation evidence only. Replace `spoolancer-headless` with the environment for your hardware. For the new DC-powered cases, remove the cover and follow [power isolation instructions](../docs/DC-POWER.md#power-connection-and-programming) before attaching computer USB. Verify 16 MB flash / 8 MB PSRAM on your actual module before installation. PlatformIO's inherited board description still says N8; `platformio.ini` overrides its flash size, memory type and partition table. The generated bootloader header is 16 MB. The firmware reports detected flash and PSRAM on the serial console. USB CDC is disabled so the programming bridge supplies serial output.

For the integrated touchscreen, use `-e spoolancer-touch-8048` for build, filesystem and upload commands. It pins Arduino_GFX 1.4.9 and uses the reference RGB panel/GT911 layout described below. Treat it as experimental until the exact purchased board revision has been checked and tested.

The `spiffs` partition contains LittleFS web assets at `0x810000`, size `0x6f0000` (7,274,496 bytes). A separate 1 MB `inventory` partition at `0xf00000` stores local spools and the remote-operation journal. Its CSV subtype is written numerically as `0x82`, the same ESP32 SPIFFS subtype, so PlatformIO 6.12 does not mistake the last private filesystem for the asset-upload target. A post-build configuration guard refuses filesystem build/upload targets with the wrong address or size. Configuration, PIN and calibration use NVS. Uploading assets leaves inventory separate; erasing the whole flash destroys all three. Inventory initializes once; a later mount or parsing failure disables writes and preserves data for recovery.

After building firmware and filesystems, `python3 firmware/tests/check_filesystem.py --env spoolancer-headless` checks the generated binary partition table, image size, and unpacked contents against the five current web files. Omit `--env` to inspect all five built variants. It does not upload anything.

## Five LEDs and optional 0.96-inch OLED

Use `-e spoolancer-headless-led` or `-e spoolancer-oled-led` in the build, asset and upload commands. These profiles use the separate ESP32-S3 N16R8 and keep the headless BOOT setup gestures and browser editor. They do not use the ST7789 buttons or integrated touchscreen pin map. Connect OLED SDA to GPIO4, OLED SCK/SCL to GPIO5 and the LED data signal to GPIO6 through the level shifter described in [OLED-RGB-VARIANT.md](../docs/OLED-RGB-VARIANT.md).

The LED driver defaults to five 5 V WS2812-style **GRB / 800 kHz** pixels at brightness **51/255 (20%)**. The user's strip chip and color order are not yet confirmed. `SPOOLANCER_LED_ORDER=NEO_RGB` changes the order; `SPOOLANCER_LED_ORDER=NEO_GRBW` supports compatible RGBW strips while leaving white off. Add these definitions to the selected environment's `build_flags` with `-D`; `SPOOLANCER_LED_BRIGHTNESS` is also configurable from 0 to 255. A firmware brightness limit is not an electrical current limiter.

| LED appearance | Meaning |
|---|---|
| Cyan bar, each complete LED representing 20% | Valid remaining filament fraction; next LED dims for a partial segment |
| Blue pulse | Temporary Wi-Fi setup network active |
| Amber | Measurement unavailable, unstable or missing required spool information |
| Purple | Explicit NFC read/write in progress |
| Blue/cyan | Local save or Spoolman operation in progress |
| Green or red for 2.5 seconds | Operation succeeded or failed; then the current measurement/setup state returns |

Routine background tag scans do not flash the strip. The PN532 wait callback samples the scale and refreshes the status UI, so a scan cannot leave a stale remaining display frozen. Explicit NFC and HTTP operations show a steady busy indicator. Results are driven by operation outcomes and expire; saved error/message strings cannot keep the LEDs green or red indefinitely. NFC read success requires exactly one target without an error.

The OLED shows material, valid remaining grams, gross grams, status and IP address. Unknown empty weight, stale/unstable data, missing calibration, out-of-range gross or impossible remaining weights suppress remaining grams. Busy operations also hide remaining grams because the screen cannot refresh during blocking work. The OLED is an indicator; editing and confirmation stay in the browser.

**SSD1306 at I2C address 0x3c is provisional**, not identified from screen size alone. Add `-DSPOOLANCER_OLED_SH1106=1` if the delivered controller is SH1106; use `-DSPOOLANCER_OLED_ADDRESS=0x3d` if its address differs. U8g2 supports both controllers; acknowledging an I2C address does not identify the controller. An absent OLED is skipped, and a later I2C failure disables display updates without stopping the browser. Repair the wiring and restart to enable it again. LED signaling, OLED rendering and fit still need checks on the actual hardware.

## Headless first use

Use the development board’s existing **BOOT button on GPIO0**. No separate setup button, button wiring, or button perfboard is needed. The firmware enables GPIO0’s pull-up. Keep HX711 DOUT15/CLK16 and PN532 RX18/TX17 as specified in [WIRING.md](../WIRING.md), with both PN532 switches OFF. Leave the screen and Previous/Next/Select controls unconnected; this variant does not use them.

Use BOOT **after the firmware has started**, with the button initially released. Holding BOOT while powering on or pressing RST selects the ESP32 download boot mode, not Wi-Fi recovery. If that happens, release BOOT and press RST once, then use BOOT while the scale is running. The gesture handler ignores a button already held when it first starts polling until it sees a release. RST alone only restarts the board.

1. On first boot, join `Spoolancer-xxxx` from your phone. It is a temporary network **without a password**. Open `http://192.168.4.1` manually; no serial console or display is needed.
2. Choose your own eight-digit device PIN in the browser. Hold BOOT for two seconds, then release it. Complete PIN enrollment within 60 seconds. Each physical press authorizes one enrollment attempt, including a rejected PIN attempt.
3. Configure your home Wi-Fi. After the scale connects, note the LAN address shown in the browser, reconnect your phone to that network and open the scale there. `http://spoolancer.local` may also work on networks supporting mDNS. Pair with your chosen PIN whenever this browser tab reloads or its two-hour session expires.
4. The open setup AP closes after ten minutes, or 60 seconds after a successful LAN connection, whichever comes first. A failed connection on an already configured headless device does **not** automatically open an AP. Hold BOOT eight seconds while running to recover, then enroll a new PIN and configure Wi-Fi again.
5. Zero the empty stable cradle, calibrate with a known 100–2000 g reference, and verify independent known weights. Add/select a spool with known empty-spool weight before saving a measurement or syncing it.

**Temporary setup network has no password; use it in a trusted location.** Pairing and Wi-Fi credentials travel over local HTTP on this open network and can be observed by someone nearby. The short window and physical button limit enrollment access; they do not encrypt this traffic or protect against radio interception. No universal private password is supplied. A future per-device random setup key delivered by a printed label/QR code or initial USB step would allow a protected setup AP; that alternative is not implemented here.

Enrollment is accepted only while the device has no PIN, through the AP's local destination address and subnet, while its station connection is disconnected, with the expected setup Host/Origin, and during the physical enrollment window. Enrollment consumes that window. The resulting random browser session token is separate from the chosen PIN. An eight-second reset removes Wi-Fi credentials and pairing access, closes existing sessions, and preserves inventory, Spoolman settings/links and calibration. It does not erase the entire device.

If a PIN has been enrolled but Wi-Fi setup is interrupted before saving credentials, use the same eight-second reset to restart the process. Switching from an existing screen build can retain its old PIN; reset once to enroll a PIN without needing the former screen or serial output.

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
| Physical setup/reset | Onboard BOOT, GPIO0 |

PN532/HX711 power and logic remain 3.3 V with common ground. The peripheral pins differ from the separate N16R8 development board. GPIO11/12/13 are reused from the SD interface: SD is disabled, CS10 is held high, and the card slot must remain empty. Do not connect the PN532 to GPIO18 or the HX711 to GPIO15/16 on this board; those pins belong to touch/display functions.

The touchscreen uses the same temporary open-AP onboarding and chosen browser PIN as the headless build. Its screen shows setup instructions and the address/PIN. Use the onboard BOOT button **only after firmware is running**: hold/release two seconds to arm enrollment, or hold eight seconds to clear Wi-Fi/pairing. Holding BOOT during power-up/reset enters the ESP32 bootloader instead of performing a Wi-Fi reset.

Native controls select spools, read tags, write inventory tags, save measurements, sync Spoolman, and zero the cradle. Writes, sync and zero require native confirmation; the handlers recheck the current spool selection, tag UID and measurement conditions before acting. Local storage and NFC/Spoolman operations are shared with the browser paths. Brand/material/color/tare editing, calibration-reference entry and network/server configuration remain browser actions. The native theme is stored separately from browser theme preferences.

The configured measurement range is 20–2000 g **gross**, including the empty spool. This remains a limit even though inventory can describe an initial filament weight up to 5000 g. Unknown empty-spool weight blocks net-weight saving and synchronization. Calibration and zero are separate from the empty-spool weight.

The HX711 is sampled only when ready. Stability uses 20 samples with at most a 5 g spread. A gap of 600 ms invalidates the window. Firmware clears the selection after a loaded cradle returns near zero; wait for this before placing the next spool. A new unrecognized tag or an explicit read without a local match also clears selection. The sensor cannot identify a different untagged spool swapped without a detected empty interval: select the correct record manually.

## Buttons

The headless and integrated-touch builds use onboard **BOOT / GPIO0** after startup: hold for two seconds and release to authorize setup enrollment; hold eight seconds to reset Wi-Fi/pairing and reopen setup. Short presses do nothing. Reset fires once per hold and does not reboot the ESP32. Release BOOT before starting another gesture. Inventory, calibration and Spoolman settings/links are kept. In the headless build, tare, calibration and spool selection are browser actions.

The existing screen build retains these controls:

| Action | Result |
|---|---|
| Previous / Next, short press | Select a local spool |
| Previous, hold 3 seconds | Show the pairing PIN for one minute |
| Select, hold 2 seconds | Zero the empty, stable cradle and save calibration |
| All three, hold 8 seconds | Reset Wi-Fi and pairing PIN; keep inventory and calibration |

## NFC format and limits

Original Bambu Lab, Snapmaker and other ISO14443A tags are **read-only UID identification**. Their proprietary filament fields are not decoded. Linking a scanned UID to a local spool only changes local inventory. It does not change printer data or make a tag recognizable to a stock printer.

Writes require one detected, unlocked NTAG216 with the exact NTAG216 version response and unprotected read/write capability container. Only factory blank NDEF or Spoolancer-owned MIME records are accepted. Other tag types, locked/protected tags, and foreign content are refused. UID is checked against the confirmation, reselected during each 16-byte write group, and checked on final read-back. Tag movement, interruption or power loss can leave an incomplete record; the UI must report failure and the tag must be rescanned. This is not a transactional NFC device.

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

This is **not OpenSpool-compatible or a Bambu/Snapmaker printer tag format**. No credentials, server URLs or remaining-weight measurements are written to tags. Foreign Spoolancer records are displayed after validation but are not automatically imported. Existing matching local IDs or explicitly bound UIDs select the local record. NTAG216 authenticity is not cryptographically verified; the safeguards prevent accidental writes, not a deliberate tag impersonation.

## Spoolman

Use a numeric IPv4 HTTP base address such as `http://192.168.1.20:7912`; a port and base path are optional. DNS names, HTTPS, and authenticated reverse proxies are not implemented in this first version. Use it on a trusted local network: device pairing and Spoolman requests use HTTP, and flash storage is not encrypted.

Every sync saves the current validated measurement locally first. An existing link uses `PATCH /api/v1/spool/{id}` with the **absolute** `remaining_weight`, avoiding duplicate consumption subtraction. A new link finds an exact vendor name or creates the vendor, then creates a filament and spool. The density and diameter fields are editable; the defaults are not measurements. Existing linked filament metadata is not updated automatically.

Before every create request, the device persists a pending marker. Timeout, invalid success response, or server error can mean creation happened without a received confirmation. That state blocks automatic creation retries. Check Spoolman and enter the correct existing spool ID; if only a vendor/filament was created, finish the spool manually and link it. There is no background sync queue. A pending marker is not a scheduled retry.

Clear an existing Spoolman spool ID in the editor to unlink that record. Changing the configured server is blocked while any links or unresolved creates remain, so numeric IDs cannot silently target a different database. HTTP uses finite connect/read/body timeouts, limits responses to 20 KB, disables redirects, and requests HTTP/1.0. The browser waits up to 30 seconds for sync and does not retry automatically. A browser timeout is an unknown outcome, not proof that the operation was canceled.

## API

`GET /api/status` is public and contains hardware status, active spool ID and connection state. `POST /api/session` accepts `{ "pin": "your-eight-digit-PIN" }` and returns a bearer token. Headless first enrollment uses `{ "pin": "your-chosen-eight-digits", "setup": true }` under the physical/AP restrictions above. All routes below require `Authorization: Bearer TOKEN`, including the two private GET routes. JSON request bodies are limited to 4096 bytes; errors are `{ "error": "message" }`.

Status identifies `device.variant` as `headless`, `touch-8048`, or `st7789`; `device.headless` is true only for the screenless N16R8 variant. Headless and integrated-touch status both include `setup:{ap_active,open_network,via_ap,enrollment_required,enrollment_armed,enrollment_seconds,ap_seconds,warning}`. `via_ap` describes the current HTTP connection. These fields contain no PIN, Wi-Fi password or session token. The browser uses them to offer PIN enrollment only through the setup network, show its warning and enable enrollment after the physical press.

| Route | Payload / result |
|---|---|
| `GET /api/spools` | `{spools:[...]}`; at most 100 local records |
| `POST /api/spools` | Create/update full metadata; optional existing `id`, `spoolman_id`, `tag_uid` |
| `POST /api/active-spool` | `{id}` or `{id:null}` |
| `POST /api/measurements` | `{spool_id,confirm:true}`; save fresh stable net weight locally |
| `POST /api/scale/tare` | `{}`; zero empty stable cradle |
| `POST /api/scale/calibrate` | `{reference_g}` |
| `POST /api/nfc/read` | `{}`; return `nfc`, validated metadata and matched `spool_id` |
| `POST /api/nfc/write` | `{spool_id,expected_uid,confirm:true}`; success only after read-back |
| `GET /api/config` | Device name, Wi-Fi SSID, Spoolman URL, `has_wifi_password`; no password/PIN/token |
| `POST /api/config` | Partial `device_name`, `wifi_ssid`, `wifi_password`, `clear_wifi_password`, `spoolman_url` |
| `POST /api/spoolman/test` | `{}`; check the server's spool-list endpoint |
| `POST /api/spoolman/sync` | `{spool_id,confirm:true}`; local save then create or absolute update |

Config save returns `reconnect_required`, `ap_ssid`, `ip` and a human-readable `message`. Reconnect occurs after the response; firmware does not reboot. Omitting the Wi-Fi password keeps it. `clear_wifi_password:true` deliberately selects an open network.

Text limits are UTF-8 **bytes** on the device: name64, brand60, material30, device name48, SSID32. Browser character limits can accept fewer-byte ASCII and more-byte international text differently; the device reports validation failure when its byte limit is exceeded.

## Evidence and remaining verification

The native tests exercise measurement rejection rules, identity/color validation, NTAG216 protection flags, NDEF boundaries, malformed records, all truncated record lengths, HX711 calibration/freshness using mocked GPIO/time, BOOT button timing/rollover, startup-held suppression, one-reset-per-hold behavior, enrollment expiry/one-use behavior, and hostile Host/Origin rejection under AddressSanitizer/UndefinedBehaviorSanitizer. All three build variants and filesystem packaging validate build integration. They do not test a physical button, sensor, UART, NFC RF placement, display, Wi-Fi/AP onboarding, or a running Spoolman server. Those bench checks remain necessary, especially with the spool seated and the movable reader wiring connected.

Primary references: [Espressif Arduino core](https://github.com/espressif/arduino-esp32), [PlatformIO ESP32-S3 board definition](https://github.com/platformio/platform-espressif32/blob/v6.12.0/boards/esp32-s3-devkitc-1.json), [PN532 user manual](https://www.nxp.com/docs/en/user-guide/141520.pdf), [NTAG213/215/216 datasheet](https://www.nxp.com/docs/en/data-sheet/NTAG213_215_216.pdf), and Spoolman [spool](https://github.com/Donkie/Spoolman/blob/master/spoolman/api/v1/spool.py), [filament](https://github.com/Donkie/Spoolman/blob/master/spoolman/api/v1/filament.py) and [vendor](https://github.com/Donkie/Spoolman/blob/master/spoolman/api/v1/vendor.py) API source.
