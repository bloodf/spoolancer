# Contributing to Spoolancer

Small fixes, measurements and reproducible hardware reports are welcome. Read the [project overview](README.md), [current evidence](docs/STATUS.md) and the guide for the area you change. Use an issue to discuss a larger change before spending time on it.

Contributions to original project files are accepted under the [MIT license](LICENSE). Submit only material you have the right to share. Keep third-party copyright and license notices, identify the source of reused material, and do not treat the project license as permission to relicense dependencies or manufacturer images.

## Local checks

Run commands from the repository root. The web interface has no npm dependencies or installation step. CI uses Node.js 24, Python 3.12, Clang for native tests and PlatformIO Core 6.1.19. The ESP32 platform and direct firmware library versions are pinned in [platformio.ini](firmware/platformio.ini). GitHub Actions are pinned to commit hashes in [ci.yml](.github/workflows/ci.yml).

```sh
node --check web/app.js
node --check web/logic.js
node --check web/demo.js
node --test web/tests/*.test.js
python3 firmware/tests/run.py
```

Native firmware tests need a C++17 compiler with AddressSanitizer and UndefinedBehaviorSanitizer. They use `c++` by default; set `CXX=clang++` to match CI.

Install PlatformIO Core 6.1.19 in a Python virtual environment or your existing isolated tool environment. Then compile and package all affected firmware variants:

```sh
python3 -m pip install 'platformio==6.1.19'
pio run -d firmware -e spoolancer-headless -e spoolancer-touch-8048 -e spoolancer-s3
pio run -d firmware -e spoolancer-headless -e spoolancer-touch-8048 -e spoolancer-s3 -t buildfs
python3 firmware/tests/check_filesystem.py
```

These commands do not flash a board. The firmware pre-build step copies device web assets into generated `firmware/data`; edit the sources in `web/`. Filesystem checks verify the asset partition, separate inventory partition and unpacked contents. See the [firmware guide](firmware/README.md) for hardware installation commands and limitations.

For a local browser demonstration:

```sh
python3 -m http.server 8873 --bind 127.0.0.1 --directory web
```

Open `http://127.0.0.1:8873/?demo=1` and use the documented demo PIN `12345678`. The demonstration uses simulated data and cannot verify a sensor, tag or Spoolman connection.

## CAD changes

Edit [cad/parameters.json](cad/parameters.json), the relevant variant parameters and [cad/build.py](cad/build.py), then regenerate affected exports with a Python environment that can import FreeCAD, Part and Mesh. The current Mac installation commands are documented in the [CAD guide](cad/README.md).

The regeneration entry points are:

```sh
python cad/build.py --variant headless
python cad/verify_exports.py --variant headless
python cad/render.py --variant headless
```

Here `python` means the configured CAD environment, not necessarily the system interpreter. Use `--variant touch-8048` for that variant. Running each script without `--variant` updates the root headless copy in `cad/`; keep that copy consistent when changing headless geometry. Rendering also requires VTK, NumPy and Matplotlib. CAD generation and rendering are not run by the current GitHub workflow.

Generated files are replaced by these scripts. Preserve manual CAD edits separately, include the corresponding source change in the pull request, and inspect the regenerated validation reports and previews. Run affected fit samples before describing a change as physically compatible. Do not update historical archives unless correcting their documentation as historical records.

## Reports and pull requests

Use the software-bug or hardware-report issue template. State the exact revision, board variant, changes made, steps performed and observed results. Keep these kinds of evidence separate:

- Native tests and browser demonstrations verify software behavior within their test environment.
- Firmware builds and filesystem checks verify compiled and packaged artifacts.
- CAD validation and slicing verify modeled geometry or estimated print use.
- Physical fit, load-cell calibration, cable influence and seated NFC need bench measurements.
- Live Spoolman and printer behavior require separate integration tests. A UID read is not proprietary tag decoding or stock-printer write compatibility.

Add focused tests for behavior you change, run the checks that exercise it, and state any checks you could not perform. Explain effects on other variants, stored inventory/calibration, wiring and tag formats. Include sources and license terms for new dependencies or reference material.

Keep real passwords, pairing PINs, session tokens, private spool records and server data out of commits, logs and screenshots. Use [SECURITY.md](SECURITY.md) for suspected vulnerabilities.

## Release bundles

After committing a clean tree, run `python3 tools/package_release.py`. It reads the exact commit and `VERSION`, then creates source, headless print and touch print ZIPs plus `SHA256SUMS.txt` under ignored `dist/`. Each ZIP contains a per-file manifest with the commit and SHA-256 hashes. A print bundle includes every listed STEP/STL pair, fit samples and complete native/STEP assemblies. The script refuses to overwrite an existing bundle; use `--output` with a fresh directory to reproduce it. Publication is a separate maintainer step.
