"""Smoke test of the menus: title screen pages, starting a case, the pause menu, back to the title.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_menu.py" -unattended -nosplash
"""
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels.load_level("/Game/Maps/L_Menu")
levels.editor_request_begin_play()

state = {"t": 0.0, "step": 0}


def log(msg):
    unreal.log("[Playtest] " + msg)


def world():
    return ues.get_game_world()


def pc():
    return unreal.GameplayStatics.get_player_controller(world(), 0)


def shot():
    unreal.SystemLibrary.execute_console_command(world(), "Shot showui")


def report(what):
    def run():
        w = world()
        log("%s: map %s, menu open %s, paused %s" % (what, w.get_name(), pc().is_menu_open(),
                                                    unreal.GameplayStatics.is_game_paused(w)))
    return run


STEPS = [
    (6.0, report("title")), (0.5, shot),
    (0.5, lambda: pc().show_menu_page("case")), (0.8, shot),
    (0.3, lambda: pc().show_menu_page("settings")), (0.8, shot),
    (0.3, lambda: pc().show_menu_page("controls")), (0.8, shot),
    (0.3, lambda: pc().show_menu_page("main")),
    (0.5, lambda: pc().start_case("TEST-gothic-Detective-0", "Detective", False)),
    (25.0, report("case")), (0.5, shot),
    (0.5, lambda: pc().toggle_pause_menu()), (1.0, report("paused")), (0.3, shot),
    (0.5, lambda: pc().close_menu()), (0.5, report("resumed")),
    (0.5, lambda: pc().show_case_closed(3, 4)), (1.0, report("case closed")), (0.3, shot),
    (0.5, lambda: pc().close_menu()), (0.5, report("looking around")),
    (0.5, lambda: pc().quit_to_main_menu()), (6.0, report("back at title")), (0.5, shot),
    (1.0, None),
]


def tick(dt):
    state["t"] += dt
    if not world() or state["step"] >= len(STEPS):
        return
    delay, action = STEPS[state["step"]]
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
