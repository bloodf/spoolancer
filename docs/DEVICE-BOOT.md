# USB flash and startup record

Recorded 27 October 2026 on one owned standalone ESP32-S3 N16R8 board. Environment: `spoolancer-oled-led`; application: `0.5.0-prototype`. This is a controller startup record, not an assembled scale qualification.

## Backup and upload

A full 16,777,216-byte flash copy was read before writing and stored privately outside the repository. A backup restore was not tested. The backup, device identifiers, credentials and raw flash contents are not public release assets.

The programming tool identified ESP32-S3 revision 0.2, 16 MB flash and 8 MB embedded PSRAM. Both application and web filesystem uploads succeeded with tool-reported hash verification. No erase-all command was used. The asset image targets `0x810000`; the separate inventory partition starts at `0xf00000`. This first installation did not test retention of an existing Spoolancer inventory.

| Uploaded artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `bootloader.bin` | 15,104 | `2a71d69b471e20c2bac7fb469f3c6a807b3ebee780e348e5889db0da849ca363` |
| `partitions.bin` | 3,072 | `738628ef4c5163a1a1a838f58e78510c9c7c6fd9fab282e6f67969fdfdd44492` |
| `firmware.bin` | 960,944 | `8599494de17abdddf88b057847ffe2d7d5b39b217c49f8530455b94da8a6df22` |
| `littlefs.bin` | 7,274,496 | `042314748a63f32f467d41fab9836b717d086f58f8eb8cf965de8f6ecc82db59` |

These identify the local artifacts actually uploaded. Public packages distribute source and CAD, not these compiled binaries. A rebuild can differ because toolchain versions and build metadata affect binary output.

## Observed startup

After normal reset, a bounded serial capture reported:

```text
Flash: 16777216 bytes; PSRAM: 8386279 bytes
Temporary OPEN setup Wi-Fi: [device-specific name omitted]
Status OLED: not found; web remains available (address 0x3c)
Spoolancer ready. Variant: oled-led; assets: ready; inventory: ready; setup AP: [device-specific name omitted]; address: 192.168.4.1
```

The flash count is exactly 16 MiB; the reported PSRAM count is approximately 8 MiB. The application reached its ready line and mounted both filesystems. No panic or reset loop was observed during the short startup captures. This was not a prolonged stability test.

The OLED did not acknowledge at the configured address. Its presence, wiring and controller identity were not established; absence at one address does not diagnose the cause. No display image or physical LED output was observed.

## OLED initialization repair, 30 October 2026

After the peripherals were soldered, the same board advertised its setup network but stopped before the ready message. A 45-second serial capture did not reach OLED status or application readiness. A diagnostic application then showed that the initial LED transfer completed and the OLED acknowledged at `0x3c`, but execution stopped inside `oled.begin()`.

The firmware had initialized `Wire` on GPIO4/5 and also supplied those pins to the U8g2 constructor. In the installed U8g2/Arduino combination, display initialization reconfigured those explicit pins as ordinary outputs; the subsequent `Wire.begin()` returned early because the bus was already initialized. The corrected constructors leave pin configuration to the existing `Wire.begin(4, 5)`. The SH1106 alternative uses the same correction. Startup markers now identify the last completed stage if a peripheral blocks initialization.

The corrected OLED/LED application was written at `0x10000` with flash hash verification. No filesystem, partition table, NVS or inventory image was written. The existing calibration webpage remains installed. The uploaded application is 961,456 bytes with SHA-256 `70aa7361c0f18ae4940c30c2a3c0642776633982ebcc6efef165748f43a1c925`.

The post-fix serial capture reported:

```text
Status UI: initial LED transfer complete
Status UI: probing OLED on SDA4/SCL5
Status UI: OLED acknowledged; initializing display
Status OLED: I2C acknowledged; verify controller and image (address 0x3c)
Startup: status UI ready
Spoolancer ready. Variant: oled-led; assets: ready; inventory: ready; setup AP: [omitted]; address: 192.168.4.1
```

All five firmware profiles compiled. This establishes that the observed initialization stall cleared on this board. The owner then confirmed that the OLED and webpage work. That report establishes basic visible display and browser access; physical LED colors, detailed page actions, safe power wiring and weighing accuracy remain separate checks. The owner reported USB-only power, direct GPIO6-to-DIN wiring, and the converter output still connected to the board's 5 V rail; that arrangement differs from the power/buffer circuit in [the wiring guide](../WIRING.md#10-usb-programming-and-service).

## Wi-Fi setup update, 30 October 2026

The owner requested the new firmware and confirmed USB-only power, with the 12 V adapter unplugged and the buck converter's OUT+ disconnected. The connected programming bridge was rechecked; the tool identified ESP32-S3 revision 0.2, 16 MB flash and 8 MB embedded PSRAM.

