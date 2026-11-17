"""Embed the device UI in firmware and retain legacy filesystem packaging."""
from pathlib import Path
import shutil
import sys

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
    else:
        raise RuntimeError(f"Missing UI asset: {path}")


# The existing OTA app upload now carries the webpage, too. Each environment
# generates its own header so parallel builds do not share a mutable output.
sys.path.insert(0, str(root / "scripts"))
from embed_web import write
generated = Path(env.subst("$BUILD_DIR")) / "generated"
write(source, generated / "WebAssets.h")
env.Append(CPPPATH=[str(generated)])
