"""Builds /Game/Maps/L_Manor: a manor wing for generated cases.

  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript=Scripts/build_manor.py -unattended -nosplash -nullrhi

A hall with three rooms on each side and the case board at its west end. Every room is a
NoirRoom with four pieces of searchable furniture (of the kinds the case generator hides its
spots in) and two stand points; the NoirCaseDirector casts the rooms as the case's crime scene
and alibi rooms on play, and fills them with suspects and evidence.

Placeholder geometry, like the look-dev room; reuses its materials and helpers.
"""
import math
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_lookdev as L  # noqa: E402  (helpers only; its build runs under __main__)
import mindplace  # noqa: E402

MAP_PATH = "/Game/Maps/L_Manor"
WALL_H = 320.0
DOOR_W = 140.0
DOOR_H = 220.0
HALL_Y = 150.0     # hall walls at y = +/-150
ROOM_D = 600.0     # rooms run from the hall wall to y = +/-750
ROOM_XS = (-600.0, 0.0, 600.0)  # room centres along the hall; each room is 600 wide

actors = L.actors
block = L.block


def log(msg):
    unreal.log("[Manor] " + msg)


# ---------------------------------------------------------------- placement in a local frame

class Frame:
    """A position and yaw; furniture is modelled with its back toward local +Y."""

    def __init__(self, x, y, yaw):
        self.x, self.y, self.yaw = x, y, yaw

    def at(self, lx, ly, z=0.0):
        r = math.radians(self.yaw)
        return (self.x + lx * math.cos(r) - ly * math.sin(r), self.y + lx * math.sin(r) + ly * math.cos(r), z)

    def child(self, lx, ly, yaw):
        x, y, _ = self.at(lx, ly)
        return Frame(x, y, self.yaw + yaw)

    def piece(self, label, lx, ly, z, size, mat, folder, mesh=L.CUBE, shadow=True):
        return block(label, self.at(lx, ly, z), size, mat, folder, shadow=shadow, mesh=mesh, rot=(0, 0, self.yaw))


def searchable(label, frame, lx, ly, z, extent, kind, name):
    a = actors.spawn_actor_from_class(unreal.NoirSearchable, unreal.Vector(*frame.at(lx, ly, z)),
                                      unreal.Rotator(0, 0, frame.yaw))
    a.set_actor_label(label)
    a.set_folder_path("Gameplay/Searchables")
    a.set_editor_property("display_name", unreal.Text(name))
    a.set_editor_property("furniture_kind", unreal.Name(kind))
    a.get_editor_property("bounds").set_box_extent(unreal.Vector(*extent))
    return a


# ---------------------------------------------------------------- furniture, one builder per kind

def desk(tag, f, m, rng):
    folder = "Rooms/" + tag
    f.piece(tag + "_DeskTop", 0, 0, 76, (150, 75, 5), m["wood"], folder)
    for side in (-1, 1):
        f.piece(tag + "_DeskPed%d" % side, side * 55, 0, 37, (35, 70, 74), m["wood"], folder)
    f.piece(tag + "_DeskPapers", 20, -5, 79, (30, 22, 1), m["paper"], folder)
    searchable(tag + "_Desk", f, 0, 0, 45, (80, 42, 40), "desk", "the desk")


def bookshelf(tag, f, m, rng):
    folder = "Rooms/" + tag
    f.piece(tag + "_ShelfBack", 0, 15, 110, (180, 4, 220), m["wood"], folder)
    for side in (-1, 1):
        f.piece(tag + "_ShelfSide%d" % side, side * 88, 0, 110, (4, 34, 220), m["wood"], folder)
    for i, z in enumerate((10, 62, 114, 166, 218)):
        f.piece("%s_Shelf%d" % (tag, i), 0, 0, z, (172, 34, 3), m["wood"], folder)
        if i == 4:
            continue
        x = -82.0
        n = 0
        while x < 80:
            w = rng.uniform(3, 6.5)
            h = rng.uniform(22, 36)
            if rng.random() > 0.08:
                f.piece("%s_Book%d_%d" % (tag, i, n), x + w / 2, 3, z + 1.5 + h / 2, (w, 22, h),
                        rng.choice(m["books"]), folder + "/Books")
                n += 1
            x += w + 0.5
    searchable(tag + "_Bookshelf", f, 0, 0, 110, (92, 24, 112), "bookshelf", "the bookshelf")


