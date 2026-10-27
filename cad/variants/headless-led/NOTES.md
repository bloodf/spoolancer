# Five LEDs with web interface · integrated DC enclosure fit prototype

The front is vertical and part of the **170 × 160 mm base**. The five status lights are centered across the front. The projecting indicator fascia and rear cover have been removed. The NFC guide now carries a covered cable channel and one removable cover. The [other LED variant](../oled-led/NOTES.md) uses the same cradle. Original headless and touchscreen release artifacts remain unchanged.

**Physical validation is false.** The 60 LEDs/metre pitch and 0.96-inch display size are user-confirmed; strip width, coating, PCB/glass/header dimensions, cable flexibility, USB plug, converter and all printed fits remain provisional. Print the small fit samples first. Parameters are editable in [parameters.json](parameters.json), with geometry in [build.py](../../build.py), [indicators.py](../../indicators.py) and [cable_route.py](../../cable_route.py).

## Integrated front and service

Five 6.4 mm holes have **1000/60 = 16.6667 mm** center spacing, at z=29 mm. Their row center is x=0 mm. Each front opening has a 0.4 mm chamfer; the throat diameter remains unchanged. The strip reference is 83.33 × 10 × 0.5 mm with five 5 × 5 × 1.5 mm packages. Only pitch is confirmed. The built-in gutter includes nominal 0.2 mm adhesive and 0.2 mm LED-to-wall allowances, and stays 1 mm above the modeled load-cell lead envelope. The two front lid-support posts move to y=-75 mm to clear the strip.

Install and service the strip through the main lid. Front-control wiring stays entirely in the fixed base. Use suitable insulating adhesive; do not force LED packages against the wall. Check cut pads, coating, adhesion, light bleed and diffusion on real hardware. No transparent light pipes or sealing claim is included.

## Concealed adjustable reader wiring

The channel body is integrated into `nfc_guide`; `nfc_cable_cover` is the only added print. The rear cassette and lower side gutter open together when this cover is removed. Two **M2.5×10 prototype plastic-pilot screws** secure it; retention and screw-head fit require a print test. The cover can be removed without undoing the reader height clamp. A conservative 5.5 mm head/tool corridor is checked along both height slots.

The design uses four fine flexible leads within a **provisional 4 mm bundle envelope**. Inside the cassette, the reader pigtail and U-shaped slack loop move over the full **90–175 mm reader-center range**. The moving route has a modeled **10 mm bend radius** and constant 220.25 mm centerline length including its lower exit. This is an envelope calculation, not a prescribed cut length or proof of how a real cable will flex. Leave additional lead length for soldering and strain relief.

The pigtail passes through a new rear-cover port into an overlapping hood, away from the clamp slots. The channel returns inward below the reader's lowest position and ends at z=42 mm over the carrier's open bay. Its outlet center is at x=guide inner face+7.8 mm, y=0. Across 50–100 mm spool widths, this stays clear of the carrier ribs and fixed lid. No printed bridge connects the moving guide to the fixed base.

A flexible tail passes into the enclosure below the lid, so it is concealed in ordinary assembled views. The base-tail reference is illustrative: its small arc is R5 mm, and its length changes in the drawn width examples. The first bend is a tangent cubic Bezier enclosed by its control-point hull, which provides the checked continuous-width space bound. It does **not** prove a constant-length flex mechanism or acceptable cable force. Dress actual slack within the checked interior space; avoid stiff jackets and tight cable ties. Tie slots are provided at the moving pigtail hood, guide-side anchor and fixed-base saddle. Do not immobilize the flexible section between moving and fixed anchors.

Before use, adjust reader height and spool width through their complete ranges with the cover open, checking bends, strain relief, abrasion, slack and clamp access. Close the cover, then confirm the scale returns to zero and gives the same known load at every setting. Digital clearance does not prove that cable forces are negligible.

## Power and BOOT access

There is **no external USB opening and no pre-cut DC jack hole**. Drill the jack opening only after checking the actual jack body, converter and wiring inside the base. A 12 V-to-5 V converter is required. Do not connect 12 V directly to the ESP32, reader or 5 V strip. The owner chooses a wall attachment for the converter after checking clearances; no converter footprint or reserved mounting volume is claimed. Keep it and its wires away from the moving structure and NFC tail.

