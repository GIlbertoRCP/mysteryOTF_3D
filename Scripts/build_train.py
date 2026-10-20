"""Builds /Game/Maps/L_Train: the snowbound express, for generated "express" cases.

  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript=Scripts/build_train.py -unattended -nosplash -nullrhi

Six carriages in a line along +X, joined by vestibules, stopped in a mountain pass at night.
Each carriage is a NoirRoom: panelled, warm-lit, windows down both sides onto the snow, with
four or five pieces of searchable furniture of the kinds the generator hides its spots in
(a banquette plays the sofa, the overhead luggage rack the bookshelf, and so on), two stand
points beside the aisle and room in the aisle for the body. The case director casts the
carriages as the case's places ("Dining Car", "Compartment 7") on play.

Same placeholder geometry, materials and tags as the manor (Mains, Candle, Lightning), so
blackouts and storms work here too.
"""
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_lookdev as L  # noqa: E402
import build_manor as M  # noqa: E402  (furniture, tags and helpers)
import mindplace  # noqa: E402

MAP_PATH = "/Game/Maps/L_Train"
CAR_L = 1000.0       # interior length along X
CAR_W = 300.0        # interior width, Y from -150 to +150
CAR_H = 270.0
GAP = 160.0          # vestibule between carriages
DOOR_W, DOOR_H = 90.0, 205.0
WIN_XS = (-300.0, -150.0, 0.0, 150.0, 300.0)  # window centres along each side
WIN_W, WIN_Z0, WIN_Z1 = 110.0, 95.0, 190.0

actors = L.actors
block = L.block
Frame = M.Frame
searchable = M.searchable
tag_actor = M.tag_actor
mains = M.mains


def log(msg):
    unreal.log("[Train] " + msg)


# ---------------------------------------------------------------- furniture, in a carriage's local frame
# Like the manor's: origin at the piece's centre on the floor, back toward local +Y (the wall).

def banquette(tag, f, m, rng):
    """Two facing benches and a table between them, against the wall: plays the sofa."""
    folder = "Train/" + tag
    for side in (-1, 1):
        f.piece(tag + "_Seat%d" % side, side * 62, 0, 22, (46, 80, 44), m["leather"], folder)
        f.piece(tag + "_Back%d" % side, side * 84, 0, 70, (8, 80, 60), m["leather"], folder)
    f.piece(tag + "_Table", 0, 10, 72, (60, 60, 4), m["wood"], folder)
    f.piece(tag + "_TableLeg", 0, 25, 36, (6, 6, 70), m["brass"], folder)
    f.piece(tag + "_Cloth", 0, 10, 74.5, (50, 56, 1), m["paper"], folder)
    searchable(tag + "_Banquette", f, 0, 0, 30, (95, 42, 32), "sofa", "the banquette")


def writing_table(tag, f, m, rng):
    """A small fixed desk under a window with a shaded lamp: plays the desk."""
    folder = "Train/" + tag
    f.piece(tag + "_Top", 0, 0, 74, (90, 50, 4), m["wood"], folder)
    f.piece(tag + "_Drawer", 0, 2, 64, (86, 44, 16), m["wood"], folder)
    f.piece(tag + "_Leg", 0, 18, 32, (8, 8, 64), m["brass"], folder)
    f.piece(tag + "_Papers", 12, -4, 76.5, (26, 20, 1), m["paper"], folder)
    f.piece(tag + "_Chair", 0, -48, 24, (42, 40, 46), m["leather"], folder)
    x, y, _ = f.at(-30, 8)
    block(tag + "_LampBase", (x, y, 78), (8, 8, 4), m["brass"], folder, mesh=L.CYLINDER)
    tag_actor(block(tag + "_LampShade", (x, y, 98), (16, 16, 12), m["shade_green"], folder, shadow=False), "Mains")
    mains(L.practical(tag + "_Lamp", (x, y, 94), 3.0, 380, 2500, source=4))
    searchable(tag + "_WritingTable", f, 0, 0, 60, (48, 28, 24), "desk", "the writing table")


def luggage_rack(tag, f, m, rng):
    """Brass rack high on the wall with suitcases: plays the bookshelf."""
    folder = "Train/" + tag
    f.piece(tag + "_Rack", 0, 0, 205, (170, 40, 3), m["brass"], folder)
    for side in (-1, 1):
        f.piece(tag + "_Bracket%d" % side, side * 80, 15, 195, (3, 10, 22), m["brass"], folder)
    x = -78.0
    n = 0
    while x < 70:
        w = rng.uniform(26, 48)
        h = rng.uniform(16, 26)
        if x + w > 80:
            break
        f.piece("%s_Case%d" % (tag, n), x + w / 2, -2, 206.5 + h / 2, (w, 34, h),
                rng.choice((m["leather"], m["frame"], m["wood"])), folder)
        x += w + rng.uniform(2, 8)
        n += 1
    # Seats below it, so the rack is over somewhere to sit.
    f.piece(tag + "_Bench", 0, 0, 22, (150, 50, 44), m["leather"], folder)
    f.piece(tag + "_BenchBack", 0, 22, 66, (150, 8, 60), m["leather"], folder)
    searchable(tag + "_LuggageRack", f, 0, 0, 210, (88, 24, 20), "bookshelf", "the luggage rack")


