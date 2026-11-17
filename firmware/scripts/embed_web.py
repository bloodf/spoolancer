"""Generate the device webpage as compressed, immutable firmware content."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path

ASSETS = ("index.html", "app.js", "styles.css", "logic.js", "favicon.svg")


def render(source):
    digest = hashlib.sha256()
    lines = ["#pragma once", "#include <cstddef>", "#include <cstring>", "namespace WebAssets {",
             "struct Asset { const char *path; const unsigned char *data; size_t size; };"]
    for index, name in enumerate(ASSETS):
        raw = (source / name).read_bytes()
        digest.update(name.encode() + b"\0" + len(raw).to_bytes(4, "little") + raw)
        compressed = gzip.compress(raw, compresslevel=9, mtime=0)
        lines.append(f"inline constexpr unsigned char data{index}[] = {{")
        for at in range(0, len(compressed), 24):
            lines.append(",".join(str(byte) for byte in compressed[at:at + 24]) + ",")
        lines.append("};")
    lines.append(f'inline constexpr char buildId[] = "{digest.hexdigest()}";')
    lines.append("inline constexpr Asset files[] = {")
    for index, name in enumerate(ASSETS):
        lines.append(f'{{{json.dumps("/" + name)}, data{index}, sizeof(data{index})}},')
    lines.extend(["};", "inline const Asset *find(const char *path) {",
                  "  for (const auto &file : files) if (std::strcmp(file.path, path) == 0) return &file;",
                  "  return nullptr;", "}", "}"])
    return "\n".join(lines) + "\n"


def write(source, output):
    content = render(source)
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text() != content:
        output.write_text(content)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    write(args.source, args.output)
