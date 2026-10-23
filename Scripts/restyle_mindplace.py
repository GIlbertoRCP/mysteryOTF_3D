"""Re-dresses the Mind Place in the case levels (L_Manor, L_Train) in its study look without
rebuilding them: imports any new textures, rebuilds the world-projected materials, then gives every
part of the room its surface (see mindplace.LOOK and PARTS).

  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript="Scripts/restyle_mindplace.py" -unattended
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import import_polyhaven  # noqa: E402,F401  (imports what is new; already imported: skipped)
import build_grounds  # noqa: E402
import mindplace  # noqa: E402

build_grounds.build_world_pbr()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
for level in ("/Game/Maps/L_Manor", "/Game/Maps/L_Train"):
    if not unreal.EditorAssetLibrary.does_asset_exist(level):
        continue
    levels.load_level(level)
    n = mindplace.restyle()
    levels.save_current_level()
    unreal.log("[MindLook] %s: %d parts dressed" % (level, n))
unreal.log("[MindLook] done")