def wardrobe(tag, f, m, rng):
    folder = "Train/" + tag
    f.piece(tag + "_Body", 0, 0, 95, (90, 50, 190), m["wood"], folder)
    f.piece(tag + "_Seam", 0, -25.5, 95, (1, 1, 180), m["ink"], folder)
    for side in (-1, 1):
        f.piece(tag + "_Knob%d" % side, side * 6, -26, 100, (2, 2, 6), m["brass"], folder)
    searchable(tag + "_Wardrobe", f, 0, 0, 95, (48, 30, 98), "cupboard", "the wardrobe")


def coat_hooks(tag, f, m, rng):
    folder = "Train/" + tag
    f.piece(tag + "_Board", 0, 10, 175, (90, 4, 14), m["wood"], folder)
    for i, x in enumerate((-30, 0, 30)):
        f.piece("%s_Hook%d" % (tag, i), x, 4, 172, (2, 8, 2), m["brass"], folder)
    f.piece(tag + "_Coat", -20, -4, 125, (40, 18, 90), m["leather"], folder)
    f.piece(tag + "_Coat2", 22, -2, 135, (36, 16, 72), m["rug"], folder)
    searchable(tag + "_CoatHooks", f, 0, -5, 130, (48, 20, 60), "coatrack", "the coat hooks")


def window_seat(tag, f, m, rng):
    """A window with a sill and curtains drawn back: plays the window."""
    folder = "Train/" + tag
    f.piece(tag + "_Sill", 0, 0, WIN_Z0 - 3, (WIN_W + 16, 22, 5), m["wood"], folder)
    for side in (-1, 1):
        f.piece(tag + "_Curtain%d" % side, side * (WIN_W / 2 + 10), -4, 150, (18, 6, 120), m["rug"], folder)
    searchable(tag + "_Window", f, 0, 0, 140, (WIN_W / 2 + 6, 18, 55), "window", "the window")


def runner(tag, f, m, rng):
    f.piece(tag + "_Runner", 0, 0, 1.2, (220, 110, 1), m["rug"], "Train/" + tag)
    searchable(tag + "_Runner", f, 0, 0, 5, (110, 55, 6), "rug", "the carpet runner")


def bin_(tag, f, m, rng):
    f.piece(tag + "_Bin", 0, 0, 16, (26, 26, 32), m["brass"], "Train/" + tag, mesh=L.CYLINDER)
    searchable(tag + "_Wastebasket", f, 0, 0, 16, (16, 16, 18), "wastebasket", "the wastebasket")


BUILDERS = {"sofa": banquette, "desk": writing_table, "bookshelf": luggage_rack, "cupboard": wardrobe,
            "coatrack": coat_hooks, "window": window_seat, "rug": runner, "wastebasket": bin_}

# ---------------------------------------------------------------- decor (nothing searchable)

def armchair(label, x, y, yaw, m, folder):
    f = Frame(x, y, yaw)
    f.piece(label + "_Seat", 0, 0, 22, (64, 62, 40), m["leather"], folder)
    f.piece(label + "_Back", 0, 28, 62, (64, 10, 70), m["leather"], folder)
    for s in (-1, 1):
        f.piece("%s_Arm%d" % (label, s), s * 29, 0, 50, (8, 60, 18), m["leather"], folder)


def table_for_two(label, x, side, m, folder, shade):
    """A small table under a window with a chair either side and a little shaded lamp."""
    y = side * (CAR_W / 2 - 34)
    block(label + "_Top", (x, y, 72), (56, 56, 4), m["wood"], folder)
    block(label + "_Cloth", (x, y, 74.5), (50, 60, 1), m["paper"], folder)
    block(label + "_Leg", (x, y, 36), (6, 6, 70), m["brass"], folder)
    for s in (-1, 1):
        armchair("%s_Chair%d" % (label, s), x + s * 48, y, 90 * s, m, folder)
    block(label + "_LampBase", (x, y + side * 14, 80), (5, 5, 10), m["brass"], folder, mesh=L.CYLINDER)
    tag_actor(block(label + "_LampShade", (x, y + side * 14, 90), (12, 12, 8), m[shade], folder, shadow=False), "Mains")
    mains(L.practical(label + "_Lamp", (x, y + side * 10, 86), 1.2, 240, 2400, source=3))


