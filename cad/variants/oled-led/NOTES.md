# 0.96-inch OLED and five LEDs · simplified-fastener DC enclosure fit prototype

The front is vertical and part of the **170 × 160 mm base**. The OLED sits on the left and five status lights on the right. The projecting indicator fascia and rear cover have been removed. The NFC guide carries a covered cable channel and one removable cover. This v0.7 revision uses 20 screws in three sizes for removable rails, or [18 screws with glued rails](../../../docs/GLUE-ASSEMBLY.md); use its complete matching print set. The [other LED variant](../headless-led/NOTES.md) uses the same cradle. Original headless and touchscreen release artifacts remain unchanged.

**Physical validation is false.** The 60 LEDs/metre pitch and 0.96-inch display size are user-confirmed; strip width, coating, PCB/glass/header dimensions, cable flexibility, USB plug, converter and all printed fits remain provisional. Print the small fit samples first. Parameters are editable in [parameters.json](parameters.json), with geometry in [build.py](../../build.py), [indicators.py](../../indicators.py), [cable_route.py](../../cable_route.py), [simple_fasteners.py](../../simple_fasteners.py) and [nfc_mount.py](../../nfc_mount.py).

## Integrated front and service

Five 6.4 mm holes have **1000/60 = 16.6667 mm** center spacing, at z=29 mm. Their row center is x=22 mm; the OLED center is x=-52,z=19 mm. Each front opening has a 0.4 mm chamfer; the throat diameter remains unchanged. The strip reference is 83.33 × 10 × 0.5 mm with five 5 × 5 × 1.5 mm packages. Only pitch is confirmed. The built-in gutter includes nominal 0.2 mm adhesive and 0.2 mm LED-to-wall allowances, and stays 1 mm above the modeled load-cell lead envelope. The two front lid-support posts are at y=-75 mm to clear the strip.

Install and service the strip through the main lid. Remove the spool and set both guides to the 50 mm spool-width setting. Remove the two front lid screws, slide the lid about 1.4 mm toward the front to release its rigid rear hooks, then lift it clear. Reverse this motion when closing; do not flex or lever the hooks. Front-control wiring stays entirely in the fixed base. Use suitable insulating adhesive; do not force LED packages against the wall. Check cut pads, coating, adhesion, light bleed and diffusion on real hardware. No transparent light pipes or sealing claim is included.

The OLED reference uses a **27.3 × 27.3 × 1.6 mm PCB**, 26 × 15 × 3 mm glass, 24 × 14 mm opening, and 21.74 × 11.18 mm active area. Open-top side guides, a bottom stop, a lower front PCB-edge pad and rear edge rails retain the board. A nominal 0.4 mm rear gap allows a soft edge shim. No PCB screw-hole pattern is assumed; do not clamp the glass. Controller, pins and real component protrusions must be checked before power is applied.

