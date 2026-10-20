"""The Mind Place: the detective's study outside the case's world (Alan Wake 2's Mind Place).

Imported by level build scripts: build(mats, origin) adds the room, its NoirMindPlace actor and the
case board (a NoirCase) at origin, and returns the board so the level's NoirCaseDirector can use it.
Every case level gets the same room, away from its playable map; the detective reaches it with Tab.
"""
import random

import unreal

import build_lookdev as L

W, D, H = 900.0, 700.0, 330.0   # interior: X from -450 (the board wall) to +450, Y from -350 to +350


def build(m, origin):
    ox, oy, oz = origin
    rng = random.Random(1940)
    f = "MindPlace"

    def at(x, y, z):
        return (ox + x, oy + y, oz + z)

    def box(label, x, y, z, size, mat, folder=f, shadow=True, mesh=L.CUBE, rot=(0, 0, 0)):
        return L.block("MP_" + label, at(x, y, z), size, mat, folder, shadow=shadow, mesh=mesh, rot=rot)

    # Shell: panelled walls, dark floor, a low warm ceiling.
    box("Floor", 0, 0, -10, (W + 40, D + 40, 20), m["floor"])
    box("Ceiling", 0, 0, H + 10, (W + 40, D + 40, 20), m["ceiling"])
    for sign in (-1, 1):
        box("WallY%d" % sign, 0, sign * (D / 2 + 10), H / 2, (W + 40, 20, H), m["wall"])
        box("WallX%d" % sign, sign * (W / 2 + 10), 0, H / 2, (20, D, H), m["wall"])
        box("Rail%d" % sign, 0, sign * (D / 2 - 2), 95, (W, 4, 5), m["wood"])
    box("Rug", 60, 0, 1, (420, 300, 1), m["rug"])

    # The case board fills the back wall, between two lamps.
    bx = -W / 2 + 2
    box("Board", bx, 0, 175, (4, 400, 185), m["wood"], f + "/Board")
    box("BoardFrame", bx + 1, 0, 175, (2, 412, 197), m["frame"], f + "/Board")
    for i, y in enumerate((-150.0, -50.0, 50.0, 150.0)):
        # Dark brass hoods; the light is in the board, not in the lamps.
        box("BoardLamp%d" % i, bx + 18, y, 286, (18, 22, 12), m["brass"], f + "/Board", shadow=False)
        box("BoardLampArm%d" % i, bx + 8, y, 292, (20, 2, 2), m["brass"], f + "/Board", shadow=False)
        # Turned back toward the board (yaw 180) and down onto its middle.
        L.spot("MP_BoardLamp%d_Light" % i, at(bx + 60, y, 278), (0, -55, 180), 3.0, 72, 30, 520)
    # A low warm fill so nothing in here is lost in the dark: this is where you think.
    L.practical("MP_Fill", at(80, 0, 300), 6.0, 1100, 2900, source=60)
    board = L.actors.spawn_actor_from_class(unreal.NoirCase, unreal.Vector(*at(bx - 4, 0, 175)))
    board.set_actor_label("Case_Board")
    board.set_folder_path("Gameplay")
    board.get_editor_property("bounds").set_box_extent(unreal.Vector(6, 195, 92))
    board.set_editor_property("board_size", unreal.Vector2D(380, 172))

    # The evidence table, under a banker's lamp, where examined items are laid out.
    tx = 250.0
    box("TableTop", tx, 0, 78, (90, 260, 5), m["wood"], f + "/Table")
    for sx in (-1, 1):
        for sy in (-1, 1):
            box("TableLeg%d%d" % (sx, sy), tx + sx * 38, sy * 120, 38, (6, 6, 76), m["wood"], f + "/Table")
    box("TableCloth", tx, 0, 81, (80, 240, 1), m["leather"], f + "/Table")
    box("TableLampBase", tx + 30, 115, 83, (14, 10, 3), m["brass"], f + "/Table")
    box("TableLampShade", tx + 30, 108, 106, (12, 30, 6), m["shade_green"], f + "/Table", shadow=False)
    L.practical("MP_TableLamp", at(tx + 30, 100, 100), 5.0, 360, 2600)
    box("TablePendant", tx, 0, 255, (30, 30, 12), m["shade"], f + "/Table", shadow=False)
    L.practical("MP_TablePendantLight", at(tx, 0, 248), 6.0, 420, 2700, source=10)

    # The hearth corner: a fire and an armchair facing it.
    box("Fireplace", 60, -D / 2 + 25, 60, (160, 40, 120), m["brass"], f + "/Hearth")
    box("FireplaceMouth", 60, -D / 2 + 46, 40, (90, 4, 60), m["ink"], f + "/Hearth", shadow=False)
    fire = L.practical("MP_Fire", at(60, -D / 2 + 70, 40), 14.0, 600, 1900, source=25)
    box("ArmchairSeat", 60, -170, 25, (80, 80, 45), m["leather"], f + "/Hearth")
    box("ArmchairBack", 60, -132, 70, (80, 14, 90), m["leather"], f + "/Hearth")

    # Shelves along the window wall, and the night outside.
    for i, x in enumerate((-260.0, -60.0)):
        box("Shelf%d" % i, x, D / 2 - 18, 120, (180, 34, 240), m["wood"], f + "/Shelves")
        for row, z in enumerate((40, 92, 144, 196)):
            y0 = x - 85.0
            n = 0
            while y0 < x + 82:
                w = rng.uniform(3, 6.5)
                h = rng.uniform(22, 36)
                box("Book%d_%d_%d" % (i, row, n), y0 + w / 2, D / 2 - 30, z + h / 2, (w, 20, h),
                    rng.choice(m["books"]), f + "/Shelves/Books")
                y0 += w + 0.6
                n += 1
    box("Window", 230, D / 2 - 6, 175, (160, 4, 150), m["night_glass"], f, shadow=False)
    for i, x in enumerate((152.0, 230.0, 308.0)):
        box("WindowMullion%d" % i, x, D / 2 - 10, 175, (6, 6, 156), m["wood"])

    # The room itself, and where the detective arrives: facing the board.
    mp = L.actors.spawn_actor_from_class(unreal.NoirMindPlace, unreal.Vector(*at(0, 0, H / 2)))
    mp.set_actor_label("MindPlace")
    mp.set_folder_path("Gameplay")
    mp.get_editor_property("bounds").set_box_extent(unreal.Vector(W / 2, D / 2, H / 2))
    mp.set_editor_property("entry", unreal.Transform(unreal.Vector(130, 0, -H / 2 + 92), unreal.Rotator(0, 0, 180),
                                                     unreal.Vector(1, 1, 1)))
    mp.set_editor_property("table_centre", unreal.Vector(tx, 0, -H / 2 + 82))
    mp.set_editor_property("table_size", unreal.Vector2D(70, 225))
    mp.set_editor_property("board", board)

    # Its own grade: warmer, softer, darker at the edges: a different headspace.
    ppv = L.light(unreal.PostProcessVolume, "MP_PostProcess", at(0, 0, H / 2), folder=f)
    ppv.set_actor_scale3d(unreal.Vector(W / 200 + 0.2, D / 200 + 0.2, H / 200 + 0.2))
    ppv.set_editor_property("unbound", False)
    ppv.set_editor_property("priority", 2.0)
    s = ppv.get_editor_property("settings")
    L.set_pp(s,
             auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
             auto_exposure_bias=1.1,
             auto_exposure_apply_physical_camera_exposure=False,
             color_saturation=unreal.Vector4(0.62, 0.62, 0.62, 1),
             color_gain_highlights=unreal.Vector4(1.08, 1.0, 0.86, 1),
             color_gain_shadows=unreal.Vector4(1.0, 0.96, 0.9, 1),
             vignette_intensity=0.9,
             film_grain_intensity=0.35,
             bloom_intensity=0.7)
    ppv.set_editor_property("settings", s)
    return board