def dress_sleeping(cx, m, folder, rng):
    """A corridor down the north side, four compartments off it to the south."""
    wall_y = 55.0
    doors = (-375.0, -125.0, 125.0, 375.0)
    edges = [-CAR_L / 2] + [e for d in doors for e in (d - 38, d + 38)] + [CAR_L / 2]
    for i in range(0, len(edges), 2):
        a, b = edges[i], edges[i + 1]
        block("%s_Partition%d" % (folder, i), (cx + (a + b) / 2, wall_y, CAR_H / 2), (b - a, 8, CAR_H), m["wall"], folder)
    for i, d in enumerate(doors):
        block("%s_DoorHead%d" % (folder, i), (cx + d, wall_y, 235), (76, 8, CAR_H - 200), m["wall"], folder)
        block("%s_DoorFrame%d" % (folder, i), (cx + d, wall_y, 202), (84, 12, 5), m["brass"], folder)
        # The door itself, slid half open.
        block("%s_Door%d" % (folder, i), (cx + d - 30, wall_y - 9, 100), (40, 4, 196), m["wood"], folder)
        block("%s_Number%d" % (folder, i), (cx + d, wall_y + 5, 212), (14, 1, 7), m["brass"], folder, shadow=False)
        lamp = (cx + d, -50, CAR_H - 16)
        tag_actor(block("%s_CompLamp%d" % (folder, i), lamp, (20, 20, 6), m["shade"], folder, shadow=False), "Mains")
        mains(L.practical("%s_CompLight%d" % (folder, i), (lamp[0], lamp[1], lamp[2] - 8), 1.4, 260, 2600, source=5))
    for x in (-250.0, 0.0, 250.0):
        block("%s_Divider%d" % (folder, int(x)), (cx + x, (-CAR_W / 2 + wall_y) / 2, CAR_H / 2),
              (8, wall_y + CAR_W / 2, CAR_H), m["wall"], folder)


def dress_dining(cx, m, folder, rng, taken):
    for side in (-1, 1):
        for x in WIN_XS:
            if (x, side) not in taken:
                table_for_two("%s_Table%d_%d" % (folder, int(x), side), cx + x, side, m, folder, "shade_rose")


def dress_bar(cx, m, folder, rng):
    """A bar counter along the south side, stools, bottles; armchairs to the north."""
    y = -CAR_W / 2 + 30
    block(folder + "_Counter", (cx - 100, y, 52), (480, 46, 104), m["wood"], folder)
    block(folder + "_CounterTop", (cx - 100, y + 3, 106), (488, 54, 4), m["brass"], folder)
    block(folder + "_FootRail", (cx - 100, y + 30, 18), (480, 3, 3), m["brass"], folder, shadow=False)
    for k in range(6):
        sx = cx - 300 + k * 80
        block("%s_Stool%d" % (folder, k), (sx, y + 60, 38), (6, 6, 76), m["brass"], folder, mesh=L.CYLINDER)
        block("%s_StoolSeat%d" % (folder, k), (sx, y + 60, 77), (34, 34, 6), m["leather"], folder, mesh=L.CYLINDER)
    glass = [m["glass_amber"], m["glass_green"], m["wax"]]
    for k in range(14):
        bx = cx - 320 + k * 30 + rng.uniform(-6, 6)
        h = rng.uniform(22, 32)
        block("%s_Bottle%d" % (folder, k), (bx, y - 10, 108 + h / 2), (7, 7, h), rng.choice(glass), folder, mesh=L.CYLINDER)
    for k, x in enumerate((-300.0, 0.0)):
        armchair("%s_Armchair%d" % (folder, k), cx + x, CAR_W / 2 - 50, 180, m, folder)


def dress_library(cx, m, folder, rng, taken):
    """Low bookcases under the windows, reading chairs."""
    for side in (-1, 1):
        for x in WIN_XS:
            if (x, side) in taken:
                continue
            y = side * (CAR_W / 2 - 16)
            block("%s_Case%d_%d" % (folder, int(x), side), (cx + x, y, 44), (WIN_W, 28, 88), m["wood"], folder)
            bx = x - WIN_W / 2 + 4
            n = 0
            while bx < x + WIN_W / 2 - 6:
                w = rng.uniform(3, 6)
                h = rng.uniform(20, 30)
                for z in (8, 50):
                    block("%s_Book%d_%d_%d_%d" % (folder, int(x), side, n, z), (cx + bx + w / 2, y - side * 2, z + h / 2),
                          (w, 22, h), rng.choice(m["books"]), folder)
                bx += w + 0.5
                n += 1
    for k, (x, side) in enumerate(((-150.0, -1), (150.0, 1))):
        if (x, side) not in taken:
            armchair("%s_Reader%d" % (folder, k), cx + x, side * 40, 0 if side < 0 else 180, m, folder)


