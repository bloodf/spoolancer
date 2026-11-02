# Spoolancer interface revision

## Direction

Product: a local bench scale for identifying spools and updating filament inventory.
Character: precise, calm, practical.
World: a workshop measuring instrument, using the existing green brand and neutral surfaces.
Moments: the current weight, selected spool and confirmed save receive emphasis.
Signature: gross, empty spool and remaining filament are clearly separated, with the inventory source always visible.

## Surface classification

| Route | Class | User outcome | Appropriate persuasion |
| --- | --- | --- | --- |
| Initial setup | C: trust | Connect Wi-Fi and choose access protection without opening the case | Explain choices and failure recovery |
| Scale | B: work | Identify and weigh a spool accurately | None; show readiness and save outcome |
| Inventory | B: work | Find the correct spool in the authoritative inventory | None; show source and freshness |
| New spool | C: write | Register accurate material and tare data | Explain full-spool inference and where data is saved |
| Settings / Spoolman | C: trust | Discover or configure a server and verify connection | Explain unverified/offline states |
| Settings / Calibration | C: trust | Save a valid reference without corrupting calibration | Show prerequisites and verification |

## Before changes

- Settings mixes device, network and server configuration in one long form. Spoolman appears below the first viewport at 850 px width.
- Calibration is duplicated in primary navigation and a settings card.
- Initial setup requires physical BOOT access after the enclosure is assembled.
- Inventory lacks an authoritative source/freshness indicator for remote mode.
- New-spool form cannot explicitly derive tare from a full spool's declared initial filament weight.
- Brand and themes already exist. Keep them, reuse the existing plain HTML/CSS/JS controls and package offline assets.

## Existing primitives

| Need | Exists at |
| --- | --- |
| Themes, surfaces and controls | web/styles.css |
| Weight display and progress | web/index.html and web/app.js |
| Material and color selection | web/index.html and web/logic.js |
| Navigation, dialogs and notifications | web/app.js |
| Fixture states | web/demo.js |

## Screen seen

| Stage | Route | Viewport | Data state | Observation |
| --- | --- | --- | --- | --- |
| Before | Settings | 850 px | Demo, view only | Large combined form; server controls below fold; duplicate calibration route |
| Before | Setup | 850 px | Demo, first setup | Wi-Fi and optional PIN are present; physical BOOT instruction blocks enclosure-friendly setup |
| After | Settings | 850 px | Demo, PIN off, light | Spoolman is the first form section; heading at Y=448 px and Save server at Y=788 px including the demo banner; calibration is a secondary route |
| After | Scale | 1280 px | Demo, PIN off, dark | Remaining weight dominates; gross weight and spool tare remain separate; no console errors or warnings |
| After | Setup | 390 px | Demo, open Wi-Fi, PIN off | Network selection and setup complete without a BOOT gesture; saved and connected states are separate |
| After | New spool | 390 px | Demo, full spool | 1,248 g gross minus declared 1,000 g filament previews 248 g estimated empty weight and saves to the simulated server |
| After | Inventory | 320–390 px | Demo, empty / offline cached / offline without cache | Empty inventory, stale server cache and unavailable inventory have separate messages; no local-inventory fallback |
| After | Settings | 320 px | Demo under the firmware CSP | Server-only saving preserves unsaved device/Wi-Fi fields; discovery offers manual entry when no server is found |
| After | Scale | 320 px | Demo under the firmware CSP, moving spool | Remaining weight stays at 592 g while Settling; it no longer switches to 842 g gross; save is disabled |

At the same 850 px width, the original server section was below the first viewport. It now precedes the device/Wi-Fi form. The exact after positions above are DOM measurements; no exact before coordinate was retained, so no pixel-reduction percentage is claimed. Checked 320 px and 390 px views had zero horizontal document overflow.

The native editor dialog closes with Escape and returns focus to Add a spool. The old `#calibration` address redirects to `#settings/calibration`. A failed discovery leaves the manual server URL field available.

Screenshots: [scale, dark](web-scale-updated.png), [settings, light](web-settings-updated.png), [first setup](web-setup.png). Each is a localhost preview with simulated data, not a device screenshot.

## Verification

- 48 web behavior tests pass. Native C++ tests pass with AddressSanitizer and UndefinedBehaviorSanitizer, including scale response, setup rules, remote inventory recovery, HTTP framing and NFC refresh/write behavior. Storage guards pass.
- All five firmware profiles and all five LittleFS images build. The filesystem checker unpacks each image and verifies the exact five current runtime assets, image size and separate private-inventory partition.
- A synthetic 10-sample/second 1,000 g step becomes stable after 700 ms, versus the previous 2,000 ms window. This does not measure the installed load cell, noise, cable forces or end-to-end device latency. Tare and calibration retain a separate 20-sample raw-data gate.
- The Unslop detector reported zero anti-pattern matches in its regex scan. Three border/shadow advisories refer to mutually exclusive native dialogs, not repeated cards. Element-dependent rules did not run, so this is not a full accessibility audit. The copy scanner's sole match is the arithmetic subtraction in `Date.now() - state.lastSeen`, not user-facing punctuation. The Visant source scanners did not recognize this plain HTML application; browser checks supply the UI evidence instead.

