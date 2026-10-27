# Spoolancer v0.4.1 wiring

Prototype wiring updated 27 October 2026. Choose the firmware environment and pin column for your controller. The screenless and integrated-touch peripheral pin maps are different; both use onboard BOOT for setup. Compilation does not verify physical wiring, regulator capacity, display or NFC operation.

## Controller variants

| Build | Controller | Firmware environment |
| --- | --- | --- |
| Screenless | Owned ESP32-S3 N16R8 development board | `spoolancer-headless` |
| Touchscreen | Confirmed ESP-8048S043C-I integrated 4.3-inch capacitive board | `spoolancer-touch-8048` |
| Archived small display | Owned ESP32-S3 plus Waveshare ST7789 and three buttons | `spoolancer-s3` |

The integrated touchscreen uses its own ESP32-S3. Do not wire it as a display peripheral of the separate development board. Its RGB panel and GT911 touch controller are already connected internally.

The [owned ESP32 photo](docs/reference/esp32-s3-n16r8-v6.png) shows 44 header positions, V6.0.0, N16R8 marking and separate COM/USB connectors. Use printed GPIO labels rather than counting pins. The image does not establish manufacturer authenticity or detected memory. The [PN532 photo](docs/reference/pn532-v3-listing.png) shows a red Elechouse-style NFC MODULE V3; verify the actual clone before powering it.

## Shared peripherals, different pins

All numbers below are **GPIO labels**, not connector positions. All modules share GND. PN532 and the selected HX711 use 3.3 V supply and signal levels.

| Connection | Screenless N16R8 | ESP-8048S043C-I touch | Direction |
| --- | ---: | ---: | --- |
| PN532 TXD | GPIO18 | GPIO13 | Reader → ESP RX |
| PN532 RXD | GPIO17 | GPIO11 | ESP TX → reader |
| PN532 VCC / GND | 3V3 / GND | Verified 3V3 / GND pads | Power |
| HX711 DOUT / DT | GPIO15 | GPIO12 | Amplifier → ESP |
| HX711 SCK / CLK | GPIO16 | GPIO17 | ESP → amplifier |
| HX711 VCC / GND | 3V3 / GND | Verified 3V3 / GND pads | Power |
| Physical setup control | Onboard BOOT, GPIO0 | Onboard BOOT, GPIO0 | Active low; internal pull-up; no external button wiring |

The exact map is also in [firmware/include/Pins.h](firmware/include/Pins.h). Touch GPIO11/12/13 reuse the SD interface. **Leave the SD slot empty**; SD is disabled and CS GPIO10 is held high. Do not carry the screenless GPIO18 or HX711 15/16 connections over to the touch board: those belong to its touch/display functions.

Set PN532 switch **1 OFF, 2 OFF**, with power disconnected, for UART/HSU. Read the numbered switches and their ON mark; do not infer orientation from the photo. Use the reverse-side UART TXD/RXD labels on the shared four-pin UART/I2C header. Firmware configures 115200 baud, 8N1, on the explicit RX/TX pins above; do not use UART0 programming-header TX/RX instead.

