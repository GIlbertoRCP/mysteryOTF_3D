"""Smoke test for the search loop: starts PIE, faces the desk, searches it, screenshots with HUD.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_search.py" -unattended -nosplash

Screenshots land in Saved/Screenshots/WindowsEditor/ (ScreenShotNNNNN.png): one with the
interaction prompt, one after the search. Results are also logged as [Playtest] lines.
"""
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels.load_level("/Game/Maps/L_LookDev")
levels.editor_request_begin_play()

state = {"t": 0.0, "step": 0}


def log(msg):
    unreal.log("[Playtest] " + msg)


def world_and_pawn():
    world = ues.get_game_world()
    if not world:
        return None, None
    return world, unreal.GameplayStatics.get_player_pawn(world, 0)


def desk(world):
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NoirSearchable):
        if "Desk" in a.get_actor_label() or str(a.get_editor_property("display_name")) == "the desk":
            return a
    return None


def tick(dt):
    state["t"] += dt
    world, pawn = world_and_pawn()
    step = state["step"]
    if step == 0 and state["t"] > 25 and pawn:
        log("pawn class: %s" % pawn.get_class().get_name())
        pawn.set_actor_location(unreal.Vector(-110, -130, 100), False, True)
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        pc.set_control_rotation(unreal.Rotator(0, -35, -90))  # (roll, pitch, yaw): down at the desk
        state["step"], state["t"] = 1, 0.0
    elif step == 1 and state["t"] > 2:
        unreal.SystemLibrary.execute_console_command(world, "Shot showui")
        state["step"], state["t"] = 2, 0.0
    elif step == 2 and state["t"] > 1:
        d = desk(world)
        if d:
            d.interact(pawn)
            log("searched desk; prompt now: %s" % d.get_interact_prompt())
        else:
            log("desk searchable not found")
        state["step"], state["t"] = 3, 0.0
    elif step == 3 and state["t"] > 1:
        unreal.SystemLibrary.execute_console_command(world, "Shot showui")
        state["step"], state["t"] = 4, 0.0
    elif step == 4 and state["t"] > 2:
        levels.editor_request_end_play()
        state["step"], state["t"] = 5, 0.0
    elif step == 5 and state["t"] > 2:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
