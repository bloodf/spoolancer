# Material comparison

These are offline OrcaSlicer estimates, not measured printed masses or approved printer jobs. No model has been sent to a printer.

The headless results describe the old v0.4 external-switch design. The v0.4.1 BOOT revision removes its button cap and changes its cover, and has not been re-sliced. The touch geometry and its estimate are unchanged.

All compared revisions use OrcaSlicer 2.4.1, a P1S reference profile with a 0.4 mm nozzle, Generic PETG, 0.2 mm layers, four walls, five top/bottom layers and 20% gyroid infill. Supports and brims are disabled. Each unique part is sliced separately in its supplied orientation; its material estimate is multiplied by the CAD quantity. Fit coupons are excluded; optional button caps are included.

The reference printer standardizes the comparison. It does not identify the user's Bambu model or qualify these settings for the Snapmaker U1. Inspect each part's bridging and support needs in the actual printer profile before printing. Added supports, brims, wall changes and material density change the final amount.

`v0.2.json`, `v0.3.json`, `v0.4-headless.json` and `v0.4-touch.json` record per-part results. Reproduce on this Mac with `python3 tools/compare_filament.py v0.2` and `python3 tools/compare_filament.py v0.3`. The script reads bundled OrcaSlicer presets into a task-local directory; it does not alter the user's profiles. The exported comparison G-code/3MF files are working artifacts and are excluded from the delivery ZIP.

## Results

| Revision | Estimated PETG |
| --- | ---: |
| v0.2 upright | 617.96 g |
| v0.3 compact | 421.80 g |
| v0.4 headless | 392.32 g |
| v0.4 headless reduction from v0.2 | 225.64 g (36.51%) |
| v0.4 integrated touch | 427.29 g |
| v0.4 touch reduction from v0.2 | 190.67 g (30.85%) |

For v0.3, the final base and carrier were re-sliced after their captive-nut retainers changed. Each per-part result includes the SHA-256 of its source STL. The comparison excludes electronic parts, fasteners and reference spool bodies.

For v0.4 headless, the base, cover and reset cap were sliced again. The other six distinct meshes are byte-for-byte identical to v0.3, verified by SHA-256, so their prior slicing results are reused with the current quantities. To reproduce that historical estimate, restore the headless CAD from the preserved `spoolancer-v0.4-variants.zip` into a separate directory before running `python3 tools/compare_filament.py v0.4-headless`. The current headless CAD cannot reproduce the old cap/cover result.

The source package includes the results and comparison script. Older revisions require their archived STLs and validation reports. Bulk working G-code/3MF files and user-local slicer profiles are not needed to inspect the recorded results.

The touch estimate adds the bezel (11.57 g), stand (18.03 g) and flat bridge (5.72 g), substitutes its blank cover (23.03 g), and omits the headless reset cap. Shared parts are reused only after matching their STL hashes. Reproduce the full estimate with `python3 tools/compare_filament.py v0.4-touch`. The touch assembly is 34.97 g more than the old v0.4 headless assembly under these settings; inspect its tilted pads and short bridges before printing.
