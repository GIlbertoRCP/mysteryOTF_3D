"""Smoke test of the evidence puzzles: find everything, go to the Mind Place, work on each item.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_puzzle.py [SEED] [DIFFICULTY]" -unattended -nosplash

TEST-gothic-Novice-30 has all six kinds (hand, sole, uv, blood, torn, cipher). For each item on the
Mind Place's table with a puzzle: inspect it, press R (work on it), screenshot the puzzle, solve it,
screenshot what it gives up, put it down. Ends on the board.
"""
import sys

import unreal

SEED = sys.argv[1] if len(sys.argv) > 1 else "TEST-gothic-Novice-30"
DIFFICULTY = sys.argv[2] if len(sys.argv) > 2 else "Novice"

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eactors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level("/Game/Maps/L_Manor")
for a in eactors.get_all_level_actors():
    if isinstance(a, unreal.NoirCaseDirector):
        a.set_editor_property("seed", SEED)
        a.set_editor_property("difficulty", DIFFICULTY)
levels.editor_request_begin_play()

state = {"t": 0.0, "step": 0, "steps": None, "solved": 0}


def log(msg):
    unreal.log("[Puzzle] " + msg)


def all_of(world, cls):
    return list(unreal.GameplayStatics.get_all_actors_of_class(world, cls))


def script(world, pawn):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    mind = all_of(world, unreal.NoirMindPlace)[0]
    board = all_of(world, unreal.NoirCase)[0]
    shot = lambda: unreal.SystemLibrary.execute_console_command(world, "Shot showui")
    steps = []

    def search_all():
        for s in all_of(world, unreal.NoirSearchable):
            s.interact(pawn)
            pawn.end_inspect()

    def work_through_table():
        items = [e for e in all_of(world, unreal.NoirEvidence) if mind.contains(e.get_actor_location())]
        with_puzzles = [e for e in items if str(e.get_editor_property("puzzle").get_editor_property("type")) != "None"]
        log("table: %d items, %d with puzzles: %s" % (len(items), len(with_puzzles),
            [str(e.get_editor_property("puzzle").get_editor_property("type")) for e in with_puzzles]))
        for k, e in enumerate(with_puzzles):
            kind = str(e.get_editor_property("puzzle").get_editor_property("type"))

            def pick(e=e):
                pawn.end_inspect()
                e.interact(pawn)

            def work(kind=kind):
                pawn.work_on_inspected()
                log("%s: puzzle open %s" % (kind, pc.is_puzzle_open()))

            def solve(kind=kind):
                pc.solve_puzzle_for_test()
                state["solved"] += 1

            steps.extend([(0.4, pick)] + ([(1.0, shot)] if k == 0 else []) +
                         [(0.3, work), (1.0, shot), (0.3, solve), (0.8, shot), (0.3, lambda: pc.close_puzzle())])
        steps.extend([(0.4, lambda: pawn.end_inspect()), (0.5, lambda: board.interact(pawn)), (1.5, shot),
                      (0.3, lambda: log("solved %d puzzles" % state["solved"])), (1.0, None)])

    return [(0.5, search_all), (0.5, lambda: pawn.toggle_mind_place()), (1.5, work_through_table)], steps


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
    if state["steps"] is None:
        if not pawn or state["t"] < 25:
            return
        first, later = script(world, pawn)
        state["steps"], state["later"] = first, later
        state["t"] = 0.0
    steps = state["steps"]
    if state["step"] >= len(steps):
        if state["later"]:
            steps.extend(state["later"])
            state["later"] = []
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
