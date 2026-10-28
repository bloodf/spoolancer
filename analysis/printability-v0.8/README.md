# Offline printability evidence

`tools/slice_simple.py` slices each production STL on its exported bed face
with OrcaSlicer. It does not send jobs to a printer. The checks provide toolpath
evidence, not physical strength, adhesion, dimensional accuracy, or print quality.

The diagnostic setup is the same for each revision within each printer:

- Bundled Bambu Lab P1S 0.4 mm or Snapmaker U1 0.4 mm machine preset.
- The printer's bundled PETG preset; temperature, flow and speed remain specific
  to that preset. Comparisons between printers are not speed benchmarks.
- 0.20 mm layers including the first layer; four walls; five top and bottom
  layers; 20% gyroid infill.
- Automatic **normal supports enabled**, including supports on model surfaces,
  with the bundled 30-degree threshold and 0.20 mm top/bottom support gaps.
- No brim, skirt or raft. Automatic orientation is disabled. Arrange may
  translate and rotate a part within the XY plane, while retaining its exported
  bed face. Arc fitting is disabled to make extrusion accounting explicit.

Run from the repository root after building the simplified exports:

```sh
python3 tools/slice_simple.py --baseline-ref ac554fe77525a2b2db122d7b7bb781936e773888
```

That commit is the v0.7.0-alpha.1 print set. Baseline manifests preserve its
original STL hashes and quantities. `--prepare-only` resolves profiles and
extracts the baseline without slicing. `--variant oled-led --printer u1` selects
one configuration. `--only nfc_cover` makes a partial report with a distinct
filename; it must not be cited as a complete assembly result.

Each summary records the source STL, script, validation report and resolved
preset hashes. A changed input invalidates cached results. The exported 3MF
settings are checked against every diagnostic override before a result passes.
Large 3MF files, settings, baseline STL copies and logs are retained under the
ignored `local/` directory; committed JSON reports contain the evidence and
hashes needed to reproduce a run.

The [orientation check](orientation-verification.json) inspects the actual 3MF
transform matrices for all 60 completed slices. All preserve the exported bed
face without tilt, inversion or scaling. Some parts are rotated in the bed plane;
the `orientation_preserved` field in summaries refers to the bed face, not a
fixed XY heading.

## Reading the measurements

| Field | What it measures |
|---|---|
| `bed_contact_area_mm2` | Sum of horizontal STL triangles at the lowest Z, without supports or brim. |
| `bed_contact_bounds_xy_mm` | Bounds of that flat geometric contact in the exported STL coordinates. |
| `first_layer_toolpath_bounds_xy_mm` | Bounds of positive model extrusion on layer one, expanded by the recorded line width. |
| `first_layer_deposited_area_estimate_mm2` | First-layer model extrusion volume divided by layer height; approximate deposited area, not a polygon union. |
| `slicer_object_footprints` | Orca's exported object-area and bounding-box metadata, retained without treating it as an adhesion result. |
| `support_extrusion_volume_mm3` | Extrusion on XY moves tagged as support or support interface, excluding stationary unretraction and startup purge. |
| `filament_g`, `filament_mm` | Orca's total filament estimate, including any generated supports. |
| `model_time_s`, `total_estimated_time_s` | Orca's model and total time estimates. Separate-part totals repeat startup overhead. |
| `metadata_warnings`, `log_diagnostics` | Warnings actually exposed by the CLI and sliced-project metadata. |

Zero generated support means this diagnostic profile did not generate supports.
It does not prove the part will print successfully. An empty warnings list means
no warning was exported through those channels; it does not replace inspection
of the sliced preview or a physical test print.

## v0.7 baseline

All 36 separate-part baseline slices completed: nine unique parts in each of two
variants, on each of two printers. The rail quantity is two, so each assembled
variant contains ten printed pieces.

| Variant | Printer profile | Estimated filament, supports included | Generated support volume |
|---|---|---:|---:|
| LED | P1S | 448.72 g | 26,276 mm³ |
| LED | U1 | 443.00 g | 26,044 mm³ |
| OLED + LED | P1S | 449.55 g | 26,611 mm³ |
| OLED + LED | U1 | 443.82 g | 26,368 mm³ |

The old `nfc_pod_cover` has only **48.76 mm²** of flat STL contact and stands
55.55 mm high in its exported print pose. It generated approximately 2,247–2,278
mm³ of support. The old `nfc_guide` generated approximately 14,938–15,110 mm³ of
support. These are specific printing burdens, independent of an untested claim
about physical strength.

The P1S runs exported `bed_temperature_too_high_than_filament` from the bundled
PETG profile. This warning is retained in the reports; the tool does not silently
change the temperature to suppress it. U1 baseline runs exported no warnings
through the recorded channels.

## v0.8 result

All 24 new slices completed: six parts in each variant, on each printer. All six
parts have broad flat contact in their exported poses. The smallest is the
28 × 27 mm load-cell spacer, with 715 mm² of actual flat STL contact. The new
flat NFC rear cover contacts the bed over **14,692 mm²** and prints 3 mm high.
None of the six new parts uses the old cover's narrow-edge print pose.

The cradle, spacer, cassette and rear cover generated no supports on either
printer. The **base and NFC tower still generate localized supports**. Remove
those supports and check the openings before installing electronics or hardware:

| Part | Locations that require sliced-preview inspection and cleanup |
|---|---|
| Base | Round LED apertures, the OLED window when present, and underside load-cell screw counterbores. |
| NFC tower | Long slot-counterbore shoulders, the bottom cable-exit roof, and cover-fastener/side-loaded nut pockets. |

These locations were identified from actual support toolpaths and the source
geometry. Keep normal automatic supports enabled for this diagnostic result.
Choosing to bridge any opening without supports requires a separate physical
test; the project does **not** claim that the entire assembly is support-free.

| Variant / printer | v0.7 filament → v0.8 | v0.7 support → v0.8 | Estimated separate-job time, v0.7 → v0.8 |
|---|---:|---:|---:|
| LED / P1S | 448.72 → 521.16 g | 26,276 → 3,597 mm³ | 15.57 → 15.25 h |
| LED / U1 | 443.00 → 511.61 g | 26,044 → 3,575 mm³ | 19.43 → 17.58 h |
| OLED + LED / P1S | 449.55 → 520.86 g | 26,611 → 4,397 mm³ | 15.66 → 15.27 h |
| OLED + LED / U1 | 443.82 → 511.37 g | 26,368 → 4,374 mm³ | 19.62 → 17.62 h |

The design reduces printed pieces from **10 to 6** and generated support volume
by **83.4–86.3%**. Thicker, simpler parts use **15.2–16.1% more estimated
filament** at these settings. This is a deliberate geometry tradeoff, not a
measured improvement in physical strength. All dimensions, hardware fits, glue
bonds and load-bearing behavior still need a physical build.

The [comparison report](comparison.json) links all eight complete revision,
variant and printer summaries by hash. It also lists the support locations and
the retained P1S temperature warning.
