# Five LEDs · six-part v0.8 enclosure

This print-first redesign replaces the complete v0.7 LED/OLED mechanism. Print **six pieces**, use **12 screws**, and glue one broad tower joint. The fixed bay accepts the modeled 50–100 mm spool widths without sliding side guides. The current model has no separate lid, removable ramps, thin frame, hooks or flexible retaining fingers.

**Physical validation is false.** Geometry checks, export checks and slicer evidence remain separate from actual printed fit, strength, adhesive retention, stability, wire behavior, RF operation and weighing accuracy. Use one complete current set and keep the old release only for an existing old build.

## Files and construction

Print one each of `base`, `cradle`, `cell_spacer`, `nfc_tower`, `nfc_cassette` and `nfc_cover` from [exports](exports/). Every individual STEP/STL has its broad bed face at Z=0. The assembly STEP includes purchased-component and spool references and must not be printed as a whole.

The base is **170 × 170 × 32 mm**, with a 4 mm floor and 3.2 mm walls. Its electronics rest on broad blocks, with removable insulating retention chosen around the actual boards. The cradle has an **8 mm floor**, integral solid V ramps and a **6 mm thick fixed left fence**. The slicer supplies infill inside solid CAD bodies; solid volume is not a filament-mass estimate.

The tower's front datum is X=52; the left fence datum is X=−52, giving a **104 mm bay**. The cradle's shallow three-sided nest supports the tower's broad glued bottom. The panel is **3 mm thick** with reinforced recessed clamp slots; the rear tray opens upward in its front-face-down print pose. The cassette prints closed-back down and the 3 mm cover prints flat. Exterior plan corners are chamfered; functional bearing faces and holes retain their geometry.

The printed assembly is 202 mm high before its nominal 2 mm feet. The cover screw heads extend the hardware footprint to approximately 173.7 × 170 mm. A seated spool extends further in height and depth. These dimensions do not establish tipping stability.

Use the [print manifest](../../../docs/PRINTING.md), [assembly guide](../../../docs/ASSEMBLY.md) and [3D viewer](https://bloodf.github.io/spoolancer/?model=headless-led). Current geometry is in [simple_geometry.py](../../simple_geometry.py), with assumptions in [simple-parameters.json](../../simple-parameters.json). The older `parameters.json` and old generator belong to the superseded design.

## Front controls and power

Five **7.4 mm LED holes** sit at 16.667 mm pitch, centered at X=22, Z=21. The five-pixel strip reference is 83.33 × 10 × 0.5 mm with 5 × 5 × 1.5 mm packages. Pitch is confirmed; strip width, coating, connector and cut-pad dimensions need measurement. Retain the strip using insulating adhesive and check package clearance before wiring.

This base has no display window. Inventory editing and setup remain available through the local browser.

The base has **no USB cutouts and no pre-cut DC jack hole**. Drill only after checking the actual jack, insulated converter and wiring. The converter has no prescribed mounting footprint; attach it to a fixed wall after checking clearance. Follow the [12 V-to-5 V instructions](../../../docs/DC-POWER.md), including isolation from computer USB during programming. No 12 V goes directly to the 5 V electronics.

A **6 mm top access bore at X=−33, Y=−6** targets the estimated top-facing ESP32 BOOT button. Remove the spool and use a blunt nonconductive probe. The modeled 3 mm probe clears current geometry, but actual button position and full release must be checked before final board attachment. There is no extra button or printed plunger.

## Fixed cradle and movable reader

The integral 30-degree slopes support a nominal 160–220 mm diameter spool. Bias its right flange toward the reader side without squeezing it. The representative spool has a 0.5 mm gap from the tower; the smallest modeled spool clears the cradle floor by about 3.06 mm. Actual flange shape, rocking and lateral stability need physical checks, especially for the narrowest spools.

The reader cassette holds a provisional **42.7 × 40.4 × 1.6 mm PN532 PCB** on broad side seats. Use removable soft pads at safe PCB edges, away from antenna traces and components. The antenna front is 0.8 mm behind the 3 mm panel. With the representative 0.5 mm spool gap, separation is nominally 4.3 mm before tag depth; this is not a read-range guarantee.

Two flush nylon screws clamp the cassette at **85–170 mm reader-center height**. Adjust with the spool removed if needed for tool access, then read/write with the spool seated and its tag rotated into alignment. There is no width adjustment. The two cover screws are diagonally placed near the bottom and top; both remain removable for wiring access.

A flexible reader lead runs through the cassette rear port, loops inside the tray and exits through the **8 × 12 mm tower/cradle opening**. Leave enough slack for full height travel and the moving-to-fixed transition. The pictured route is representative, with no proven wire stiffness, bend life or prescribed cut length. Keep real wiring clear of nuts, screws, stops, the spool and the cell flexure. Check zero return at both height limits with the cover closed.

## Hardware, glue and service

Use **4 metal M3 × 16, 4 nylon M3 × 16, 2 M4 × 25 and 2 M5 × 12 screws**. Nuts: four metal M3 nylon-insert locknuts for stops and four all-nylon M3 for the reader/cover. Washers: six nylon M3, two metal M4 and two metal M5. This is **12 screws, 8 nuts and 10 washers**; keep the NFC hardware nonmetallic. Match the [hardware envelopes](../../../docs/BOM.md#current-ledoled-fasteners).

Both cell-screw stacks give 5.2 mm nominal engagement. Check actual thread size, depth and washers. Four stop screws sit in broad posts, with 3.5 mm access bores through the cradle. The initial 1 mm stop gap is untuned and supplies no demonstrated overload protection.

Follow the [glue guide](../../../docs/GLUE-ASSEMBLY.md) for the tower/nest only. Bond and fully cure the bare printed subassembly before fitting the cell or electronics. Do not glue the cell, spacer, cassette, cover, moving/fixed gap or wire loop.

For rear wiring service, remove the two nylon cover screws and lift the flat cover outward. Free the reader lead before removing the cassette's two clamp screws. For base service, remove the spool, disconnect/free the reader tail and remove **only the two moving-end M4 screws**. Lift the cradle/tower while supporting the separate spacer. The fixed M5 pair stays installed. Recheck calibration and cable clearance after service.

## Digital evidence and remaining physical work

The current [validation report](exports/validation.json) records solid and mesh checks, assembly/reference intersections, spool cases, reader settings, BOOT approach and service access. The [export verification report](exports/export-verification.json) reopens the saved artifacts. These reports do not model thread contact, adhesive behavior, material defects, wire forces, strain, RF range or structural safety.

The previous v0.7 checks, fit samples and hardware schedule do not qualify this replacement geometry. Original headless/touch models remain legacy designs. Record actual fit, progressive load, repeated weighing, off-center sensitivity, tipping stability and electronics results in the [hardware report](../../../docs/HARDWARE-VALIDATION.md).

```sh
python3 cad/build_simple.py --variant headless-led
python3 cad/verify_simple_exports.py --variant headless-led
```

Run these with the FreeCAD-capable Python environment described in the [CAD guide](../../README.md).
