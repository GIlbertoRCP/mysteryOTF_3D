"""Plays a case staged partly on the grounds: the conservatory as the crime scene, the chapel
as an alibi place. Visits both, searches their furniture, lists who stands where, and opens the
map on the grounds. Seed: NOIR_SEED (default TEST-gothic-Detective-1, body in the conservatory).

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_outbuildings.py" -unattended -nosplash
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grounds_layout as G  # noqa: E402

SEED = os.environ.get("NOIR_SEED", "TEST-gothic-Detective-1")
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
    unreal.log("[Outbuildings] " + msg)


def inside(box, p):
    return box[0] <= p.x <= box[2] and box[1] <= p.y <= box[3]


def script(world, pawn):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    director = unreal.GameplayStatics.get_actor_of_class(world, unreal.NoirCaseDirector)
    shot = lambda: unreal.SystemLibrary.execute_console_command(world, "Shot showui")
    for line in director.describe_staging().splitlines():
        log(line)
    for r in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NoirRoom):
        if r.get_editor_property("only_suits"):
            log("%s plays '%s'%s" % (r.get_editor_property("room_id"), r.get_editor_property("label"),
                                     " (the body is here)" if r.get_editor_property("plays_scene") else ""))
    for s in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NoirSuspect):
        p = s.get_actor_location()
        where = "the conservatory" if inside(G.CONSERVATORY, p) else "the chapel" if inside(G.CHAPEL, p) else "the house"
        log("%s stands in %s" % (s.get_editor_property("display_name"), where))

    def visit(box, name, yaw):
        cx, cy = (box[0] + box[2]) / 2, box[1] - 250

        def go():
            pawn.teleport(unreal.Vector(cx, cy, 100), unreal.Rotator(0, 0, yaw))
            pc.set_control_rotation(unreal.Rotator(0, -6, yaw))
            log("at the door of %s" % name)
        return go

    def search(box, name):
        def go():
            for s in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NoirSearchable):
                if inside(box, s.get_actor_location()):
                    before = len(pawn.get_known_clue_ids())
                    s.interact(pawn)
                    pawn.end_inspect()
                    found = list(pawn.get_known_clue_ids())[before:]
                    log("%s: searched %s, found %s" % (name, s.get_editor_property("display_name"), found or "nothing"))
        return go

    def step_in(box):
        def go():
            cx, cy = (box[0] + box[2]) / 2, box[1] + 120
            pawn.teleport(unreal.Vector(cx, cy, 100), unreal.Rotator(0, 0, 90))
            pc.set_control_rotation(unreal.Rotator(0, -10, 90))
        return go

    def open_map():
        pc.toggle_map()

    return [(0.5, visit(G.CONSERVATORY, "the conservatory", 90)), (2.5, shot), (0.3, step_in(G.CONSERVATORY)), (2.0, shot),
            (0.3, search(G.CONSERVATORY, "conservatory")),
            (0.5, visit(G.CHAPEL, "the chapel", 90)), (2.5, shot), (0.3, step_in(G.CHAPEL)), (2.0, shot),
            (0.3, search(G.CHAPEL, "chapel")),
            (0.3, open_map), (1.0, shot), (0.3, open_map), (0.5, None)]


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
        try:
            action()
        except Exception as e:
            log("step failed: %s" % e)
    else:
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
