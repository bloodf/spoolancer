# Assemble Spoolancer

Start with the [fit samples for the selected build](PRINTING.md) and check them against the actual boards and load cell. The enclosure is a CAD-checked prototype. One standalone ESP32-S3 has been flashed and booted over USB; the assembled scale and its peripherals remain unvalidated.

Use the [rotatable assembly and numbered exploded views](ASSEMBLY-VIEW.md) to locate each printed part and the electronics before assembling. The explosion distances are for visibility, not mounting measurements.

Use [SHOPPING.md](../SHOPPING.md) for electronics, [WIRING.md](../WIRING.md) for connections, [firmware/README.md](../firmware/README.md) for installation, and the [web-interface guide](WEB-INTERFACE.md) for operation. All current builds have a side-mounted NFC pod. Choose one complete export set and keep its matching hardware; archived full-saddle designs use different parts.

The steps below distinguish the simplified v0.7 LED/OLED joints from the original headless/touch joints. Do not mix v0.6 and v0.7 LED/OLED parts. Follow [LED-only notes](../cad/variants/headless-led/NOTES.md) or [OLED/LED notes](../cad/variants/oled-led/NOTES.md) for their integrated vertical front controls, concealed reader cable route and current print quantities. Their USB walls are closed and BOOT has a separate tool bore. Follow [DC power instructions](DC-POWER.md) before installing a converter or connecting USB.

## Prints and tools

Print one base, top cover and carrier; two rim rails; one left guide; one right NFC guide; one NFC pod and its cover. These are **nine printed pieces** for the original headless build. Current LED/OLED builds also have one `nfc_cable_cover`, for **ten pieces**. The headless build reuses onboard BOOT and has no separate reset-button cap. Use the selected build's [print files and orientations](PRINTING.md); its validation manifest records part bounds. The NFC fit sample is the actual pod and can be reused if it fits.

The base needs a **170 × 160 mm** bed footprint plus clearance or brim. Start with ordinary PETG, 0.2 mm layers, four perimeters, five top/bottom layers and 20–25% infill. These settings are unvalidated. Use unfilled filament around the NFC antenna. Keep the exported orientations, particularly the rim rails printed on their outer end faces and both guides printed inner-face down; inspect small bridges in the slicer.

Have calipers, screwdrivers/hex keys, a 2.5 mm hex key for the stops, a soldering iron, and a multimeter. Use known reference masses or objects weighed on a trusted reference scale for calibration.

## Mounting hardware

### Current LED/OLED hardware

