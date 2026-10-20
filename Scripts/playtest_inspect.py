"""Smoke test for inspection: search the desk, inspect the watch, turn it over, put it down.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_inspect.py" -unattended -nosplash

Takes three screenshots (ScreenShotNNNNN.png in Saved/Screenshots/WindowsEditor/): the watch's
face just after picking it up, its back after turning it, and the room after putting it down.
"""
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels.load_level("/Game/Maps/L_LookDev")
levels.editor_request_begin_play()

state = {"t": 0.0, "step": 0}


def log(msg):
    unreal.log("[Playtest] " + msg)


def shot(world):
    unreal.SystemLibrary.execute_console_command(world, "Shot showui")


def find(world, cls, label):
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls):
        if a.get_actor_label() == label:
            return a
    return None


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
    step, t = state["step"], state["t"]

    def advance():
        state["step"], state["t"] = step + 1, 0.0

    if step == 0 and t > 25 and pawn:
        pawn.set_actor_location(unreal.Vector(-110, -130, 100), False, True)
        unreal.GameplayStatics.get_player_controller(world, 0).set_control_rotation(unreal.Rotator(0, -35, -90))
        advance()
    elif step == 1 and t > 1:
        find(world, unreal.NoirSearchable, "Search_Desk").interact(pawn)
        advance()
    elif step == 2 and t > 2:
        shot(world)  # watch face, "stopped at 10:42" noticed
        advance()
    elif step == 3 and t > 0.5:
        pawn.turn_inspected(180, 0)
        advance()
    elif step == 4 and t > 2:
        shot(world)  # case back, engraving noticed
        advance()
    elif step == 5 and t > 0.5:
        pawn.end_inspect()
        advance()
    elif step == 6 and t > 2:
        shot(world)  # watch put back down on the desk, prompt "Examine the pocket watch"
        advance()
    elif step == 7 and t > 0.5:
        advance()
    elif step == 8 and t > 0.5:
        levels.editor_request_end_play()
        advance()
    elif step == 9 and t > 2:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
