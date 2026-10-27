# 12 V input for the LED and OLED variants

The `headless-led` and `oled-led` cases use a user-drilled DC jack opening and have no external USB cutouts. A separate small tool opening reaches onboard BOOT. Remove the cover to connect a programming cable.

**12 V is the input to a buck converter, never the supply to the ESP32 or the 5 V LED strip.** Use a regulated fixed 5 V converter rated for at least 2 A continuous at 12 V input. The owner will glue an insulated converter assembly to a fixed enclosure wall; no dedicated converter mount is required. Its actual dimensions, heat dissipation and wiring must clear the moving assembly. A converter and jack have not yet been physically verified in this enclosure.

```mermaid
flowchart LR
  Jack[12 V DC jack] --> Buck[Regulated 5 V buck converter]
  Buck --> LEDs[Five 5 V addressable LEDs]
  Buck --> ESP[ESP32-S3 5 V input]
  ESP --> Rail[3.3 V peripherals]
  Rail --> OLED[OLED: verify actual module rating]
  Rail --> NFC[PN532]
  Rail --> HX[3.3 V compatible HX711]
```

All grounds join. Check jack polarity with a meter; a centre-positive supply is the intended convention. The jack's switched terminal, if present, is not automatically a ground terminal. Verify the converter outputs 5.0 V before connecting electronics. An adjustable converter must be set and secured before use.

## Power connection and programming

Use one controller supply path at a time. The [Espressif development-board guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html#power-supply-options) treats USB, 5 V header and 3.3 V header supplies as alternatives. The owned V6.0.0 clone's protection circuit is unverified, so do not assume it isolates two sources.

The preferred service connection is a removable internal USB-C power plug into COM if the actual angled plug fits the new case. Disconnect it and replace it with the computer's data cable when programming. The CAD plug is a provisional clearance reference, not a qualified cable recommendation.

If using the board's **5V and GND header** instead, use a removable two-wire connector. Before attaching a computer USB cable, disconnect the 12 V supply **and the converter's 5 V connection to the board**. Removing only the 12 V input can still leave a path for USB to feed backward into the converter. Keep the LED harness disconnected during USB-only controller programming unless its separate power arrangement has been checked; an unpowered strip must not be fed through its data input.

LED power branches directly from the converter's 5 V output. It does not pass through the ESP32's 3.3 V regulator. Keep LED supply/return wires short and away from the load-cell leads and HX711 input. Brightness limiting reduces normal consumption but does not replace a correctly sized supply.

## LED signal and protection

For the provisional WS2812B-compatible firmware profile:

| Connection | Signal |
| --- | --- |
| ESP32 GPIO6 | Data, through a 3.3 V-to-5 V logic buffer |
| Buffer output | Through a 330–470 ohm resistor near the first LED's DIN |
| Converter 5 V / GND | Strip 5 V / GND, common with ESP32 ground |
| Strip DOUT | Leave unconnected after the fifth LED |

Use a suitable fast non-inverting buffer such as 74AHCT125, powered from 5 V, with its enable wired correctly and local bypass capacitor. A preassembled buffer module can avoid a perfboard. Do not substitute a generic slow I²C level-shifter module without verifying its suitability for the strip protocol. Add a 500–1000 µF capacitor rated at least 6.3 V across the strip's **5 V** supply, observing polarity. These parts follow [Adafruit's NeoPixel guidance](https://learn.adafruit.com/adafruit-neopixel-uberguide/best-practices); the actual strip model remains to be identified.

The OLED firmware uses SDA on GPIO4 and SCL on GPIO5. Confirm the module's 3.3 V operation and ensure I²C pull-ups do not raise either signal above 3.3 V. A 0.96-inch size does not establish the controller or supply circuit.

## Mechanical and electrical checks

Drill with all electronics removed. Check both sides of the chosen wall for the jack's complete body, nut and wire bend radius; do not cut the load-cell supports, BOOT passage or cover posts. The jack opening has deliberately not been generated in CAD.

Before permanent assembly, verify converter output under the combined Wi-Fi, NFC, OLED and LED load, check temperatures, and compare repeatable scale readings with LEDs/display on and off. CAD clearance and a successful USB firmware boot do not verify the 12 V power path or weighing accuracy.
