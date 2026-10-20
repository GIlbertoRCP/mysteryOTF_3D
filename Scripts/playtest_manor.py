"""End-to-end smoke test of a generated case in L_Manor.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_manor.py SEED [DIFFICULTY] [en|es]" -unattended -nosplash

Plays seed SEED (default TEST-gothic-Detective-0): logs the staging, then screenshots the hall,
the body at the crime scene, the murder weapon found in its furniture, an interview with a
presented clue, and an accusation with the case's own answers through to the dossier.
"""
import sys

import unreal

SEED = sys.argv[1] if len(sys.argv) > 1 else "TEST-gothic-Detective-0"
DIFFICULTY = sys.argv[2] if len(sys.argv) > 2 else "Detective"
SPANISH = len(sys.argv) > 3 and sys.argv[3] == "es"

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eactors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
# Express cases play on the train.
levels.load_level("/Game/Maps/L_Train" if "express" in SEED else "/Game/Maps/L_Manor")
for a in eactors.get_all_level_actors():
    if isinstance(a, unreal.NoirCaseDirector):
        a.set_editor_property("seed", SEED)  # carried into the PIE copy, not saved
        a.set_editor_property("difficulty", DIFFICULTY)
        a.set_editor_property("language", unreal.NoirCaseLanguage.SPANISH if SPANISH else unreal.NoirCaseLanguage.ENGLISH)
levels.editor_request_begin_play()

state = {"t": 0.0, "step": 0}


def log(msg):
    unreal.log("[Playtest] " + msg)


def all_of(world, cls):
    return list(unreal.GameplayStatics.get_all_actors_of_class(world, cls))


def shot(world):
    unreal.SystemLibrary.execute_console_command(world, "Shot showui")


def look_at(world, pawn, target):
    loc = pawn.get_actor_location()
    rot = unreal.MathLibrary.find_look_at_rotation(loc + unreal.Vector(0, 0, 64), target)
    unreal.GameplayStatics.get_player_controller(world, 0).set_control_rotation(rot)


def stand_in_front(world, pawn, actor, distance=170):
    """Put the detective in the room, facing the actor, on the side toward the room's middle."""
    room = next(r for r in all_of(world, unreal.NoirRoom) if r.contains(actor.get_actor_location()))
    target = actor.get_actor_location()
    to_mid = room.get_actor_location() - target
    to_mid.z = 0
    to_mid = to_mid.normal()
    pawn.set_actor_location(unreal.Vector(target.x + to_mid.x * distance, target.y + to_mid.y * distance, 100), False, True)
    look_at(world, pawn, target)


def script(world, pawn):
    director = all_of(world, unreal.NoirCaseDirector)[0]
    board = all_of(world, unreal.NoirCase)[0]
    suspects = all_of(world, unreal.NoirSuspect)
    # Furniture hiding something; prefer one with several items (where the weapons were left).
    hiding = sorted((f for f in all_of(world, unreal.NoirSearchable) if f.get_editor_property("items")),
                    key=lambda f: -len(f.get_editor_property("items")))
    weapon_spot = hiding[0] if hiding else None
    rooms = all_of(world, unreal.NoirRoom)
    scene = next((r for r in rooms if str(r.get_editor_property("label")) and r.get_editor_property("can_be_scene")
                  and director.describe_staging().find("case room 0 -> " + str(r.get_editor_property("room_id"))) >= 0), rooms[0])

    def body_view():
        bp = scene.get_editor_property("body_point")
        body = unreal.MathLibrary.transform_location(scene.get_actor_transform(), bp.translation)
        pawn.set_actor_location(unreal.Vector(body.x + 170, body.y + 120, 100), False, True)
        look_at(world, pawn, body)

    def accuse(part):
        board_picks = [board.get_editor_property(k) + 1 for k in ("killer", "motive", "weapon", "broken_alibi", "accomplice")]
        return lambda: pawn.choose(board_picks[part])

    log("seed played: " + director.get_played_seed())
    log(director.describe_staging())
    log("suspects spawned: %d; searchables with items: %d" % (
        len(suspects), sum(1 for f in all_of(world, unreal.NoirSearchable) if f.get_editor_property("items"))))
    return [
        (1, lambda: shot(world)),                                    # hall, case title message
        (0.5, body_view),
        (2, lambda: shot(world)),                                    # the body at the scene
        (0.5, lambda: stand_in_front(world, pawn, weapon_spot) if weapon_spot else None),
        (1, lambda: weapon_spot.interact(pawn) if weapon_spot else None),
        (2, lambda: shot(world)),                                    # an item from its furniture
        (0.5, lambda: pawn.end_inspect()),
        (0.5, lambda: stand_in_front(world, pawn, suspects[0], 160)),
        (1, lambda: suspects[0].interact(pawn)),
        (1.5, lambda: pawn.choose(1)),
        (0.5, lambda: shot(world)),                                  # an answer from the generated interview
        (0.3, lambda: pawn.toggle_present()),
        (0.3, lambda: shot(world)),                                  # the case file to present
        (0.3, lambda: pawn.choose(1)),
        (0.5, lambda: shot(world)),                                  # their reaction
        (0.3, lambda: pawn.end_interview()),
        (0.3, lambda: pawn.set_actor_location(unreal.Vector(-850, 0, 100), False, True)),
        (0.2, lambda: look_at(world, pawn, board.get_actor_location())),
        (1, lambda: board.interact(pawn)),
        (0.3, accuse(0)), (0.3, accuse(1)), (0.3, accuse(2)), (0.3, accuse(3)),
        (0.3, accuse(4) if board.get_editor_property("has_accomplice") else (lambda: None)),
        (0.5, lambda: shot(world)),                                  # the accusation, confirm step
        (0.3, lambda: pawn.choose(1)),
        (6, lambda: shot(world)),                                    # the reveal
        (40, lambda: shot(world)),                                   # the dossier, once the story has played
        (3, None),
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
