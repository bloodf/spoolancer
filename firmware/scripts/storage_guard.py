"""Fail before buildfs/uploadfs if the toolchain selects private storage."""
import csv
from pathlib import Path
from SCons.Script import COMMAND_LINE_TARGETS

Import("env")
if any(target in COMMAND_LINE_TARGETS for target in ("buildfs", "uploadfs", "uploadfsota")):
    table = Path(env.subst("$PROJECT_DIR")) / "partitions.csv"
    rows = [row for row in csv.reader(table.read_text().splitlines()) if row and not row[0].lstrip().startswith("#")]
    by_name = {row[0].strip(): [field.strip() for field in row] for row in rows}
    assets, private = by_name["spiffs"], by_name["inventory"]
    start, size = int(assets[3], 0), int(assets[4], 0)
    private_start, private_size = int(private[3], 0), int(private[4], 0)
    if (int(env.get("FS_START", -1)) != start or int(env.get("FS_SIZE", -1)) != size or
            not (start + size <= private_start or private_start + private_size <= start)):
        raise RuntimeError("Filesystem target is not the public assets partition. Refusing to build/upload over inventory.")
    print(f"Verified asset filesystem target: offset 0x{start:x}, size 0x{size:x}; inventory excluded.")