def rug(tag, f, m, rng):
    f.piece(tag + "_Rug", 0, 0, 1, (260, 170, 1), m["rug"], "Rooms/" + tag)
    searchable(tag + "_Rug", f, 0, 0, 5, (130, 85, 6), "rug", "the rug")


def sofa(tag, f, m, rng):
    folder = "Rooms/" + tag
    f.piece(tag + "_SofaSeat", 0, 0, 22, (180, 80, 40), m["leather"], folder)
    f.piece(tag + "_SofaBack", 0, 33, 62, (180, 14, 60), m["leather"], folder)
    for side in (-1, 1):
        f.piece(tag + "_SofaArm%d" % side, side * 84, 0, 50, (14, 80, 26), m["leather"], folder)
    searchable(tag + "_Sofa", f, 0, 0, 40, (95, 44, 45), "sofa", "the sofa")


def cupboard(tag, f, m, rng):
    folder = "Rooms/" + tag
    f.piece(tag + "_Cupboard", 0, 0, 95, (100, 50, 190), m["wood"], folder)
    f.piece(tag + "_CupboardSeam", 0, -25.5, 95, (1, 1, 180), m["ink"], folder)
    for side in (-1, 1):
        f.piece(tag + "_CupboardKnob%d" % side, side * 6, -26, 100, (2, 2, 6), m["brass"], folder)
    searchable(tag + "_Cupboard", f, 0, 0, 95, (54, 30, 98), "cupboard", "the cupboard")


def window(tag, f, m, rng):
    """Against an outer wall: a night window (dim teal glass), with a sill to search."""
    folder = "Rooms/" + tag
    tag_actor(f.piece(tag + "_WindowGlass", 0, 12, 170, (150, 2, 150), m["night_glass"], folder, shadow=False), "Lightning")
    for i, lx in enumerate((-78, 0, 78)):
        f.piece("%s_WindowMullion%d" % (tag, i), lx, 10, 170, (6, 6, 156), m["wood"], folder)
    for i, z in enumerate((92, 170, 248)):
        f.piece("%s_WindowTransom%d" % (tag, i), 0, 10, z, (162, 6, 6), m["wood"], folder)
    f.piece(tag + "_WindowSill", 0, 0, 90, (170, 26, 5), m["wood"], folder)
    searchable(tag + "_Window", f, 0, 0, 150, (85, 20, 80), "window", "the window")
    # Dark unless the case has a storm: then lightning flashes in through the glass.
    x, y, _ = f.at(0, 4)
    c = L.light(unreal.RectLight, tag + "_Lightning", (x, y, 175), (0, 0, f.yaw - 90)).rect_light_component
    c.set_editor_property("intensity", 0.0)
    c.set_editor_property("source_width", 150)
    c.set_editor_property("source_height", 150)
    c.set_editor_property("attenuation_radius", 1100)
    c.set_editor_property("use_temperature", True)
    c.set_editor_property("temperature", 9500)
    c.set_editor_property("light_color", L.rgb(190, 225, 235))
    tag_actor(c.get_owner(), "Lightning")


def tag_actor(actor, tag):
    actor.set_editor_property("tags", list(actor.get_editor_property("tags")) + [unreal.Name(tag)])


def mains(c):
    """An electric light: dead in a blackout."""
    tag_actor(c.get_owner(), "Mains")
    return c


def candle(label, at, folder, m):
    """A candlestick brought out in a blackout; hidden otherwise."""
    x, y = at
    flame = L.practical(label + "_Flame", (x, y, 121), 3.5, 520, 1800, source=1.5)
    parts = [block(label + "_Stick", (x, y, 50), (5, 5, 100), m["brass"], folder, mesh=L.CYLINDER),
             block(label + "_Dish", (x, y, 100), (14, 14, 2), m["brass"], folder, mesh=L.CYLINDER),
             block(label + "_Wax", (x, y, 108), (3, 3, 14), m["candle"], folder, mesh=L.CYLINDER, shadow=False),
             block(label + "_Wick", (x, y, 117), (1.2, 1.2, 3), m["shade"], folder, mesh=L.CYLINDER, shadow=False),
             flame.get_owner()]
    for a in parts:
        tag_actor(a, "Candle")
        a.set_actor_hidden_in_game(True)


