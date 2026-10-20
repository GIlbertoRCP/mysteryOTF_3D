"""Smoke test of the physical case board in L_Manor.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_board.py [SEED]" -unattended -nosplash

Finds a few items, steps up to the board, then screenshots: the board as first pinned, after
moving a card and tying string between cards, and with a card hovered (its full text shown).
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
    board = all_of(world, unreal.NoirCase)[0]
    hiding = [f for f in all_of(world, unreal.NoirSearchable) if f.get_editor_property("items")][:3]

    def search(f):
        def run():
            f.interact(pawn)
            pawn.end_inspect()
        return run

    def at_board():
        pawn.set_actor_location(unreal.Vector(-850, 0, 100), False, True)
        pc.set_control_rotation(unreal.Rotator(0, 0, 180))
        board.interact(pawn)

    def arrange():
        ids = board.get_card_ids()
        log("cards on the board: %d" % len(ids))
        suspects = [i for i in ids if i.startswith("suspect:")]
        clues = [i for i in ids if not i.startswith("suspect:")]
        board.move_card(clues[0], unreal.Vector2D(-60, 10))
        board.toggle_string(suspects[0], clues[0])
        board.toggle_string(clues[0], clues[-1])
        board.toggle_string(suspects[-1], clues[1])
        log("strings tied: %d" % board.num_strings())

    def hover():
        # Put the mouse over the moved card: board centre + its position, projected to the screen.
        t = board.get_actor_transform()
        world_pos = unreal.MathLibrary.transform_location(t, unreal.Vector(8, -60, 10))
        screen = unreal.GameplayStatics.project_world_to_screen(pc, world_pos, False)
        ok = screen is not None
        if ok:
            pc.set_mouse_location(int(screen.x), int(screen.y))
        log("hover at %s: %s" % (screen, ok))

    return [
        (0.5, search(hiding[0])), (0.5, search(hiding[1])), (0.5, search(hiding[2])),
        (0.5, lambda: shot(world)),   # control: before the board
        (0.5, at_board),
        (2.0, lambda: shot(world)),   # the board as first pinned
        (0.5, lambda: unreal.SystemLibrary.execute_console_command(world, "HighResShot 1920x1080")),
        (0.5, arrange),
        (0.5, lambda: shot(world)),   # a card moved, string tied
        (0.3, hover),
        (0.6, lambda: shot(world)),   # the hovered card's full text
        (0.5, lambda: pawn.end_board()),
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
