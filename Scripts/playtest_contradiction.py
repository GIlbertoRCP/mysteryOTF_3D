"""Smoke test of challenging statements in an interview (generated case).

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_contradiction.py [SEED]" -unattended -nosplash

Finds everything in the manor, then picks a suspect and a clue that cracks them, and plays the
flow as a player would: ask all four questions, press P, challenge the WRONG statement with that
clue (they should be rattled but hold), then the RIGHT one (they should crack). Logs composure and
what they say at each step; screenshots the statement menu and the crack.
"""
import sys

import unreal

SEED = sys.argv[1] if len(sys.argv) > 1 else "TEST-gothic-Detective-0"
STATEMENTS = ["where", "relation", "heard", "noticed"]
PER_PAGE = 8

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
    unreal.log("[Contradiction] " + msg)


def all_of(world, cls):
    return list(unreal.GameplayStatics.get_all_actors_of_class(world, cls))


def script(world, pawn):
    director = all_of(world, unreal.NoirCaseDirector)[0]
    shot = lambda: unreal.SystemLibrary.execute_console_command(world, "Shot showui")
    for s in all_of(world, unreal.NoirSearchable):
        s.interact(pawn)
        pawn.end_inspect()
    clues = list(pawn.get_known_clue_ids())

    # A suspect, a clue that cracks them, the statement it breaks, and one it doesn't.
    pick = None
    for sus in all_of(world, unreal.NoirSuspect):
        for j, cid in enumerate(clues):
            right = [st for st in STATEMENTS if director.evaluate_contradiction(sus.get_case_id(), st, cid)]
            if right:
                wrong = [st for st in STATEMENTS if st not in right][0]
                pick = (sus, j, cid, STATEMENTS.index(right[0]), STATEMENTS.index(wrong))
                break
        if pick:
            break
    if not pick:
        log("no cracking clue known yet in this case")
        return [(0.5, None)]
    sus, j, cid, right, wrong = pick
    log("suspect %s, clue %s: breaks statement %s, not %s" % (sus.get_editor_property("display_name"), cid,
                                                              STATEMENTS[right], STATEMENTS[wrong]))

    def say(what):
        return lambda: log("%s -> composure %.2f, cracked %s: %s" % (what, sus.get_composure(), sus.is_cracked(), pawn.get_dialogue_text()))

    def pick_clue():
        for _ in range(j // PER_PAGE):
            pawn.choose(PER_PAGE + 1)  # "More..."
        pawn.choose(j % PER_PAGE + 1)

    def challenge(statement):
        # Every topic has been asked, so the statement menu lists all four in order.
        return [(0.3, lambda: pawn.toggle_present()), (0.3, lambda: pawn.choose(statement + 1))]

    steps = [(0.5, lambda: sus.interact(pawn))]
    steps += [(0.2, (lambda k: lambda: pawn.choose(k))(k)) for k in (1, 2, 3, 4)]
    steps += [(0.3, lambda: pawn.toggle_present()), (1.0, shot), (0.2, lambda: pawn.choose(0)), (0.2, lambda: pawn.choose(0))]
    steps += challenge(wrong) + [(0.3, pick_clue), (0.3, say("wrong statement")), (1.0, shot)]
    steps += challenge(right) + [(0.3, pick_clue), (0.3, say("right statement")), (1.2, shot)]
    steps += [(0.3, lambda: pawn.end_interview()), (0.5, None)]
    return steps


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
    if state["steps"] is None:
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
