# Explore the assembly in 3D

**[Open the interactive assembly viewer](https://bloodf.github.io/spoolancer/)** · [Numbered exploded image](../cad/variants/oled-led/previews/exploded.png) · [Component locations](../cad/variants/oled-led/previews/component-locations.png)

The assembly viewer uses the tessellated solid geometry from the printable CAD. Choose the OLED + five-LED or LED-only model, rotate it, select a part, and move between assembled and exploded positions.

The viewer is a build guide. It does not connect to an ESP32, read a scale, write tags or change inventory. It runs on a computer or phone and is distributed separately from the device's five web assets.

## Controls

- Rotate, zoom and pan to inspect every side.
- Move the explosion control to separate parts; return it to zero for their actual modeled locations.
- Select a numbered part in the list or model to identify it. Isolate a part to inspect hidden features.
- Hide the reference spool or housing to inspect the electronics and load cell.
- Toggle clearance references to see provisional plug and wire space.
- Use a standard camera view or reset the view if the assembly moves off screen.

An exploded view spreads parts for visibility; its separated distances are not assembly measurements or a required installation sequence. In the current LED/OLED design, remove the spool, set the guides to the 50 mm spool-width setting and remove the two front lid screws. Slide the lid about 1.4 mm forward to release its rigid rear hooks before lifting it. Follow the [assembly guide](ASSEMBLY.md) for the keyed rails and single-screw NFC covers. The PN532 pod must be removed from the guide before its cover can open on the bench; an exploded-view offset is not this removal path.

## What the model represents

The printed parts use the same geometry as the selected variant's individual STEP/STL files. The two rim rails are separate physical instances. Electronics, the load cell and reference spool are simplified envelopes with the locations used by the CAD checks. Their detailed component shapes and wiring are not manufacturer CAD.

The converter and jack are not modeled. The builder chooses where to glue an insulated converter and drill the DC-jack opening after checking the real parts against the internal cable route and moving assembly. Cable references show a nominal routing envelope, not simulated bending forces or a physically verified harness. See [power instructions](DC-POWER.md) and [the variant fit notes](OLED-RGB-VARIANT.md).

The fixed base carries the controller, HX711 and the vertical front-wall OLED/LED supports. The load cell connects the fixed base to the floating carrier. The rim rails, side guides, NFC pod, cable channel and spool move with that carrier. The channel hides the reader-height slack behind a removable cover; a flexible tail connects to the fixed electronics inside the base. Wires must not become a second support path or pull on the moving assembly.

## Local copy and editable source

Download the assembly-viewer ZIP from the release, extract it, and follow its README to start a local static server. The JavaScript library and model assets are bundled; no CDN connection is required. Opening the HTML directly as a file may be blocked by browser module/data loading restrictions.

In the repository, the viewer is under `viewer/`. Regenerate its mesh assets after a CAD change with:

```sh
python3 tools/build_viewer.py
python3 tools/build_viewer.py --check
```

The shared part labels and explosion directions live in `cad/assembly-parts.json`. Full parametric sources, individual STEP/STL, and editable FreeCAD assemblies remain in `cad/`. Use [the print manifest](PRINTING.md) to print individual parts; reference electronics and assembly STEP are not print plates.

Physical fit, stiffness, tipping stability, cable forces and calibrated weighing accuracy still require bench checks. Rendering a full assembly does not establish those results.
