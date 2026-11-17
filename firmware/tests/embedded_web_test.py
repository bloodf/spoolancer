#!/usr/bin/env python3
"""Prove firmware serves the exact current UI without a filesystem image."""
from pathlib import Path
import gzip
import os
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "scripts"))
from embed_web import ASSETS, render

with tempfile.TemporaryDirectory(prefix="spoolancer-web-") as directory:
    directory = Path(directory)
    header = directory / "WebAssets.h"
    content = render(root.parent / "web")
    assert content == render(root.parent / "web"), "Non-deterministic embedded assets"
    header.write_text(content)
    main = directory / "check.cpp"
    main.write_text("""
#include "WebAssets.h"
#include <cassert>
#include <cstdio>
int main(int argc, char **argv) {
  assert(argc == 2);
  assert(!WebAssets::find("/inventory.json"));
  assert(!WebAssets::find("/demo.js"));
  for (const auto &asset : WebAssets::files) {
    assert(WebAssets::find(asset.path) == &asset);
    char path[1024]; std::snprintf(path, sizeof(path), "%s%s.gz", argv[1], asset.path);
    auto *out = std::fopen(path, "wb"); assert(out);
    assert(std::fwrite(asset.data, 1, asset.size, out) == asset.size);
    assert(std::fclose(out) == 0);
  }
}
""")
    binary = directory / "check"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(main), "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(directory)], check=True)
    for name in ASSETS:
        assert gzip.decompress((directory / (name + ".gz")).read_bytes()) == (root.parent / "web" / name).read_bytes()
    # A webpage change must change the generated app input, triggering a rebuild.
    source = directory / "source"
    source.mkdir()
    for name in ASSETS:
        (source / name).write_bytes((root.parent / "web" / name).read_bytes())
    (source / "index.html").write_bytes(b"<html>new OTA webpage</html>")
    assert render(source) != content
    try:
        (source / "logic.js").unlink()
        render(source)
        raise AssertionError("A missing UI file must fail the firmware build")
    except FileNotFoundError:
        pass

# Optional post-build proof: each compressed asset must actually be in the app image.
for environment in sys.argv[1:]:
    build = root / ".pio/build" / environment
    generated = (build / "generated/WebAssets.h").read_text()
    assert generated == render(root.parent / "web"), "Embedded webpage is stale"
    image = (build / "firmware.bin").read_bytes()
    for body in re.findall(r"data\d+\[\] = \{([^}]+)\}", generated):
        data = bytes(int(value) for value in body.split(",") if value.strip())
        assert data in image, "Compiled app does not contain an embedded asset"
    print(f"PASS: {environment}: all five current web assets are inside firmware.bin")
print("PASS: embedded routes, exact gzip round-trip, deterministic generation and UI rebuild detection")