The current LED-only and OLED-plus-LED cases use **20 screws in three sizes**. Buy metal and nylon M3 × 12 separately. The [BOM](BOM.md#current-ledoled-fasteners) lists all quantities, including 16 nuts and 16 washers.

| Joint | Hardware | How the printed joint works |
| --- | --- | --- |
| Main lid | 2 metal M3 × 12; 2 standard M3 nuts; 2 metal M3 washers | Two rigid rear hooks locate the lid; two front screws retain it. |
| Rim rails | 2 metal M3 × 12; 2 standard M3 nuts; 2 metal M3 washers | Each rail has two locating keys and one central screw. |
| Width guides | 4 metal M3 × 12; 4 standard M3 nuts; 4 metal M3 washers | Two screws per guide preserve sliding adjustment and clamping. |
| Carrier stops | 4 metal M3 × 12; 4 M3 nylon-insert locknuts | Raised posts accept the shorter screws; the initial 1 mm gap still needs tuning. |
| Fixed load-cell end | 2 metal M5 × 12 and 2 M5 washers | Keep the cell's actual M5 thread and verify engagement. |
| Moving load-cell end | 2 metal M4 × 25 and 2 M4 washers | Keep the cell's actual M4 thread and verify engagement. |
| Reader-height clamps | 2 nylon M3 × 12; 2 nylon M3 nuts and washers | Adjust both clamps together; check sliding and retention. |
| NFC pod cover | 1 nylon M3 × 12; 1 nylon M3 nut and washer | One rigid locating tongue and one removable screw. |
| Cable cover | 1 nylon M3 × 12; 1 nylon M3 nut and washer | One rigid locating tongue and one removable screw. |
| PN532 PCB | Nonconductive soft edge shims | Printed edge pads retain the board without PCB mounting screws. |

Metal M3 washers are nominally 0.5 mm thick; nylon M3 washers are also 0.5 mm. M4 and M5 washers are nominally 0.8 and 1 mm thick. Check the actual parts. Use the fit pod to confirm PCB clearance and shim compression; keep pressure off components and antenna traces. The rigid hooks and tongues do not require bending a snap arm or fitting heat-set inserts.

### Original headless and touch hardware

The table below applies to the preserved v0.4.1 headless model. The touch build uses these joints plus the additions in its own section. It does not apply to current LED/OLED prints.

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

The cell drawing's `6H` is a thread tolerance class, not a 6 mm depth. Each screw must clamp its mounting stack without bottoming out. The two 5.2 mm engagement figures are CAD calculations. These are nominal hardware dimensions, not proof of the fit of received screws, nuts or the load cell.

## 1. Fit the components and check onboard BOOT access

1. Check the ESP32 rails, cell-hole spacing and NFC-pod fit samples. The ESP32 sample does not prove that the complete base's USB openings and plugs fit.
2. Fit the ESP32 with its COM/USB sockets facing left. The original headless case has a service opening; current LED/OLED cases have a closed USB wall and require the main lid to be removed for programming. Identify the button labeled **BOOT** on the actual board. The adjacent RST button only restarts the controller.
3. Check that BOOT can be pressed and fully released using a blunt nonconductive tool. Use the service opening on the original headless case or the dedicated BOOT bore on current LED/OLED cases. The modeled access path uses photo-derived button placement; confirm the real button, cables and wire bends before closing the cover. Keep the tool clear of components and the moving cradle. No extra switch, button wires, perfboard, button mounting screws or printed cap is required.
4. After firmware starts, hold/release BOOT for **2 seconds** to authorize setup enrollment, or hold it for **8 seconds** to clear Wi-Fi/pairing. Release after the reset. Inventory, calibration and Spoolman settings remain stored. Do not hold BOOT during power-on or RST: that selects the bootloader. If this happens, release BOOT and tap RST.

The touch variant also uses onboard BOOT while powered. Use its own cover and holder notes, and keep its BOOT and USB accessible. The legacy v0.3 external buttons remain in the archive; their mounts are not part of the current headless build.

If a fit differs, adjust [parameters.json](../cad/parameters.json), [regenerate](../cad/README.md#edit-and-regenerate) and repeat the affected sample. Do not force a mismatched board or fastener into place.

## 2. Assemble the base and load cell

1. Fit two ordinary M3 lid nuts for current LED/OLED builds, or four for the original headless/touch builds, before lowering the lid. Fit the four stop locknuts in their separate pockets with metal thread entry up and nylon ring down. Current LED/OLED raised posts use M3 × 12 stop screws; original builds use M3 × 20. Insert the stop screws from above, keeping their heads clear of normal carrier movement.
2. Confirm the actual cell's thread sizes and mounting orientation. Fit its M5 end to the fixed supports from the base underside, using the M5 screws and washers. Keep the central sensing section free.
3. For headless, secure ESP32 and HX711 to their rails with the three cable ties, just tight enough to retain them. Route fixed wiring without pressing on pins, components or the antenna overhang.
4. Attach the carrier to the M4 cell end through its open frame using the M4 screws and washers. The carrier must not touch the base, cover, stops or wires. Guide and rail nuts can be inserted before this step if access is easier.
5. Route the load-cell cable with slack and secure it to the fixed structure. Fit all four feet and check that the base does not rock.

The load cell must be the only intended load path from the moving assembly to the base. Its 5 kg rating includes the carrier, rails, guides, reader, pod, moving wiring and fasteners as well as the spool. The operating target remains up to 2 kg gross spool weight; printed strength and overload protection are untested.

## 3. Fit the rails and width guides

1. Fit the rails before the width guides. Nuts may be loaded in advance: four ordinary M3 guide nuts enter from the carrier's left/right edges, two per side. Current LED/OLED builds use one ordinary M3 nut per rail; original builds use two per rail.
2. For current LED/OLED builds, seat each rail's two locating keys in the matching carrier recesses, then retain the rail with one central M3 × 12 screw and 0.5 mm washer. Each key has 0.2 mm nominal side and floor clearance and a 45-degree ramp at its bed-leading edge. Tighten only when the bearing faces sit flat; the keys must not prop a rail above its support. Original builds use two M3 × 12 screws and washers per rail. Rotate the second rail 180 degrees so the slopes face each other.
3. Fit the plain left guide and the taller right NFC guide, using two M3 × 12 screws and 0.5 mm washers per guide. Start the screws loosely, then check screw tips and free movement against the selected variant's fastener report. Actual washer thickness, screw length and print error can change the modeled clearances. Confirm neither rail twists or lifts when pressed gently at each end.
4. Measure the spool width and set the guides symmetrically to **width plus 4 mm**. A 100 mm spool needs a 104 mm inner gap. Adjust with the spool removed, then tighten the guides and place the spool gently on the rails. Do not squeeze the flanges.

For rail service on current LED/OLED builds, remove the width guides first. Do not try to pull a keyed rail past an installed guide.

The 50–100 mm width range and 160–220 mm diameter range are modeled envelopes. Check actual flange shapes, rail contact and upright stability. This mechanism is for occasional weighing, not feeding filament during printing.

## 4. Fit the side NFC pod

1. With power disconnected, set both PN532 switches OFF for UART/HSU. Fit the board in the pod with its antenna facing the thin window. On current LED/OLED builds, seat its PCB edges on the printed pads and fit thin nonconductive compressible edge shims. The cover retains the board without screws through its mounting holes. Confirm that the board cannot rattle and that no pad or shim presses on components or antenna traces. Original headless/touch builds retain their two provisional M2.5 × 8 flat-head nylon PCB screws and matching nuts.
2. Route compact leads through the pod into the covered channel on current LED/OLED builds. Fit the pod cover's rigid tongue and retain it with one nylon M3 × 12 screw, nylon nut and washer. Do not force the tongue or pinch the leads. Original builds use the bottom wire exit and two M2.5 × 6 nylon cover screws. Their modeled connector/wire allowance is 7 × 12 × 8 mm; large Dupont housings remain unqualified.
3. Insert two M3 nylon nuts in the right guide's vertical channels. Attach the pod tabs with two nylon M3 × 12 screws for current LED/OLED builds, or M3 × 8 for the original models, and 0.5 mm nylon washers. Check that loosening permits height adjustment and tightening clamps the pod. Keep antenna-adjacent hardware nonmetallic.
4. With the spool seated, adjust the reader center within **90–175 mm** height and rotate the spool to align its tag with the antenna. The pod window is nominally 2 mm from the spool side. The 1.2 mm window and 0.8 mm board gap give about **4 mm antenna-to-spool separation**, before embedded-tag depth or board variation. Test the actual tag; this spacing is not a read-range guarantee.

For the original headless and integrated-touch variants, run the reader wires along the moving guide, then form a **loose, flexible silicone cable loop to the fixed base**. The current LED/OLED variants instead use a covered channel behind the reader; follow their variant notes to place the height-adjustment slack and route the flexible tail into the base. Leave slack for the full height and width adjustment. Secure the loop so it cannot rub the spool, rails, cover, stops or cell flexure. A wire loop can exert force on the scale even without visible contact; it must be tested for zero shift.

### Covered reader cable path: current LED/OLED cases

The reader's rear cover opens into the channel behind the height-adjustable pod. The channel body is integrated into the NFC guide; a separate cover hides the wiring. Its lower chute turns inward and enters the base through an open region of the moving carrier. A free flexible tail then reaches the fixed controller inside the base.

1. Open the channel cover and main lid before fitting the four flexible PN532 leads. Follow the selected variant's modeled cable-envelope and bend allowances.
2. Retain the leads at the intended strain-relief points. Leave the height-adjustment loop inside the channel, clear of the clamping slots and cover screw seats. Do not pull it taut at either height limit.
3. Route the tail through the lower chute into the base. Keep its internal loop clear of the load cell, carrier, stops, converter and fixed walls at both guide-width limits. Nothing should clamp the cable across the moving/fixed gap.
4. Adjust the reader through its full height range with the channel open, then engage the cable cover's rigid tongue and fit its one nylon M3 × 12 screw, nut and washer. Repeat the full travel check. Check for rubbing, pinching and tension before calibration.
5. Compare settled zero and known-mass readings at the reader/guide extremes. A concealed cable can still exert force; revise the slack if readings shift.

### Service the current LED/OLED reader

Open the PN532 pod on the bench after removing it from the guide. Its cover cannot release while the pod is mounted against the cable channel.

1. Remove the spool, set the guides to the **50 mm spool-width setting** and place the reader center at **155 mm**.
2. Remove the cable-cover screw, washer and nut. Move the cover **3.5 mm toward the rear**, then pull it **20 mm outward to the right**.
3. Disconnect and free the pigtail from the channel and its strain relief so it can move without pulling on the board.
4. Hold the assembled pod and fully remove both height-clamp screws and washers. Leave the unthreaded nuts in the mast tracks. Lift the pod until its center reaches **215 mm**, above the guide, then pull it **20 mm outward**. This is a service position only; the operating adjustment range is still **90–175 mm**.
5. On the bench, remove the single pod-cover screw, washer and nut. Slide that cover **down 4 mm**, then pull it **outward 20 mm**. The PCB can then withdraw straight outward.
6. Reverse the sequence to reassemble. Check clamp retention, full cable travel and return to zero before weighing. Keep slack in the leads throughout service and never pull on solder joints.

The routing model establishes proposed space, not a measured cable stiffness, fatigue life or RF read range. Use thin flexible stranded wires and check the actual bundle before the full print. Keep the channel cover removable for inspection.

## Touch variant: fixed front panel

Use the [ESP-8048S043C-I holder, blank cover and assembly notes](../cad/variants/touch-8048/TOUCH-NOTES.md) when building the touch option. Print one bezel, one stand and one flat bridge. The holder adds four M3 × 10 PCB-clamp screws, two M3 × 12 bridge screws, six standard M3 nuts and six nominal 0.5 mm washers. Replace the two front cover screws with M3 × 16 and use two extra rubber feet matching the base. The rear cover screws remain M3 × 10. Confirm the modeled screw/nut stack against the actual board before tightening. The integrated ESP32 replaces the separate controller. Its pin map differs from the headless version: do not reuse the headless HX711 connections. Leave the microSD socket empty because its pins are repurposed. The screen pod belongs entirely to the fixed structure and must clear the spool, carrier and reader at every adjustment. PCB-hole slots and clearance allowances are based on a family drawing; verify the received capacitive variant before a full print.

## 5. Finish wiring and set the stops

Follow [WIRING.md](../WIRING.md) with power disconnected. Check each peripheral independently and verify a stable 3.3 V supply with the display and reader active. The cover's underside pocket leaves 1 mm above the modeled 25 mm ESP32 wire/header allowance and a 1.4 mm roof. Check actual plug housings and wire bends before closing it.

For current LED/OLED builds, remove the spool and set both guides to the **50 mm spool-width setting** before closing or opening the lid. Engage the two rigid rear hooks and fit the two front **M3 × 12 screws with 0.5 mm washers**. To open it, remove those screws, withdraw the lid about **1.4 mm toward the front**, then lift it clear. Do not lever against the moving carrier or force the hooks. The narrower guide position provides the required withdrawal clearance.

The original headless lid uses four M3 × 10 screws and 0.5 mm washers. The touch bridge changes its front screw lengths as described above. Keep fixed wiring out of the carrier opening and preserve the reader's flexible service loop in every build.

The rails and carrier have aligned **3.5 mm access holes** for a **2.5 mm hex key** to adjust the stops. The initial **1 mm gap is untuned**. Measure actual deflection with the complete moving assembly plus the intended 2 kg gross spool load. Set each stop to clear that normal movement, then check that the carrier does not rock or touch a stop while weighing.

The 5 kg label alone cannot establish a safe stop gap. Allowable cell deflection and an overload test are still missing; these stops provide no demonstrated overload protection. Do not calibrate while a stop carries any normal weighing load.

## 6. Test the assembled device

1. Check free carrier movement, four-foot support, rail/guide clamping and loaded stability. Check USB plugs and onboard BOOT access after closing the cover; for touch, also check the panel. Confirm both guide-width limits and the reader's full height travel without cable contact or tension.
2. Follow the [firmware installation guide](../firmware/README.md). Zero the complete empty cradle, including reader, pod and installed wiring. Calibrate with a known 100–2000 g reference, then test independent masses and repeated spool placements.
3. Repeat zero-return and loaded measurements with the reader at minimum/maximum height and the guides at 50/100 mm spool settings. If moving the reader or loop shifts zero, improve the routing and re-zero before relying on the readings. Check warm-up drift and off-center sensitivity.
4. Test seated NFC reads and supported NTAG216 writes at the final window, with display and Wi-Fi active. Original Bambu/Snapmaker tags remain UID-only and read-only in this firmware; link them to a manual profile if desired. Keep hands off the spool while weighing settles.
5. Check unknown empty-spool weight, local measurement saving and configured Spoolman sync through the [web interface](WEB-INTERFACE.md). Network failures must not become success messages. This version uses manual retry and reconciliation after uncertain creates; it has no automatic sync queue.

CAD validation, firmware compilation and browser demonstrations are separate from these physical checks. Record fit changes and measured performance before accepting the prototype.
