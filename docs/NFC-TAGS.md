# Printer tags and Spoolancer inventory tags

Research checked 27 October 2026. These are format and implementation findings, not a test on the owner's printers.

| Use | Tag family | What is required |
| --- | --- | --- |
| Bambu Lab AMS automatic identification | MIFARE Classic 1K-compatible, 13.56 MHz | Bambu's data layout and a valid manufacturer signature |
| Snapmaker U1 with stock firmware | MIFARE Classic 1K, 13.56 MHz | Snapmaker's data layout and a valid manufacturer signature |
| Current Spoolancer writable inventory | NTAG216 | Supported blank unlocked tag, then Spoolancer's own NDEF record |

A blank tag with the same chip family is not enough for either printer. Arbitrary changes to signed material/color data invalidate the signature. Bambu and Snapmaker formats are distinct, so their stock tags are not interchangeable simply because their radio technology is similar. See the community's [Bambu tag research](https://github.com/Bambu-Research-Group/RFID-Tag-Guide), [Bambu data layout](https://github.com/queengooborg/Bambu-Lab-RFID-Tag-Guide/blob/main/docs/BambuLabRfid.md), and Snapmaker's [published signature verification](https://github.com/Snapmaker/u1-klipper/blob/main/klippy/extras/filament_protocol.py).

The [U1-RFID project](https://github.com/DnG-Crafts/U1-RFID) supports writable NTAG215/216 OpenSpool tags through community extended firmware. That is a different setup from the owner's decision to keep stock U1 firmware. The [stock/extended support matrix](https://github.com/paxx12-snapmaker-u1/SnapmakerU1-Extended-Firmware/blob/develop/docs/design/rfid.md) distinguishes those paths.

For Spoolancer, use **NTAG216 inventory stickers**. Current firmware reads original printer tags only as identifiers to link to manually entered profiles; it does not decode proprietary material fields, clone printer tags or sign custom printer records. Its writable inventory format is not yet OpenSpool. See the [implemented NFC safeguards](../firmware/README.md#nfc-format-and-limits).
