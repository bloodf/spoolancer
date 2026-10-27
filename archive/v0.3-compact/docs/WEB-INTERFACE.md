# Local web interface

Spoolancer serves its interface directly from the ESP32-S3. Open the device's local address from a phone or computer on the same network. Internet access and a Spoolman installation are optional. The separate desktop demonstration uses sample data and is labeled as a demo.

## Daily use

1. Place the spool upright in the cradle. Adjust the side reader to face the tag. Rotate the spool while it stays seated if needed, then release it and wait for a stable reading.
2. Read its inventory tag or select its saved local record. For a new spool, enter its name, brand, material, color, empty-spool weight and starting filament weight.
3. Save a stable measurement locally. Remaining filament is gross weight minus the known empty-spool weight. An unknown empty-spool weight leaves remaining filament unknown.
4. Add or link the spool to Spoolman and confirm synchronization when desired. Network errors leave the local record available. The initial implementation requires manual retry.
5. To write an inventory tag, check the displayed tag UID and spool details, then confirm. The device rereads the tag before reporting success. Keep the tag aligned and hands off the scale until weighing settles again.

Scale zero and empty-spool weight are different. Zero the fully assembled, empty cradle, including its reader and cables. Empty-spool weight belongs to the spool record and must not be subtracted twice.

## Interface areas

| Area | Purpose |
| --- | --- |
| Weigh | Live gross weight, stability and calibration state; active spool; calculated remaining filament; save measurement |
| Spools | Create and edit name, brand, material, color, empty-spool weight and initial filament weight; associate a Spoolman ID |
| NFC | Read the seated tag, identify its family and UID, preview a supported inventory-tag write, and verify the result |
| Settings | Device pairing, Wi-Fi, device name, Spoolman address, zero and reference-mass calibration |

Material and brand come from tag data or a saved profile. Weight alone cannot identify either. Color is stored as a color value in the record, not inferred from the spool's appearance.

## Local access and persistence

The ESP32 serves the assets itself. There are no remote fonts, CDN scripts or mandatory cloud accounts. Pair using the device's PIN before viewing private records or changing data. The browser keeps its session token in memory. Saved Wi-Fi passwords are not returned by the settings API.

Records, configuration and calibration are stored on the device. The firmware's build guide describes capacities, storage and recovery behavior. Web assets and inventory use separate flash partitions. The normal assets upload preserves inventory; a full flash erase or partition-layout change can erase it. Preserve records before those operations.

The UI distinguishes uncalibrated, unstable, disconnected and unsupported-tag states. A demonstration value must never appear as a live device reading. An accepted measurement is a deliberate operation, not every sample received by the browser.

## Spoolman

Spoolman is optional. The initial firmware accepts an HTTP address with a numeric IPv4 host, such as `http://192.168.1.50:7912`, on the trusted local network. Hostnames, HTTPS and server authentication are not implemented. Save locally first, then use the configured server to create or link a spool and send the accepted remaining-filament amount. Brand corresponds to a Spoolman vendor; material and color belong to its filament record. Density and diameter are needed when creating filament records. Existing linked records receive remaining-weight updates only; changing local brand, material or color does not edit their remote filament records.

Updates use absolute remaining weight rather than repeated consumption deltas. If a create request has an uncertain outcome, reconcile it with the server before creating again. Pending measurements remain local; automatic background retries are not part of this first implementation.

## NFC limits

Writable NTAG216 tags use Spoolancer schema 1 in an `application/vnd.spoolancer+json` NDEF record. OpenSpool compatibility is not implemented. Original Bambu Lab and Snapmaker tags remain read-only and are UID-only in this initial firmware; their proprietary material decoders are still pending. A UID can be linked to a manually entered spool profile without writing the original tag. A detected UID is not proof that proprietary material data has been decoded, and custom inventory tags do not become authenticated tags for stock printers. Snapmaker U1 keeps its stock firmware.

A side reader permits seated operation when its antenna and the tag align. It does not establish universal compatibility with every embedded tag position. Tag orientation, nearby metal and electrical noise affect reception, as described in [Elechouse's PN532 documentation](https://www.elechouse.com/docs/pn532-v4/). Test the final plastic window, flange and actual tags before accepting the geometry.

See [web/README.md](../web/README.md) for the browser implementation and [firmware/README.md](../firmware/README.md) for building and loading the device software. Browser demonstration tests, firmware compilation, hardware operation and live Spoolman integration are separate checks.
