# Assemble the six-part Spoolancer

These steps apply to the **v0.8 LED-only and OLED-plus-LED cases**. Both use a fixed 104 mm spool bay, a single cradle with integral ramps and left fence, and a broad side tower with a height-adjustable NFC cassette. They replace the v0.7 LED/OLED design completely. Do not mix their parts.

Use the [print manifest](PRINTING.md), [parts list](BOM.md), [numbered 3D views](ASSEMBLY-VIEW.md), [wiring guide](../WIRING.md) and [DC power instructions](DC-POWER.md). The model has digital geometry checks; **physical fit, retention, strength, tipping stability and weighing performance remain untested**.

## Prints and tools

Print one each: `base`, `cradle`, `cell_spacer`, `nfc_tower`, `nfc_cassette`, `nfc_cover`. Keep the supplied broad-face-down orientations. The base is 170 × 170 mm; allow slicer clearance or a brim beyond that. All six files belong to one selected variant.

Have calipers, suitable drivers, a blunt nonconductive probe for BOOT, a multimeter and known check masses. The four metal stop screws use a 2.5 mm hex key through 3.5 mm access holes. Check actual nylon screw-head drive style before purchasing tools.

## Current LED/OLED hardware

| Joint | Hardware | Function |
| --- | --- | --- |
| Fixed cell end | 2 metal M5 × 12; 2 M5 washers, nominally 1 mm thick | Secures cell to base from underneath |
| Moving cell end | 2 metal M4 × 25; 2 M4 washers, nominally 0.8 mm thick | Clamps cradle and separate spacer to cell |
| Four stops | 4 metal M3 × 16; 4 M3 nylon-insert locknuts | Adjustable clearance below cradle |
| Reader-height clamps | 2 nylon M3 × 16; 2 all-nylon nuts; 4 nylon washers | Flush heads in front slots; washer at each clamped side |
| Reader rear cover | 2 nylon M3 × 16; 2 all-nylon nuts; 2 nylon washers | Removable flat wiring cover |
| Tower to cradle | One broad adhesive joint | Fixed side support, bonded before electronics |

