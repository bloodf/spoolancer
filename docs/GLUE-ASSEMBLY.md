# Assemble the LED cases with glued rails

For the v0.7 `headless-led` and `oled-led` prints, bond the two fixed rim rails to the moving carrier to use **18 screws, 14 nuts and 14 washers**. The same STEP/STL files work for this option. There are still ten printed pieces and three screw sizes.

This is a proposed adhesive assembly method, not a tested bond specification. The CAD contact faces and access paths have been checked; adhesive strength, printed fit and weighing performance have not. The released CAD assemblies, exploded views and fastener reports show the removable **20-screw** build. Omit hardware group **27**, the two rail fastener sets, for the glued build. The v0.7.0 release ZIPs predate this guide; use this guide alongside their unchanged print files.

## What to buy

| Quantity | Screw | Use |
| ---: | --- | --- |
| 10 | Metal M3 × 12 mm | Two lid screws, four guide clamps, four adjustable stops |
| 4 | Nylon M3 × 12 mm | Two reader-height clamps, one pod cover, one cable cover |
| 2 | Metal M4 × 25 mm | Moving load-cell end |
| 2 | Metal M5 × 12 mm | Fixed load-cell end |

Nuts: **6 standard metal M3, 4 metal M3 nylon-insert locknuts and 4 all-nylon M3**. Washers: **6 metal M3, 4 nylon M3, 2 M4 and 2 M5**. Retain the head, nut, washer and thread-depth requirements in the [BOM](BOM.md#current-ledoled-fasteners).

Use a small amount of cyanoacrylate superglue suitable for the actual printed plastic. A gel is easier to place without running into nearby openings. First try the selected glue on spare prints made from the same filament. [Prusa's adhesive guide](https://blog.prusa3d.com/the-great-guide-to-gluing-and-assembling-3d-prints_44908/) discusses CA on PLA and PETG; its material tests do not qualify this rail joint. For one gel product, [Henkel's technical sheet](https://dm.henkel-dam.com/is/content/henkel/TDS-1363589-1699233-1739050-1801751-Loctite-Super-Glue-Ultra-Gel-Control-Carded-Bottle-4-g-5-g-2016-08-30) calls for close-fitting, clean, dry surfaces, sparing application and a full 24-hour cure. Follow the instructions for the glue you actually buy; do not use glue to fill a badly fitting joint.

## Bond only the fixed rail joints

| Joint | Assembly method | Reason |
| --- | --- | --- |
| Rails 04/05 to carrier 03 | Superglue on the flat mating faces; retain the printed locating keys | Both rails have fixed positions on the moving cradle |
| Main lid 02 | Keep two screws and rear hooks | Wiring, programming and repairs require access |
| Width guides and reader-height clamps | Keep all six clamp screws | Spool width and tag height remain adjustable |
| Load cell and four stops | Keep all eight screws | Cell attachment and stop-gap tuning require mechanical fasteners |
| Reader pod and cable covers | Keep both nylon screws | Board replacement and hidden-wire inspection require access |

The OLED and PN532 already use edge retention without PCB screws. Keep their soft shims removable. The LED strip and insulated converter mounting already use adhesive as described in the [variant](OLED-RGB-VARIANT.md) and [power](DC-POWER.md) guides; that does not mean applying superglue to bare electronics or display glass.

## Glue before installing the mechanism

1. **Dry-fit on the bench.** Use the bare carrier and two rails, away from the base, load cell and electronics. Rotate the second rail 180 degrees so the two slopes face inward. Seat both keys fully. Each rail's flat underside must meet its carrier support without rocking. Correct print interference before using adhesive.
2. **Prepare the mating faces.** Remove dust and grease and let them dry. Lightly roughen only the intended bonding faces if the glue instructions call for it. Keep the keys and their recesses dry. Protect the central screw holes, nut entries, guide slots and 3.5 mm stop-access holes. Do not fit the two unused rail nuts.
3. **Apply sparingly, then seat flat.** Use two small bonding regions on each carrier crossbar, one on each side of the central rail screw hole and inward of the locating-key pockets. Suggested placement regions are 30 × 6 mm each. Keep adhesive inside those regions, clear of their edges; they are placement guides, not areas to flood. Seat the rail with its keys and hold it with light, even pressure for the adhesive's specified fixture time. The glue must not create a thick spacer or lift one end.
4. **Allow full cure with the subassembly open.** Follow the glue's full cure time before loading or enclosing it; initial grip is not full strength. Check that both rails remain flat and that access holes and nut entries are clear. If a rail rocks, lifts or cracks at the joint, do not use that bond to support a spool. Repair or replace affected prints and use the screw-mounted option instead.
5. **Continue the normal assembly.** Install the bonded carrier on the load cell, fit the adjustable guides and follow the [assembly guide](ASSEMBLY.md). Test the complete mechanism progressively within the intended spool range, supported on a bench. Check rail retention, moving-part clearance, guide travel and return to zero. Record the glue, material, cure conditions and results in the [hardware report](HARDWARE-VALIDATION.md).

For CAD users, the suggested bonding regions lie on the carrier's top plane at **Z = 50 mm**, with **X = −45…−15 mm and +15…+45 mm**. Rear-rail regions use **Y = 56…62 mm**; front-rail regions use **Y = −62…−56 mm**. These are assembled-model coordinates, not the rotated STL print coordinates. Geometry inspection found both regions fully within the shared bearing faces. No adhesive thickness or strength is modeled.

Never bridge the moving carrier to the fixed base, lid or stops with adhesive. The load cell must remain the only intended structural load path between them. Keep glue away from the cell's sensing section, wire slack and electronics.

## Service and tradeoff

Treat the carrier and rails as one permanent subassembly. Bonding preserves guide-nut loading, access to the two carrier-to-cell screws and normal lid removal. The carrier and rails can be removed together after disconnecting/freeing the reader cable and removing the guide assemblies and the carrier-to-cell screws. Do not pry a rail loose while it is attached to the load cell.

Replacing an individual bonded rail may damage the carrier. Keep the two rail screws, nuts and washers instead if separate rail replacement matters. All other joints remain as documented in the screw-mounted assembly. This option saves two screws, two nuts and two washers; it does not establish a faster total build because the adhesive must cure.
