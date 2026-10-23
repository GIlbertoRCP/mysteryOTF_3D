"""Recompiles materials and reports the errors of their final state (not the passing errors a
script's half-built graph logs along the way).

  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript="Scripts/check_materials.py [/Game/Folder ...]" -unattended

Logs "[Check] <material>: ok" or the compiler's messages for each Material under the folders
(default: /Game/Environment).
"""
import sys

import unreal

folders = [a for a in sys.argv[1:] if a.startswith("/Game")] or ["/Game/Environment"]
eal = unreal.EditorAssetLibrary
for folder in folders:
    for path in eal.list_assets(folder, recursive=True):
        a = unreal.load_asset(path)
        if not isinstance(a, unreal.Material):
            continue
        unreal.log("[Check] >>> " + a.get_path_name())
        unreal.MaterialEditingLibrary.recompile_material(a)
        unreal.log("[Check] <<< " + a.get_path_name())
unreal.log("[Check] done")
