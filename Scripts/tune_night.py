"""Sets L_Manor's night light levels without rebuilding the grounds: moon, sky light, sky glow.

  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript="Scripts/tune_night.py [moon sky glow]" -unattended

Defaults are build_grounds.py's MOON_LUX, SKY_LIGHT and SKY_GLOW; numbers on the command line
override them, for trying levels out.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_grounds as B  # noqa: E402

args = [float(a) for a in sys.argv[1:4]] if len(sys.argv) >= 4 else [B.MOON_LUX, B.SKY_LIGHT, B.SKY_GLOW]
moon, sky, glow = args
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level(B.MAP_PATH)
for a in B.actors.get_all_level_actors():
    if isinstance(a, unreal.DirectionalLight) and a.get_actor_label() == "Moon":
        a.light_component.set_editor_property("intensity", moon)
    elif isinstance(a, unreal.SkyLight):
        a.light_component.set_editor_property("intensity", sky)
B.SKY_GLOW = glow
B.build_sky()  # rebuilt with the new glow as its default
levels.save_current_level()
unreal.log("[Night] moon %.2f lux, sky light %.2f, sky glow %.3f" % (moon, sky, glow))