The `spoolancer-oled-led` application and web assets were rebuilt from the working tree based on `6304c2c`, including the new first-run Wi-Fi setup changes. The filesystem inspection passed: the image contained exactly the five current device web files and targeted the public assets partition. The desktop demo was not packaged.

A fresh full 16,777,216-byte flash backup, exact upload images and verification logs were stored privately outside the repository. The backup's size and SHA-256 were recorded; restoration was not tested. Device identifiers, credentials and raw flash contents are excluded from this public record.

Before writing, the device's bootloader, partition table and app0 boot metadata matched the expected local artifacts. Only the application at `0x10000` and public web assets at `0x810000` were written, at 460800 baud. The tool reported hash verification for both images. No erase-all operation or bootloader, partition-table, OTA, NVS or inventory write was requested.

| Uploaded artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `firmware.bin` | 972,592 | `1474859082b35a8d21be64c722c73f5ecc055092e81ddfec0dec401c1a9660b6` |
| `littlefs.bin` | 7,274,496 | `636cedd25adc1dfdfb159506ea4833ed84f8f8b7e521c13bf95c0979cb7bd364` |

Before normal startup, readback of the first 64 KiB (`0x0–0xffff`, including NVS) and the complete 1 MiB inventory partition (`0xf00000–0xffffff`) matched the fresh backup byte-for-byte. This verifies that the flash operation preserved stored configuration, pairing, calibration and inventory bytes. The new firmware may subsequently migrate legacy authentication metadata during startup; the byte comparison was deliberately made before that application behavior.

After a normal reset, the serial capture reported:

```text
Flash: 16777216 bytes; PSRAM: 8386279 bytes
Status UI: initial LED transfer complete
Status UI: OLED acknowledged; initializing display
Status OLED: I2C acknowledged; verify controller and image (address 0x3c)
Startup: status UI ready
Spoolancer ready. Variant: oled-led; assets: ready; inventory: ready; setup AP: [omitted]; address: 192.168.4.1
```

The 30-second startup observation reached one ready message without a detected panic, brownout or repeated ready sequence. This confirms successful installation and basic startup on this controller. First-run setup, Wi-Fi scanning/association, optional-PIN behavior, calibration and physical peripheral behavior still require their own device checks.

## Measurement and Spoolman update, 30 October 2026

The owner requested flashing the completed measurement, setup, Spoolman and interface changes. The connected serial bridge was rechecked and reported ESP32-S3 revision 0.2, 16 MB flash and 8 MB embedded PSRAM. The `spoolancer-oled-led` application and web filesystem were rebuilt, and the asset checker confirmed exactly the five current runtime files.

A fresh full 16,777,216-byte backup, exact upload images, source hashes and logs were stored privately outside the repository. Backup size and SHA-256 were checked. The device bootloader, partition table and app0 OTA metadata matched the expected artifacts before writing. Backup restoration was not tested.

Only the application at `0x10000` and public assets at `0x810000` were written at 460800 baud. Both the write operation's hash checks and a separate `verify_flash` operation passed. No erase-all, bootloader, partition-table, NVS, OTA or inventory write was requested. Before application startup, the first 64 KiB and complete 1 MiB inventory partition were read back and matched the fresh backup byte-for-byte.

| Uploaded artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `firmware.bin` | 966,880 | `1e3c4c5c16e8e50ead8aacd697a47f36552b9b248054aa9e772c8561657046ce` |
| `littlefs.bin` | 7,274,496 | `405a4e54866b353e80f4cfdffd9b20b364de972336360e0697ea114d0c3e7b4a` |

After normal reset, a 30-second serial capture reached one OLED/LED ready message, acknowledged the OLED and reported both assets and inventory ready. No panic, brownout or repeated startup was detected. The initial ready line preceded Wi-Fi association; a subsequent live HTTP check confirmed that the device joined its saved network.

The live `/api/status` response reported `mode:device`, variant `oled-led`, connected Wi-Fi, fresh calibrated scale readings and `setup.physical_confirmation_required:false`. All five web files served by the ESP matched the current source bytes exactly. This verifies the installed application, actual device-hosted interface and retained network access. It does not prove weighing accuracy, physical OLED appearance, NFC actions or Spoolman discovery/synchronization.

## NFC identification and inventory actions update, 30 October 2026

The connected `spoolancer-oled-led` controller received the completed NFC identification and inventory action changes from the working tree based on `6304c2c`. All five firmware profiles compiled, all five asset images passed exact-file extraction checks, native sanitizer tests passed, and the web behavior suite passed 66 tests before upload.

