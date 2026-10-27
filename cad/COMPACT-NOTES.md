# v0.3 compact upright prototype

This revision keeps the ESP32-S3, PN532, selected load cell, HX711 and display. It replaces the full V block with two hollow rim rails, opens the carrier, reduces the base, and moves the PN532 to the adjustable right guide. It is CAD checked, not physically validated.

The preceding CAD package is preserved unchanged in `archive/v0.2-upright/cad/`.

## Dimensions and material evidence

| Quantity | v0.2 | v0.3 |
|---|---:|---:|
| Fixed base footprint | 190 × 230 mm | 170 × 190 mm |
| Base footprint area | 43,700 mm² | 32,300 mm² |
| Quantity-inclusive CAD solid volume | 885,509.25 mm³ | 444,606.27 mm³ |
| Reference 200 mm spool loaded height, nominal feet included | 271.87 mm | 256.87 mm |
| Lowest unloaded assembly height, nominal feet included | 121.4 mm | 185.4 mm |
| Highest unloaded assembly height, nominal feet included | 121.4 mm | 199.8 mm |

Footprint area falls **26.09%** and CAD solid volume falls **49.79%**. The latter is analytic solid volume multiplied by the actual print quantities, including three optional button caps and excluding fit samples. **It is not grams or sliced filament.** The separate OrcaSlicer 2.4.1 comparison, using the same 0.2 mm / four-wall / five-top-and-bottom / 20% gyroid PETG profile, estimates **421.80 g versus 617.96 g**, or **31.74% less material**. Those are slicer estimates, not weighed prints; details and STL hashes are in `analysis/material-comparison/`.

The empty assembly is taller because the integrated reader needs a vertical mast. Its fixed mast is 184 mm high; raising the reader fully sets the printed assembly height to 198.4 mm. These dimensions exclude the nominal 1.4 mm net foot contribution. The whole unloaded assembly remains within the 170 × 190 mm footprint at every modeled width/height setting, including the pod, cover and mounting tabs. The pod's outer back reaches X = 67.2 mm at the 100 mm spool setting, inside the base edge at X = 85 mm.

The loaded depth includes spool overhang: 195 / 215 / 225 mm for diameters 160 / 200 / 220 mm. The 200 mm reference spool makes the assembly 170 × 215 × 256.87 mm including nominal feet. The 220 mm case reaches 278.42 mm high. Compactness here means a smaller base, less modeled/sliced material, and a lower loaded spool; it does not mean the unloaded assembly is shorter.

## Print quantities and orientation

| STL | Quantity | Exported orientation |
|---|---:|---|
| `base.stl` | 1 | Floor down |
| `top_cover.stl` | 1 | Outside face down |
| `carrier.stl` | 1 | Top down |
| `rim_rail.stl` | 2 | Outer end face down; rotate the second rail 180° in the assembly |
| `side_guide.stl` | 1 | Left guide inner face down |
| `nfc_guide.stl` | 1 | Right guide inner face down |
| `nfc_pod.stl` | 1 | Antenna window down |
| `nfc_pod_cover.stl` | 1 | Flat face down |
| `button_cap.stl` | 3 optional | Head down |

There are nine main printed pieces and three optional caps. Fit samples are separate and are not included in assembly volume/material totals. The two guides are now different parts. The old `saddle.stl` and old v0.2 assembly exports belong only in the archive.

The rail's outer end face is deliberately the print base: its hollow bays are present from the first layer and the inclined roof moves gradually across successive layers, instead of requiring a long roof bridge. The guide's mast begins on the same inner-face plane as the lower guide. Small captive-nut channels and holes still require normal short-span bridging. The intended orientations avoid large supports; printer-specific quality remains untested.

## Geometry and adjustment

- Same 80 × 12.7 × 12.7 mm cell stack: fixed M5 end below, moving M4 end above. The sensing section remains clear.
- The moving carrier is 140 × 130 mm. Its upper surfaces remain at Z = 50 mm, rib bottoms at Z = 38 mm. All spool-contact parts and the reader are carried by the cell.
- Rim rails span Y = 34…65 mm and −65…−34 mm. Their contact surface is `Z = 40 + |Y| tan(30°)`. Rim contacts occur at |Y| = 40 / 50 / 55 mm for 160 / 200 / 220 mm diameters.
- Guides retain the 50–100 mm spool width range. Set the inner spacing to spool width + 4 mm, giving a nominal 2 mm gap per side. The range is a CAD envelope, not a fit guarantee for every commercial spool profile.
- The smallest modeled flange bottom is Z = 52.38 mm, above the carrier top. Lowering the V apex by 15 mm preserves this clearance while reducing rail height and loaded spool height.
- PN532 center slides independently from Z = 90 to 175 mm. Nominal spool-axis heights are 132.38 / 155.47 / 167.02 mm, so the range includes all three modeled hub centers. Rotate the spool to put the tag opposite the antenna; adjust height to suit the actual tag position.
- The pod's front window is nominally 2 mm from the spool side. Window thickness is 1.2 mm, with 0.8 mm PCB air gap: nominal antenna-to-spool separation is 4 mm. Actual read/write performance remains unverified.
- Keep the PN532 antenna side facing the window. The mounting-hole pattern and board/component envelopes are estimates of the pictured clone and must be checked against the received board. The fit pod is the actual pod geometry.