def dress_luggage(cx, m, folder, rng):
    """Bare planks, crates and trunks, a mail sack or two."""
    for k, (x, side) in enumerate(((-300, 1), (-120, -1), (60, 1), (300, -1), (360, 1))):
        y = side * (CAR_W / 2 - 40)
        h = 0
        for j in range(rng.randint(1, 3)):
            w, d, t = rng.uniform(50, 80), rng.uniform(40, 60), rng.uniform(30, 50)
            block("%s_Crate%d_%d" % (folder, k, j), (cx + x + rng.uniform(-8, 8), y, h + t / 2), (w, d, t),
                  rng.choice((m["plank"], m["wood"], m["leather"])), folder, rot=(0, 0, rng.uniform(-6, 6)))
            h += t
    for k, x in enumerate((-40, 200)):
        block("%s_Sack%d" % (folder, k), (cx + x, -CAR_W / 2 + 35, 25), (50, 40, 50), m["canvas"], folder, mesh=L.SPHERE)


def dress_observation(cx, m, folder, rng, taken):
    """Armchairs turned to the windows, and a wide window at the end of the train."""
    for k, x in enumerate((-300.0, 300.0)):
        for side in (-1, 1):
            if (x, side) not in taken:
                armchair("%s_Chair%d_%d" % (folder, k, side), cx + x, side * 70, 0 if side > 0 else 180, m, folder)
                block("%s_SideTable%d_%d" % (folder, k, side), (cx + x + 50, side * 95, 30), (30, 30, 60), m["wood"], folder, mesh=L.CYLINDER)


# Carriages, front to back. "pieces": the searchable furniture, as (kind, local x, side) with
# side +1 the north wall, or (kind, local x, local y, yaw) placed exactly. "suits": the case places
# the carriage is dressed for (staging casts it as one of them when it can).
CARS = [
    dict(id="C1", style="sleeping", scene=True, wall="wall", suits=["Compartment 7", "Sleeping-Car Corridor"],
         pieces=[("sofa", -375, -1), ("desk", -125, -1), ("bookshelf", 125, -1), ("window", 300, -1), ("rug", 0, 102, 0)],
         stands=((-250, 100, 180), (250, 100, 0)), body=(-125, -45, 0)),
    dict(id="C2", style="dining", scene=False, wall="teak", suits=["Dining Car"],
         pieces=[("sofa", -300, 1), ("sofa", 150, 1), ("desk", -430, -1), ("wastebasket", 430, -1), ("window", 0, -1)]),
    dict(id="C3", style="bar", scene=False, wall="wall", suits=["Bar Car", "Smoking Salon"],
         pieces=[("coatrack", -430, 1), ("desk", -150, 1), ("sofa", 300, 1), ("cupboard", 430, -1)]),
    dict(id="C4", style="library", scene=False, wall="baize", suits=["Library Car", "Smoking Salon"],
         pieces=[("bookshelf", -300, -1), ("desk", -150, 1), ("rug", 0, 0), ("sofa", 300, 1), ("cupboard", 430, -1)]),
    dict(id="C5", style="luggage", scene=True, wall="plank", suits=["Luggage Van", "Third-Class Carriage"],
         pieces=[("cupboard", -430, 1), ("window", -150, 1), ("coatrack", 430, -1), ("wastebasket", -430, -1), ("sofa", 150, -1)]),
    dict(id="C6", style="observation", scene=True, wall="teak", suits=["Observation Car", "Bar Car"],
         pieces=[("window", 0, 1), ("sofa", -150, -1), ("bookshelf", 150, -1), ("desk", 150, 1)]),
]


def car_centre(i):
    return i * (CAR_L + GAP)


# ---------------------------------------------------------------- shell

