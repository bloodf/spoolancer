# Spoolancer web interface

This is a local ESP32-hosted interface for live weight, saved spool profiles, NFC actions, calibration, Wi-Fi setup and optional Spoolman synchronization. It uses plain HTML, CSS and JavaScript, with no CDN, framework or package installation.

The default page calls the device API on its own origin. It never falls back to simulated readings. Public status remains visible while locked; inventory and settings load only after pairing with the device PIN. The session token stays in tab memory. No PIN, token or Wi-Fi password is written to browser storage. Only the appearance preference (`spoolancer.theme`: `system`, `light` or `dark`) is stored locally. Blocked storage does not prevent using the interface.

## Device assets

Copy these five files into the firmware's LittleFS image at its root:

- `index.html`
- `styles.css`
- `app.js`
- `logic.js`
- `favicon.svg`

Serve `/` as `index.html`, CSS as `text/css`, JavaScript as `text/javascript`, and SVG as `image/svg+xml`. There is no asset build step. Do not include `demo.js`, tests or this README in the firmware image. The firmware build owns asset copying and filesystem packaging. The interface expects a modern browser with ES modules, `fetch` and native dialogs.

## Desktop preview

From the repository root:

```sh
python3 -m http.server 8765 --bind 127.0.0.1 --directory web
```

Open <http://127.0.0.1:8765/?demo=1>. Pair with **12345678**. A permanent amber **Demo** banner identifies the simulated adapter. The state selector exercises moving loads, missing calibration, unknown tare, removed/read-only tags, disconnection and Spoolman errors. Select **Screenless first setup** to preview choosing a PIN; **Simulate BOOT press** opens the demo enrollment window. This button exists only in the desktop demo, and cannot arm real hardware. That scenario accepts your chosen preview PIN until you switch scenarios or reload. Changes last only for that preview session.

Demo loading requires both `?demo=1` and a localhost hostname. The plain URL, without that query, tries the real API and shows disconnection on this static server. No demo action contacts hardware or Spoolman. Browser preview verification does not establish physical weighing, NFC or printer compatibility.

## Controls and appearance

The spool editor offers ten named color swatches, a native custom color picker and an editable hex field. Three-digit hex is expanded to six digits before saving. The material selector includes PLA, PLA+, PETG, ABS, ASA, TPU, PA, PC, PVA and HIPS. **Custom material** preserves other saved values, such as PA-CF, without forcing them into a preset.

The header's **Appearance** selector offers System, Light and Dark. System follows the browser's operating-system preference, including changes while the page is open. The responsive layout supports phone and 800 × 480 browser viewports with scrolling dialogs. This is a browser interface, not a native touchscreen runtime.

## Screenless and integrated-touch first setup

The headless build advertises its temporary setup network without a password. The page prominently warns **Temporary setup network has no password; use it in a trusted location** and states that Wi-Fi credentials use local HTTP.

Join that network and open `http://192.168.4.1`. Choose an 8-digit PIN, then hold and release onboard BOOT for 2 seconds while the scale is running. Submit within the reported 60-second arm window. The firmware consumes one enrollment attempt even if the submitted PIN is rejected. The page enables enrollment only when fresh public status confirms headless mode or the integrated-touch variant, required enrollment, an active setup AP, access through that AP and a physically armed window. The firmware independently enforces the interface, origin, arm and PIN checks.

After pairing, save Wi-Fi settings. The setup AP closes after 10 minutes, or 60 seconds after LAN connection. It does not reopen automatically on a later Wi-Fi failure. Holding BOOT for 8 seconds while running resets Wi-Fi and pairing and reopens setup; inventory and calibration remain stored. A forgotten PIN uses the same recovery procedure. Only the legacy ST7789 build keeps the displayed/Serial PIN pairing flow. Holding BOOT during power-on or RST enters programming mode; release BOOT and tap RST to start normally.

## API contract

Responses are JSON. A failed request uses a non-success HTTP status and `{ "error": "Human-readable reason" }`. Pairing returns `{ "token": "..." }`. All POST requests except pairing, and both private GETs, carry `Authorization: Bearer <token>`. HTTP 401 clears private state and requires pairing again. The firmware must independently authenticate and validate every mutation.

| Endpoint | Purpose |
| --- | --- |
| `GET /api/status` | Public device, scale, NFC, active spool ID, network and Spoolman status |
| `POST /api/session` | Pair with `{pin}`; first screenless enrollment uses `{pin, setup:true}` after physical arming |
| `GET /api/spools`, `POST /api/spools` | Read private inventory; create or update a profile with optional `id` |
| `GET /api/config`, `POST /api/config` | Read non-secret settings; save device, Wi-Fi and Spoolman configuration |
| `POST /api/active-spool` | Select `{id}` for this weighing session |
| `POST /api/measurements` | Save `{spool_id, confirm:true}` locally |
| `POST /api/scale/tare` | Zero the empty cradle with `{}` |
| `POST /api/scale/calibrate` | Set span using `{reference_g}` |
| `POST /api/nfc/read` | Read the current tag with `{}` |
| `POST /api/nfc/write` | Write `{spool_id, expected_uid, confirm:true}` and verify read-back |
| `POST /api/spoolman/test` | Test the saved server with `{}` |
| `POST /api/spoolman/sync` | Create or update the linked spool using `{spool_id, confirm:true}` |