A new complete 16,777,216-byte flash backup was saved privately and checked for size and SHA-256. The existing bootloader, partition table and app0 boot metadata matched the expected artifacts. Only the application at `0x10000` and web assets at `0x810000` were written at 460800 baud. Write-time hash checks and a separate `verify_flash` operation passed. The first 64 KiB and complete 1 MiB inventory partition then matched the backup byte-for-byte before application startup. No erase-all, configuration reset or inventory/tag mutation was requested. Backup restoration was not tested.

| Uploaded artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `firmware.bin` | 980,752 | `753d05a8d3ba38190bb649976ecf6d943f6bc820258fcd95d3cdb553bd508ca6` |
| `littlefs.bin` | 7,274,496 | `f9957b137fd5c40b31e6c5b0325a5fe14fa6f9a908fedda0095f64880fa3427f` |

A 30-second serial capture reached exactly one OLED/LED ready message, acknowledged the OLED and mounted both filesystems, without panic, brownout or repeated startup. The initial ready line preceded network association. A subsequent live HTTP check confirmed connected Wi-Fi, ready storage, OLED availability, calibrated/fresh scale data and no initial-setup button requirement. All five web assets served by the actual device matched the current source bytes exactly.

This confirms installation, storage retention, startup and device-hosted assets. No physical tag association/write, inventory deletion, Spoolman archive or live server recovery was performed during verification. The expanded tag-family and server-action results remain simulated/source evidence until those bench checks are completed.

## Hardware diagnostics and numeric JSON update, 30 October 2026

The OLED/LED controller received the Hardware tests panel, state animations and colored 5% partial weight segments. Live verification of the first upload exposed a pre-existing numeric JSON error: fresh/calibrated HX711 status arrived without the gross-weight field. A reproduction against the actual installed ArduinoJson library confirmed that conversion of an absent member to JsonVariant discarded assignment. The helper now preserves the member proxy. A real-library sanitizer test covers finite values, unknown/null fields and spool-weight serialization/reload, and CI runs it after firmware dependencies are installed.

All five applications and all five exact runtime asset images built. The web suite passed 72 tests and the native sanitizer suite passed. After the correction, a fresh full 16,777,216-byte backup was saved privately with integrity metadata. The bootloader, partition table and app0 selection matched the expected artifacts. Only application and public asset partitions were written, with write-time hash verification and separate flash verification. Before startup, the first 64 KiB and complete 1 MiB inventory partition matched the fresh backup byte-for-byte. Backup restoration was not tested.

| Final uploaded artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `firmware.bin` | 986,912 | `72a1ed163edece5a86d30967054815ebf060e643a048fc46a84c246ab4877f94` |
| `littlefs.bin` | 7,274,496 | `2413c724339f07fa90171b304be9de1fe3866fff724595fda5218f4819386bca` |

A 30-second capture reached one ready message, acknowledged the OLED and mounted both filesystems, without a detected panic, brownout or startup loop. Live HTTP verified connected Wi-Fi, retained calibration, fresh scale samples, an actual numeric gross-weight field and all five exact current served assets.

Authenticated device requests rejected missing sessions and unknown diagnostic components. The scale diagnostic returned `pass` for fresh stable samples and numeric grams. The NFC diagnostic returned `fail`, with PN532 not responding; seated tag reading/writing remains unverified. The LED diagnostic returned `observe`; its countdown was positive after starting and zero after 13 seconds. Stored configuration was unchanged. These requests did not zero/calibrate, modify a spool or write tag memory.

The actual device-hosted Hardware tests page was then exercised through the browser. Scale displayed numeric gross weight and a passing signal result; NFC showed the no-response message. No browser errors or warnings were observed. The existing calibration produced a negative gross reading in this setup; no empty-cradle zero or reference-weight accuracy procedure was performed. A passing signal is not a passing accuracy check. [Live panel screenshot](web-hardware-device.png). Physical emitted LED colors/order still require visual confirmation.

## Still untested

- New first-run enrollment, BOOT timing/access, Wi-Fi scanning and recovery, and authenticated HTTP mutations. Existing-network association, public status and served assets passed in the latest update.
- Detailed OLED states, LED color/order/current and combined peripheral power behavior.
- The 12 V jack, buck converter, internal power plug and enclosure fit.
- HX711 readings, calibration, independent known weights and mechanical stability.
- PN532 operation, seated tags, writes/read-back and stock-printer tag identity.
- Real Spoolman create/link/update and uncertain-response recovery.

Use [the build guide](BUILD-GUIDE.md), [DC power instructions](DC-POWER.md) and [hardware validation report](HARDWARE-VALIDATION.md) for the next checks.