def side_wall(label, cx, side, m, folder, wall):
    """A carriage side with window openings; panelled inside, lacquered outside."""
    y = side * (CAR_W / 2 + 6)
    x0, x1 = cx - CAR_L / 2, cx + CAR_L / 2
    xs = [cx + wx for wx in WIN_XS]
    block(label + "_Low", (cx, y, WIN_Z0 / 2), (CAR_L, 12, WIN_Z0), wall, folder)
    block(label + "_High", (cx, y, (WIN_Z1 + CAR_H) / 2), (CAR_L, 12, CAR_H - WIN_Z1), wall, folder)
    edges = [x0] + [e for x in xs for e in (x - WIN_W / 2, x + WIN_W / 2)] + [x1]
    for i in range(0, len(edges), 2):
        a, b = edges[i], edges[i + 1]
        block("%s_Pier%d" % (label, i // 2), ((a + b) / 2, y, (WIN_Z0 + WIN_Z1) / 2), (b - a, 12, WIN_Z1 - WIN_Z0), wall, folder)
    yo = side * (CAR_W / 2 + 14)
    block(label + "_OutLow", (cx, yo, (WIN_Z0 - 60) / 2), (CAR_L + 20, 4, WIN_Z0 + 60), m["lacquer"], folder)
    block(label + "_OutHigh", (cx, yo, (WIN_Z1 + CAR_H + 30) / 2), (CAR_L + 20, 4, CAR_H + 30 - WIN_Z1), m["lacquer"], folder)
    block(label + "_Line", (cx, side * (CAR_W / 2 + 16.5), WIN_Z0 - 18), (CAR_L + 20, 1, 3), m["brass"], folder, shadow=False)
    for i, x in enumerate(xs):
        block("%s_WinSill%d" % (label, i), (x, side * (CAR_W / 2 - 2), WIN_Z0 - 2), (WIN_W + 8, 10, 4), m["wood"], folder)
        block("%s_WinHead%d" % (label, i), (x, side * (CAR_W / 2 - 2), WIN_Z1 + 2), (WIN_W + 8, 10, 4), m["wood"], folder)


def end_wall(label, x, m, folder, wall, door=True):
    """A carriage end with a central door, or (the last carriage) a wide window onto the line."""
    if door:
        half = (CAR_W - DOOR_W) / 2
        for s in (-1, 1):
            block("%s_%d" % (label, s), (x, s * (DOOR_W / 2 + half / 2), CAR_H / 2), (12, half, CAR_H), wall, folder)
        block(label + "_Head", (x, 0, (DOOR_H + CAR_H) / 2), (12, DOOR_W, CAR_H - DOOR_H), wall, folder)
        block(label + "_Frame", (x, 0, DOOR_H + 3), (16, DOOR_W + 14, 6), m["wood"], folder)
        return
    block(label + "_Low", (x, 0, 45), (12, CAR_W, 90), wall, folder)
    block(label + "_High", (x, 0, (220 + CAR_H) / 2), (12, CAR_W, CAR_H - 220), wall, folder)
    for s in (-1, 1):
        block("%s_Jamb%d" % (label, s), (x, s * (CAR_W / 2 - 15), 155), (12, 30, 130), wall, folder)
    block(label + "_Mullion", (x, 0, 155), (10, 6, 130), m["wood"], folder)


def lights(car_id, cx, style, m, folder):
    if style == "luggage":
        # Two bare bulbs and nothing else.
        for k, wx in enumerate((-250, 250)):
            tag_actor(block("%s_Bulb%d" % (car_id, k), (cx + wx, 0, CAR_H - 25), (8, 8, 10), m["shade"], folder,
                            mesh=L.SPHERE, shadow=False), "Mains")
            mains(L.practical("%s_BulbLight%d" % (car_id, k), (cx + wx, 0, CAR_H - 32), 1.6, 480, 2300, source=2))
        return
    sides = (1,) if style == "sleeping" else (-1, 1)
    for k, wx in enumerate((-225, 225)):
        for side in sides:
            y = side * (CAR_W / 2 - 8)
            tag_actor(block("%s_Sconce%d_%d" % (car_id, k, side), (cx + wx, y, 175), (10, 8, 14), m["shade"], folder, shadow=False), "Mains")
            mains(L.practical("%s_SconceLight%d_%d" % (car_id, k, side), (cx + wx, y - side * 8, 172), 1.6, 300, 2500, source=4))
    if style in ("dining", "bar", "library", "observation"):
        for k, wx in enumerate((-250, 250)):
            tag_actor(block("%s_CeilingLamp%d" % (car_id, k), (cx + wx, 0, CAR_H - 12), (36, 22, 8), m["shade"], folder, shadow=False), "Mains")
            mains(L.practical("%s_CeilingLight%d" % (car_id, k), (cx + wx, 0, CAR_H - 20), 2.2 if style != "bar" else 1.4, 520, 2700, source=10))
    if style == "sleeping":
        for k, wx in enumerate((-250, 250)):
            mains(L.practical("%s_CorridorLight%d" % (car_id, k), (cx + wx, 102, CAR_H - 20), 1.2, 320, 2600, source=6))
    if style == "bar":
        # A warm glow along the counter.
        mains(L.practical("%s_BarGlow" % car_id, (cx - 100, -CAR_W / 2 + 70, 100), 2.0, 300, 2200, source=30))


def build_car(i, car, m, rng, last):
    cx = car_centre(i)
    car_id, style = car["id"], car["style"]
    folder = "Train/" + car_id
    wall = m[car["wall"]]
    block(car_id + "_Floor", (cx, 0, -6), (CAR_L + 20, CAR_W + 40, 12), m["plank"] if style == "luggage" else m["floor"], folder)
    block(car_id + "_Ceiling", (cx, 0, CAR_H + 6), (CAR_L + 20, CAR_W + 40, 12), m["ceiling"], folder)
    if style != "luggage":
        block(car_id + "_Clerestory", (cx, 0, CAR_H - 6), (CAR_L - 40, 120, 4), m["wood"], folder)
    block(car_id + "_Roof", (cx, 0, CAR_H + 30), (CAR_L + 30, CAR_W + 30, 36), m["lacquer"], folder)
    block(car_id + "_Underframe", (cx, 0, -45), (CAR_L - 100, CAR_W - 40, 60), m["ink"], folder)
    for side in (-1, 1):
        side_wall("%s_Side%d" % (car_id, side), cx, side, m, folder, wall)
        if style != "luggage":
            block("%s_Cornice%d" % (car_id, side), (cx, side * (CAR_W / 2 - 6), CAR_H - 14), (CAR_L, 12, 10), m["wood"], folder)
            block("%s_Dado%d" % (car_id, side), (cx, side * (CAR_W / 2 - 1), 92), (CAR_L, 4, 4), m["brass"], folder, shadow=False)
    for end in (-1, 1):
        end_wall("%s_End%d" % (car_id, end), cx + end * (CAR_L / 2 + 6), m, folder, wall,
                 door=not (last and end > 0 and style == "observation"))
        block("%s_Bogie%d" % (car_id, end), (cx + end * (CAR_L / 2 - 170), 0, -95), (240, CAR_W - 60, 40), m["ink"], folder)
        for w in (-1, 1):
            for s in (-1, 1):
                block("%s_Wheel%d_%d_%d" % (car_id, end, w, s), (cx + end * (CAR_L / 2 - 170) + w * 80, s * 72, -100),
                      (60, 8, 60), m["steel"], folder, mesh=L.CYLINDER, rot=(90, 0, 0))

    taken = set()
    for n, piece in enumerate(car["pieces"]):
        if len(piece) == 4:
            kind, lx, ly, yaw = piece
            f = Frame(cx + lx, ly, yaw)
        elif piece[2] == 0:
            kind, lx, _ = piece
            f = Frame(cx + lx, 0, 0)
        else:
            kind, lx, side = piece
            depth = {"sofa": 45, "desk": 28, "bookshelf": 25, "cupboard": 28, "coatrack": 8,
                     "window": 10, "wastebasket": 15}.get(kind, 30)
            f = Frame(cx + lx, side * (CAR_W / 2 - depth), 0 if side > 0 else 180)
            taken.add((float(lx), side))
        BUILDERS[kind]("%s_%s%d" % (car_id, kind, n), f, m, rng)

    if style == "sleeping":
        dress_sleeping(cx, m, folder, rng)
    elif style == "dining":
        dress_dining(cx, m, folder, rng, taken)
    elif style == "bar":
        dress_bar(cx, m, folder, rng)
    elif style == "library":
        dress_library(cx, m, folder, rng, taken)
    elif style == "luggage":
        dress_luggage(cx, m, folder, rng)
    elif style == "observation":
        dress_observation(cx, m, folder, rng, taken)

    room = actors.spawn_actor_from_class(unreal.NoirRoom, unreal.Vector(cx, 0, CAR_H / 2))
    room.set_actor_label("Room_" + car_id)
    room.set_folder_path("Gameplay/Rooms")
    room.set_editor_property("room_id", unreal.Name(car_id))
    room.set_editor_property("can_be_scene", car["scene"])
    room.set_editor_property("suits", car["suits"])
    room.get_editor_property("bounds").set_box_extent(unreal.Vector(CAR_L / 2, CAR_W / 2, CAR_H / 2))
    bx, by, byaw = car.get("body", (-80, -10, 8))
    room.set_editor_property("body_point", unreal.Transform(unreal.Vector(bx, by, -CAR_H / 2),
                                                           unreal.Rotator(0, 0, byaw), unreal.Vector(1, 1, 1)))
    for k, (lx, ly, yaw) in enumerate(car.get("stands", ((-240, 62, 200), (240, -62, 20)))):
        stand = actors.spawn_actor_from_class(unreal.NoirStandPoint, unreal.Vector(cx + lx, ly, 0), unreal.Rotator(0, 0, yaw))
        stand.set_actor_label("Stand_%s_%d" % (car_id, k))
        stand.set_folder_path("Gameplay/Rooms")

    lights(car_id, cx, style, m, folder)
    for k, (wx, side) in enumerate(((-150, 1), (150, -1))):
        M.candle("%s_Candle%d" % (car_id, k), (cx + wx, side * (CAR_W / 2 - 10)), folder + "/Candles", m)
    for side in (-1, 1):
        c = L.light(unreal.RectLight, "%s_Lightning%d" % (car_id, side), (cx, side * (CAR_W / 2 + 120), 150),
                    (0, 0, -90 if side > 0 else 90)).rect_light_component
        c.set_editor_property("intensity", 0.0)
        c.set_editor_property("source_width", CAR_L)
        c.set_editor_property("source_height", 200)
        c.set_editor_property("attenuation_radius", 800)
        c.set_editor_property("use_temperature", True)
        c.set_editor_property("temperature", 9500)
        tag_actor(c.get_owner(), "Lightning")


def vestibule(i, m):
    """The gangway between carriage i and i+1: narrow, dark, bellows at the sides."""
    x = car_centre(i) + CAR_L / 2 + 12 + (GAP - 24) / 2
    folder = "Train/Vestibules"
    block("Vest%d_Floor" % i, (x, 0, -6), (GAP, 160, 12), m["steel"], folder)
    block("Vest%d_Ceiling" % i, (x, 0, DOOR_H + 30), (GAP, 160, 12), m["ink"], folder)
    for s in (-1, 1):
        block("Vest%d_Bellows%d" % (i, s), (x, s * 76, (DOOR_H + 30) / 2), (GAP - 24, 8, DOOR_H + 30), m["ink"], folder)
        for k in range(5):
            block("Vest%d_Fold%d_%d" % (i, s, k), (x - (GAP - 40) / 2 + k * (GAP - 40) / 4, s * 70, (DOOR_H + 30) / 2),
                  (4, 6, DOOR_H + 30), m["leather"], folder, shadow=False)
    mains(L.practical("Vest%d_Light" % i, (x, 0, DOOR_H + 10), 0.8, 220, 2300, source=3))


def outside(m, length):
    """The pass: snow, the rails, pines, the mountain wall and a cold moon."""
    folder = "Outside"
    x0, x1 = -CAR_L, length + CAR_L
    mid = (x0 + x1) / 2
    block("Snowfield", (mid, 0, -130), (x1 - x0 + 20000, 30000, 20), m["snow"], folder)
    block("TrackBed", (mid, 0, -121), (x1 - x0 + 8000, 360, 4), m["ballast"], folder)
    for s in (-1, 1):
        block("Rail%d" % s, (mid, s * 72, -118), (x1 - x0 + 8000, 7, 8), m["steel"], folder)
        # Banks of drifted snow along the line.
        block("Drift%d" % s, (mid, s * 520, -110), (x1 - x0 + 8000, 400, 60), m["snow"], folder, rot=(s * 4, 0, 0))
    rng = random.Random(1923)
    n = 0
    for s in (-1, 1):
        for _ in range(70):
            x = rng.uniform(x0 - 2000, x1 + 2000)
            y = s * rng.uniform(700, 4500)
            h = rng.uniform(500, 1500)
            r = h * rng.uniform(0.22, 0.3)
            block("Pine%d_Trunk" % n, (x, y, -130 + h * 0.1), (r * 0.12, r * 0.12, h * 0.2), m["bark"], folder + "/Pines", mesh=L.CYLINDER)
            block("Pine%d" % n, (x, y, -130 + h * 0.55), (r * 2, r * 2, h * 0.9), m["foliage"], folder + "/Pines", mesh=L.CONE)
            block("Pine%d_Snow" % n, (x, y, -130 + h * 0.78), (r * 1.2, r * 1.2, h * 0.4), m["snow"], folder + "/Pines", mesh=L.CONE)
            n += 1
    # The mountainside rising on the north, falling away on the south.
    block("Mountain", (mid, 9000, 1500), (x1 - x0 + 30000, 4000, 4000), m["rock"], folder, rot=(-30, 0, 0))

    moon = L.light(unreal.DirectionalLight, "Moonlight", (0, 0, 2000), (0, -28, -60), folder="Atmosphere").light_component
    moon.set_editor_property("intensity", 3.0)
    moon.set_editor_property("use_temperature", True)
    moon.set_editor_property("temperature", 9000)
    moon.set_editor_property("light_color", L.rgb(170, 205, 230))
    sky = L.light(unreal.SkyLight, "SkyLight", (0, 0, 800), folder="Atmosphere").light_component
    sky.set_editor_property("real_time_capture", False)
    sky.set_editor_property("intensity", 0.4)
    sky.set_editor_property("lower_hemisphere_is_black", False)
    fog = L.light(unreal.ExponentialHeightFog, "HeightFog", (0, 0, -100), folder="Atmosphere").component
    fog.set_editor_property("fog_density", 0.035)
    fog.set_editor_property("fog_height_falloff", 0.05)
    fog.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.02, 0.032, 0.04, 1))
    fog.set_editor_property("enable_volumetric_fog", True)
    fog.set_editor_property("volumetric_fog_extinction_scale", 1.2)


