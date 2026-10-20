"""Smoke test of a case's complication made physical in L_Manor (blackout, storm, tampered evidence).

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_complication.py SEED" -unattended -nosplash

Gothic seeds: TEST-gothic-Detective-3 (blackout), -6 (tampered), -8 (storm).
"""
import sys

import unreal

SEED = sys.argv[1] if len(sys.argv) > 1 else "TEST-gothic-Detective-3"

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
    director = all_of(world, unreal.NoirCaseDirector)[0]
    comp = director.get_complication()
    key = comp.get_key() if comp else ""
    log("seed %s  complication: %s" % (SEED, key or "none"))
    for a in unreal.GameplayStatics.get_all_actors_with_tag(world, "Mains")[:6]:
        for c in a.get_components_by_class(unreal.ActorComponent):
            if isinstance(c, unreal.StaticMeshComponent):
                mat = c.get_material(0)
                em = mat.get_vector_parameter_value("Emissive") if isinstance(mat, unreal.MaterialInstanceDynamic) else "-"
                log("  %s mesh %s %s visible=%s" % (a.get_actor_label(), mat.get_class().get_name(), em, c.is_visible()))
            elif isinstance(c, unreal.LightComponent):
                log("  %s light visible=%s intensity=%s" % (a.get_actor_label(), c.is_visible(), c.intensity))

    def stand_before(target, back, height=0.0):
        loc = target.get_actor_location()
        eye = loc - target.get_actor_right_vector() * back
        eye.z = 100.0
        pawn.set_actor_location(eye, False, True)
        pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye + unreal.Vector(0, 0, 64),
                                                                       loc + unreal.Vector(0, 0, height)))

    def at_window():
        windows = [s for s in all_of(world, unreal.NoirSearchable) if str(s.get_editor_property("furniture_kind")) == "window"]
        stand_before(windows[0], 260, 20)

    def at_room():
        # From a room's door side, looking into it.
        rooms = all_of(world, unreal.NoirRoom)
        room = rooms[1]
        loc = room.get_actor_location()
        eye = unreal.Vector(loc.x + 40, loc.y - room.get_actor_right_vector().y * 0 - 200 * (1 if loc.y > 0 else -1), 100)
        pawn.set_actor_location(eye, False, True)
        pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye + unreal.Vector(0, 0, 64), loc + unreal.Vector(0, 0, -40)))

    def search_all():
        for s in all_of(world, unreal.NoirSearchable):
            s.interact(pawn)
            pawn.end_inspect()

    def inspect(eid):
        def run():
            pawn.end_inspect()
            for e in all_of(world, unreal.NoirEvidence):
                if e.get_editor_property("case_evidence_id") == eid:
                    e.interact(pawn)
                    log("inspecting %s: %s" % (eid, e.get_editor_property("display_name")))
                    return
            log("no evidence %s" % eid)
        return run

    steps = [(0.5, at_room), (1.5, lambda: shot(world)), (0.3, at_window), (1.5, lambda: shot(world))]
    if key == "storm":
        steps += [(0.5, lambda: comp.strike()), (0.02, lambda: shot(world)),
                  (2.0, at_room), (1.0, lambda: comp.strike()), (0.25, lambda: shot(world))]
    if key == "tampered":
        steps += [(0.5, search_all)]
        for eid in ("ev-735340", "ev-44fd62", "ev-7282a0"):
            steps += [(0.5, inspect(eid)), (1.2, lambda: shot(world)),
                      (0.2, lambda: pawn.turn_inspected(180, 0)), (1.0, lambda: shot(world))]
        steps += [(0.3, lambda: pawn.end_inspect())]
    steps += [(1.0, None)]
    return steps


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
