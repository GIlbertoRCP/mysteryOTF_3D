"""Smoke test of the map (M): search a few things, open the map, screenshot it.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_map.py [SEED]" -unattended -nosplash

Seeds containing "express" play on the train, the rest in the manor.
"""
import sys

import unreal

SEED = sys.argv[1] if len(sys.argv) > 1 else "TEST-gothic-Detective-0"

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eactors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level("/Game/Maps/L_Train" if "express" in SEED else "/Game/Maps/L_Manor")
for a in eactors.get_all_level_actors():
    if isinstance(a, unreal.NoirCaseDirector):
        a.set_editor_property("seed", SEED)
levels.editor_request_begin_play()

state = {"t": 0.0, "step": 0, "steps": None}


def all_of(world, cls):
    return list(unreal.GameplayStatics.get_all_actors_of_class(world, cls))


def script(world, pawn, pc):
    shot = lambda: unreal.SystemLibrary.execute_console_command(world, "Shot showui")

    def search_some():
        for s in all_of(world, unreal.NoirSearchable)[:7]:
            s.interact(pawn)
            pawn.end_inspect()

    def toggle():
        pc.toggle_map()
        unreal.log("[Map] open %s" % pc.is_map_open())

    return [(0.5, search_some), (0.5, toggle), (1.0, shot), (0.3, toggle), (0.5, None)]


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    if not world:
        return
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if state["steps"] is None:
        if state["t"] < 25 or not pawn:
            return
        state["steps"] = script(world, pawn, pc)
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
