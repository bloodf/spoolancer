# Spoolancer printable CAD

The current **v0.8 LED/OLED models use six printed pieces**: a low electronics base, one-piece cradle, load-cell spacer, NFC tower, reader cassette and flat rear cover. The cradle has integral spool ramps and fixed sides. The reader height is the only adjustment. The tower glues into the cradle; the rear cover remains removable for wiring access.

[Interactive 3D assembly](https://bloodf.github.io/spoolancer/) · [Numbered exploded view](variants/oled-led/previews/exploded.png) · [Print orientations](variants/oled-led/previews/print-orientations.png) · [Print manifest](../docs/PRINTING.md) · [Parts list](../docs/BOM.md)

The new geometry replaces the previous LED/OLED lid, separate rails, sliding side guides and small latches. Use a complete matching v0.8 set. Do not mix it with v0.7 parts or follow the old glued-rail instructions. The [assembly guide](../docs/ASSEMBLY.md) and [glue guide](../docs/GLUE-ASSEMBLY.md) describe the current joints.

Physical fit, bond retention, strength, NFC performance and weighing accuracy remain untested. The six-part design targets upright spools 50–100 mm wide and 160–220 mm in diameter, used for occasional weighing. Place the reader-side flange beside the tower and rotate the seated spool to align its tag.

## Current LED/OLED files

| Variant | Printable files | Editable assembly | Notes |
|---|---|---|---|
| Five LEDs + web controls | [Six STEP/STL pairs](variants/headless-led/exports) | [FreeCAD](variants/headless-led/exports/spoolancer-v0.8-headless-led.FCStd) · [Assembly STEP](variants/headless-led/exports/spoolancer-v0.8-headless-led-assembly.step) | [Variant notes](variants/headless-led/NOTES.md) |
| 0.96-inch OLED + five LEDs | [Six STEP/STL pairs](variants/oled-led/exports) | [FreeCAD](variants/oled-led/exports/spoolancer-v0.8-oled-led.FCStd) · [Assembly STEP](variants/oled-led/exports/spoolancer-v0.8-oled-led-assembly.step) | [Variant notes](variants/oled-led/NOTES.md) |

Both variants use the existing ESP32-S3 and five LEDs at 16.67 mm centres. Their vertical front is part of the base. The DC-jack opening is user-drilled after checking the actual jack and converter. A separate 12 V-to-5 V converter is required. No converter bracket is prescribed.

Each export folder contains one STL and one STEP per production part. Print each once. Individual files already have their intended print rotation and a bottom at Z = 0. The named `*-assembly.step` and `.FCStd` files retain assembled coordinates and include simplified hardware references. Use those to inspect or edit the design, not as slicer plates.

| Part | Intended bed face |
|---|---|
| `base` | Flat underside of the enclosure |
| `cradle` | Broad flat underside of the cradle |
| `cell_spacer` | Flat spacer face |
| `nfc_tower` | Broad front face, rear cavity upward |
| `nfc_cassette` | Closed back face, antenna cup upward |
| `nfc_cover` | Flat panel face |

The exact orientations and bed-contact areas are recorded in each variant's `exports/validation.json`. The [orientation preview](variants/oled-led/previews/print-orientations.png) uses actual print-oriented meshes, with each panel independently scaled. It is not a packing layout or a substitute for slicer inspection. See [printing instructions](../docs/PRINTING.md) for current slicing evidence and unresolved physical checks.

The base and front controls stay fixed. The cell spacer, cradle, glued tower and reader float on the moving end of the load cell. Wires crossing into the fixed base need slack so they do not pull on the weighing assembly. To service the base, remove the spool, disconnect power and the reader lead, remove the two moving-end load-cell screws, and lift the cradle while supporting the loose spacer.

## Edit and regenerate the current models

The source files are:

| File | Purpose |
|---|---|
| [simple-parameters.json](simple-parameters.json) | Shared dimensions for the six-part geometry |
| `variants/{headless-led,oled-led}/parameters.json` | Variant overrides, applied after shared dimensions |
| [simple_geometry.py](simple_geometry.py) | Printable solids, reference components, print poses and joint geometry |
| [build_simple.py](build_simple.py) | Exports, assembled/print scenes and geometry checks |
| [verify_simple_exports.py](verify_simple_exports.py) | Reopens exported files and checks their correspondence |
| [render_simple.py](render_simple.py) | Numbered publication views from the exported scene meshes |
| [assembly-parts.json](assembly-parts.json) | Shared part numbers, descriptions and illustrative explosion offsets |

The public entry points remain `build.py`, `verify_exports.py` and `render.py`. They select the v0.8 implementation for the current LED/OLED variants and retain the original implementation for older headless/touch models.

Run from the repository root with Python supplied by, or compatible with, your FreeCAD installation. The generator and verifier need the native `FreeCAD`, `Part`, `Mesh` and `MeshPart` modules. Rendering also needs VTK, NumPy and Matplotlib. A package named FreeCAD from PyPI is not a substitute for the desktop application's native modules.

```sh
python3 cad/build.py --variant headless-led
python3 cad/verify_exports.py --variant headless-led
python3 cad/render.py --variant headless-led
python3 cad/build.py --variant oled-led
python3 cad/verify_exports.py --variant oled-led
python3 cad/render.py --variant oled-led
python3 tools/build_viewer.py
python3 tools/build_viewer.py --check
node viewer/model.test.mjs
```

For the project's macOS FreeCAD installation, the interpreter path is `/Applications/FreeCAD.app/Contents/Resources/bin/python`. If needed, set `FREECAD_LIB` to the directory containing the compatible native FreeCAD module. Use the corresponding interpreter and module directory for your own installation; do not assume host-specific paths work on Linux or Windows.

Each build emits six individual STEP/STL pairs, a complete STEP/FCStd assembly, a validation report, `previews/scene.json` in assembled coordinates, and `previews/scene-print.json` in actual print poses. The renderer generates assembled, exploded, component-location, service, front-control, edge, cable and print-orientation views. Its manifest records source and image hashes. The viewer builder consumes the assembled scene and shared part map, and rejects unlabeled, missing or multiply assigned meshes.

Keep manual CAD edits in a separate file if they must survive regeneration. The FreeCAD Dimensions spreadsheet records the build values; regeneration reads JSON and the generator source. STEP timestamps and FreeCAD metadata can change binary hashes between rebuilds. Geometric correspondence is checked separately.

## What the checks establish

Current per-variant `validation.json` and `export-verification.json` files record the performed geometric checks and exact source hashes. They distinguish printable solids from reference components. The source includes checks for intersections, spool envelopes, reader positions, print poses and service lifts; report results apply only to the recorded geometry.

Electronics, fasteners, the load cell and spool are simplified envelopes, not manufacturer CAD. Thread form, actual wire stiffness, adhesive behavior and real component variations are not simulated. The rear wire cavity contains only a nominal route. Confirm cable slack and scale zero through the full reader travel. Stop clearance is a starting setting, not demonstrated overload protection.

`physical_validation` remains `false`. Check received hardware, slice the actual exports and physically test the assembly before treating the design as a proven tool. Current evidence and open checks are recorded in [verification status](../docs/STATUS.md).

## Preserved legacy models

The original **headless v0.4.1** and **touch v0.4** mechanisms retain separate rails, sliding guides and their original part counts. They are not the six-part redesign. Their old material estimates, screenshots and validation reports apply only to those preserved files.

- [Original headless notes](variants/headless/HEADLESS-NOTES.md) and [exports](variants/headless/exports). The convenience copy in `cad/exports` is also this legacy model. It has nine production pieces, including two identical rim rails, plus its fit samples.
- [ESP-8048S043C-I touch notes](variants/touch-8048/TOUCH-NOTES.md) and [exports](variants/touch-8048/exports). The twelve-piece legacy assembly includes its separate display stand and bezel.
- [Earlier v0.3 compact design](../archive/v0.3-compact/SUPERSEDED.md) and [v0.2 upright design](../archive/v0.2-upright/cad/README.md) remain archived.
- [Historical v0.6 edge-finish report](edge-finish-verification.json) and [earlier material comparison](../analysis/material-comparison/README.md) do not validate v0.8.

Use `--variant headless` or `--variant touch-8048` with the same build, verification and render entry points to regenerate those legacy models. Their shared dimensions remain in `parameters.json`, with touch overrides under `variants/touch-8048/parameters.json`. Omitting `--variant` addresses the legacy headless convenience copy. For the current redesign, always name `headless-led` or `oled-led` explicitly.

### Supported JSON controls

`simple-parameters.json` contains the reference spool sizes and fit-check cases, ramp geometry, stop positions, reference reader height and provisional BOOT location/bore. These controls must still pass the geometry checks after editing. The reader travel (85–170 mm), LED pitch (60 pixels/metre), cell-hole pattern, board envelopes, wall thicknesses and overall construction dimensions are fixed in `simple_geometry.py`; edit the generator to change them. They are not unused JSON settings. The build command’s `--variant` selects the OLED option; variant JSON records that model and the revision. The native FreeCAD document contains editable solids and a dimension record, not a spreadsheet-driven parametric feature tree.
