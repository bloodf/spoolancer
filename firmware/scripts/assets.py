"""Copy only the device UI into the asset filesystem; inventory is separate."""
from pathlib import Path
import shutil
from SCons.Script import COMMAND_LINE_TARGETS

Import("env")
root = Path(env.subst("$PROJECT_DIR"))
source = root.parent / "web"
target = root / "data"
target.mkdir(exist_ok=True)
assets = ("index.html", "app.js", "styles.css", "logic.js", "favicon.svg")
for name in assets:
    path = source / name
    if path.exists():
        shutil.copyfile(path, target / name)
    elif "buildfs" in COMMAND_LINE_TARGETS or "uploadfs" in COMMAND_LINE_TARGETS:
        raise RuntimeError(f"Missing UI asset: {path}")