def build_level(m):
    rng = random.Random(1883)
    if L.eal.does_asset_exist(MAP_PATH):
        L.levels.load_level(MAP_PATH)
        actors.destroy_actors([a for a in actors.get_all_level_actors()
                               if not isinstance(a, unreal.WorldSettings) and type(a) is not unreal.Brush])
    elif not L.levels.new_level(MAP_PATH):
        raise RuntimeError("could not create " + MAP_PATH)

    for i, car in enumerate(CARS):
        build_car(i, car, m, rng, last=i == len(CARS) - 1)
        if i < len(CARS) - 1:
            vestibule(i, m)
    length = car_centre(len(CARS) - 1)
    # The front of the train: the door forward is snowed shut. The back is the observation window.
    block("TrainEnd0", (-CAR_L / 2 - 20, 0, CAR_H / 2), (16, CAR_W, CAR_H), m["lacquer"], "Train")
    snow = actors.spawn_actor_from_class(unreal.NoirSnowfall, unreal.Vector(0, 0, 0))
    snow.set_actor_label("Snowfall")
    snow.set_folder_path("Outside")
    outside(m, length)

    case = mindplace.build(m, (0.0, 0.0, 4000.0))
    director = actors.spawn_actor_from_class(unreal.NoirCaseDirector, unreal.Vector(-CAR_L / 2 + 100, 0, 50))
    director.set_actor_label("CaseDirector")
    director.set_folder_path("Gameplay")
    director.set_editor_property("board", case)
    director.set_editor_property("theme", "express")

    ps = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-CAR_L / 2 + 120, 0, 100), unreal.Rotator(0, 0, 0))
    ps.set_folder_path("Gameplay")

    ppv = L.light(unreal.PostProcessVolume, "PostProcess_Global", (0, 0, 0), folder="Atmosphere")
    ppv.set_editor_property("unbound", True)
    s = ppv.get_editor_property("settings")
    L.set_pp(s,
             auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
             auto_exposure_bias=0.8,
             auto_exposure_apply_physical_camera_exposure=False,
             color_saturation=unreal.Vector4(0.8, 0.8, 0.8, 1),
             color_gain_shadows=unreal.Vector4(0.9, 1.0, 1.04, 1),
             color_gain_highlights=unreal.Vector4(1.03, 1.0, 0.94, 1),
             color_contrast=unreal.Vector4(1.08, 1.08, 1.08, 1),
             bloom_intensity=0.6,
             vignette_intensity=0.6,
             film_grain_intensity=0.25)
    ppv.set_editor_property("settings", s)

    cam = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(-CAR_L / 2 + 60, -60, 165), unreal.Rotator(0, -4, 6))
    cam.set_actor_label("Cam_Car")
    cam.set_folder_path("Cameras")
    L.levels.save_current_level()


