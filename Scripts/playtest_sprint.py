"""Checks running: walk, then hold Shift and run along the drive until winded, then rest.
Logs speed, breath and whether running every half second; screenshots the walk and the run.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_sprint.py" -unattended -nosplash
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grounds_layout as G  # noqa: E402

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels.load_level("/Game/Maps/L_Manor")
levels.editor_request_begin_play()
state = {"t": 0.0, "phase": "wait", "clock": 0.0, "log": 0.0, "shots": set()}
# (seconds into the test, what happens): walk, run, rest.
PLAN = [(0.0, "walk"), (2.0, "run"), (11.0, "rest"), (17.0, "done")]


def log(msg):
    unreal.log("[Sprint] " + msg)


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
    if not pawn:
        return
    if state["phase"] == "wait":
        if state["t"] < 25:
            return
        x, y = -2400.0, -400.0
        pawn.teleport(unreal.Vector(x, y, G.height(x, y) + 100), unreal.Rotator(0, 0, 180))
        unreal.GameplayStatics.get_player_controller(world, 0).set_control_rotation(unreal.Rotator(0, 0, 180))
        state["phase"] = "go"
        state["clock"] = 0.0
        return
    if state["phase"] != "go":
        return
    state["clock"] += dt
    c = state["clock"]
    step = [p for at, p in PLAN if c >= at][-1]
    if step == "done":
        state["phase"] = "end"
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
        return
    pawn.set_sprint_held(step == "run")
    if step in ("walk", "run"):
        pawn.add_movement_input(unreal.Vector(-1, 0, 0), 1.0)
    if c - state["log"] >= 0.5:
        state["log"] = c
        v = pawn.get_velocity()
        log("%5.1fs %-4s speed %4.0f cm/s, breath %.2f, running %s" % (c, step, (v.x ** 2 + v.y ** 2) ** 0.5, pawn.get_stamina(), pawn.is_sprinting()))
    for at, name in ((1.5, "walk"), (5.0, "run")):
        if c >= at and name not in state["shots"]:
            state["shots"].add(name)
            unreal.SystemLibrary.execute_console_command(world, "Shot showui")


handle = unreal.register_slate_post_tick_callback(tick)