def coatrack(tag, f, m, rng):
    folder = "Rooms/" + tag
    f.piece(tag + "_RackPole", 0, 0, 90, (5, 5, 180), m["wood"], folder, mesh=L.CYLINDER)
    f.piece(tag + "_RackBase", 0, 0, 2, (40, 40, 4), m["wood"], folder, mesh=L.CYLINDER)
    f.piece(tag + "_RackCoat", 0, -10, 130, (45, 22, 90), m["leather"], folder)
    searchable(tag + "_CoatRack", f, 0, -5, 95, (30, 28, 95), "coatrack", "the coat rack")


def wastebasket(tag, f, m, rng):
    f.piece(tag + "_Basket", 0, 0, 18, (30, 30, 36), m["brass"], "Rooms/" + tag, mesh=L.CYLINDER)
    searchable(tag + "_Wastebasket", f, 0, 0, 18, (18, 18, 20), "wastebasket", "the wastebasket")


BUILDERS = {"desk": desk, "bookshelf": bookshelf, "rug": rug, "sofa": sofa, "cupboard": cupboard,
            "window": window, "coatrack": coatrack, "wastebasket": wastebasket}

# Slots in a room's frame (origin at the room centre, outer wall at +Y, door at -Y):
# against the outer wall left and right, against the side wall, and the middle of the floor.
SLOTS = [(-140, 255, 0), (150, 255, 0), (-255, 20, 90), (30, 40, 0)]

# Six rooms: (id, centre x, side +1 north / -1 south, can hold the body, furniture by slot).
ROOMS = [
    ("N1", -600, 1, False, ["cupboard", "coatrack", "desk", "rug"]),
    ("N2", 0, 1, True, ["bookshelf", "window", "desk", "sofa"]),
    ("N3", 600, 1, False, ["window", "bookshelf", "wastebasket", "sofa"]),
    ("S1", -600, -1, True, ["desk", "bookshelf", "wastebasket", "rug"]),
    ("S2", 0, -1, False, ["window", "cupboard", "coatrack", "sofa"]),
    ("S3", 600, -1, False, ["cupboard", "window", "desk", "rug"]),
]


# ---------------------------------------------------------------- walls

def wall_x(label, y, x0, x1, mat, doors=(), folder="Walls"):
    """A wall along X at y, from x0 to x1, with door openings centred on the given x values."""
    cuts = sorted(doors)
    x = x0
    for i, dx in enumerate(cuts):
        a, b = dx - DOOR_W / 2, dx + DOOR_W / 2
        if a > x:
            block("%s_%d" % (label, i), ((x + a) / 2, y, WALL_H / 2), (a - x, 20, WALL_H), mat, folder)
        block("%s_Head%d" % (label, i), (dx, y, (DOOR_H + WALL_H) / 2), (DOOR_W, 20, WALL_H - DOOR_H), mat, folder)
        block("%s_Frame%d" % (label, i), (dx, y, DOOR_H + 3), (DOOR_W + 16, 24, 6), L_MATS["wood"], folder)
        x = b
    if x1 > x:
        block("%s_End" % label, ((x + x1) / 2, y, WALL_H / 2), (x1 - x, 20, WALL_H), mat, folder)


def wall_y(label, x, y0, y1, mat, folder="Walls"):
    block(label, (x, (y0 + y1) / 2, WALL_H / 2), (20, y1 - y0, WALL_H), mat, folder)


L_MATS = {}


# ---------------------------------------------------------------- level

def add_hall(hall_end):
    hall = actors.spawn_actor_from_class(unreal.NoirRoom, unreal.Vector(0, 0, WALL_H / 2))
    hall.set_actor_label("Room_Hall")
    hall.set_folder_path("Gameplay/Rooms")
    hall.set_editor_property("room_id", unreal.Name("Hall"))
    hall.set_editor_property("cast", False)
    hall.set_editor_property("fixed_name", "Hall")
    hall.set_editor_property("fixed_name_es", "Vestíbulo")
    hall.get_editor_property("bounds").set_box_extent(unreal.Vector(hall_end, HALL_Y, WALL_H / 2))
    return hall


