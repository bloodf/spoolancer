"""Exercise the actual SCons guard with synthetic build environments."""
from pathlib import Path
import runpy
import sys
import types

root = Path(__file__).resolve().parents[1]
script_module = types.ModuleType("SCons.Script")
sys.modules.setdefault("SCons", types.ModuleType("SCons"))
sys.modules["SCons.Script"] = script_module

class Environment(dict):
    def subst(self, value):
        assert value == "$PROJECT_DIR"
        return str(root)

def run(target, offset, size):
    script_module.COMMAND_LINE_TARGETS = [target]
    env = Environment(FS_START=offset, FS_SIZE=size)
    runpy.run_path(str(root / "scripts/storage_guard.py"), init_globals={"env": env, "Import": lambda *_: None})

for target in ("buildfs", "uploadfs", "uploadfsota"):
    run(target, 0x810000, 0x6f0000)
    for offset, size in ((0xf00000, 0x100000), (0x810000, 0x100000), (-1, -1)):
        try:
            run(target, offset, size)
        except RuntimeError:
            pass
        else:
            raise AssertionError(f"{target} accepted incorrect filesystem target")
print("PASS: filesystem build/upload guard rejects inventory and wrong-size targets")
