# Copyright (c) 2026 CORDEL contributors. MIT.
"""Run checkout Ren'Py with SDK modules; reserve FD 1 for protocol only."""
import importlib.util
import os
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ["SDL_AUDIODRIVER"] = "dummy"
os.environ["RENPY_DISABLE_JOYSTICK"] = "1"
sys.stdout = sys.stderr
sys.path.insert(0, str(root))
sys.argv = [str(root / "renpy.py"), str(root / "narrative/phase1_adapter/fixtures"), "cordel_worker"]
spec = importlib.util.spec_from_file_location("cordel_renpy_entry", root / "renpy.py")
entry = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = entry
spec.loader.exec_module(entry)
entry.main()
