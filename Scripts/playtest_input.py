"""Checks the game gets its input back after a case is started from the title menu.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_input.py [menu|direct]" -nosplash

menu (default): starts on L_Menu and begins a case through the menu, as a player would; direct:
straight into L_Manor. Logs the controller's input state on the title and once the case has loaded
(it should read "ignore input 0 ... focus SViewport"), then the detective's position for 20 s.
"""
import sys

import unreal

DIRECT = len(sys.argv) > 1 and sys.argv[1] == "direct"  # control run: straight into the manor, no menu
REAPPLY = len(sys.argv) > 1 and sys.argv[1] == "reapply"  # experiment: set game-only input again at READY

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels.load_level("/Game/Maps/L_Manor" if DIRECT else "/Game/Maps/L_Menu")
levels.editor_request_begin_play()

state = {"t": 0.0, "phase": "menu", "logs": 0}


def log(msg):
    unreal.log("[Input] " + msg)


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    if not world:
        return
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    if state["phase"] == "menu" and DIRECT:
        state["phase"], state["t"] = "travel", 0.0
    elif state["phase"] == "menu" and state["t"] > 6:
        log("title state: " + pc.describe_input())
        pc.start_case("TEST-gothic-Detective-0", "Detective", False)
        state["phase"], state["t"] = "travel", 0.0
    elif state["phase"] == "travel" and state["t"] > 25:
        log("state: " + pc.describe_input())
        log("READY")
        state["phase"], state["t"] = "watch", 0.0
    elif state["phase"] == "watch" and state["t"] > 0.5:
        state["t"] = 0.0
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        log("pos %s" % (pawn.get_actor_location() if pawn else None))
        state["logs"] += 1
        if state["logs"] >= 40:
            levels.editor_request_end_play()
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
