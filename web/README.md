# Spoolancer web interface

This is a local ESP32-hosted interface for live weight, saved spool profiles, NFC actions, calibration, Wi-Fi setup and optional Spoolman synchronization. It uses plain HTML, CSS and JavaScript, with no CDN, framework or package installation.

The default page calls the device API on its own origin. It never falls back to simulated readings. Public status remains visible while locked; inventory and settings require a tab session. PIN-protected devices ask for their PIN; devices explicitly configured without a PIN obtain a local session automatically. The session token stays in tab memory. No PIN, token or Wi-Fi password is written to browser storage. Only the appearance preference (`spoolancer.theme`: `system`, `light` or `dark`) is stored locally. Blocked storage does not prevent using the interface.

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

Open <http://127.0.0.1:8765/?demo=1>. Pair with **12345678**. A permanent amber **Demo** banner identifies the simulated adapter. The state selector exercises moving loads, missing calibration, unknown tare, removed/read-only tags, disconnection and Spoolman errors. Select **Screenless first setup** to preview network discovery and optional PIN protection without pressing a hardware button. Server states distinguish unavailable inventory, cached inventory and an empty server. **New full spool** supplies a stable simulated 1,248 g gross reading for tare estimation. A completed preview retains the selected access mode across scenarios; reloading resets the demo. Changes last only for that preview session.

Demo loading requires both `?demo=1` and a localhost hostname. The plain URL, without that query, tries the real API and shows disconnection on this static server. No demo action contacts hardware or Spoolman. Browser preview verification does not establish physical weighing, NFC or printer compatibility.

## Controls and appearance

The spool editor offers ten named color swatches, a native custom color picker and an editable hex field. Three-digit hex is expanded to six digits before saving. The material selector includes PLA, PLA+, PETG, ABS, ASA, TPU, PA, PC, PVA and HIPS. **Custom material** preserves other saved values, such as PA-CF, without forcing them into a preset.

The header's **Appearance** selector offers System, Light and Dark. System follows the browser's operating-system preference, including changes while the page is open. The responsive layout supports phone and 800 × 480 browser viewports with scrolling dialogs. This is a browser interface, not a native touchscreen runtime.

## Guided scale calibration

Open **Settings → Calibration** (`#settings/calibration`). The old `#calibration` URL redirects there. The three steps are zero, known reference and independent verification. Zero must succeed in the current browser session before span calibration is enabled, even if a previous zero is stored on the device. Session lock/reload and **Start again** reset that browser progress; they do not erase device calibration.

Zero requires fresh readings and `scale.tare_ready:true`. Calibration requires fresh readings, `scale.calibration_ready:true`, `scale.zero_set:true`, a successful zero in this browser flow and a numeric reference from 100 to 2,000 g. The separate readiness flags let the firmware collect a longer raw sample window for calibration than for ordinary weighing. Successful saves show explicit feedback; the live check waits for fresh samples and does not claim accuracy verification.

