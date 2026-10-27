#!/usr/bin/env python3
"""Inspect built partition tables and unpack asset images; never flash a device."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--env", action="append")
parser.add_argument("--tool", type=Path, default=Path.home() / ".platformio/packages/tool-mklittlefs/mklittlefs")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
environments = args.env or ["spoolancer-s3", "spoolancer-headless", "spoolancer-touch-8048"]
names = {"index.html", "styles.css", "app.js", "logic.js", "favicon.svg"}
for environment in environments:
    build = root / ".pio/build" / environment
    table = (build / "partitions.bin").read_bytes()
    entries = {}
    for at in range(0, len(table), 32):
        row = table[at:at + 32]
        if len(row) != 32 or row[:2] != b"\xaa\x50":
            continue
        _, kind, subtype, offset, size = struct.unpack_from("<HBBII", row)
        name = row[12:28].split(b"\0", 1)[0].decode()
        entries[name] = (kind, subtype, offset, size)
    assert entries["spiffs"] == (1, 0x82, 0x810000, 0x6f0000), entries
    assert entries["inventory"] == (1, 0x82, 0xf00000, 0x100000), entries
    image = build / "littlefs.bin"
    assert image.stat().st_size == entries["spiffs"][3], "Asset image has wrong partition size"
    with tempfile.TemporaryDirectory(prefix="spoolancer-assets-check-") as directory:
        subprocess.run([str(args.tool), "-u", directory, "-s", str(image.stat().st_size), "-b", "4096", "-p", "256", str(image)], check=True, capture_output=True)
        unpacked = Path(directory)
        assert {p.name for p in unpacked.iterdir()} == names, "Unexpected files packaged"
        for name in names:
            assert (unpacked / name).read_bytes() == (root.parent / "web" / name).read_bytes(), name
    print(f"PASS: {environment}: assets at 0x810000, {image.stat().st_size} bytes, five current files; private inventory at 0xf00000")
