# Assemble Spoolancer

Start with the [fit samples](../cad/README.md#fit-samples-first) and check them against the actual boards and load cell. The enclosure is a CAD-checked prototype. One standalone ESP32-S3 has been flashed and booted over USB; the assembled scale and its peripherals remain unvalidated.

Use [SHOPPING.md](../SHOPPING.md) for electronics, [WIRING.md](../WIRING.md) for connections, [firmware/README.md](../firmware/README.md) for installation, and the [web-interface guide](WEB-INTERFACE.md) for operation. The current build has a side-mounted NFC pod; use the v0.4.1 headless exports or the touch variant, not the archived full-saddle design.

The shared mechanism instructions below also apply to the new LED/OLED variants, but use their complete selected export set. Follow [LED-only notes](../cad/variants/headless-led/NOTES.md) or [OLED/LED notes](../cad/variants/oled-led/NOTES.md) for their changed base/cover, eleven-piece count, front pod and longer front screws. Their USB walls are closed and BOOT has a separate tool bore. Follow [DC power instructions](DC-POWER.md) before installing a converter or connecting USB.

## Prints and tools

Print one base, top cover and carrier; two rim rails; one left guide; one right NFC guide; one NFC pod and its cover. These are **nine printed pieces**. The headless build reuses onboard BOOT and has no separate reset-button cap. Exact [STL links, orientations and bounds](../cad/README.md#main-prints) are in the CAD guide. The NFC fit sample is the actual pod and can be reused if it fits.

The base needs a **170 × 160 mm** bed footprint plus clearance or brim. Start with ordinary PETG, 0.2 mm layers, four perimeters, five top/bottom layers and 20–25% infill. These settings are unvalidated. Use unfilled filament around the NFC antenna. Keep the exported orientations, particularly the rim rails printed on their outer end faces and both guides printed inner-face down; inspect small bridges in the slicer.

Have calipers, screwdrivers/hex keys, a 2.5 mm hex key for the stops, a soldering iron, and a multimeter. Use known reference masses or objects weighed on a trusted reference scale for calibration.

## Mounting hardware

Lengths below assume the modeled stack. Check the received screw heads, washers, nut dimensions and cell thread depths before tightening.

| Location | Quantity and hardware | Check |
| --- | --- | --- |
| Top-cover closure | 4 × M3 × 10 mm pan-head screws; 4 ordinary M3 nuts; 4 washers about 0.5 mm thick | Nuts enter sideways from inside the housing, under the base posts' retaining roofs. |
| Rim rails | 4 × M3 × 12 mm socket-head screws; 4 ordinary M3 nuts; 4 washers about 0.5 mm thick | Nuts enter from the front/rear carrier edges; counterbores give driver access. |
| Sliding guides | 4 × M3 × 12 mm socket-head screws; 4 ordinary M3 nuts; 4 washers about 0.5 mm thick | Nuts enter from the left/right carrier edges; guides must slide when loosened and clamp when tightened. |
| Adjustable carrier stops | 4 × M3 × 20 mm socket-head screws; 4 M3 nylon-insert locknuts | Locknut height no more than 4.2 mm; check pocket fit. |
| Fixed load-cell end | 2 × M5 × 12 mm socket-head screws; 2 washers about 1 mm thick | About 5.2 mm nominal engagement; measure actual usable thread depth. |
| Moving load-cell end | 2 × M4 × 25 mm socket-head screws; 2 washers about 0.8 mm thick | About 5.2 mm nominal engagement; measure actual usable thread depth. |
| NFC pod height adjustment | 2 × M3 × 8 mm nylon screws; 2 M3 nylon nuts; 2 nylon washers about 0.5 mm thick | Nuts load from the tops of the vertical guide channels; check sliding and clamping. |
| PN532 PCB in pod | 2 × M2.5 × 8 mm flat-head nylon screws; 2 matching nylon nuts | Provisional: use the matching PCB-hole pair and test the countersinks, components and screw clearance. |
| NFC pod cover | 2 × M2.5 × 6 mm nylon screws | Provisional: thread gently into 2.1 mm printed pilots; cover holes are 2.7 mm. |
| ESP32 and HX711 | 3 small cable ties about 2.5 mm wide | Two for ESP32 and one for HX711, clear of pins/components. |
| Feet | 4 adhesive rubber feet, about 12 mm diameter × 2 mm thick | Support the base evenly in its shallow underside pockets. |

The cell drawing's `6H` is a thread tolerance class, not a 6 mm depth. Each screw must clamp its mounting stack without bottoming out. The two 5.2 mm engagement figures are CAD calculations. Most fasteners, including the PN532 PCB screws, are not part of the collision model.

## 1. Fit the components and check onboard BOOT access

1. Check the ESP32 rails, cell-hole spacing and NFC-pod fit samples. The ESP32 sample does not prove that the complete base's USB openings and plugs fit.
2. Fit the ESP32 with its COM/USB sockets facing the existing left-side service opening. Identify the button labeled **BOOT** on the actual board. The adjacent RST button only restarts the controller.
3. Check that BOOT can be pressed and fully released through the service opening using a blunt nonconductive tool. The modeled access path uses photo-derived button placement; confirm the real button, cables and wire bends before closing the cover. Keep the tool clear of components and the moving cradle. No extra switch, button wires, perfboard, button mounting screws or printed cap is required.
4. After firmware starts, hold/release BOOT for **2 seconds** to authorize setup enrollment, or hold it for **8 seconds** to clear Wi-Fi/pairing. Release after the reset. Inventory, calibration and Spoolman settings remain stored. Do not hold BOOT during power-on or RST: that selects the bootloader. If this happens, release BOOT and tap RST.

The touch variant also uses onboard BOOT while powered. Use its own cover and holder notes, and keep its BOOT and USB accessible. The legacy v0.3 external buttons remain in the archive; their mounts are not part of the current headless build.

If a fit differs, adjust [parameters.json](../cad/parameters.json), [regenerate](../cad/README.md#edit-and-regenerate) and repeat the affected sample. Do not force a mismatched board or fastener into place.

## 2. Assemble the base and load cell

1. Slide the four ordinary M3 cover nuts horizontally from inside the housing into the side-post channels, before lowering the cover. Each nut sits beneath a retaining roof. Fit the four stop locknuts in their separate pockets with metal thread entry up and nylon ring down. Insert stop screws from above, keeping their heads low enough to clear normal carrier movement.
2. Confirm the actual cell's thread sizes and mounting orientation. Fit its M5 end to the fixed supports from the base underside, using the M5 screws and washers. Keep the central sensing section free.
3. For headless, secure ESP32 and HX711 to their rails with the three cable ties, just tight enough to retain them. Route fixed wiring without pressing on pins, components or the antenna overhang.
4. Attach the carrier to the M4 cell end through its open frame using the M4 screws and washers. The carrier must not touch the base, cover, stops or wires. Guide and rail nuts can be inserted before this step if access is easier.
5. Route the load-cell cable with slack and secure it to the fixed structure. Fit all four feet and check that the base does not rock.

The load cell must be the only intended load path from the moving assembly to the base. Its 5 kg rating includes the carrier, rails, guides, reader, pod, moving wiring and fasteners as well as the spool. The operating target remains up to 2 kg gross spool weight; printed strength and overload protection are untested.

## 3. Fit the rails and width guides

1. Slide four ordinary M3 guide nuts into the carrier from its **left/right edges**, two per side. Their channels are below a 3 mm retaining roof. Fit the plain left guide and the taller NFC guide on the right, using two M3 × 12 mm screws and 0.5 mm washers per guide. Start the screws loosely to retain the nuts while handling.
2. Slide four ordinary M3 rail nuts into the carrier from its **front/rear edges**. Fit both rim rails with M3 × 12 mm screws and 0.5 mm washers. Rotate the second rail 180 degrees so their slopes face each other. Tightening must pull each nut against its channel roof and clamp the part to the carrier.
3. Check screw tips and free movement. Nominal guide screw tips reach Z = 43.5 mm; rail tips reach Z = 42.5 mm. Actual washer thickness, screw length and print error can change these clearances.
4. Measure the spool width and set the guides symmetrically to **width plus 4 mm**. A 100 mm spool needs a 104 mm inner gap. Adjust with the spool removed, then tighten the guides and place the spool gently on the rails. Do not squeeze the flanges.

The 50–100 mm width range and 160–220 mm diameter range are modeled envelopes. Check actual flange shapes, rail contact and upright stability. This mechanism is for occasional weighing, not feeding filament during printing.

## 4. Fit the side NFC pod

1. With power disconnected, set both PN532 switches OFF for UART/HSU. Fit the board in the pod with its antenna facing the thin window. Use only the matching pair of PCB holes and provisional flat-head nylon hardware. Confirm hole pitch, head seating and rear component clearance on the fit pod.
2. Route compact leads through the bottom exit. The modeled connector/wire allowance is **7 × 12 × 8 mm**; large Dupont housings are not qualified. Direct solder leads or another measured connector arrangement may be needed. Close the pod using its two nylon screws without overtightening the printed pilots.
3. Insert two M3 nylon nuts from the tops of the right guide's vertical channels. Attach the pod tabs with M3 × 8 mm nylon screws and 0.5 mm nylon washers. Check that loosening permits height adjustment and tightening clamps the pod. Keep antenna-adjacent hardware nonmetallic.
4. With the spool seated, adjust the reader center within **90–175 mm** height and rotate the spool to align its tag with the antenna. The pod window is nominally 2 mm from the spool side. The 1.2 mm window and 0.8 mm board gap give about **4 mm antenna-to-spool separation**, before embedded-tag depth or board variation. Test the actual tag; this spacing is not a read-range guarantee.

Run the reader wires along the moving guide, then form a **loose, flexible silicone cable loop to the fixed base**. Leave slack for the full height and width adjustment. Secure the loop so it cannot rub the spool, rails, cover, stops or cell flexure. A wire loop can exert force on the scale even without visible contact; it must be tested for zero shift.

## Touch variant: fixed front panel

Use the [ESP-8048S043C-I holder, blank cover and assembly notes](../cad/variants/touch-8048/TOUCH-NOTES.md) when building the touch option. Print one bezel, one stand and one flat bridge. The holder adds four M3 × 10 PCB-clamp screws, two M3 × 12 bridge screws, six standard M3 nuts and six nominal 0.5 mm washers. Replace the two front cover screws with M3 × 16 and use two extra rubber feet matching the base. The rear cover screws remain M3 × 10. Confirm the modeled screw/nut stack against the actual board before tightening. The integrated ESP32 replaces the separate controller. Its pin map differs from the headless version: do not reuse the headless HX711 connections. Leave the microSD socket empty because its pins are repurposed. The screen pod belongs entirely to the fixed structure and must clear the spool, carrier and reader at every adjustment. PCB-hole slots and clearance allowances are based on a family drawing; verify the received capacitive variant before a full print.

## 5. Finish wiring and set the stops

Follow [WIRING.md](../WIRING.md) with power disconnected. Check each peripheral independently and verify a stable 3.3 V supply with the display and reader active. The cover's underside pocket leaves 1 mm above the modeled 25 mm ESP32 wire/header allowance and a 1.4 mm roof. Check actual plug housings and wire bends before closing it.

Close the cover with **M3 × 10 mm screws and 0.5 mm washers** into the side-loaded nuts. Their nominal tips reach Z = 28.5 mm. Keep fixed wiring out of the carrier opening and maintain the reader's loose service loop.

The rails and carrier have aligned **3.5 mm access holes** for a **2.5 mm hex key** to adjust the stops. The initial **1 mm gap is untuned**. Measure actual deflection with the complete moving assembly plus the intended 2 kg gross spool load. Set each stop to clear that normal movement, then check that the carrier does not rock or touch a stop while weighing.

The 5 kg label alone cannot establish a safe stop gap. Allowable cell deflection and an overload test are still missing; these stops provide no demonstrated overload protection. Do not calibrate while a stop carries any normal weighing load.

## 6. Test the assembled device

1. Check free carrier movement, four-foot support, rail/guide clamping and loaded stability. Check USB plugs and onboard BOOT access after closing the cover; for touch, also check the panel. Confirm both guide-width limits and the reader's full height travel without cable contact or tension.
2. Follow the [firmware installation guide](../firmware/README.md). Zero the complete empty cradle, including reader, pod and installed wiring. Calibrate with a known 100–2000 g reference, then test independent masses and repeated spool placements.
3. Repeat zero-return and loaded measurements with the reader at minimum/maximum height and the guides at 50/100 mm spool settings. If moving the reader or loop shifts zero, improve the routing and re-zero before relying on the readings. Check warm-up drift and off-center sensitivity.
4. Test seated NFC reads and supported NTAG216 writes at the final window, with display and Wi-Fi active. Original Bambu/Snapmaker tags remain UID-only and read-only in this firmware; link them to a manual profile if desired. Keep hands off the spool while weighing settles.
5. Check unknown empty-spool weight, local measurement saving and configured Spoolman sync through the [web interface](WEB-INTERFACE.md). Network failures must not become success messages. This version uses manual retry and reconciliation after uncertain creates; it has no automatic sync queue.

CAD validation, firmware compilation and browser demonstrations are separate from these physical checks. Record fit changes and measured performance before accepting the prototype.
