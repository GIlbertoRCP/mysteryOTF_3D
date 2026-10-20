"""Renders Cam_Hero in L_LookDev to Saved/Screenshots/lookdev.png, then quits.

Needs a real RHI (no -nullrhi):
  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/capture_lookdev.py" -unattended -nosplash
(-ExecutePythonScript would quit before the warm-up ticks run.)
"""
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level("/Game/Maps/L_LookDev")

WARMUP_S = 90  # first run compiles material shaders; Lumen and fog then converge
state = {"t": 0.0, "shot": None}


def tick(dt):
    state["t"] += dt
    if state["shot"] is None:
        if state["t"] < WARMUP_S:
            return
        cam = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == "Cam_Hero")
        state["shot"] = unreal.AutomationLibrary.take_high_res_screenshot(
            1600, 900, "lookdev.png", camera=cam)
        state["t"] = 0.0
    elif state["t"] > 10:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
