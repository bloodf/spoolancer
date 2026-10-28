#!/usr/bin/env python3
"""Build source and print ZIPs from the exact committed tree. Never publish or flash."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import posixpath
import re
import subprocess
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    args = parser.parse_args()
    if git("status", "--porcelain", "--untracked-files=normal").strip():
        raise SystemExit("Commit project changes before packaging; ignored build files may remain.")
    commit = git("rev-parse", "HEAD").decode().strip()
    timestamp = int(git("show", "-s", "--format=%ct", "HEAD").decode())
    with zipfile.ZipFile(io.BytesIO(git("archive", "--format=zip", "HEAD"))) as archive:
        files = {name: archive.read(name) for name in archive.namelist() if not name.endswith("/")}
    version = files["VERSION"].decode().strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?", version):
        raise SystemExit("Invalid VERSION for a release filename.")
    prefix = "spoolancer-v" + version
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    written = []

    def bundled_guide(source):
        # Keep guide text available offline and resolve repo-relative links at this release.
        def link(match):
            target = match.group(2)
            if target.startswith(("http:", "https:", "#", "mailto:")):
                return match.group(0)
            resolved = posixpath.normpath(posixpath.join(posixpath.dirname(source), target))
            base = f"https://raw.githubusercontent.com/bloodf/spoolancer/v{version}" if match.group(1).startswith("!") else f"https://github.com/bloodf/spoolancer/blob/v{version}"
            return f"{match.group(1)}{base}/{resolved})"
        return re.sub(r"(!?\[[^\]]*\]\()([^\s)]+)\)", link, files[source].decode()).encode()

    def write_bundle(suffix, contents):
        name = prefix + "-" + suffix
        path = output / (name + ".zip")
        if path.exists():
            raise SystemExit(f"Refusing to overwrite {path}; choose a fresh output directory.")
        manifest = {
            "version": version, "commit": commit, "bundle": suffix,
            "physical_validation": False,
            "files": {n: digest(data) for n, data in sorted(contents.items())},
        }
        data = dict(contents)
        data["PACKAGE-MANIFEST.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
        with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as target:
            for relative, body in sorted(data.items()):
                info = zipfile.ZipInfo(name + "/" + relative, time.gmtime(timestamp)[:6])
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                target.writestr(info, body)
        with zipfile.ZipFile(path) as check:
            if check.testzip() is not None:
                raise SystemExit(f"ZIP checksum failure: {path}")
            for relative, expected in manifest["files"].items():
                assert digest(check.read(name + "/" + relative)) == expected, relative
        written.append(path)
        print(f"Verified {path.name}: {len(data)} files, {path.stat().st_size:,} bytes")

    write_bundle("source", files)
    viewer_files = {name.removeprefix("viewer/"): data for name, data in files.items() if name.startswith("viewer/")}
    if viewer_files:
        viewer_files["LICENSE"] = files["LICENSE"]
        write_bundle("assembly-viewer", viewer_files)
    for variant in ("headless", "touch-8048", "headless-led", "oled-led"):
        folder = f"cad/variants/{variant}/"
        report = json.loads(files[folder + "exports/validation.json"])
        parts = report["parts"]
        contents = {"LICENSE": files["LICENSE"], "parameters.json": files["cad/parameters.json"],
                    "variant-parameters.json": files[folder + "parameters.json"]}
        rows = []
        for name, part in parts.items():
            for extension in ("step", "stl"):
                source = folder + f"exports/{name}.{extension}"
                contents[f"parts/{name}.{extension}"] = files[source]
            purpose = "Fit sample" if name.startswith("fit_") else "Production"
            rows.append(f"| `{name}` | {part['quantity']} | {purpose} |")
        for name, data in files.items():
            if not name.startswith(folder + "exports/"):
                continue
            short = name.rsplit("/", 1)[1]
            if short.endswith(".FCStd") or short.endswith("-assembly.step"):
                contents["assembly/" + short] = data
            elif short.endswith(".json"):
                contents["checks/" + short] = data
        production = sum(part["quantity"] for name, part in parts.items() if not name.startswith("fit_"))
        indicator_note = ""
        if variant in ("headless-led", "oled-led"):
            del contents["parameters.json"]
            contents["simple-parameters.json"] = files["cad/simple-parameters.json"]
            contents["VARIANT-NOTES.md"] = bundled_guide(folder + "NOTES.md")
            contents["DC-POWER.md"] = bundled_guide("docs/DC-POWER.md")
            contents["INDICATOR-GUIDE.md"] = bundled_guide("docs/OLED-RGB-VARIANT.md")
            contents["ASSEMBLY-VIEW.md"] = bundled_guide("docs/ASSEMBLY-VIEW.md")
            contents["BOM.md"] = bundled_guide("docs/BOM.md")
            contents["ASSEMBLY.md"] = bundled_guide("docs/ASSEMBLY.md")
            contents["PRINTING.md"] = bundled_guide("docs/PRINTING.md")
            contents["GLUE-ASSEMBLY.md"] = bundled_guide("docs/GLUE-ASSEMBLY.md")
            for preview in ("assembled", "exploded", "component-locations", "edge-detail", "cable-routing", "print-orientations", "service-interior"):
                path = folder + f"previews/{preview}.png"
                if path in files:
                    contents[f"views/{preview}.png"] = files[path]
            indicator_note = """