if __name__ == "__main__":
    log("building materials")
    mats = L.build_materials()
    mi = lambda *a, **k: L.make_instance(unreal.load_asset(L.MAT_DIR + "/M_Master"), *a, **k)
    mats["night_glass"] = mi("MI_Glass_Night", (0.01, 0.02, 0.02), 0.1, emissive=(0.004, 0.012, 0.012))
    mats["candle"] = mi("MI_Candle_Wax", (0.55, 0.5, 0.4), 0.6, emissive=(0.05, 0.03, 0.01))
    mats["lacquer"] = mi("MI_Lacquer_Midnight", (0.012, 0.018, 0.04), 0.3)
    mats["snow"] = mi("MI_Snow", (0.62, 0.66, 0.7), 0.7)
    mats["ballast"] = mi("MI_Ballast", (0.05, 0.05, 0.055), 0.9)
    mats["rock"] = mi("MI_Rock_Dark", (0.03, 0.035, 0.04), 0.9)
    mats["teak"] = mi("MI_Wall_Teak", (0.07, 0.035, 0.016), 0.55)
    mats["baize"] = mi("MI_Wall_Baize", (0.016, 0.035, 0.022), 0.85)
    mats["plank"] = mi("MI_Plank_Raw", (0.06, 0.045, 0.03), 0.85)
    mats["shade_rose"] = mi("MI_LampShade_Rose", (0.5, 0.3, 0.26), 0.9, emissive=(2.4, 1.2, 0.8))
    mats["glass_amber"] = mi("MI_Glass_Amber", (0.25, 0.1, 0.02), 0.1)
    mats["glass_green"] = mi("MI_Glass_Green", (0.03, 0.08, 0.04), 0.1)
    M.L_MATS.update(mats)
    log("building level")
    build_level(mats)
    log("done")
    unreal.SystemLibrary.quit_editor()