## Fastener changes and assembly order

Existing cell mounting is unchanged: two M5×12 with nominal 1 mm washers at the base, two M4×25 with nominal 0.8 mm washers at the carrier. Nominal cell engagement is 5.2 mm for both. **Measure actual threaded depth before tightening; 6H is a thread class, not a depth.**

1. Assemble the fixed electronics and cell. Reuse the four adjustable M3×20 stop screws and captive M3 nyloc nuts. Stops begin 1 mm below the carrier and must be tuned from measured deflection; this is not validated overload protection.
2. Mount the carrier to the M4 end. The two cell screws remain accessible from above through the open frame.
3. Slide four plain M3 nuts into the guide channels from the carrier's left/right edges (X = ±70 mm), two per side. The channels lie at Z = 44.4…47.0 mm under a 3 mm retaining roof. Each guide uses two **M3×12** screws with nominal 0.5 mm washers. The 5 mm guide feet sit at Z = 50…55 mm; screw tips end at Z = 43.5 mm, clear of hardware below. Orient each nut with its flats against the narrow channel walls. Tightening pulls the nuts against the channel roof and clamps the feet to the carrier. This replaces the v0.2 M3×20 guide-foot screws.
4. Slide four plain M3 nuts into the rail channels from the front/rear carrier edges (Y = ±65 mm). These channels also lie at Z = 44.4…47.0 mm with a 3 mm retaining roof. Attach the two rails using four **M3×12** screws and nominal 0.5 mm washers. The counterbore seat is at Z = 54 mm; screw tips end at Z = 42.5 mm. This replaces the former M3×8 saddle screws. Counterbores provide driver access from above, and separate 3.5 mm holes access all stop screws. Both rail and guide nuts can be inserted before the carrier is mounted if that is easier; keep them engaged loosely with the screws while handling.
5. Insert two **M3 nylon nuts** from the tops of the NFC guide's vertical channels. Use two **M3×8 nylon screws** and nominal 0.5 mm nylon washers through the pod's side tabs. The tab is 2 mm thick, the mast 6 mm; modeled screw-tip position is 0.5 mm behind the guide's inner face. Keep the hardware near the antenna nonmetallic.
6. Mount the PN532 using the matching pair of four possible PCB holes. The pod provides 2.8 mm holes and front countersinks: provisional **M2.5×8 flat-head nylon screws and nylon nuts**, two of each. Confirm actual head seating, nut/component clearance and board hole pitch on the fit pod before the final build. These PCB fasteners are not included in the simplified collision model.
7. Close the pod with two provisional **M2.5×6 nylon screws** into its 2.1 mm printed pilots. They are fit-dependent printed threads, without heat-set inserts. The cover holes are 2.7 mm. Check fit and avoid overtightening.

Before closing the base, insert four plain M3 cover nuts horizontally from the housing interior toward each side post. Their channels are at Z = 29.4…32.0 mm, under a 3 mm post roof. Use four **M3×10** cover screws with nominal 0.5 mm washers from Z = 38 mm; tips end at Z = 28.5 mm. Fit these nuts before lowering the electronics-bearing cover. The display, button-board and rubber-foot hardware is otherwise carried over; verify existing instructions against the received components. Guide screws and pod screws are accessed from outside the spool. Remove the spool before fitting nuts or changing width.

## Wiring and weighing limitations

Use direct solder leads or a connector arrangement that fits the modeled 7 × 12 × 8 mm PN532 wire allowance. A large Dupont plug stack is not qualified. Leads exit the bottom of the pod, run along the moving guide, and form a loose silicone service loop to the fixed base. Leave slack for both height and width adjustment. Secure the loop so it cannot rub a spool, rail, cover, stop or sensor flexure.

Moving wires can apply a measurable force and shift zero. **A flexible loop is a proposal, not proof of negligible bias.** On the bench, compare repeated weights with the reader at minimum/maximum height and both guide widths; inspect zero return and wire contact. Calibrate with the complete cradle, reader and installed wiring. If repositioning changes zero, re-zero and improve the loop before relying on measurements.

The smaller base and lighter cradle also change tipping resistance and stiffness. Loaded stability, off-center accuracy, creep, structural strength, NFC read/write range, and overload protection remain untested. The supported range concerns static occasional weighing, not feeding filament during printing.

## Completed checks

`cad/exports/validation.json` records valid single solids, closed STL meshes, no mesh self-intersections, no unintended printed-part or hardware-envelope intersections, the retained ESP wire allowance, and:

- 27 spool/pod combinations: three diameters × three widths × three reader heights;
- nine reader-placement checks against support geometry, board, component and wire envelopes;
- quantity-aware CAD volume and footprint comparison.

`cad/exports/clamp-fastener-check.json` checks 20 nominal guide/rail/cover screw, washer and nut settings and records the retaining roofs. `cad/exports/pod-fastener-check.json` additionally checks the two nominal M3×8 pod mounting screws, washers and nuts in all nine width/height settings. `cad/exports/export-verification.json` records reopening every exported STL, native FreeCAD assembly and STEP assembly. Geometry checks do not establish real hardware fit, print quality or mechanical performance. Most fasteners and the flexible service loop are not modeled. The next meaningful gate is fit samples and physical assembly.