These are adjustable design assumptions. [Winstar WEA012864D-03](https://www.winstar.com.tw/products/oled-module/graphic-oled-display/4-pin-oled.html) and [Joy-IT SBC-OLED01](https://www.joy-it.net/en/products/SBC-OLED01) are dimensional references, not identification of the user's module. `fit_oled_face` includes the actual front opening, edge supports and rear rails; test headers and cables in the complete base as well.

## Concealed adjustable reader wiring

The channel body is integrated into `nfc_guide`; `nfc_cable_cover` is the only added print. The rear cassette and lower side gutter open together when this cover is removed. One **nylon M3 × 12 screw, nylon nut and nylon washer** secure it together with a rigid locating tongue. The matching nut pocket replaces the old printed pilot threads. Check tongue fit, head/tool clearance and retention on a print before use. For service, remove the spool, set the guides to the 50 mm spool-width setting and put the reader center at 155 mm. Remove the cable-cover screw, slide the cover 3.5 mm toward the rear, then pull it 20 mm outward to the right. Keep it removable for cable inspection.

The design uses four fine flexible leads within a **provisional 4 mm bundle envelope**. Inside the cassette, the reader pigtail and U-shaped slack loop move over the full **90–175 mm reader-center range**. The moving route has a modeled **10 mm bend radius** and constant 220.25 mm centerline length including its lower exit. This is an envelope calculation, not a prescribed cut length or proof of how a real cable will flex. Leave additional lead length for soldering and strain relief.

The pigtail passes through a new rear-cover port into an overlapping hood, away from the clamp slots. The channel returns inward below the reader's lowest position and ends at z=42 mm over the carrier's open bay. Its outlet center is at x=guide inner face+7.8 mm, y=0. Across 50–100 mm spool widths, this stays clear of the carrier ribs and fixed lid. No printed bridge connects the moving guide to the fixed base.

A flexible tail passes into the enclosure below the lid, so it is concealed in ordinary assembled views. The base-tail reference is illustrative: its small arc is R5 mm, and its length changes in the drawn width examples. The first bend is a tangent cubic Bezier enclosed by its control-point hull, which provides the checked continuous-width space bound. It does **not** prove a constant-length flex mechanism or acceptable cable force. Dress actual slack within the checked interior space; avoid stiff jackets and tight cable ties. Tie slots are provided at the moving pigtail hood, guide-side anchor and fixed-base saddle. Do not immobilize the flexible section between moving and fixed anchors.

Before use, adjust reader height and spool width through their complete ranges with the cover open, checking bends, strain relief, abrasion, slack and clamp access. Close the cover, then confirm the scale returns to zero and gives the same known load at every setting. Digital clearance does not prove that cable forces are negligible.

## Power and BOOT access

There is **no external USB opening and no pre-cut DC jack hole**. Drill the jack opening only after checking the actual jack body, converter and wiring inside the base. A 12 V-to-5 V converter is required. Do not connect 12 V directly to the ESP32, reader or 5 V strip. The owner chooses a wall attachment for the converter after checking clearances; no converter footprint or reserved mounting volume is claimed. Keep it and its wires away from the moving structure and NFC tail.

The ESP32 rails sit 8 mm inward and 5.5 mm toward the front relative to the original headless model. A **provisional 10 × 22 × 12 mm internal USB plug envelope** is checked; actual right-angle plug fit is unverified. The estimated PCB edge is 11.35 mm from the closed inner wall. The header-wire envelope is 55.8 mm long and 25 mm above the PCB: 22 positions at 2.54 mm pitch span 53.34 mm, plus 2.46 mm total allowance. It leaves 1.1 mm to the modeled moving-cell mount. The PCB itself has 2 mm nominal separation from the load-cell body; unknown solder and cable bends must remain inside checked space.

A dedicated angled **4 mm BOOT bore** accepts the modeled 2.5 mm blunt nonconductive tool. Its position is based on the board photograph and needs a physical button-access test. Open the main lid for programming and disconnect converter power before computer USB, following the project's wiring guide.

## Prints, chamfers and assembly

Print one each of `base`, `top_cover`, `carrier`, `side_guide`, `nfc_guide`, `nfc_pod`, `nfc_pod_cover` and `nfc_cable_cover`, and **two `rim_rail`** pieces: **10 production pieces, nine unique models**. Use the [current LED/OLED fastener list](../../../docs/BOM.md#current-ledoled-fasteners): **12 metal M3 × 12, 4 nylon M3 × 12, 2 M4 × 25 and 2 M5 × 12**, for 20 screws. Nuts total 16: eight ordinary metal M3, four metal M3 nylon-insert locknuts and four all-nylon M3. Washers total 16: eight metal M3, four nylon M3, two M4 and two M5. Actual load-cell thread depth, head shapes and washers must match the printed stack.

For the [glued-rail option](../../../docs/GLUE-ASSEMBLY.md), omit two metal M3 × 12 screws, two ordinary metal M3 nuts and two metal M3 washers from that list: **18 screws, 14 nuts and 14 washers** remain. Bond the bare rail/carrier subassembly before fitting the load cell or electronics. The CAD views and reports retain the removable 20-screw assembly. Glued rails stay with the carrier permanently; the separate rail-service instructions below apply only to screwed rails. Adhesive strength is untested.

Two rigid rear hooks and two front screws retain the main lid. Two locating keys and one central screw retain each rim rail. Each key has 0.2 mm nominal side and floor clearance and a 45-degree bed-leading ramp. Install both rails before fitting the width guides; remove the guides before servicing a rail. Seat the rail bearing faces fully before tightening. The width guides keep two screws each; raised stop posts accept M3 × 12 screws with the same provisional 1 mm initial gap. Tune that gap from real movement before weighing. The stops have no demonstrated overload rating.

The PN532 rests on printed PCB-edge pads with thin nonconductive compressible shims. Its front remains 0.8 mm from the 1.2 mm antenna window. Nominal edge clearance is 0.3 mm; three rear fingers leave a 0.3 mm foam gap at component-free side margins. Measure the actual board before choosing shim thickness. The pod cover captures its edges without screws through the PCB holes. Keep all pressure off components and antenna traces. Both the reader cover and cable cover use one rigid tongue and one nylon M3 × 12 screw with a nut and washer. The two reader-height clamps use the same nylon screw size. These joints use no flexible snap arms or heat-set inserts. The modeled nylon hardware has a 5.5 mm diameter × 3 mm high head, an M3 nut 5.5 mm across flats × 2.4 mm high, and a washer 7 mm outside diameter, 3.2 mm inside diameter and 0.5 mm thick. Match those envelopes before assembly.

Open the PN532 pod on the bench, after removing it from the guide. Its cover cannot release while the pod is mounted against the cable channel.

1. With the spool removed, guides at the 50 mm setting and reader center at 155 mm, remove the cable cover using the motion described above.
2. Disconnect and free the pigtail from the channel and its strain relief so it can move without pulling on the board.
3. Support the assembled pod while fully removing both reader-height screws and washers. Leave the unthreaded height-clamp nuts in the mast tracks. Lift the pod from the 155 mm reference position until its center reaches 215 mm, above the guide, then pull it outward 20 mm. **215 mm is a removal position only; the operating range remains 90–175 mm.**
4. On the bench, remove the single pod-cover screw, washer and nut. Slide the cover down 4 mm, then pull it outward 20 mm.
5. Withdraw the PCB straight outward. Keep leads free and do not pull on solder joints. Reverse the sequence to reassemble, then check clamping, cable slack and return to zero.

Test the tongue fit, complete removal path and PCB retention before installing electronics.

The external style is modeled straight chamfers, not render-only smoothing. The base plan has 8 mm corner bevels, with 1.0/0.65 mm bottom/top rim chamfers. The lid uses a 0.8 mm outer rim and 0.45 mm aperture edge chamfer. The opening's plan-corner bevels are reduced from 6 to 2 mm so the lid clears the rear rail during its 1.4 mm forward release and lift. Carrier lower edges use 0.6 mm, rail outer ends 1.2 mm, plain-guide top 1.2 mm, mast long corners/top 1.5/0.6 mm, NFC pod/cover corners 2.2 mm, and reader-cover rear rim 0.45 mm. Channel, cover and pigtail-hood exposed edges use 0.5 mm chamfers.

Bearing pads, bolt seats, nut pockets, guide sliding feet, the 1.2 mm antenna window and contact slopes within the 100 mm spool width retain their functional faces. Narrow internal supports, clamp faces, cable-entry interfaces and mounting pads intentionally retain flat or sharp edges where a chamfer would weaken walls or alter fit. Circular bores and cable-bend surfaces remain functional curves. **Only individual edge treatments are removal-only**; the integrated front and cable channel add/remove geometry and are reported separately.

STEP/STL files are exported in their intended print poses. Print the base floor down, lid and carrier in their exported inverted poses, rails on their exported side, and NFC parts in their exported poses. The revised 1.6 mm pod-cover plate prints with its positive-Y edge down, using the exported -90-degree rotation about X. Use a brim and inspect local supports for the retaining fingers and cable hood. The cable cover prints flat back down, using its exported +90-degree rotation about Y, with its tongue and chute upward; inspect their support needs. The cable cover opens the lower chute sideways, so any support in that gutter can be removed before assembly. Inspect support needs at the internal front gutter, OLED edge pads, cable channel bridges, hood and lower gutter. Do not assume support-free printing. Fit, stiffness, layer orientation and slicer settings need validation with the intended material.

Fit samples reuse production geometry: `fit_led_face`, `fit_led_carrier`, `fit_esp32`, `fit_cell`, `fit_nfc`, and `fit_oled_face`. The ESP32 sample does not reproduce the complete closed wall; test the real USB plug and BOOT approach in the assembled base. Do not print the complete assembly STEP: it includes hardware, cable and spool reference envelopes.

## Digital verification

The nominal printed assembly occupies **170 × 160 × 187 mm**, before feet, or **188.4 mm with the nominal feet allowance**; the highest reader position reaches **198.4 mm** before feet. A 200 mm reference spool extends depth to 200 mm. These are geometry bounds, not tested stability or hardware fit.

[validation.json](exports/validation.json) records valid single solids, closed meshes, detected self-intersections, assembly/component clearance, 27 spool cases, nine reader placements and BOOT access. The OLED checks include a 40 mm continuous vertical insertion sweep for its PCB, glass, components and headers with the main lid removed. The cable report additionally checks nine actual cable paths, conservative continuous reader-height envelopes, clamp-tool access and the full spool-width tail/outlet envelope. [export-verification.json](exports/export-verification.json) reopens every individual STEP/STL and the FreeCAD/assembly STEP. The current variant's `validation.json` records its chamfer operations. [edge-finish-verification.json](../../edge-finish-verification.json) is retained as historical v0.6 evidence and does not validate the v0.7 joints. Hardware references are simplified envelopes; they do not model threads, shims or adhesive behavior. Current fastener and service checks are recorded in the selected variant's reports.

Physical fit, cable behavior, print strength, tipping stability, accuracy, electronics and electrical safety remain unverified.

```sh
python3 cad/build.py --variant oled-led
python3 cad/verify_exports.py --variant oled-led
python3 cad/render.py --variant oled-led
```
