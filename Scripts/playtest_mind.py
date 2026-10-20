"""Smoke test of the Mind Place in L_Manor: find items, go in (Tab), use the table and board, come back.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_mind.py [SEED]" -unattended -nosplash
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

state = {"t": 0.0, "step": 0}


def log(msg):
    unreal.log("[Playtest] " + msg)


def all_of(world, cls):
    return list(unreal.GameplayStatics.get_all_actors_of_class(world, cls))


def shot(world):
    unreal.SystemLibrary.execute_console_command(world, "Shot showui")


def script(world, pawn):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    mind = all_of(world, unreal.NoirMindPlace)[0]
    board = all_of(world, unreal.NoirCase)[0]
    hiding = [f for f in all_of(world, unreal.NoirSearchable) if f.get_editor_property("items")][:3]
    where = {}

    def search(f):
        def run():
            f.interact(pawn)
            pawn.end_inspect()
        return run

    def remember():
        where["before"] = pawn.get_actor_location()
        log("before: %s" % where["before"])

    def look_at_table():
        centre = unreal.MathLibrary.transform_location(mind.get_actor_transform(), mind.get_editor_property("table_centre"))
        rot = unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_location() + unreal.Vector(0, 0, 64), centre)
        pc.set_control_rotation(rot)

    def pick_up_from_table():
        on_table = [e for e in all_of(world, unreal.NoirEvidence) if mind.contains(e.get_actor_location())]
        log("items on the table: %d" % len(on_table))
        if on_table:
            on_table[0].interact(pawn)

    def check_back():
        after = pawn.get_actor_location()
        log("after: %s  (moved %.1f cm)" % (after, (after - where["before"]).length()))

    return [
        (0.5, search(hiding[0])), (0.5, search(hiding[1])), (0.5, search(hiding[2])),
        (0.5, remember),
        (0.3, lambda: pawn.toggle_mind_place()),
        (1.5, lambda: shot(world)),                       # arriving: facing the board
        (0.3, look_at_table),
        (0.8, lambda: shot(world)),                       # the evidence table
        (0.3, pick_up_from_table),
        (1.5, lambda: shot(world)),                       # re-inspecting a copy
        (0.3, lambda: pawn.end_inspect()),
        (0.5, lambda: board.interact(pawn)),
        (1.5, lambda: shot(world)),                       # working the board in the Mind Place
        (0.3, lambda: pawn.end_board()),
        (0.8, lambda: pawn.toggle_mind_place()),
        (1.5, check_back),
        (0.1, lambda: shot(world)),                       # back where we were
        (1.0, None),
    ]


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
    if "steps" not in state:
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
