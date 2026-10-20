"""Smoke test for the accusation: accuse Graves (with one wrong part) and run to the dossier.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_accuse.py" -unattended -nosplash

Screenshots (ScreenShotNNNNN.png): the board prompt, the first question, the confirm step,
the reveal mid-way, and the dossier (expected 3 of 4: the motive is deliberately wrong).
"""
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels.load_level("/Game/Maps/L_LookDev")
levels.editor_request_begin_play()

state = {"t": 0.0, "step": 0}


def find(world, cls, label):
    return next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls) if a.get_actor_label() == label)


def shot(world):
    unreal.SystemLibrary.execute_console_command(world, "Shot showui")


def script(world, pawn):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    case = find(world, unreal.NoirCase, "Case_ThorneMurder")
    return [
        (25, lambda: pawn.set_actor_location(unreal.Vector(170, -130, 100), False, True)),
        (0.2, lambda: pc.set_control_rotation(unreal.Rotator(0, 0, -90))),
        (1.5, lambda: shot(world)),           # prompt at the board
        (0.5, lambda: case.interact(pawn)),
        (0.5, lambda: shot(world)),           # "Who killed Harold Thorne?"
        (0.3, lambda: pawn.choose(2)),        # Edmund Graves
        (0.3, lambda: pawn.choose(4)),        # motive: wrong on purpose
        (0.3, lambda: pawn.choose(1)),        # the fireplace poker
        (0.3, lambda: pawn.choose(2)),        # Graves's alibi
        (0.5, lambda: shot(world)),           # confirm
        (0.3, lambda: pawn.choose(1)),        # make the accusation
        (7, lambda: shot(world)),             # reveal, second line
        (14, lambda: shot(world)),            # dossier
        (0.5, lambda: pawn.end_accusation()),
        (1, lambda: levels.editor_request_end_play()),
        (2, None),
    ]


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
    if "steps" not in state:
        if not pawn:
            return
        state["steps"] = script(world, pawn)
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
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
