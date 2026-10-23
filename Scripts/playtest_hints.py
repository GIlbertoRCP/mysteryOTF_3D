"""Smoke test of the pause menu's hints (generated case).

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_hints.py [SEED]" -unattended -nosplash

Opens the pause menu, asks for a hint (the menu closes and the hint shows), asks for the rest and
one too many, then closes the case to see the hint count in the verdict. Screenshots each step.
Note: this saves the case and then closes it, which deletes the save slot; back it up first.
"""
import sys

import unreal

SEED = sys.argv[1] if len(sys.argv) > 1 else "TEST-gothic-Detective-0"

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eactors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level("/Game/Maps/L_Manor")
for a in eactors.get_all_level_actors():
    if isinstance(a, unreal.NoirCaseDirector):
        a.set_editor_property("seed", SEED)
levels.editor_request_begin_play()
state = {"t": 0.0, "step": 0, "steps": None}


def log(msg):
    unreal.log("[Hints] " + msg)


def script(world, pawn):
    director = unreal.GameplayStatics.get_actor_of_class(world, unreal.NoirCaseDirector)
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    shot = lambda: unreal.SystemLibrary.execute_console_command(world, "Shot showui")
    left = lambda what: lambda: log("%s: %d used, %d left" % (what, director.get_hints_used(), director.get_hints_left()))
    steps = [(0.5, left("start")), (0.2, lambda: pc.toggle_pause_menu()), (1.0, shot)]
    steps += [(0.2, lambda: pc.take_hint()), (0.6, shot), (0.1, left("after one"))]
    steps += [(0.3, lambda: pc.take_hint()) for _ in range(4)] + [(0.2, left("after five asks"))]
    steps += [(0.2, lambda: pc.toggle_pause_menu()), (1.0, shot), (0.2, lambda: pc.close_menu())]
    steps += [(0.3, lambda: pc.show_case_closed(4, 4)), (1.0, shot), (0.5, None)]
    return steps


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
    if state["steps"] is None:
        if not pawn or state["t"] < 25:
            return
        state["steps"] = script(world, pawn)
        state["t"] = 0.0
    steps = state["steps"]
    if state["step"] >= len(steps):
        return
    delay, action = steps[state["step"]]
    if state["t"] < delay:
        return
    state["t"] = 0.0
    state["step"] += 1
    if action:
        action()
    else:
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