Read [variant notes](VARIANT-NOTES.md), [printing instructions](PRINTING.md), [DC power instructions](DC-POWER.md) and [indicator behavior](INDICATOR-GUIDE.md) before printing. This is the six-piece v0.8 fixed-width design. The reader support has one broad glued joint; the load cell, reader adjustment and rear cover use 12 screws: 4 metal M3 × 16, 4 nylon M3 × 16, 2 M4 × 25 and 2 M5 × 12. Use the [fastener list](BOM.md#current-ledoled-fasteners), [assembly instructions](ASSEMBLY.md) and [bonding guide](GLUE-ASSEMBLY.md). Keep the complete matching set; old lid, rails and sliding guides are incompatible. Each of the six parts prints once; there are no separate fit coupons in this revision. Check actual electronics dimensions before printing.

Use [the assembly-view guide](ASSEMBLY-VIEW.md) and the numbered images in `views/` to locate parts. The companion assembly-viewer ZIP provides a rotatable model and an exploded-view slider.

This case has closed USB walls, no jack hole and no converter mount. Drill only after checking the actual jack and wiring; mount an insulated converter on a fixed wall. A 12 V input must pass through a regulated 5 V converter. Remove the spool and cradle and isolate the converter supply before computer USB programming.
"""
        parameter_note = ("Current controls are in `simple-parameters.json`; `variant-parameters.json` records the model. Other construction dimensions are explicit in `cad/simple_geometry.py` in the source bundle. See the CAD guide for supported controls."
                          if variant in ("headless-led", "oled-led") else
                          "Legacy shared defaults are in `parameters.json`; apply `variant-parameters.json` over those defaults. The source bundle contains the legacy generator.")
        readme = f"""# Spoolancer {version}: {variant}

Prototype print bundle, generated from commit `{commit}`. Physical fit, strength and weighing have not been validated.

There are {len(parts)} STEP/STL pairs and {production} production pieces. STEP and STL are alternative formats for the same part; do not print both. Units are millimeters and parts are placed in print orientation. Fit samples, where listed, belong only to that revision. Original headless/touch bundles retain legacy mechanics and cannot be mixed with the six-piece LED/OLED set.

| File stem | Quantity | Purpose |
| --- | ---: | --- |
""" + "\n".join(rows) + f"""

Use individual files in `parts/` with a slicer that supports their format. `assembly/` contains editable FreeCAD and complete STEP reference assemblies, including electronics/reference bodies. Do not send the complete assembly directly to a slicer.

Initial print settings: unfilled PETG, 0.2 mm layers, four walls, five top/bottom layers and 20–25% infill. Inspect bridges and support needs in your printer's slicer. No G-code is included. {parameter_note}
{indicator_note}

Follow the [print guide](https://github.com/bloodf/spoolancer/blob/v{version}/docs/PRINTING.md), [BOM](https://github.com/bloodf/spoolancer/blob/v{version}/docs/BOM.md) and [assembly guide](https://github.com/bloodf/spoolancer/blob/v{version}/docs/ASSEMBLY.md). Editable generators, firmware, web UI and full docs are in the companion source ZIP and [repository](https://github.com/bloodf/spoolancer/tree/v{version}).

The package manifest records SHA-256 hashes. Geometry checks in `checks/` are not physical validation. Original project CAD is MIT licensed; see `LICENSE`.
"""
        contents["README.md"] = readme.encode()
        write_bundle(variant + "-print", contents)
    sums = "".join(f"{digest(path.read_bytes())}  {path.name}\n" for path in written)
    (output / "SHA256SUMS.txt").write_text(sums)
    print(sums, end="")


if __name__ == "__main__":
    main()
