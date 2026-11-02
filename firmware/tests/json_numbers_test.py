#!/usr/bin/env python3
"""Check numeric serialization against the actual pinned PlatformIO ArduinoJson library."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
headers = sorted((root / ".pio/libdeps").glob("*/ArduinoJson/src/ArduinoJson.h"))
if not headers:
    raise SystemExit("Build a firmware profile with PlatformIO first to install ArduinoJson.")
with tempfile.TemporaryDirectory(prefix="spoolancer-json-test-") as folder:
    executable = Path(folder) / "json-numbers"
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
        "-I", str(headers[0].parent), "-I", str(root / "include"),
        str(root / "tests/json_numbers_test.cpp"), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
