"""Adds the hall's (uncast) NoirRoom to the existing L_Manor without rebuilding the level.

  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript="Scripts/add_manor_hall.py" -nullrhi -unattended
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_manor  # noqa: E402

build_manor.L.levels.load_level(build_manor.MAP_PATH)
old = [a for a in build_manor.actors.get_all_level_actors() if a.get_actor_label() == "Room_Hall"]
if old:
    build_manor.actors.destroy_actors(old)
build_manor.add_hall(900.0 + 100.0)  # hall_end in build_level
build_manor.L.levels.save_current_level()
unreal.log("[Hall] added the hall to " + build_manor.MAP_PATH)
