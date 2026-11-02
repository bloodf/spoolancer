# Glue the tower and base top cover

The v0.8.1 LED/OLED build has **three adhesive joints**: the cable cap glues to the top cover, the top cover glues to the stationary base rim, and the broad NFC tower base fits into the cradle's shallow three-sided nest. The ramps and left fence are already integral to the cradle. The old v0.7 glued-rail option is superseded and does not apply to these files.

The complete current build uses **12 screws, 8 nuts and 10 washers**. Gluing the tower is part of that design, not an additional hardware reduction. See the [current fastener list](BOM.md#current-ledoled-fasteners). Adhesive strength and the completed mechanism remain physically untested.

## Dry-fit first

1. Work with the bare cradle and tower on a bench, away from the load cell and electronics. Check that the tower seats squarely in its nest and its front face aligns with the right side of the spool bay.
2. Check the cassette, clamp screws, cover and reader-height travel before bonding. Confirm that the actual PN532 reads your intended tags through the 3 mm front panel. Verify the cassette and wiring still fit after the cover is installed.
3. Mark the mating faces only. The nominal tower bottom is 0.2 mm above the cradle floor, and the side nest has 0.2 mm clearance. These are assembly allowances, not a specified adhesive bondline or strength rating. Correct a rocking or interference fit before glue.

## Bond only the tower nest

Use an adhesive suitable for the actual printed plastic and follow its manufacturer's surface preparation, fixture and full-cure instructions. Test the selected glue on spare prints made from the same filament. A small, controlled application is easier to keep away from the cable opening than flooding the joint. [Prusa's printed-part bonding guide](https://blog.prusa3d.com/the-great-guide-to-gluing-and-assembling-3d-prints_44908/) is background reading; it does not qualify this joint.

Apply adhesive to the broad mating regions of the tower base and nest, leaving the **8 × 12 mm cable opening** clear. Align the tower without forcing it and hold it square for the adhesive's fixture time. Allow full cure before installing electronics or applying load. Initial grip is not full strength.

In assembled CAD coordinates, the cradle floor is **Z=48.5 mm**. The tower base spans approximately **X=52…82 mm, Y=−50…50 mm**, and begins at **Z=48.7 mm**. The cable opening is **X=66…74 mm, Y=−6…6 mm**. Do not fill the reader slots, nut entries or nearby stop-driver bores. Chamfered corners reduce the exact contact outline.

## Glue the base top cover last

Follow the [cover retrofit guide](TOP-COVER.md). Dry-fit the cover, electronics and cradle and test operation before bonding. The cover glues only to the stationary rim at Z=32 mm, never to the spacer, cell or cradle. All existing six prints remain unchanged; no additional screws are needed. Removing the cradle still leaves the cover in place, so board service requires releasing this adhesive bond. Place the soldered cable through the edge-open slot, then seat the small cap and retain its flange with small adhesive spots on the fixed cover. Keep adhesive away from the lead and leave it free in the smaller opening. The cap is positioned by rigid seating surfaces, not a snap latch; adhesive supplies retention. Keep the NFC rear cover removable.

## Keep these joints removable

| Joint | Retention | Reason |
| --- | --- | --- |
| Load cell and spacer | Two M5 and two M4 screws with washers | Accurate mounting and service access |
| Four travel stops | Four M3 screws and locknuts | Gap must be adjusted on the real assembly |
| Reader cassette | Two nylon M3 screws, nuts and four washers | Reader height and PCB service |
| Rear cover | Two nylon M3 screws, nuts and two washers | Hidden wiring inspection and repair |
| Electronics | Removable insulating foam/adhesive at safe board edges | Replacement without destroying printed parts |

Do not superglue bare electronics, display glass, the cell's sensing section or the flexible wire loop. Do not bridge the moving cradle to the fixed base or stops. The load cell must remain the only intended structural path between the moving assembly and the base.

Treat the cured tower and cradle as one permanent subassembly. To open the base, remove the spool, free/disconnect the reader lead and remove the **two moving-end M4 screws**. Lift the cradle/tower while supporting the separate spacer. The fixed M5 pair stays installed. To reach the boards, separately release the stationary top-cover adhesive without levering against electronics or the cell. Replacing a bonded tower may require replacing the cradle; do not pry against a mounted load cell.

Record adhesive, filament, preparation, cure conditions and the results of progressive load, stability and zero-return checks in the [hardware report](HARDWARE-VALIDATION.md). The presence of glue does not establish a shorter total build time because full cure is still required.
