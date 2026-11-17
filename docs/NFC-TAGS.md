# Printer tags and Spoolancer inventory tags

Research checked 27 October 2026. These are format and implementation findings, not a test on the owner's printers.

| Use | Tag family | What is required |
| --- | --- | --- |
| Bambu Lab AMS automatic identification | MIFARE Classic 1K-compatible, 13.56 MHz | Bambu's data layout and a valid manufacturer signature |
| Snapmaker U1 with stock firmware | MIFARE Classic 1K, 13.56 MHz | Snapmaker's data layout and a valid manufacturer signature |
| Spoolancer writable inventory | NTAG213/215/216, subject to capacity | Supported blank unlocked tag with enough space for the complete Spoolancer NDEF record |

A blank tag with the same chip family is not enough for either printer. Arbitrary changes to signed material/color data invalidate the signature. Bambu and Snapmaker formats are distinct, so their stock tags are not interchangeable simply because their radio technology is similar. See the community's [Bambu tag research](https://github.com/Bambu-Research-Group/RFID-Tag-Guide), [Bambu data layout](https://github.com/queengooborg/Bambu-Lab-RFID-Tag-Guide/blob/main/docs/BambuLabRfid.md), and Snapmaker's [published signature verification](https://github.com/Snapmaker/u1-klipper/blob/main/klippy/extras/filament_protocol.py).

The [U1-RFID project](https://github.com/DnG-Crafts/U1-RFID) supports writable NTAG215/216 OpenSpool tags through community extended firmware. That is a different setup from the owner's decision to keep stock U1 firmware. The [stock/extended support matrix](https://github.com/paxx12-snapmaker-u1/SnapmakerU1-Extended-Firmware/blob/develop/docs/design/rfid.md) distinguishes those paths.

## Capacity-aware inventory records

The scale identifies NTAG213, NTAG215 and NTAG216 using GET_VERSION and checks the capability container and lock state before writing. The factory NDEF areas are 144, 496 and 872 bytes, respectively; physical user memory is 144, 504 and 888 bytes. NDEF framing also consumes space. See the [NXP datasheet](https://www.nxp.com/docs/en/data-sheet/NTAG213_215_216.pdf).

For Spoolancer inventory writes, the web page previews the actual encoded size. A full profile is used when it fits; otherwise the tag carries a compact schema-1 inventory link with `spool_id` and `link_only: true`. Full metadata stays in the scale or Spoolman. Another scale needs the corresponding inventory and UID association; this link does not carry a portable full material profile. No field is silently truncated. Optional Spoolman details (`details`: temperatures, article number, multi-color, external ID, lot, price, location, comments) are included when they fit and dropped least-important first (comments, location, price, lot…) on smaller tags. **Erase tag** clears tag memory without changing UID links.

Printer-specific records retain their format and must fit in full. A smaller tag does not make an unsupported printer format compatible. Unknown, protected or official tags are not made writable by this change.

## Identifying a spool without writing its tag

In **Inventory → Add RFID / Change RFID**, scan one tag and confirm the spool and tag ID. The association is saved on the scale; the tag's contents stay unchanged. This also works when the tag's contents are protected or cannot be decoded. A spool can be registered first and linked later.

The 30 October implementation covers the PN532 reader's passive target families:

| Family | Identification used by Spoolancer | Memory writing |
| --- | --- | --- |
| NFC-A / ISO14443A: NTAG, Ultralight, MIFARE Classic, Type 4A including DESFire | 4-, 7- or 10-byte UID, stored as uppercase hexadecimal | Supported NTAG inventory records and experimental printer-specific NTAG/Classic formats |
| NFC-B / ISO14443B | PUPI, stored as `B:` plus hexadecimal | Not implemented |
| NFC-F / FeliCa, 212 or 424 kbit/s | IDm, stored as `F:` plus hexadecimal | Not implemented |
| Jewel / Topaz | ID, stored as `J:` plus hexadecimal | Not implemented |
| NFC-V / ISO15693 / Type 5, including ICODE | Not supported by the PN532 | Not supported |

Actual detection depends on the PN532 firmware's reported capabilities and the tag. A tag using a randomized identifier cannot reliably represent the same spool across scans. Keep **one physical tag** in the antenna field: scanning stops at the first detected family, so mixed-family collisions are not established, and Jewel polling cannot report multiple targets. These are implementation and simulated-protocol results; the expanded families still need tests on physical tags.

## Writing a portable inventory record

Writing stores Spoolancer schema 1 on the tag itself. This is separate from linking an identifier on one scale.

| Chip | Physical user memory | NDEF capacity accepted by this implementation | Practical choice |
| --- | ---: | ---: | --- |
| NTAG213 | 144 bytes | 144 bytes | ID linking works; the complete current profile normally does not fit |
| NTAG215 | 504 bytes | 496 bytes | Short profiles can fit; the actual serialized size is checked |
| NTAG216 | 888 bytes | 872 bytes | Recommended when writing complete profiles |

The firmware checks exact chip version, capability container, lock/protection state and existing content. It calculates the full record size before any write. If a full profile does not fit, it uses a compact inventory link and previews its size before confirmation. It accepts supported blank NDEF or its own records, refuses foreign records and verifies read-back. Factory lock-control layouts that the writer does not support remain read-only. It never writes lock, configuration or credential pages. An interrupted write can still leave an incomplete record.

Original Bambu/Snapmaker tags can identify a linked spool. Explicit reads also attempt a material preview. The firmware includes experimental printer-format writers and a Bambu magic-card clone path; it does not create manufacturer signatures. These features have software fixtures and byte read-back checks, but physical printer acceptance is unverified. Spoolancer inventory and OpenSpool are separate selectable formats. See the [implemented NFC safeguards and formats](../firmware/README.md#nfc-format-and-limits).