A compatible 20 kg load cell uses the same reference-weight workflow. Its capacity is not an input. The normal gross-spool limit remains 2 kg. The demo models zero state, stale readings, motion rejection and reference limits without accessing hardware. See the [user procedure](../docs/WEB-INTERFACE.md#calibrate-the-scale).

## Screenless and integrated-touch first setup

The headless build advertises its temporary setup network without a password. The page prominently warns **Temporary setup network has no password; use it in a trusted location** and states that Wi-Fi credentials use local HTTP.

Join that network and open `http://192.168.4.1`. Unconfigured headless, OLED and integrated-touch devices open **Connect your scale** before the normal app. Select a nearby 2.4 GHz network, enter its password, and leave **Protect access with a PIN** checked to choose and confirm an eight-digit PIN. Uncheck it for local access without a PIN. In that mode, anyone on the same network can manage the device; private APIs still use browser session tokens.

The selector scans asynchronously, deduplicates SSIDs by strongest signal and marks unsupported security. **Hidden or other network** permits exact manual SSID entry and an explicit open-network choice. Failed or empty scans retain a manual path and a retry button. Settings uses the same selector and preserves the current network even when it is absent from the latest scan.

Select **Connect & finish setup** on the temporary setup network. No physical button press is required. The firmware accepts initial setup only while unconfigured and disconnected from station Wi-Fi, through its trusted setup AP origin. Wi-Fi and the PIN choice are saved together before connecting; the result page distinguishes saved settings from a confirmed LAN connection. The setup AP closes after ten minutes, or 60 seconds after connecting. If credentials fail, use Settings while the AP remains available. Hold BOOT for eight seconds while running to reset Wi-Fi and access setup; inventory, calibration and Spoolman settings remain stored. Holding BOOT during power-on or RST enters programming mode instead.

Already configured devices retain their existing PIN after upgrading. Even a legacy device with a PIN but no saved Wi-Fi remains protected: pair, then use Settings. Only a fresh device or a physical setup reset enters the new wizard. The legacy ST7789 display profile retains its generated-PIN onboarding.

## API contract

Responses are JSON. A failed request uses a non-success HTTP status and `{ "error": "Human-readable reason" }`. Pairing returns `{ "token": "..." }`. Private requests carry `Authorization: Bearer <token>`. Initial setup and Wi-Fi scanning on the trusted unconfigured setup AP do not require a token. Spoolman discovery requires a paired session. HTTP 401 clears private state; PIN-protected devices pair again, while explicitly configured no-PIN devices reacquire a local session. The firmware must independently authenticate and validate every mutation.

| Endpoint | Purpose |
| --- | --- |
| `GET /api/status` | Public device, scale, NFC, active spool ID, network and Spoolman status |
| `POST /api/session` | Pair with `{pin}`; explicitly configured no-PIN devices accept `{}` from their trusted local origin |
| `POST /api/setup` | First setup: `{wifi_ssid,wifi_password,wifi_open,pin_required,pin?}` on the trusted, unconfigured setup AP; returns `{ok:true,token,reconnect_required:true}` |
| `POST /api/wifi/scan`, `GET /api/wifi/scan` | Start/poll discovery; `{state:"scanning"\|"ready"\|"failed",networks:[{ssid,rssi,secure,channel,supported}]}`; paired session or trusted unconfigured setup AP |
| `GET /api/spools`, `POST /api/spools` | Read authoritative inventory and recovery drafts; create a server spool or save a local profile |
| `POST /api/spoolman/discover`, `GET /api/spoolman/discover` | Start/poll bounded LAN discovery; return candidates for user selection |
| `GET /api/config`, `POST /api/config` | Read non-secret settings; save device, Wi-Fi and Spoolman configuration |
| `POST /api/active-spool` | Select `{id}` for this weighing session |
| `POST /api/spools/remove` | Remove `{id,confirm:true}` locally, or archive a linked spool in Spoolman; return `{ok:true,id,archived,message}` |
| `POST /api/spools/tag` | Link `{id,expected_uid,confirm:true}` after rechecking the current single tag, or unlink `{id,expected_uid,unlink:true,confirm:true}` after checking the saved ID; return `{ok:true,spool,message}` without writing tag memory |
| `POST /api/measurements` | Save `{spool_id, confirm:true}` locally |
| `POST /api/scale/tare` | Zero the empty cradle with `{}` |
| `POST /api/scale/calibrate` | Set span using `{reference_g}` |
| `POST /api/nfc/read` | Read the current tag with `{}` |
| `POST /api/nfc/write` | Write `{spool_id, expected_uid, confirm:true}` and verify read-back |
| `POST /api/spoolman/test` | Test the saved server with `{}` |
| `POST /api/spoolman/sync` | Create or update the linked spool using `{spool_id, confirm:true}` |

`/api/status` must include `mode:"device"`, `device.name`, `scale.{gross_g,stable,calibrated,fresh,zero_set,tare_ready,calibration_ready}`, `nfc.{present,uid,type,writable}`, `active_spool_id`, `network.{connected,ip}` and `spoolman.configured`. The UI also displays optional `nfc.last_message`, `nfc.last_error`, `spoolman.pending`, `spoolman.last_message` and `spoolman.last_error`. Current status identifies `device.variant` as `headless`, `oled-led`, `touch-8048` or `st7789`. Explicit `setup.required`, `setup.complete` and `auth.pin_required` determine onboarding and access. `auth.invalid:true` requires physical recovery and never enables no-PIN access. The headless, OLED and touch profiles use the open setup flow and additionally supply `setup.{ap_active,open_network,via_ap,can_submit,physical_confirmation_required,ap_seconds,warning}`. The UI reflects `can_submit` and does not show a physical-enrollment step. A browser refresh failure or an ADC `fresh:false` state suppresses live weight. The browser polls status every 500 ms while visible, without overlapping status requests. Private inventory/config polling occurs separately every 60 seconds or after an explicit refresh or mutation; a failed private load also backs off. Hidden tabs stop status polling until visible again.

Spool records contain `id`, `name`, `brand`, `material`, six-digit hex `color`, nullable `empty_weight_g`, required `initial_weight_g`, `density_g_cm3`, `diameter_mm`, optional `spoolman_id` and optional last saved `remaining_weight_g`. The agreed inventory limits are name 64 UTF-8 bytes, brand 60 bytes, material 30 bytes on-device, tare 0–2,000 g or unknown, initial filament 1–5,000 g, density 0.1–5 g/cm³ and diameter 0.5–5 mm. Larger nominal inventory values do not increase the scale's 2,000 g gross operating target.

Name, brand and material are required. An unknown brand can be entered as Generic. Reference masses for calibration must be 100–2,000 g. Requests use bounded timeouts, extended to 30 seconds for tag writes and Spoolman sync, and 20 seconds for NFC reads and server tests. Polling pauses during mutations. A timeout does not prove an action failed; the UI asks the operator to inspect the result before retrying.

Saving a measurement or syncing requires a current, calibrated, stable reading from 20–2,000 g, the same active spool, a known nonnegative tare, and remaining weight between zero and the recorded initial filament mass. Remaining weight is gross minus the known empty spool weight. The UI keeps the same remaining-weight basis while a valid selected spool settles, labels it as an estimate and disables saving. It never substitutes a saved measurement for a live reading. For a new, full spool only, the explicit **New, full spool** choice estimates empty weight as stable gross minus the declared initial filament mass (default 1,000 g). The UI labels this as an estimate, rejects negative results and sends `infer_empty_from_measurement:true` so the backend rechecks the actual reading before saving. Used spools never infer tare from nominal capacity. The firmware rechecks the current reading when an action is submitted.

When Spoolman is configured, the inventory contains only server records. `GET /api/spools` returns `{spools,drafts,inventory:{source,configured,stale,cached,error,limit,age_seconds}}`. Server errors stay distinct from an empty inventory and local profiles never silently replace missing server data. Remote profiles are marked `source:"spoolman",metadata_readonly:true`; edit their material metadata on the server. Recoverable local drafts are listed separately.

**Settings → Connections** offers **Find on network**, a manual URL, **Save server**, and **Test saved server**. Saving only the server address does not submit unsaved Wi-Fi settings. Discovery returns `{state:"searching"|"ready"|"failed",candidates:[{url,label}],message}` and never saves candidates automatically. It is bounded to the device’s local subnet slice and default Spoolman port, so an unfound server still has a manual address path.

An unlinked Spoolman profile can create vendor, filament and spool records after confirmation. The UI includes editable density and diameter for creation, and manual ID linking for an existing spool. Creation posts `confirm:true` and `spool_condition:"full"|"used"`. Full is an explicit user assertion; used requires current stable measured remaining weight. A failed or ambiguous create remains an error; the UI never retries automatically. Error responses may carry `{spool,draft:true,recovery_required}`; the editor retains that draft ID rather than creating a duplicate. Resolve uncertain creation by checking the server and linking its existing spool ID. The current firmware reports pending/manual retry status, not a durable automatic queue.

NFC writes target the project's own `application/vnd.spoolancer+json` schema 1 inventory record. This is not an OpenSpool or stock-printer tag format. Original Bambu/Snapmaker tags stay read-only; detecting a UID does not mean their proprietary material data has been decoded. The firmware should report write success only after matching read-back, and explain partial writes on failure.

Each inventory card offers **Add RFID** or **Change RFID** after registration. The dialog keeps the chosen profile separate from the active scale selection, scans via `POST /api/nfc/read`, previews the UID and posts `{id,expected_uid,confirm:true}` to `/api/spools/tag`. The firmware reads the physical UID again before linking. Read-only tags and unreadable contents can still identify a spool when a single UID is detected. The association stays on this scale, including for a Spoolman profile. It does not change material metadata or write tag memory. Missing, swapped and duplicate tags stay errors. IDs are opaque strings: legacy NFC-A hex IDs and new `B:`, `F:` and `J:` prefixes remain intact through preview, comparison and submission. The web interface never strips a protocol prefix or treats identities from different protocols as equivalent. Embedded tag metadata alone does not select a spool after its local association is removed. Selection requires an explicit UID binding; a successful verified inventory-tag write also saves that binding. Linking a new UID replaces the previous association for that spool. **Write inventory tag** remains a separate action for unlocked NTAG213/215/216 tags with enough space for the serialized record. The PN532 can identify NFC-A, NFC-B, FeliCa and Jewel tags; NFC Type 5 is unsupported. Some cards randomize their ID between reads and cannot provide a persistent spool identity. Recognizing an ID does not provide access to protected memory.

**Remove** on each card confirms the spool name and destination. A local unlinked record is deleted with its saved measurements and UID association. A linked record is archived in Spoolman, preserving server history; it cannot be silently deleted locally when Spoolman is disabled or unreachable. New remote changes require a fresh inventory. An ambiguous archive returns `recovery_required:true` and `spool.removal_pending:true`; the card then offers **Check archive status** through the same removal endpoint. If a refreshed server inventory omits the archived spool, its pending journal remains under **Inventory recovery** with **Check & finish archive**. Only that explicit recovery action can use a hidden draft, and it requires a positive Spoolman ID. Editing, linking a tag and ordinary removal cannot use hidden records. Hidden server records with a retained RFID association and no pending journal are flagged `inactive_tag_link:true`. They offer only **Unlink RFID**, naming the spool and stored tag ID. This posts `{id,expected_uid,unlink:true,confirm:true}` to `/api/spools/tag`, clears only the local association, and needs neither a physical tag nor a fresh server response. A changed stored ID is rejected. Clean local drafts with no server ID or pending journal offer **Discard draft** through `/api/spools/remove`; uncertain creation journals cannot be discarded. Draft lookup is limited to the matching operation and record kind. If archive recovery confirms that the server record was already deleted, the response reports `archived:false,removed_missing:true` and the interface displays the server result without claiming that history was preserved. Other pending operations must be resolved first. Neither removal nor archiving erases the physical tag.

Configuration accepts `device_name`, `wifi_ssid`, `spoolman_url`, optional `wifi_password`, and explicit `wifi_open`. An omitted or blank password keeps the saved one only for the unchanged SSID. A different secured network requires a new password; an open network requires `wifi_open:true`. A background scan cannot change the selected network’s saved security. Passphrases accept 8–63 UTF-8 bytes or a 64-digit hexadecimal key. SSIDs retain leading/trailing spaces and are limited to 32 UTF-8 bytes. GET returns only `has_wifi_password`. Wi-Fi changes return `{ok:true,reconnect_required:true,ap_ssid,ip,message}` before reconnecting. The screenless UI describes the timed setup network and physical recovery procedure, not an automatic access-point fallback. The UI explains how to reopen the device and never displays the saved password. Configuration and editor input remain intact after failed requests, except secrets cleared when locking or pairing.

This firmware accepts an HTTP Spoolman base URL with a numeric IPv4 address, such as `http://192.168.1.50:7912`. Port is optional and defaults to 80; an optional base path is supported. Hostnames, HTTPS, credentials, query and fragment are rejected. Server authentication is not implemented. A URL change can be rejected while existing spool links need migration; the UI keeps the entered value and displays the device's reason.

## Validation

```sh
node --test web/tests/*.test.js
```

These dependency-free tests cover unknown versus zero tare, stale and uncalibrated readings, physical mass bounds, inconsistent profiles, UID replacement, local saving without Spoolman, private endpoint access, explicit simulated failures, local removal and remote archive recovery, post-registration RFID binding, missing/swapped/duplicate/read-only tag checks, color/material normalization, appearance persistence, AP-only web setup and expiry, optional-PIN access, network discovery, authoritative inventory, explicit full-spool inference, uncertain-creation recovery and Wi-Fi credential validation. They validate browser logic and the deterministic preview adapter only. Firmware compilation and actual device tests are separate checks.

## Icon credits

Lucide icons are embedded as an SVG symbol set in `index.html`, with licenses included in that file and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). No icon font or external request is required. The existing Spoolancer brand mark is unchanged.


## Hardware tests

The route `#settings/hardware` contains authenticated NFC, LED and scale test controls. They call `POST /api/diagnostics {component}` and show structured `pass`, `warning`, `fail` or `observe` outcomes. A LED `observe` result requires visual confirmation; `diagnostics.led_test_remaining_ms` in regular status drives the 12-second countdown and disables overlapping tests. No sensor calibration or inventory mutation is performed.

The five-segment preview uses `ledSegments(percent)`: cyan for each completed 20%, and red/orange/yellow/green for the four 5% partial bands. Firmware uses the same literal color values. Demo scenarios cover missing reader/samples, no/multiple tags, uncalibrated/moving readings and automatic LED-test expiry. These fixtures do not establish physical emitted colors, antenna range or measurement accuracy.
