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

## Still untested

- PIN enrollment, BOOT timing/access, Wi-Fi association and recovery, and HTTP actions beyond opening the page.
- Detailed OLED states, LED color/order/current and combined peripheral power behavior.
- The 12 V jack, buck converter, internal power plug and enclosure fit.
- HX711 readings, calibration, independent known weights and mechanical stability.
- PN532 operation, seated tags, writes/read-back and stock-printer tag identity.
- Real Spoolman create/link/update and uncertain-response recovery.

Use [the build guide](BUILD-GUIDE.md), [DC power instructions](DC-POWER.md) and [hardware validation report](HARDWARE-VALIDATION.md) for the next checks.