def build_level(m):
    rng = random.Random(1888)

    if L.eal.does_asset_exist(MAP_PATH):
        L.levels.load_level(MAP_PATH)
        actors.destroy_actors([a for a in actors.get_all_level_actors()
                               if not isinstance(a, unreal.WorldSettings) and type(a) is not unreal.Brush])
    elif not L.levels.new_level(MAP_PATH):
        raise RuntimeError("could not create " + MAP_PATH)

    half_len = 900.0
    outer = HALL_Y + ROOM_D
    block("Floor", (0, 0, -10), (2 * half_len + 240, 2 * outer + 40, 20), m["floor"], "Shell")
    block("Ceiling", (0, 0, WALL_H + 10), (2 * half_len + 240, 2 * outer + 40, 20), m["ceiling"], "Shell")
    block("HallRunner", (0, 0, 1), (2 * half_len + 160, 120, 1), m["rug"], "Shell")

    # Hall walls with a door into each room; outer walls; partitions; hall ends.
    for side, tag in ((1, "N"), (-1, "S")):
        wall_x("HallWall" + tag, side * HALL_Y, -half_len, half_len, m["wall"], ROOM_XS)
        wall_x("OuterWall" + tag, side * outer, -half_len, half_len, m["wall"])
        for px in (-300.0, 300.0, -half_len, half_len):
            ys = sorted((side * HALL_Y, side * outer))
            wall_y("Partition%s_%d" % (tag, int(px)), px, ys[0], ys[1], m["wall"])
    hall_end = half_len + 100
    for sx in (-1, 1):
        wall_y("HallEnd%d" % sx, sx * hall_end, -HALL_Y, HALL_Y, m["wall"])
        for side in (-1, 1):
            block("HallCorner%d_%d" % (sx, side), (sx * (half_len + 50), side * HALL_Y, WALL_H / 2),
                  (100, 20, WALL_H), m["wall"], "Walls")

    # The hall: never cast by the case, but on the map and named on screen.
    add_hall(hall_end)

    # Rooms.
    for room_id, cx, side, can_be_scene, kinds in ROOMS:
        cy = side * (HALL_Y + ROOM_D / 2)
        frame = Frame(cx, cy, 0 if side > 0 else 180)
        for kind, (lx, ly, yaw) in zip(kinds, SLOTS):
            BUILDERS[kind](room_id + "_" + kind, frame.child(lx, ly, yaw), m, rng)

        room = actors.spawn_actor_from_class(unreal.NoirRoom, unreal.Vector(cx, cy, WALL_H / 2),
                                             unreal.Rotator(0, 0, frame.yaw))
        room.set_actor_label("Room_" + room_id)
        room.set_folder_path("Gameplay/Rooms")
        room.set_editor_property("room_id", unreal.Name(room_id))
        room.set_editor_property("can_be_scene", can_be_scene)
        room.get_editor_property("bounds").set_box_extent(unreal.Vector(290, ROOM_D / 2 - 10, WALL_H / 2))
        # The body lies across the middle of the room, head toward the door side.
        room.set_editor_property("body_point", unreal.Transform(unreal.Vector(-40, -90, -WALL_H / 2),
                                                               unreal.Rotator(0, 0, -20), unreal.Vector(1, 1, 1)))

        for i, (lx, ly) in enumerate(((-130, -150), (140, -175))):
            x, y, _ = frame.at(lx, ly)
            stand = actors.spawn_actor_from_class(unreal.NoirStandPoint, unreal.Vector(x, y, 0),
                                                  unreal.Rotator(0, 0, frame.yaw - 90))
            stand.set_actor_label("Stand_%s_%d" % (room_id, i))
            stand.set_folder_path("Gameplay/Rooms")

        # A warm lamp in a corner and a dim pendant.
        lx, ly, _ = frame.at(-235, 225)
        block(room_id + "_LampPole", (lx, ly, 70), (4, 4, 140), m["brass"], "Rooms/" + room_id, mesh=L.CYLINDER)
        tag_actor(block(room_id + "_LampShade", (lx, ly, 150), (34, 34, 26), m["shade"], "Rooms/" + room_id, shadow=False), "Mains")
        mains(L.practical(room_id + "_Lamp", (lx, ly, 148), 7.0, 650, 2500, source=8))
        px, py, _ = frame.at(0, 20)
        tag_actor(block(room_id + "_Pendant", (px, py, 270), (28, 28, 12), m["shade"], "Rooms/" + room_id, shadow=False), "Mains")
        mains(L.practical(room_id + "_PendantLight", (px, py, 262), 3.5, 520, 2700, source=8))
        for i, (cx, cy) in enumerate(((-90, 165), (230, -205))):
            x, y, _ = frame.at(cx, cy)
            candle("%s_Candle%d" % (room_id, i), (x, y), "Rooms/" + room_id + "/Candles", m)

    # Hall: sconces between the doors.
    for i, x in enumerate((-800, -300, 300, 800)):
        for side in (-1, 1):
            y = side * (HALL_Y - 14)
            tag_actor(block("HallSconce%d_%d" % (i, side), (x, y, 215), (16, 12, 14), m["shade"], "Hall", shadow=False), "Mains")
            mains(L.practical("HallLight%d_%d" % (i, side), (x, y - side * 10, 212), 2.2, 380, 2600, source=6))
    for i, (x, side) in enumerate(((-550, 1), (0, -1), (550, 1))):
        candle("HallCandle%d" % i, (x, side * (HALL_Y - 30)), "Hall/Candles", m)

    # Analysis happens in the Mind Place (Tab), a study 40 m above the wing; the case board is there.
    case = mindplace.build(m, (0.0, 0.0, 4000.0))

    bx = -hall_end + 12
    director = actors.spawn_actor_from_class(unreal.NoirCaseDirector, unreal.Vector(bx + 100, 0, 50))
    director.set_actor_label("CaseDirector")
    director.set_folder_path("Gameplay")
    director.set_editor_property("board", case)

    ps = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-hall_end + 160, 0, 100), unreal.Rotator(0, 0, 0))
    ps.set_folder_path("Gameplay")

    # Atmosphere: thin haze, dark sky light, the look-dev grade.
    sc = L.light(unreal.SkyLight, "SkyLight", (0, 0, 500)).light_component
    sc.set_editor_property("real_time_capture", False)
    sc.set_editor_property("intensity", 0.06)
    sc.set_editor_property("lower_hemisphere_is_black", True)
    fog = L.light(unreal.ExponentialHeightFog, "HeightFog", (0, 0, 0), folder="Atmosphere").component
    fog.set_editor_property("fog_density", 0.02)
    fog.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.0008, 0.0016, 0.0016, 1))
    fog.set_editor_property("enable_volumetric_fog", True)
    fog.set_editor_property("volumetric_fog_extinction_scale", 1.5)
    ppv = L.light(unreal.PostProcessVolume, "PostProcess_Global", (0, 0, 0), folder="Atmosphere")
    ppv.set_editor_property("unbound", True)
    s = ppv.get_editor_property("settings")
    L.set_pp(s,
             auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
             auto_exposure_bias=0.7,
             auto_exposure_apply_physical_camera_exposure=False,
             color_saturation=unreal.Vector4(0.8, 0.8, 0.8, 1),
             color_gain_shadows=unreal.Vector4(0.92, 1.0, 0.98, 1),
             color_gain_highlights=unreal.Vector4(1.03, 1.0, 0.94, 1),
             color_contrast=unreal.Vector4(1.08, 1.08, 1.08, 1),
             bloom_intensity=0.6,
             vignette_intensity=0.6,
             film_grain_intensity=0.25)
    ppv.set_editor_property("settings", s)

    # A camera down the hall, for capture screenshots.
    cam = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(-hall_end + 120, -60, 165),
                                        unreal.Rotator(0, -5, 8))
    cam.set_actor_label("Cam_Hall")
    cam.set_folder_path("Cameras")

    L.levels.save_current_level()


if __name__ == "__main__":
    log("building materials")
    mats = L.build_materials()
    mats["night_glass"] = L.make_instance(unreal.load_asset(L.MAT_DIR + "/M_Master"), "MI_Glass_Night",
                                          (0.01, 0.02, 0.02), 0.1, emissive=(0.004, 0.012, 0.012))
    mats["candle"] = L.make_instance(unreal.load_asset(L.MAT_DIR + "/M_Master"), "MI_Candle_Wax",
                                     (0.55, 0.5, 0.4), 0.6, emissive=(0.05, 0.03, 0.01))
    L_MATS.update(mats)
    log("building level")
    build_level(mats)
    log("done")
    unreal.SystemLibrary.quit_editor()