`/api/status` must include `mode:"device"`, `device.name`, `scale.{gross_g,stable,calibrated,fresh}`, `nfc.{present,uid,type,writable}`, `active_spool_id`, `network.{connected,ip}` and `spoolman.configured`. The UI also displays optional `nfc.last_message`, `nfc.last_error`, `spoolman.pending`, `spoolman.last_message` and `spoolman.last_error`. Current status identifies `device.variant` as `headless`, `touch-8048` or `st7789`. The first two use the open setup flow; only the screenless build reports `device.headless:true`. Both additionally supply `setup.{ap_active,open_network,via_ap,enrollment_required,enrollment_armed,enrollment_seconds,ap_seconds,warning}`. The UI refreshes that status after each enrollment attempt and does not reuse an arm window. A browser refresh failure or an ADC `fresh:false` state suppresses live weight. The browser polls status every 1.5 seconds without overlapping requests.

Spool records contain `id`, `name`, `brand`, `material`, six-digit hex `color`, nullable `empty_weight_g`, required `initial_weight_g`, `density_g_cm3`, `diameter_mm`, optional `spoolman_id` and optional last saved `remaining_weight_g`. The agreed inventory limits are name 64 UTF-8 bytes, brand 60 bytes, material 30 bytes on-device, tare 0–2,000 g or unknown, initial filament 1–5,000 g, density 0.1–5 g/cm³ and diameter 0.5–5 mm. Larger nominal inventory values do not increase the scale's 2,000 g gross operating target.

Name, brand and material are required. An unknown brand can be entered as Generic. Reference masses for calibration must be 100–2,000 g. Requests use bounded timeouts, extended to 30 seconds for tag writes and Spoolman sync, and 20 seconds for NFC reads and server tests. Polling pauses during mutations. A timeout does not prove an action failed; the UI asks the operator to inspect the result before retrying.

Saving a measurement or syncing requires a current, calibrated, stable reading from 20–2,000 g, the same active spool, a known nonnegative tare, and remaining weight between zero and the recorded initial filament mass. Remaining weight is gross minus the known empty spool weight. The UI does not infer tare from initial filament mass or reuse a saved measurement as a live reading. The firmware rechecks the current reading when an action is submitted.

An unlinked Spoolman profile can create vendor, filament and spool records after confirmation. The UI includes editable density and diameter for creation, and manual ID linking for an existing spool. A failed or ambiguous create remains an error; the UI never retries automatically. The current firmware reports pending/manual retry status, not a durable automatic queue.

NFC writes target the project's own `application/vnd.spoolancer+json` schema 1 inventory record. This is not an OpenSpool or stock-printer tag format. Original Bambu/Snapmaker tags stay read-only; detecting a UID does not mean their proprietary material data has been decoded. The firmware should report write success only after matching read-back, and explain partial writes on failure.

The separate Link tag action explicitly saves the observed `tag_uid` to an existing local spool through `POST /api/spools`. It previews the UID and profile without writing to the tag. This gives original printer tags a manually assigned local identity. If a read returns supported inventory metadata, the interface displays its name, brand and material.

Configuration accepts `device_name`, `wifi_ssid`, `spoolman_url`, optional `wifi_password`, and `clear_wifi_password`. An omitted password keeps the saved one; clearing it requires the explicit checkbox. GET returns only `has_wifi_password`. Wi-Fi changes return `{ok:true,reconnect_required:true,ap_ssid,ip,message}` before reconnecting. The screenless UI describes the timed setup network and physical recovery procedure, not an automatic access-point fallback. The UI explains how to reopen the device and never displays the saved password. Configuration and editor input remain intact after failed requests, except secrets cleared when locking or pairing.

This firmware accepts an HTTP Spoolman base URL with a numeric IPv4 address, such as `http://192.168.1.50:7912`. Port is optional and defaults to 80; an optional base path is supported. Hostnames, HTTPS, credentials, query and fragment are rejected. Server authentication is not implemented. A URL change can be rejected while existing spool links need migration; the UI keeps the entered value and displays the device's reason.

## Validation

```sh
node --test web/tests/*.test.js
```

These dependency-free tests cover unknown versus zero tare, stale and uncalibrated readings, physical mass bounds, inconsistent profiles, UID replacement, local saving without Spoolman, private endpoint access, explicit simulated failures, color/material normalization, appearance persistence and physical enrollment gating/expiry. They validate browser logic and the deterministic preview adapter only. Firmware compilation and actual device tests are separate checks.
