"""Smoke test of a generated "express" case on the train: staging, the carriages, the snow outside.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_train.py [SEED]" -unattended -nosplash
"""
import sys

import unreal

SEED = sys.argv[1] if len(sys.argv) > 1 else "TEST-express-Detective-0"

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eactors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level("/Game/Maps/L_Train")
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
    log(director.describe_staging())
    rooms = {str(r.get_editor_property("room_id")): r for r in all_of(world, unreal.NoirRoom)}
    labels = {k: str(r.get_editor_property("label")) for k, r in rooms.items()}
    log("carriages: %s" % labels)
    suspects = all_of(world, unreal.NoirSuspect)

    def put(eye, target):
        pawn.set_actor_location(eye, False, True)
        pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye + unreal.Vector(0, 0, 64), target))

    def down_the_car():
        c1 = rooms["C1"].get_actor_location()
        put(unreal.Vector(c1.x - 460, 0, 100), unreal.Vector(c1.x + 500, 0, 150))

    def at_suspect():
        s = suspects[0]
        loc = s.get_actor_location()
        log("suspect %s at %s" % (s.get_editor_property("display_name"), loc))
        put(loc + unreal.Vector(-220, 0, 0), loc + unreal.Vector(0, 0, 40))

    def out_the_window():
        c3 = rooms["C3"].get_actor_location()
        put(unreal.Vector(c3.x - 40, -40, 100), unreal.Vector(c3.x + 120, 600, 120))

    def vestibule():
        c1 = rooms["C1"].get_actor_location()
        put(unreal.Vector(c1.x + 380, 0, 100), unreal.Vector(c1.x + 800, 0, 140))

    def look_down(car_id):
        def run():
            c = rooms[car_id].get_actor_location()
            put(unreal.Vector(c.x - 470, -20 if car_id != "C1" else 100, 100), unreal.Vector(c.x + 400, 0 if car_id != "C1" else 60, 120))
        return run

    def rear_window():
        c = rooms["C6"].get_actor_location()
        put(unreal.Vector(c.x + 300, 0, 100), unreal.Vector(c.x + 1200, 0, 120))

    steps = []
    for car_id in ("C1", "C2", "C3", "C4", "C5", "C6"):
        steps += [(0.4, look_down(car_id)), (1.4, lambda: shot(world))]
    steps += [(0.3, out_the_window), (1.5, lambda: shot(world)), (0.3, rear_window), (1.5, lambda: shot(world)), (1.0, None)]
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
