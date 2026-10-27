# Build your first Spoolancer

Start with the **headless build** if you have an ESP32-S3 N16R8 and PN532. It uses your phone for controls and the board's existing BOOT button for setup. The optional ESP-8048S043C-I touchscreen replaces the separate ESP32.

This is a prototype release. CAD checks, firmware builds and software tests have passed; physical fit, calibrated accuracy, seated NFC operation and a real Spoolman connection still need testing. Record your results in the [hardware validation report](HARDWARE-VALIDATION.md).

The [v0.4.1-alpha.1 release](https://github.com/bloodf/spoolancer/releases/tag/v0.4.1-alpha.1) provides the complete source package, separate print packs for each build and `SHA256SUMS.txt`. The print packs contain individual STEP/STL files; the source package also contains the firmware, web interface and editable CAD generator.

## 1. Choose one controller

| Build | Controller | CAD folder | Firmware environment |
| --- | --- | --- | --- |
| Headless, v0.4.1 | ESP32-S3 N16R8 development board matching the documented V6.0.0 board | [headless](../cad/variants/headless/) | `spoolancer-headless` |
| Integrated touch | ESP-8048S043C-I, 4.3-inch 800 × 480 capacitive | [touch-8048](../cad/variants/touch-8048/) | `spoolancer-touch-8048` |

Use only one controller. Both builds share the 5 kg bar load cell, HX711 and PN532. Neither needs a separate setup button or perfboard. The touchscreen pin map differs from the headless map, and its microSD slot must remain empty.

Gather the [bill of materials](BOM.md), calipers, a multimeter, soldering tools and known calibration masses. The intended spool envelope is 50–100 mm wide and 160–220 mm in diameter. The firmware's weighing target is 20–2,000 g gross spool weight; the 5 kg cell also supports the moving mechanism.

## 2. Check the parts before a full print

1. Measure your board, connectors, load-cell holes and thread depths against the [mechanical references](MECHANICAL-SOURCES.md).
2. Print your variant's three [fit samples](PRINTING.md#fit-samples). Check actual screws, nuts, PCB clearance and connector exits.
3. For headless, check BOOT access with the intended USB cable connected. The final enclosure uses a blunt nonconductive tool through the USB opening. Its modeled button position comes from a photo.
4. Adjust the [CAD parameters](../cad/parameters.json) and repeat any failed sample before printing the complete set.

The NFC sample is the actual reader pod. Reuse it as the production pod if it fits and prints cleanly. A fit sample does not qualify the complete enclosure or the load path.

## 3. Wire and install the firmware

Follow the complete [variant-specific wiring table](../WIRING.md#shared-peripherals-different-pins). Set both PN532 switches OFF for UART/HSU before powering it. PN532 and HX711 use 3.3 V supply and logic with common ground.

Power the headless board through its **COM USB-C port**, using a regulated 5 V USB supply and a USB data cable for flashing. The touch board uses its own USB input. Peripheral power comes from verified 3V3/GND connections on the selected controller. Check the rail with Wi-Fi and NFC active before closing the enclosure.

Use the [firmware installation guide](../firmware/README.md#build-and-install) to build and upload **both** the application and web filesystem for your chosen environment. Verify detected 16 MB flash and 8 MB PSRAM. The source build is not a substitute for testing the real board, sensor and reader.

For a software preview before hardware assembly, serve `web/` as described in the [project README](../README.md#local-interface). Demo readings and simulated NFC/Spoolman actions do not use hardware or a real server.

## 4. Assemble and set up Wi-Fi

Use the [print manifest](PRINTING.md), then follow the [assembly guide](ASSEMBLY.md) in order. The load cell must be the only intended connection carrying weight from the moving cradle to the fixed base. Keep the NFC cable loop loose and clear of other parts. Tune the stops from measured movement; the initial modeled gap does not establish overload protection.

After normal power-up:

1. Join the temporary `Spoolancer-xxxx` network and open `http://192.168.4.1`.
2. Choose an eight-digit PIN. Hold BOOT for two seconds, release it, and submit within 60 seconds. Each press permits one enrollment attempt.
3. Enter your local Wi-Fi details. Note the LAN address, reconnect your phone to that Wi-Fi and open the scale there. `http://spoolancer.local` may work where mDNS is supported.
4. Pair using your PIN. Choose Light, Dark or System appearance in the browser.

The temporary setup network has no password, and setup uses HTTP. Use it in a trusted location. It closes after ten minutes or 60 seconds after the scale joins the LAN. For recovery, hold BOOT for eight seconds **while the firmware is running**, then release it. This clears Wi-Fi and pairing while retaining inventory, calibration and Spoolman settings. BOOT held during power-on or RST selects programming mode instead; release BOOT and press RST to start normally.

## 5. Calibrate and measure a spool

1. With the complete cradle, NFC pod and wiring installed, empty the cradle, let it settle and zero it.
2. Calibrate with a known 100–2,000 g mass. Check other known masses and repeated placements using the [validation procedure](HARDWARE-VALIDATION.md#weighing).
3. Create a spool with its name, brand, material, color and known empty-spool weight. Unknown empty-spool weight cannot produce a trustworthy remaining-filament figure.
4. Select that record, place the spool upright, align its tag with the side reader and release the spool. Wait for a stable reading before saving.
5. Optionally link a tag or configure Spoolman. Start with a disposable Spoolman record and verify the resulting weight before using your inventory.

Writable NTAG216 tags use Spoolancer's own inventory format. Original Bambu Lab and Snapmaker tags are UID-only and read-only in this firmware. They can identify a manually entered local record; Spoolancer does not create printer-authenticated tags or decode proprietary material fields.

Spoolman is optional. The current integration accepts a numeric local IPv4 HTTP address, creates or links records, and explicitly updates absolute remaining weight. It has no automatic retry queue. If creation has an uncertain outcome, inspect Spoolman and reconcile the existing record before retrying. See the [web guide](WEB-INTERFACE.md) for normal use and the [firmware guide](../firmware/README.md#spoolman) for these limits.
