"""Smoke test of the case notebook (N): opens it at the start, then again after two hints.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_notebook.py" -unattended -nosplash

Logs the case summary the notebook shows and screenshots both states. Seed: NOIR_SEED, default
TEST-gothic-Detective-0.
"""
import os

import unreal

SEED = os.environ.get("NOIR_SEED", "TEST-gothic-Detective-0")
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eactors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level("/Game/Maps/L_Train" if "express" in SEED else "/Game/Maps/L_Manor")
for a in eactors.get_all_level_actors():
    if isinstance(a, unreal.NoirCaseDirector):
        a.set_editor_property("seed", SEED)
levels.editor_request_begin_play()
state = {"t": 0.0, "step": 0, "steps": None}


def log(msg):
    unreal.log("[Notebook] " + msg)


def script(world, pawn):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    director = unreal.GameplayStatics.get_actor_of_class(world, unreal.NoirCaseDirector)
    shot = lambda: unreal.SystemLibrary.execute_console_command(world, "Shot showui")
    toggle = lambda: pc.toggle_case_notebook()
    is_open = lambda what: lambda: log("%s: open %s" % (what, pc.is_case_notebook_open()))

    def hints():
        for _ in range(2):
            log("hint: %s" % director.take_hint())

    return [(0.3, toggle), (0.2, is_open("first")), (1.0, shot), (0.2, toggle), (0.2, is_open("closed")),
            (0.3, hints), (0.3, toggle), (1.0, shot), (0.2, toggle), (0.5, None)]


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
    if state["steps"] is None:
        if not pawn or state["t"] < 25:
            return
        try:
            state["steps"] = script(world, pawn)
        except Exception as e:  # never leave the editor hanging
            log("script failed: %s" % e)
            state["steps"] = [(0.1, None)]
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