The [Elechouse V3 manual](https://www.elechouse.com/elechouse/images/product/PN532_module_V3/PN532_%20Manual_V3.pdf) specifies 3.3–5 V module supply and the switch table. [NXP's HSU application note](https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf) documents the default baud rate. Start at 3.3 V; check the actual board regulator and output levels before considering a different supply.

The [Bit Maker HX711 code 0679](https://www.bitmaker.com.br/m-dulo-conversor-hx711-adc-24-bits-para-sensor-de-peso) is the selected single-supply board, listed for 3–5 V. Do not apply another breakout's separate VCC/VDD instructions without its schematic. For the [code 1381 load cell](https://www.bitmaker.com.br/sensor-de-peso-c-lula-de-carga-5kg), the seller specifies **red E+, black E−, green A+, white A−**. Verify the received labeling; other cells can use different colors.

## Integrated touchscreen details

The [ESP3D reference pin map](https://esp3d.io/esp3d-tft/version_1x/hardware/esp32-s3/sunton-43-8048/) identifies RGB display/backlight GPIO2, GT911 SDA19/SCL20, reset38 and optional interrupt18. The driver uses touch polling; an INT connection is not assumed. Some revisions leave INT unconnected without modification. No such modification is required by these instructions. Check the exact board revision and pin labels before attaching the external peripherals.

The selected model's mechanical basis is the supplied [Guition specification, page 6](https://www.guition.com/icms/upload/fb081940d6fc11f09850077a33e1404f/FTPData/UEditor/file/2026121/1768961092165/ESP32-4827S043%20Specifications-EN.pdf#page=6). It shows a 123 × 74 mm board, rather than the conflicting outline on the general web reference. Mounting-hole fit, glass offset and connector clearance must be checked on the received board. Header supply labels matter: a connector offering 5 V is not a 3.3 V peripheral supply.

GPIO0 is a boot-strapping pin. On both current builds, use onboard BOOT for setup gestures **only after firmware is running**. Holding it while powering on or pressing RST enters the bootloader. Release BOOT and tap RST to start normally. RST only restarts the board; it does not clear Wi-Fi. This follows [Espressif's boot-mode guide](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html). The v0.4.1 headless firmware replaces the old GPIO4 setup input; GPIO4 is unused in the screenless build, and no separate button or perfboard is required.

## Setup-button behavior

For both current builds, hold and release **BOOT** for **2 seconds** to arm one first-PIN enrollment attempt for **60 seconds**. Choose eight digits in the browser at `http://192.168.4.1`; the page enables enrollment only through the active setup network after the physical press. An invalid submitted PIN consumes the attempt too.

Hold **BOOT for 8 seconds while running** to clear Wi-Fi credentials and pairing, close existing sessions and reopen setup. Release it afterward. The firmware handles this without rebooting. Inventory, calibration and Spoolman settings/links remain stored. The temporary AP has no password, closes after ten minutes or 60 seconds after LAN connection, and does not automatically reopen after a configured Wi-Fi failure. Use a trusted location: PIN and Wi-Fi credentials travel over local HTTP. See [first-use instructions](firmware/README.md#headless-first-use).

## Power and bring-up

Use one regulated USB supply through the controller's programming/power port. On the owned development board, use COM for programming; leave power-selection solder pads unchanged. Verify 16 MB flash and 8 MB PSRAM on the actual controller. Check each peripheral separately, then the 3.3 V rail and reliable boot with NFC, Wi-Fi and the chosen display active together. The photos do not establish spare regulator current.

On the separate N16R8 board, preserve GPIO35–37 for octal memory, 19–20 for native USB, and 43–44 for UART0. Avoid boot-strap pins 0/3/45/46 for extra peripherals. These restrictions follow [Espressif's board/memory guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html) and [S3 datasheet](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf). The integrated display deliberately uses its board-defined pins; it must not inherit this development-board allocation.

Initial checks: detected memory, HX711 raw readings, PN532 firmware identification and repeated test-tag detection, setup-button timing, then touchscreen image/touch alignment if fitted. Identify the bundled card/key fob before write tests; appearance does not establish NTAG216 compatibility. Calibrate only after the complete moving assembly is installed.

## Moving reader cable

The PN532 travels with the adjustable side guide. Route four flexible stranded HSU leads through its pod and along the moving guide, then make a broad slack loop to the fixed electronics. Add strain relief at both ends. Keep it clear of rails, carrier edges, cover and stops at every supported width and reader height.

The reader, holder and moving cable count as scale dead load. Zero the complete empty assembly after changing guide or reader position. Compare zero and repeated known masses at both height extremes and at 50/100 mm guide settings. If touching or repositioning the cable changes the settled reading, revise routing before calibration. Release the spool after NFC handling and wait for a new stable reading.

## Legacy small-display map

The retained `spoolancer-s3` environment uses the screenless PN532/HX711 pins, plus ST7789 **CLK12, MOSI11, CS10, DC9, RST8**, and buttons **Previous4, Next5, Select6**, each to GND. The Waveshare screen uses 3V3/GND and BL tied to 3V3. Its detailed cable, button behavior and v0.3 mechanics are preserved in [the archived wiring guide](archive/v0.3-compact/WIRING.md). Neither current variant requires that screen or three-button panel.
