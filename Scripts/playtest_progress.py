"""Checks case progress: the new-evidence card, then the Mind Place's tally and its board (question
cards with their counts, stamps on the suspects' photographs).

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_progress.py" -unattended -nosplash

Searches one piece of furniture that hides something (screenshot of the card), asks two suspects a
question and presents a clue to a third, searches everything else, then goes to the Mind Place and
the board. Seed: NOIR_SEED (default TEST-gothic-Detective-0).
"""
import os

import unreal

SEED = os.environ.get("NOIR_SEED", "TEST-gothic-Detective-0")
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
    unreal.log("[Progress] " + msg)


def all_of(world, cls):
    return list(unreal.GameplayStatics.get_all_actors_of_class(world, cls))


def script(world, pawn):
    shot = lambda: unreal.SystemLibrary.execute_console_command(world, "Shot showui")
    board = [b for b in all_of(world, unreal.NoirCase)][0]
    furniture = all_of(world, unreal.NoirSearchable)
    hiding = [f for f in furniture if f.get_editor_property("items")]
    suspects = all_of(world, unreal.NoirSuspect)

    def search(fs):
        def run():
            for f in fs:
                f.interact(pawn)
                pawn.end_inspect()
            log("searched %d; evidence known %d" % (len(fs), len(pawn.get_known_clue_ids())))
        return run

    def question(s):
        def run():
            s.interact(pawn)
            pawn.choose(1)
            pawn.end_interview()
            log("asked %s a question" % s.get_editor_property("display_name"))
        return run

    return [(0.5, search(hiding[:1])), (0.6, shot),
            (0.3, question(suspects[0])), (0.3, question(suspects[1])),
            (0.3, search([f for f in furniture if f not in hiding[:1]])),
            (5.5, lambda: pawn.toggle_mind_place()), (1.5, shot),
            (0.2, lambda: unreal.GameplayStatics.get_player_controller(world, 0).set_control_rotation(unreal.Rotator(0, -20, 30))), (1.2, shot),
            (0.3, lambda: board.interact(pawn)), (1.5, shot),
            (0.1, lambda: pawn.set_board_view(2.6, unreal.Vector2D(60, 20))), (1.5, shot),
            (0.1, lambda: pawn.set_board_view(2.2, unreal.Vector2D(0, -45))), (1.5, shot),
            (0.3, lambda: pawn.end_board()), (0.5, lambda: pawn.toggle_mind_place()), (1.0, None)]


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
