# Assemble the Spoolancer prototype

Start with the [five fit samples](../cad/README.md#fit-samples-first). Check them against the actual boards, display and load cell before printing the complete enclosure. This guide describes the current CAD prototype; physical fit and operation remain unverified. No firmware has been implemented.

Use the electronics in [SHOPPING.md](../SHOPPING.md) and the connections in [WIRING.md](../WIRING.md). The current design is a standalone scale with planned NFC reading/writing and optional Spoolman synchronization, as described in [PLAN.md](../PLAN.md).

## Printed parts and tools

Print one each of the [base](../cad/exports/base.stl), [top cover](../cad/exports/top_cover.stl), [moving carrier](../cad/exports/carrier.stl) and [V-shaped saddle](../cad/exports/saddle.stl), plus two copies of the [side guide](../cad/exports/side_guide.stl). These are the six main pieces. Three [button caps](../cad/exports/button_cap.stl) are optional. The files are already oriented for printing: base floor down, cover exterior down, carrier upper face down, saddle flat underside down, guide inner face down and cap heads down.

The full top cover attaches with four closure screws and has a 158 × 142 mm opening around the 154 × 138 mm carrier, leaving a nominal 2 mm gap on each side. The [exploded preview](../cad/previews/exploded.png) shows the pieces; the [load-path preview](../cad/previews/load-path.png) shows the cell mounting. Use the current v0.2 files; the [flat-spool prototype is superseded](../archive/v0.1-flat/SUPERSEDED.md).

The spool stands upright in the saddle. The adjustable guides are intended for actual spool widths of **50–100 mm**, with their inner gap set to the measured width plus **4 mm**. The assembled printed mechanism is 190 × 230 × 120 mm, approximately 121.4 mm high with the specified feet. Allow about 272 mm height and 245 mm depth for the modeled 200 mm diameter spool. A modeled 220 mm spool reaches about 293 mm height and 255 mm depth. These dimensions do not establish physical stability or compatibility with every spool.

Use ordinary PETG as a starting material, with 0.2 mm layers, four perimeters, five top/bottom layers and 20–25% infill. This profile has not been physically tested. Use unfilled filament for the NFC scan face. The base and cover need a 190 × 230 mm footprint; the usable bed must accommodate it plus the slicer's clearance or brim. Check the actual printer model before slicing.

Have calipers, a ruler, appropriate screwdrivers/hex keys, a 2.5 mm hex key for the stops, a soldering iron and a way to trim/drill perfboard available. Use a multimeter during electrical checks. Borrow reference masses or use objects weighed on a trusted reference scale for calibration.

## Mounting hardware

Lengths below are the intended choices for the current CAD dimensions. Confirm the received threads, heads, washers and clearances before tightening them.

| Location | Quantity and hardware | Check |
| --- | --- | --- |
| Top-cover closure | 4 × M3 × 8 mm pan-head screws; 4 ordinary M3 nuts | Nuts fit the shallow hex pockets at the top of the base posts |
| Saddle to carrier | 4 × M3 × 8 mm screws; 4 ordinary M3 nuts; 4 washers about 0.5 mm thick | Nuts sit in carrier pockets; check the recessed head seats and clamping before loading |
| Adjustable side guides | 4 × M3 × 20 mm socket-head screws; 4 ordinary M3 nuts; 4 washers about 0.5 mm thick | Nuts slide in underside saddle channels; check retention and screw-tail clearance |
| Adjustable carrier stops | 4 × M3 × 20 mm socket-head screws; 4 M3 nylon-insert locknuts | Locknut height must be no more than 4.2 mm; check hex-pocket fit |
| Fixed load-cell end | 2 × M5 × 12 mm socket-head screws; 2 washers about 1 mm thick | About 5.2 mm nominal thread engagement with the modeled stack; measure actual usable depth |
| Moving load-cell end | 2 × M4 × 25 mm socket-head screws; 2 washers about 0.8 mm thick | About 5.2 mm nominal engagement with the modeled stack; measure actual usable depth |
| Display | 4 × M2 × 12 mm screws; 4 M2 nuts and small washers | Display hole diameter is unverified; use compatible hardware supplied with the actual display or revise the mounts if needed |
| Button strip | 2 × M2 × 6 mm screws | Thread gently into the printed 1.7 mm pilot holes; do not overtighten |
| PN532 | 2 × M2.5 × 8 mm nylon screws; 2 matching nylon nuts | Use only the diagonal pair that matches the actual board; leave the other holes unused |
| ESP32 and HX711 | 3 small cable ties about 2.5 mm wide | Two retain the ESP32, one the HX711; avoid pins and components |
| Feet | 4 adhesive rubber feet, approximately 12 mm diameter × 2 mm thick | Fit the shallow underside pockets and support the base evenly |

The cell drawing's `6H` callout is a thread tolerance class, not a 6 mm depth. A screw must clamp its mounting stack without bottoming out. If the actual depth or washer thickness differs, select another length or revise the fastener seat. The two 5.2 mm engagement figures are CAD calculations, not measured compatibility.

## 1. Fit the samples

| Sample | Acceptance check |
| --- | --- |
| [ESP32](../cad/exports/fit_esp32.stl) | Board rests on the rails without pressing on underside components; both USB connectors and the intended plug clear the wall |
| [Display](../cad/exports/fit_display.stl) | Hole pattern matches; screws fit; the window shows the complete active area without loading the glass or flex cable |
| [PN532](../cad/exports/fit_nfc.stl) | One diagonal mounting pair fits; the antenna faces the scan patch; the header and wires clear the central button area |
| [Buttons](../cad/exports/fit_buttons.stl) | Strip mounts fit, all three actuators align and each button releases without preload |
| [Cell](../cad/exports/fit_cell.stl) | The four positions match the actual cell and each screw clears the printed hole; verify thread depth separately |

The ESP32 envelope, display-window offset and PN532 hole pattern contain estimates or reference-design assumptions. Adjust [parameters.json](../cad/parameters.json), regenerate with the command in the [CAD guide](../cad/README.md#edit-and-regenerate), and reprint an affected sample if needed. Do not force a mismatch into place.

## 2. Prepare the top cover

1. Mount the display under the left window with its screen facing outward. The opening flares from 43 × 34 mm below to 43 × 40 mm at the top face. Check the complete active area is visible, and keep glass and flex cable clear of the supports. Check the rear plug before tightening the compatible fasteners.
2. Trim the button perfboard to **18 × 44 mm**. With its center as the origin, drill two **2.2 mm** mounting holes at **(-5, -19)** and **(5, 19) mm**. Position the switch centers at **(0, -12.7)**, **(0, 0)** and **(0, 12.7) mm**. Dry-fit before soldering; a 6 × 6 mm switch body does not establish lead pitch.
3. Wire a common ground and one signal for each button, then secure the strip with the two M2 screws. Check full release. The selected 7 mm switches project about **0.5 mm** above the modeled cover and can be used bare.
4. If using the caps, first try one. Its **3.4 mm bore** and **0.4 mm nominal stop gap** are unverified. Adjust them to the actual actuator and travel, or omit the caps. A cap must not hold a switch down at rest or transfer excessive force to it.
5. Set the PN532 to UART/HSU with both switches OFF while power is disconnected. Mount it under the right scan patch, antenna side toward the cover and header facing the central button bay for right-angle wire clearance. Use the matching diagonal pair of nylon fasteners only.

The scan patch is nominally 1.6 mm thick with a 0.8 mm antenna-side gap. These dimensions are starting points. Check reading and writing with actual tags before accepting the final cover material and spacing. Scan the tag at this front patch, then place the spool in the saddle.

## 3. Assemble the base and load path

1. Insert the four ordinary M3 closure nuts and four stop locknuts into their respective pockets. Orient the stop locknuts with the metal thread entry upward and the nylon ring downward. Fit the stop screws from above, heads toward the carrier. Leave them low enough that they cannot carry load during normal weighing.
2. Position the cell along the central mounts. The modeled fixed end uses the M5 pair and the moving end uses the M4 pair. Confirm the actual cell's threads and orientation first. Fasten the fixed end from the base underside with the specified M5 screws and washers.
3. Place the ESP32 and HX711 on their rails and retain them with the three cable ties. Tighten only enough to prevent movement, with ties clear of pins and components. Route the display, reader and button wires toward their cover positions.
4. Fit the moving carrier to the other cell end using the M4 screws and washers through its recessed holes. Confirm the thread engagement before tightening. The sensor's central section must remain free, and the carrier must not contact the housing, cover or wiring.
5. Route the cell cable with slack and secure it to the fixed structure so it cannot pull on the moving end. Attach the four rubber feet in their underside pockets. Support the base evenly on all four feet.

The cell must be the only intended load path between the moving assembly and base. Its 5 kg rating includes the carrier, saddle, guides and their fasteners. The first-version weighing target is up to 2 kg gross spool weight; neither the printed structure nor the stops have been qualified for overloads.

## 4. Fit the saddle and sliding guides

1. Place the four saddle-fastening M3 nuts in the carrier's upper hex pockets.
2. With the saddle lifted off the carrier, insert four ordinary M3 guide nuts into its underside channels. Fit one guide on each side of the V, with their flat inner faces toward the spool. The two prints are identical; rotate the second one to face inward.
3. Start each guide's two M3 × 20 mm socket-head screws with the 0.5 mm washers while holding the nuts in their channels. Leave them loose enough to slide, but engaged so the nuts cannot fall out as the saddle is moved. The modeled channels retain the nuts while the carrier reliefs provide room for the screw tails. Check that each nut stays captured and each guide can travel freely.
4. Lower the saddle onto the carrier and secure it with four M3 × 8 mm screws and 0.5 mm washers. Check that its underside seats fully and the guide screw tails do not bear on the carrier. Remove the saddle again if a guide nut needs repositioning.
5. Measure the actual spool width. Move both guides symmetrically to leave an inner gap of **width plus 4 mm**, then tighten their hex fasteners. For example, a 100 mm wide spool needs a 104 mm gap. Adjust with the spool removed, then place it gently in the V. Do not squeeze its flanges between the guides.

The guide counterbores provide flat screw seats at Z = 68 mm. With the intended 20 mm screws and 0.5 mm washers, the nominal screw tails reach Z = 48.5 mm, leaving 0.5 mm above the carrier relief floor. Confirm that clearance on the print. Washer changes or longer screws can make the tails bottom out in the carrier. The four saddle screws remain accessible outside the guides at the widest setting.

All parts that may contact the spool must move with the load cell. The side guides limit sideways movement; they do not turn the scale into an in-printer spool holder. Check stability with the actual flange shapes before leaving an upright spool unsupported.

## 5. Wire, close and set the stops

Follow [WIRING.md](../WIRING.md) with power disconnected. Check each peripheral independently before combining them. Confirm 3.3 V remains stable with the screen and NFC reader active, then close the top cover with the four M3 screws. Keep all cables below the carrier and out of its moving gap.

A local underside pocket in the cover clears the modeled ESP32 wiring envelope by 1 mm, leaving a 1.4 mm roof. This assumes a 25 mm wire/header allowance above the PCB. Route the actual jumper housings and bends through the pocket and check them before tightening the cover; it must not compress wires or press on the board.

Four aligned **3.5 mm access holes** through the saddle and carrier let a **2.5 mm hex key** reach the stop screws. The CAD shows a **1 mm initial gap**, which is untuned. Once weighing works, measure actual cell deflection under the intended 2 kg gross spool load, in addition to the moving assembly's own load. Set every stop so it clears that normal movement and verify the carrier does not rock or touch a stop while weighing.

The gap cannot be chosen from the 5 kg capacity label alone. Without the actual cell's allowable deflection and an overload test, these stops provide no proven protection at any overload. Do not calibrate while a stop carries part of the weight.

## 6. Validate the assembled device

1. Confirm free carrier movement, stable four-foot support and no contact from wires, housing or stops. Check the USB plug, buttons and display after closing the cover. Test guide adjustment at 50 and 100 mm widths, screw retention and upright stability with actual spools.
2. After firmware exists, calibrate zero and span with known masses. Zero with the complete moving assembly installed. Check several masses through the intended range and repeat placements at the center and different supported positions. Check actual upright spools for rocking and placement sensitivity; log drift after warm-up.
3. Test Bambu, Snapmaker and supported writable inventory tags through the final scan face. Repeat with the display and Wi-Fi active. Original printer tags remain read-only in the first firmware plan.
4. Exercise spool changes and unknown empty-spool weights. Remaining filament requires a known empty-spool tare; gross weight alone must not become guessed remaining weight or consumption.
5. With Spoolman configured in the future firmware, check the correct mapped spool receives the accepted measurement, offline use continues and queued updates recover without duplicate consumption. Exact API behavior will be checked during firmware implementation.

The current CAD files pass their recorded geometry checks. These assembly and functional checks remain open until performed on the actual build.
