"""Save and continue, across two editor sessions.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_save.py save" -unattended -nosplash
  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_save.py load" -unattended -nosplash

save: plays TEST-gothic-Novice-30 for a bit (searches, an interview, a puzzle in the Mind Place),
walks somewhere, then pauses, which saves. load: starts on the title screen, which should offer
Continue, continues, and logs and screenshots where the detective is and what they know. Compare
the "Saved case" and "Resumed case" lines in the log.
"""
import sys

import unreal

PHASE = sys.argv[1] if len(sys.argv) > 1 else "save"

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eactors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if PHASE == "save":
    levels.load_level("/Game/Maps/L_Manor")
    for a in eactors.get_all_level_actors():
        if isinstance(a, unreal.NoirCaseDirector):
            a.set_editor_property("seed", "TEST-gothic-Novice-30")
            a.set_editor_property("difficulty", "Novice")
else:
    levels.load_level("/Game/Maps/L_Menu")
levels.editor_request_begin_play()

state = {"t": 0.0, "step": 0, "steps": None}


def log(msg):
    unreal.log("[Save] " + msg)


def all_of(world, cls):
    return list(unreal.GameplayStatics.get_all_actors_of_class(world, cls))


def save_script(world, pawn, pc):
    mind = all_of(world, unreal.NoirMindPlace)[0]
    shot = lambda: unreal.SystemLibrary.execute_console_command(world, "Shot showui")

    def search_some():
        for s in all_of(world, unreal.NoirSearchable)[:6]:
            s.interact(pawn)
            pawn.end_inspect()

    def interview():
        suspect = all_of(world, unreal.NoirSuspect)[0]
        suspect.interact(pawn)
        pawn.choose(1)
        pawn.toggle_present()
        pawn.choose(1)
        pawn.end_interview()

    def puzzle():
        items = [e for e in all_of(world, unreal.NoirEvidence) if mind.contains(e.get_actor_location())
                 and str(e.get_editor_property("puzzle").get_editor_property("type")) != "None"]
        if items:
            items[0].interact(pawn)
            pawn.work_on_inspected()
            pc.solve_puzzle_for_test()
            pc.close_puzzle()
            pawn.end_inspect()

    def walk():
        pawn.set_actor_location(unreal.Vector(300, 40, 100), False, True)
        pc.set_control_rotation(unreal.Rotator(0, -5, 135))

    return [(0.5, search_some), (0.5, interview), (0.5, lambda: pawn.toggle_mind_place()), (1.5, puzzle),
            (0.5, lambda: pawn.toggle_mind_place()), (1.5, walk), (1.0, shot), (0.5, lambda: pc.toggle_pause_menu()),
            (1.0, None)]


def load_script(world, pawn, pc):
    shot = lambda: unreal.SystemLibrary.execute_console_command(ues.get_game_world(), "Shot showui")

    def resumed():
        w = ues.get_game_world()
        p = unreal.GameplayStatics.get_player_pawn(w, 0)
        director = all_of(w, unreal.NoirCaseDirector)[0]
        log("resuming %s, at %s" % (director.is_resuming(), p.get_actor_location() if p else None))

    return [(1.0, shot), (0.5, lambda: pc.continue_case()), (25.0, resumed), (0.5, shot), (1.0, None)]


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    if not world:
        return
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if state["steps"] is None:
        if state["t"] < (25 if PHASE == "save" else 6) or (PHASE == "save" and not pawn):
            return
        state["steps"] = (save_script if PHASE == "save" else load_script)(world, pawn, pc)
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