The interface checks above used simulated data. A subsequent owner-requested [device flash](DEVICE-BOOT.md#measurement-and-spoolman-update-30-october-2026) verified installation, preserved storage, startup, connected Wi-Fi and the exact served web assets. Real Spoolman writes and physical weighing accuracy remain untested.

## Sources and scope

Applied guidance from [Unslop by Damir Sokolovsky](https://github.com/heydamir/unslop), CC BY 4.0, and [Visant skills](https://github.com/pedrojaques99/visant-skills-public), specifically frontend, work/trust surfaces, copy and refine. External skill files are inspection aids outside the repository, not application dependencies.

Browser fixture checks establish UI behavior only. Physical weighing, installed-device timing and discovery against a real Spoolman server require separate bench/network evidence.

## Inventory RFID and removal update, 30 October 2026

The inventory now offers Add RFID / Change RFID without selecting the spool first, with the chosen profile and scanned identifier visible before confirmation. Removing a local spool and archiving a Spoolman spool use distinct destination text. Recovery entries remain outside the server's active inventory: pending archives can be completed, inactive bindings can be unlinked without a tag present, and clean local drafts can be discarded. Pending server registrations retain their recovery record.

The final web behavior suite passes 66 tests, including opaque A/B/F/J identifiers, duplicates, tag changes, unreadable contents, local deletion, remote archive uncertainty, hidden recovery records, unlink/ID reuse and protected creation journals. Browser fixture checks completed local binding on an initially unselected PETG spool, local removal, remote archiving, hidden archive recovery, inactive-link removal, clean-draft discard and a FeliCa link. The pending-registration view exposes Review draft without Discard. These actions used only the explicit localhost simulator.

At 320 px in the light theme, the scanned FeliCa ID, target profile and confirmation remain usable; document and dialog have no horizontal overflow (document 320/320 px; dialog 280/280 px). The 810 px dialog scrolls its 871 px content while keeping actions reachable. At 390 px the dark inventory and removal flow were also checked. Viewport overrides were reset after testing.

The expanded physical tag families, protected stock-printer tags, actual writes and live Spoolman recovery remain bench/integration checks; simulated successful actions do not establish those outcomes.

The deleted-server-record recovery fixture also completed with a message that distinguishes confirmed absence from archiving and does not claim history restoration. No browser errors or warnings were observed. Screenshots: [inventory with RFID actions](web-inventory-rfid.png) and [RFID target/identifier confirmation](web-rfid-link.png), both using simulated data.

The [subsequent firmware upload](DEVICE-BOOT.md#nfc-identification-and-inventory-actions-update-30-october-2026) verified the installed images, preserved configuration/inventory bytes, normal startup and exact device-served web assets. Actual tag and Spoolman mutations were not run on the owner's data.

## Hardware tests and colored weight segments, 30 October 2026

Settings has a Hardware tests tab beside Connections and Calibration. NFC rechecks reader communication and a tag identifier; scale checks fresh samples without saving calibration; the LED test runs a bounded 12-second color/pixel/bar sequence and requires visual observation. Test results distinguish a passing signal, an absent component, no tag, movement/calibration needed and visual confirmation. Test controls require a session, and the five-pixel test is disabled on unsupported firmware profiles.

Five cyan segments represent 100% of declared initial filament after subtracting empty-spool weight. A partial segment has 5% color bands. At 85/90/95%, four pixels remain cyan while the fifth becomes red/orange/yellow, then green for 96–99% and cyan at 100%. Hardware status animations cover setup/connection, empty cradle/selection, settling/calibration/tare, stale/invalid weight, explicit operations and expiring results. NFC and cooperative HTTP waits continue to render animations.

The 72 web tests pass, including color boundaries, authentication, invalid commands, preserved configuration/profiles, diagnostic failure states and automatic LED countdown expiry. Native sanitizer tests cover color boundaries, individual-pixel test order, animation changes, network hints, event expiry/rollover and reprobe after reader disconnection/reconnection. All five firmware profiles and exact asset images build.

Browser simulation checks exercised all three commands, the 12-second countdown and return to normal controls, missing-reader and missing-sensor errors, light/dark presentation and a 320 px layout with no horizontal overflow. An initial integration issue where the new panel was not refreshed from the normal render loop was found and fixed in browser QA. The temporary viewport override was reset. No console warnings or errors were observed. [Hardware tests screenshot](web-hardware-tests.png) uses simulated data.

Protocol/output tests do not prove emitted LED colors, RF range or weighing accuracy. Those require visual checks and independent known weights.


Live verification exposed a pre-existing ArduinoJson boundary error: the HX711 reported fresh/calibrated samples, but the numeric gross-weight field was absent from the actual HTTP JSON. A standalone reproduction using the installed ArduinoJson library confirmed that passing an absent member as JsonVariant discarded its assignment. The shared helper now preserves the member proxy, covering scale responses, diagnostic numbers and spool persistence. A real-library sanitizer test checks finite values, explicit unknown/null fields and inventory round-trips; CI runs it after dependencies are installed. Physical weight accuracy is still separate from a numeric value arriving.

After the final flash, actual device-hosted browser checks showed numeric gross grams, a passing scale-signal result and a failed reader handshake with the PN532 no-response message. The LED diagnostic API started its 12-second pattern and expired automatically; emitted light was not observed. [Live device panel](web-hardware-device.png). The existing calibration showed a negative gross value, so no physical zero/accuracy claim is made. Configuration/inventory retention, installation hashes and exact served assets passed in the [device record](DEVICE-BOOT.md#hardware-diagnostics-and-numeric-json-update-30-october-2026).
