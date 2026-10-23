"""Rebuilds M_Master and every material instance, without touching any level.

  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript=Scripts/build_materials.py -unattended -nosplash -nullrhi

The levels reference the instances by path, so they pick the changes up on their next load.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_lookdev as L  # noqa: E402

unreal.log("[Materials] building")
mats = L.build_materials()
mi = lambda *a, **k: L.make_instance(unreal.load_asset(L.MAT_DIR + "/M_Master"), *a, **k)
# The extra instances the manor and the train add (kept in step with build_manor / build_train).
mi("MI_Glass_Night", (0.01, 0.02, 0.02), 0.1, emissive=(0.004, 0.012, 0.012))
mi("MI_Candle_Wax", (0.55, 0.5, 0.4), 0.6, emissive=(0.05, 0.03, 0.01))
unreal.log("[Materials] done: %d" % len(mats))
unreal.SystemLibrary.quit_editor()
