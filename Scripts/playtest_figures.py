"""Looks at the suspects' figures: a group shot, close-ups, and one turning to face an interview.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_figures.py [SEED]" -unattended -nosplash
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
    suspects = all_of(world, unreal.NoirSuspect)
    # The room with the most suspects in it makes the group shot.
    by_room = {}
    for s in suspects:
        key = (round(s.get_actor_location().x / 300), round(s.get_actor_location().y / 300))
        by_room.setdefault(key, []).append(s)
    group = max(by_room.values(), key=len)

    def stand_back(targets, distance):
        def run():
            mid = unreal.Vector(0, 0, 0)
            for t in targets:
                mid = mid + t.get_actor_location()
            mid = mid * (1.0 / len(targets))
            fwd = targets[0].get_actor_forward_vector()
            eye = mid + fwd * distance
            eye.z = 100.0
            pawn.set_actor_location(eye, False, True)
            pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye + unreal.Vector(0, 0, 64), mid + unreal.Vector(0, 0, 30)))
            unreal.log("[Figures] looking at %s" % [str(t.get_editor_property("display_name")) for t in targets])
        return run

    def side_on():
        # From the side, so the turn toward the detective in the interview shows.
        s = group[0]
        eye = s.get_actor_location() + s.get_actor_right_vector() * 220.0
        eye.z = 100.0
        pawn.set_actor_location(eye, False, True)
        pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye + unreal.Vector(0, 0, 64), s.get_actor_location() + unreal.Vector(0, 0, 40)))

    steps = [(0.5, stand_back(group, 330.0)), (2.0, shot)]
    for s in suspects[:3]:
        steps += [(0.3, stand_back([s], 170.0)), (2.0, shot)]
    steps += [(0.3, side_on), (2.0, shot), (0.3, lambda: group[0].interact(pawn)), (2.5, shot),
              (0.3, lambda: pawn.end_interview()), (0.5, None)]
    return steps


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
