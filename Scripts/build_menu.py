"""Builds /Game/Maps/L_Menu: the title screen, seen in the detective's study (the Mind Place room).

  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript=Scripts/build_menu.py -unattended -nosplash -nullrhi

The room is the same one every case's Mind Place uses; here the board carries an old case
(pinned papers and red string) and a camera drifts slowly in front of it while the menu is up.
The level's game mode is NoirMenuGameMode.
"""
import math
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_lookdev as L  # noqa: E402
import mindplace  # noqa: E402

MAP_PATH = "/Game/Maps/L_Menu"
actors = L.actors


def log(msg):
    unreal.log("[Menu] " + msg)


def build_level(m):
    if L.eal.does_asset_exist(MAP_PATH):
        L.levels.load_level(MAP_PATH)
        actors.destroy_actors([a for a in actors.get_all_level_actors()
                               if not isinstance(a, unreal.WorldSettings) and type(a) is not unreal.Brush])
    elif not L.levels.new_level(MAP_PATH):
        raise RuntimeError("could not create " + MAP_PATH)

    mindplace.build(m, (0.0, 0.0, 0.0))

    # An old case on the board: papers, photographs and string between them.
    rng = random.Random(1947)
    bx = -mindplace.W / 2 + 6
    pins = []
    for i in range(14):
        y = rng.uniform(-170, 170)
        z = rng.uniform(110, 245)
        w, h = rng.choice(((22, 16), (16, 22), (26, 18), (14, 14)))
        mat = rng.choice((m["paper"], m["paper"], m["paper"], m["ink"]))
        L.block("Menu_Paper%d" % i, (bx, y, z), (0.6, w, h), mat, "Menu/Board",
                rot=(rng.uniform(-6, 6), 0, 0))
        pins.append((y, z + h / 2 - 3))
        L.block("Menu_Pin%d" % i, (bx + 0.6, y, z + h / 2 - 3), (1.2, 1.4, 1.4), m["wax"], "Menu/Board", shadow=False)
    for i in range(9):
        (y0, z0), (y1, z1) = rng.sample(pins, 2)
        dy, dz = y1 - y0, z1 - z0
        length = (dy * dy + dz * dz) ** 0.5
        angle = math.degrees(math.atan2(dz, dy))
        L.block("Menu_String%d" % i, (bx + 1.0, (y0 + y1) / 2, (z0 + z1) / 2), (0.25, length, 0.25), m.get("yarn", m["wax"]),
                "Menu/Board", shadow=False, rot=(angle, 0, 0))

    # The camera: from beside the evidence table, looking back at the board and its lamps,
    # with the room opening up on the right (the menu sits on the left of the screen).
    cam = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(300, 230, 150), unreal.Rotator(0, -4, 197))
    cam.set_actor_label("MenuCamera")
    cam.set_folder_path("Menu")
    cam.camera_component.set_editor_property("field_of_view", 62.0)
    cam.set_editor_property("tags", [unreal.Name("MenuCamera")])

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.NoirMenuGameMode)
    L.levels.save_current_level()


if __name__ == "__main__":
    log("building materials")
    mats = L.build_materials()
    mats["night_glass"] = L.make_instance(unreal.load_asset(L.MAT_DIR + "/M_Master"), "MI_Glass_Night",
                                          (0.01, 0.02, 0.02), 0.1, emissive=(0.004, 0.012, 0.012))
    log("building level")
    build_level(mats)
    log("done")
    unreal.SystemLibrary.quit_editor()