Total: **12 screws, 8 nuts, 10 washers**. There is no separate lid, rail attachment or width-guide hardware. Match the head, nut and washer envelopes in the [BOM](BOM.md#current-ledoled-fasteners).

## 1. Dry-fit and bond the tower

1. Check all six prints for distortion, cracks, rough bearing faces and blocked holes. Dry-fit the tower in the cradle nest. The tower front is the right spool datum; it must remain square to the cradle floor.
2. Check the actual PN532 in the cassette, slide the cassette behind the panel and test tag operation through the 3 mm panel before permanent bonding. Verify the 85–170 mm reader-center travel and all-nylon clamp hardware.
3. Follow the [glue-joint guide](GLUE-ASSEMBLY.md). Bond only the tower base/nest, keeping the lower cable opening clear. Cure the bare subassembly fully before attaching the load cell or electronics.

The ramps and left fence are already printed as part of the cradle. Do not add the previous rails, width guides or cable channel parts.

## 2. Fit electronics and the fixed cell end

1. With power disconnected, trial-fit ESP32 and HX711 on the broad support blocks. Use removable insulating adhesive/foam where it contacts safe PCB edges. The blocks are not a claim that every underside component clears; measure the actual boards. Leave their connectors and buttons accessible.
2. On the OLED build, mount the display behind the vertical front window. Secure safe PCB edges with removable insulating foam; do not clamp or glue the glass. Fit five LED pixels behind the five holes with insulating strip adhesive. Keep all of these leads in the fixed base.
3. Check the actual DC jack, buck converter and wiring, then drill the jack opening in a suitable fixed wall. The design has no USB holes or prescribed converter mount. Follow the [12 V-to-5 V and programming-isolation instructions](DC-POWER.md); disconnect converter power before programming from computer USB.
4. Fit the four M3 locknuts into the broad stop posts and insert the M3 × 16 stop screws from above. The metal thread entry should face the screw. Their initial head-top height is 39.5 mm, leaving a nominal 1 mm gap below the unloaded cradle; this is untuned.
5. Verify the cell's mounting direction, hole positions and actual thread depths. Fasten the fixed M5 end to the base from below with two M5 × 12 screws and washers. The nominal 5.2 mm engagement must not bottom out. Keep the central flexure free of supports, glue and wire pressure.
6. Fit four adhesive feet so the base sits flat without rocking. Route the load-cell cable with slack to the fixed HX711.

## 3. Fit the cradle and spacer

1. Put the broad spacer on the moving cell end, matching its two holes. It supports only that end; it must not bridge onto the sensing section.
2. Lower the cured cradle/tower subassembly onto the spacer. Insert two M4 × 25 screws with M4 washers through the recessed cradle holes. The heads finish flush with the 8 mm cradle floor. Confirm full clamping without bottoming in the cell.
3. Check that the cradle clears the fixed base, all stop heads and every fixed cable. The load cell must be the only intended structural load path between moving and fixed parts.
4. Recheck the BOOT hole against the actual ESP32 button before fixing the board's final position. The modeled hole is a provisional alignment, not a measured feature of the received controller.

The 5 kg cell rating includes the moving printed structure, reader, wires, hardware and spool. Firmware's intended gross-spool limit remains 2 kg; the printed design has no demonstrated overload rating.

## 4. Fit the reader and hidden wiring

1. Set the actual PN532 board to the interface specified in [WIRING.md](../WIRING.md), with power disconnected. Place it antenna-forward in the open-front cassette. Its edge seats and removable soft nonconductive pads retain the PCB; keep pressure off components and antenna traces.
2. Bring the cassette up behind the tower's front panel. Fit two nylon M3 × 16 screws through the recessed slots, one nylon washer under each head and one behind each cassette lug, then the two all-nylon nuts. Heads must stay flush with the spool-facing panel.
3. With the spool removed, set the reader center between **85 and 170 mm**, then tighten both clamps gently. The height slots are the only sliding adjustment; spool width is fixed. Do not overtighten nylon hardware or bend the cassette.
4. Route fine flexible reader leads through the cassette's rear port, make a generous slack loop inside the open tray and pass the tail through the tower/cradle opening into the base. The displayed cable is a representative route, not a prescribed cut length, bending model or guarantee of negligible cable force.
5. Check full reader travel before closing the cover. The loop must stay clear of clamp nuts, screw ends, cover bosses, the spool and the load-cell flexure. Provide a flexible transition to the fixed base without clamping the moving and fixed structure together.
6. Load the cover's two nylon nuts through the side entries. Place the flat rear cover and retain it with two nylon M3 × 16 screws and one nylon washer per screw. The screws sit diagonally, near the lower and upper ends of the cover.

The fixed bay is 104 mm wide and models 50–100 mm spool widths and 160–220 mm diameters. Place the upright spool on both slopes near the reader side without squeezing it. Rotate the seated spool to bring its tag to the reader. Reading and writing do not require removing a correctly aligned spool; changing reader height may require removing it for clamp access. Check actual flange geometry and loaded stability.

## 5. Set stops and check BOOT access

With the spool removed, reach the four stop screws through their 3.5 mm cradle holes. Measure actual cell movement under the completed assembly and intended loads, then set all four stops clear of normal weighing movement. The initial **1 mm gap is not a verified overload setting**. Do not calibrate while a stop carries load.

The **6 mm top access hole at X=−33, Y=−6** is for a straight blunt nonconductive probe to the estimated top-facing BOOT button. Remove the spool first. Verify the real button is aligned, presses and releases fully, and no wires occupy the probe path. No added switch, plunger or perfboard is required.

After firmware starts, hold/release BOOT for **2 seconds** to authorize setup enrollment, or hold it for **8 seconds** to clear Wi-Fi/pairing. Inventory, calibration and Spoolman settings remain stored. Do not hold BOOT during power-on or RST; that enters the bootloader. Release BOOT and tap RST if that happens.

## 6. Service and acceptance

To inspect the reader wiring, remove only the two rear-cover screws. Disconnect/free the reader leads before removing the cassette. Support the cassette while removing its two clamp screws; it withdraws outward through the open tray, without bending a latch.

To open the base, remove the spool, free/disconnect the reader tail, remove the **two moving-end M4 screws**, and lift the cradle/tower while supporting the separate spacer. The fixed-end M5 pair remains installed. Do not pull the lead tight or lever against the cell. Retain the separate spacer during handling.

Before relying on readings:

1. Check all feet, cell clamping, glue retention and upright stability with progressively placed loads. Remove hands and allow motion to settle.
2. Zero the complete moving assembly, including reader and wiring. Calibrate with a known 100–2000 g reference and compare independent masses, repeated placements and off-center loads.
3. Repeat zero-return and known-mass checks at both reader-height limits. Reroute the cable if height changes shift zero; geometric clearance alone does not establish low cable force.
4. Test actual LEDs, OLED, NFC and DC power with Wi-Fi active. Test supported inventory tags through the final panel. Original authenticated printer tags are not converted into writable stock-printer tags by this mechanical change.
5. Verify saving, material/color/brand editing and configured Spoolman operations in the [web interface](WEB-INTERFACE.md). Record fit changes and measurements in the [hardware report](HARDWARE-VALIDATION.md).

## Legacy headless and touchscreen

The original `headless` and `touch-8048` files remain separate legacy designs. Use their [preserved assembly instructions](https://github.com/bloodf/spoolancer/blob/v0.7.0-alpha.1/docs/ASSEMBLY.md#original-headless-and-touch-hardware) and [touch holder notes](../cad/variants/touch-8048/TOUCH-NOTES.md). Their rails, sliding guides, covers and fasteners do not belong to the current six-part build.