The ESP32 rails sit 8 mm inward and 5.5 mm toward the front relative to the original headless model. A **provisional 10 × 22 × 12 mm internal USB plug envelope** is checked; actual right-angle plug fit is unverified. The estimated PCB edge is 11.35 mm from the closed inner wall. The header-wire envelope is 55.8 mm long and 25 mm above the PCB: 22 positions at 2.54 mm pitch span 53.34 mm, plus 2.46 mm total allowance. It leaves 1.1 mm to the modeled moving-cell mount. The PCB itself has 2 mm nominal separation from the load-cell body; unknown solder and cable bends must remain inside checked space.

A dedicated angled **4 mm BOOT bore** accepts the modeled 2.5 mm blunt nonconductive tool. Its position is based on the board photograph and needs a physical button-access test. Open the main lid for programming and disconnect converter power before computer USB, following the project's wiring guide.

## Prints, chamfers and assembly

Print one each of `base`, `top_cover`, `carrier`, `side_guide`, `nfc_guide`, `nfc_pod`, `nfc_pod_cover` and `nfc_cable_cover`, and **two `rim_rail`** pieces: **10 production pieces, nine unique models**. All four main-cover closure screws are M3×10; the old front M3×16 bridge screws and four indicator-back screws are no longer used. Add the two M2.5×10 channel-cover screws noted above. Existing cradle, load-cell and reader hardware remains as documented in the assembly guide.

The external style is modeled straight chamfers, not render-only smoothing. The base plan has 8 mm corner bevels, with 1.0/0.65 mm bottom/top rim chamfers. The lid uses a 0.8 mm outer rim and 0.45 mm aperture chamfer. Carrier lower edges use 0.6 mm, rail outer ends 1.2 mm, plain-guide top 1.2 mm, mast long corners/top 1.5/0.6 mm, NFC pod/cover corners 2.2 mm, and reader-cover rear rim 0.45 mm. Channel, cover and pigtail-hood exposed edges use 0.5 mm chamfers.

Bearing pads, bolt seats, nut pockets, guide sliding feet, the 1.2 mm antenna window and contact slopes within the 100 mm spool width retain their functional faces. Narrow internal supports, clamp faces, cable-entry interfaces and mounting pads intentionally retain flat or sharp edges where a chamfer would weaken walls or alter fit. Circular bores and cable-bend surfaces remain functional curves. **Only individual edge treatments are removal-only**; the integrated front and cable channel add/remove geometry and are reported separately.

STEP/STL files are exported in their intended print poses. Print the base floor down, lid and carrier in their exported inverted poses, rails on their exported side, and NFC parts on the exported side faces. The cable cover opens the lower chute sideways, so any support in that gutter can be removed before assembly. Inspect support needs at the internal front gutter, OLED edge pads, cable channel bridges, hood and lower gutter. Do not assume support-free printing. Fit, stiffness, layer orientation and slicer settings need validation with the intended material.

Fit samples reuse production geometry: `fit_led_face`, `fit_led_carrier`, `fit_esp32`, `fit_cell`, `fit_nfc`. The ESP32 sample does not reproduce the complete closed wall; test the real USB plug and BOOT approach in the assembled base. Do not print the complete assembly STEP: it includes hardware, cable and spool reference envelopes.

## Digital verification

The nominal printed assembly occupies **170 × 160 × 187 mm**, before feet, or **188.4 mm with the nominal feet allowance**; the highest reader position reaches **198.4 mm** before feet. A 200 mm reference spool extends depth to 200 mm. These are geometry bounds, not tested stability or hardware fit.

[validation.json](exports/validation.json) records valid single solids, closed meshes, detected self-intersections, assembly/component clearance, 27 spool cases, nine reader placements and BOOT access. The OLED variant also passes an exact 40 mm continuous vertical insertion sweep for its PCB, glass, components and headers with the main lid removed. The cable report additionally checks nine actual cable paths, conservative continuous reader-height envelopes, clamp-tool access and the full spool-width tail/outlet envelope. [export-verification.json](exports/export-verification.json) reopens every individual STEP/STL and the FreeCAD/assembly STEP. [The edge and integration report](../../edge-finish-verification.json) separates chamfer material removal from redesign changes and compares protected functional regions. Twelve simplified M3 clamp sets and four stop references are shown; remaining hardware and adhesives are not complete manufacturing models.

Physical fit, cable behavior, print strength, tipping stability, accuracy, electronics and electrical safety remain unverified.

```sh
python3 cad/build.py --variant headless-led
python3 cad/verify_exports.py --variant headless-led
python3 cad/render.py --variant headless-led
```
