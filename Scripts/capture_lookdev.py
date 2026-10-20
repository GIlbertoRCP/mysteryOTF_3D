"""Renders Cam_Hero and Cam_Board in L_LookDev to Saved/Screenshots/, then quits.

Needs a real RHI (no -nullrhi):
  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/capture_lookdev.py" -unattended -nosplash
(-ExecutePythonScript would quit before the warm-up ticks run.)
"""
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level("/Game/Maps/L_LookDev")

WARMUP_S = 90  # first run compiles material shaders; Lumen and fog then converge
SHOTS = [("Cam_Hero", "lookdev.png"), ("Cam_Board", "lookdev_board.png")]
state = {"t": 0.0, "i": 0}


def tick(dt):
    state["t"] += dt
    wait = WARMUP_S if state["i"] == 0 else 8
    if state["t"] < wait:
        return
    state["t"] = 0.0
    if state["i"] < len(SHOTS):
        label, name = SHOTS[state["i"]]
        cam = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == label)
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, name, camera=cam)
        state["i"] += 1
    else:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
