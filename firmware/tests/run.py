#!/usr/bin/env python3
"""Run pure native logic checks; no device, network or credentials are used."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="spoolancer-tests-") as directory:
    executable = Path(directory) / "domain-test"
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
        "-I", str(root / "tests/stubs"), "-I", str(root / "include"), "-I", str(root / "src"),
        str(root / "tests/domain_test.cpp"), str(root / "tests/scale_test.cpp"), str(root / "tests/setup_test.cpp"), str(root / "src/Scale.cpp"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
subprocess.run([sys.executable, str(root / "tests/storage_guard_test.py")], check=True)
