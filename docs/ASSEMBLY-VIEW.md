# Explore the assembly in 3D

**[Open the interactive assembly viewer](https://bloodf.github.io/spoolancer/)** · [Numbered exploded image](../cad/variants/oled-led/previews/exploded.png) · [Component locations](../cad/variants/oled-led/previews/component-locations.png) · [Print orientations](../cad/variants/oled-led/previews/print-orientations.png)

The v0.8 assembly viewer uses the tessellated solid geometry from the printable CAD. Choose the OLED + five-LED or LED-only model, rotate it, select a part, and move between assembled and exploded positions. Both models contain six printed pieces:

| Number | Printed piece | Purpose |
|---|---|---|
| 01 | Base enclosure | Fixed electronics enclosure with an integrated vertical front |
| 02 | One-piece cradle | Supports the upright spool on integral ramps in a fixed-width bay |
| 03 | Load-cell spacer | Separates the cradle from the moving load-cell end |
| 04 | NFC tower | Glues into the cradle and conceals the reader cavity |
| 05 | NFC reader cassette | Holds the PN532 and adjusts vertically |
| 06 | NFC rear cover | Opens for reader adjustment and cable service |

The viewer is a build guide. It does not connect to an ESP32, read a scale, write tags or change inventory. It runs on a computer or phone and is distributed separately from the device's five web assets.

## Controls

- Rotate, zoom and pan to inspect every side.
- Move the explosion control to separate parts; return it to zero for their actual modeled locations.
- Select a numbered part in the list or model to identify it. Isolate a part to inspect hidden features.
- Hide the reference spool or housing to inspect the electronics and load cell.
- Toggle cable/clearance references to see the nominal reader wire loop and the tail into the base.
- Use a standard camera view or reset the view if the assembly moves off screen.

An exploded view spreads parts for visibility; its separated distances are not assembly measurements or a required installation sequence. To open the current LED/OLED base, remove the spool, disconnect power, free the NFC lead and remove the two moving-end load-cell screws. Lift the cradle and reader assembly while supporting the loose spacer. The fixed-end load-cell screws remain installed. Follow the [assembly guide](ASSEMBLY.md) for the complete sequence. There is no separate lid or sliding side guide.

## What the model represents

The printed parts use the same geometry as the selected variant's individual STEP/STL files. Each of the six parts has its own selection and number. Electronics, fasteners, the load cell and reference spool are simplified envelopes with the locations used by the CAD checks. Their detailed component shapes and wiring are not manufacturer CAD. The nominal spool range is 50–100 mm wide and 160–220 mm in diameter; place the reader-side flange beside the tower.

The converter and jack are not modeled. The builder chooses where to glue an insulated converter and drill the DC-jack opening after checking the real parts against the internal cable route and moving assembly. Cable references show a nominal routing envelope, not simulated bending forces or a physically verified harness. See [power instructions](DC-POWER.md) and [the variant fit notes](OLED-RGB-VARIANT.md).

The fixed base carries the controller, HX711 and the vertical front-wall OLED/LED supports. The load cell connects the fixed base to the spacer and floating cradle. The integral ramps, glued tower, reader cassette, rear cover and spool move with the cradle. Only the reader height adjusts. Its rear cavity hides the wire slack; a flexible tail connects to the fixed electronics inside the base. Wires must not become a second support path or pull on the moving assembly.

The cable toggle shows one nominal routing reference. It does not show a verified cable shape at every reader height. The [print-orientation image](../cad/variants/oled-led/previews/print-orientations.png) uses the actual exported print poses; its panels are independently scaled and do not represent a slicer plate layout.

## Local copy and editable source

Download the assembly-viewer ZIP from the release, extract it, and follow its README to start a local static server. The JavaScript library and model assets are bundled; no CDN connection is required. Opening the HTML directly as a file may be blocked by browser module/data loading restrictions.

In the repository, the viewer is under `viewer/`. Regenerate its mesh assets after a CAD change with:

```sh
python3 tools/build_viewer.py
python3 tools/build_viewer.py --check
```

The shared part labels and explosion directions live in `cad/assembly-parts.json`. Full parametric sources, individual STEP/STL, and editable FreeCAD assemblies remain in `cad/`. Use [the print manifest](PRINTING.md) to print individual parts; reference electronics and assembly STEP are not print plates.

Physical fit, strength, glue-bond retention, tipping stability, cable forces and calibrated weighing accuracy still require bench checks. Rendering a full assembly does not establish those results.
