"""Builds /Game/Maps/L_LookDev: a placeholder study lit for noir look development.

Run headless from the repo root:
  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript=Scripts/build_lookdev.py -unattended -nosplash -nullrhi

Re-running rebuilds the level and the M_Master instances in place, so tune values here
(or tweak in the editor and copy them back) rather than hand-editing twice.

Targets (Reference/, not in git): the Knives Out study (many small warm practicals,
each with its own pool; dense, muted red/brown/green clutter) and the Alan Wake 2
Mind Place board (paper on dark wood between two glowing sconces, the rest near-black).
"""
import random

import unreal

MAP_PATH = "/Game/Maps/L_LookDev"
MAT_DIR = "/Game/Materials"
CUBE = "/Engine/BasicShapes/Cube.Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"
CONE = "/Engine/BasicShapes/Cone.Cone"
SPHERE = "/Engine/BasicShapes/Sphere.Sphere"

# Moonlight direction as (roll, pitch, yaw); the moon disc is placed from it.
MOON_ROT = (0, -20, 190)

assets = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(msg):
    unreal.log("[LookDev] " + msg)


def rgb(r, g, b):
    """sRGB FColor. unreal.Color's positional order is (b, g, r, a), so always go through this."""
    return unreal.Color(r=r, g=g, b=b, a=255)


# ---------------------------------------------------------------- materials

def build_master_material():
    """M_Master: tinted base colour with world-space wear, plus optional emissive."""
    path = MAT_DIR + "/M_Master"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = assets.create_asset("M_Master", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())

    def param(cls, name, x, y, default):
        node = mel.create_material_expression(mat, cls, x, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", default)
        return node

    base = param(unreal.MaterialExpressionVectorParameter, "BaseColor", -600, -200,
                 unreal.LinearColor(0.2, 0.2, 0.2, 1))
    rough = param(unreal.MaterialExpressionScalarParameter, "Roughness", -600, 0, 0.7)
    metal = param(unreal.MaterialExpressionScalarParameter, "Metallic", -600, 100, 0.0)
    spec = param(unreal.MaterialExpressionScalarParameter, "Specular", -600, 200, 0.5)
    emissive = param(unreal.MaterialExpressionVectorParameter, "Emissive", -600, 300,
                     unreal.LinearColor(0, 0, 0, 1))

    # World-space noise breaks up flat placeholder surfaces ("nothing looks new").
    noise = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, -600, -400)
    noise.set_editor_property("scale", 0.05)
    noise.set_editor_property("output_min", 0.88)
    noise.set_editor_property("output_max", 1.0)
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, -250)
    mel.connect_material_expressions(base, "", mul, "A")
    mel.connect_material_expressions(noise, "", mul, "B")

    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def make_instance(parent, name, color, roughness, metallic=0.0, emissive=(0, 0, 0)):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = assets.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant,
                                 unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(*color, 1))
    mel.set_material_instance_vector_parameter_value(mi, "Emissive", unreal.LinearColor(*emissive, 1))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    eal.save_loaded_asset(mi)
    return mi


def build_materials():
    master = build_master_material()
    m = lambda *a, **k: make_instance(master, *a, **k)
    return {
        # Dark stained panelling: brown with a little red, never orange.
        "wall": m("MI_Wall_Oxblood", (0.045, 0.018, 0.012), 0.7),
        "floor": m("MI_Floor_DarkWood", (0.05, 0.028, 0.016), 0.5),
        "ceiling": m("MI_Ceiling_Plaster", (0.06, 0.05, 0.04), 0.9),
        "wood": m("MI_Wood_Dark", (0.035, 0.018, 0.01), 0.45),
        "brass": m("MI_Brass", (0.5, 0.36, 0.16), 0.35, 1.0),
        "rug": m("MI_Rug_Oxblood", (0.09, 0.02, 0.018), 0.95),
        "leather": m("MI_Leather_Green", (0.035, 0.045, 0.025), 0.45),
        "paper": m("MI_Paper_Aged", (0.42, 0.37, 0.28), 0.85),
        "photo": m("MI_Photo_Dark", (0.06, 0.06, 0.055), 0.3),
        "frame": m("MI_Frame_Gilt", (0.3, 0.2, 0.08), 0.5, 1.0),
        "canvas": m("MI_Canvas_Murky", (0.05, 0.045, 0.03), 0.8),
        # Lamp shades read as glowing cream, like the sconces on the AW2 board.
        "shade": m("MI_LampShade_Glow", (0.5, 0.42, 0.3), 0.9, emissive=(2.6, 1.9, 1.05)),
        "shade_green": m("MI_LampShade_Banker", (0.02, 0.06, 0.03), 0.3, emissive=(0.06, 0.22, 0.09)),
        "books": [
            m("MI_Book_Oxblood", (0.08, 0.015, 0.012), 0.6),
            m("MI_Book_Green", (0.02, 0.04, 0.025), 0.6),
            m("MI_Book_Brown", (0.06, 0.035, 0.018), 0.6),
            m("MI_Book_Navy", (0.015, 0.02, 0.035), 0.6),
            m("MI_Book_Cream", (0.25, 0.21, 0.15), 0.7),
        ],
        # Exterior: everything outside is near-black and read through fog.
        "ground": m("MI_Ground_Wet", (0.025, 0.03, 0.025), 0.25),
        "path": m("MI_Ground_Path", (0.06, 0.06, 0.05), 0.4),
        "bark": m("MI_Bark_Dark", (0.025, 0.022, 0.018), 0.8),
        "foliage": m("MI_Foliage_Pine", (0.015, 0.025, 0.018), 0.8),
        "moon": m("MI_Moon_Glow", (0.5, 0.55, 0.52), 1.0, emissive=(30.0, 36.0, 34.0)),
    }


# ---------------------------------------------------------------- level helpers

def block(label, center, size_cm, material, folder="Room", shadow=True, mesh=CUBE, rot=(0, 0, 0)):
    """Spawns an engine basic shape (100 cm, centred) scaled to size_cm."""
    a = actors.spawn_actor_from_object(unreal.load_asset(mesh), unreal.Vector(*center), unreal.Rotator(*rot))
    a.set_actor_scale3d(unreal.Vector(*(s / 100.0 for s in size_cm)))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    smc = a.static_mesh_component
    smc.set_material(0, material)
    if not shadow:
        smc.set_editor_property("cast_shadow", False)
    return a


def light(cls, label, loc, rot=(0, 0, 0), folder="Lights"):
    """rot is unreal.Rotator order: (roll, pitch, yaw)."""
    a = actors.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    return a


def practical(label, loc, intensity, radius, kelvin=2700, source=4.0):
    """Small warm point light; tight radius so each lamp makes its own pool."""
    c = light(unreal.PointLight, label, loc).point_light_component
    c.set_editor_property("intensity", intensity)
    c.set_editor_property("attenuation_radius", radius)
    c.set_editor_property("source_radius", source)
    c.set_editor_property("use_temperature", True)
    c.set_editor_property("temperature", kelvin)
    return c


def spot(label, loc, rot, intensity, outer, inner, radius, kelvin=2700):
    c = light(unreal.SpotLight, label, loc, rot).spot_light_component
    c.set_editor_property("intensity", intensity)
    c.set_editor_property("outer_cone_angle", outer)
    c.set_editor_property("inner_cone_angle", inner)
    c.set_editor_property("attenuation_radius", radius)
    c.set_editor_property("use_temperature", True)
    c.set_editor_property("temperature", kelvin)
    return c


def set_pp(settings, **values):
    """Sets PostProcessSettings fields together with their override_ flags."""
    for key, value in values.items():
        try:
            settings.set_editor_property("override_" + key, True)
            settings.set_editor_property(key, value)
        except Exception as e:  # property names drift between engine versions
            unreal.log_warning("[LookDev] post-process '%s' skipped: %s" % (key, e))


# ---------------------------------------------------------------- set dressing

def bookcase(name, x_wall, y0, y1, mats, rng):
    """Floor-to-ceiling case against the east wall (x_wall = interior face), filled with books."""
    depth, top = 35, 300
    xc = x_wall - depth / 2
    folder = "Props/" + name
    wood = mats["wood"]
    block(name + "_Back", (x_wall - 2, (y0 + y1) / 2, top / 2), (4, y1 - y0, top), wood, folder)
    block(name + "_SideA", (xc, y0 + 2, top / 2), (depth, 4, top), wood, folder)
    block(name + "_SideB", (xc, y1 - 2, top / 2), (depth, 4, top), wood, folder)
    shelves = [10, 62, 114, 166, 218, 270]
    for i, z in enumerate(shelves):
        block("%s_Shelf%d" % (name, i), (xc, (y0 + y1) / 2, z), (depth, y1 - y0 - 8, 3), wood, folder)
    for i, z in enumerate(shelves[:-1]):
        y = y0 + 5
        n = 0
        while True:
            w = rng.uniform(2.5, 6.5)
            if y + w > y1 - 5:
                break
            if rng.random() < 0.08:  # occasional gap or leaning stack keeps it lived-in
                y += rng.uniform(6, 14)
                continue
            h = rng.uniform(22, 38)
            d = rng.uniform(18, 26)
            block("%s_Book%d_%d" % (name, i, n), (x_wall - 4 - d / 2, y + w / 2, z + 1.5 + h / 2),
                  (d, w, h), rng.choice(mats["books"]), folder + "/Books")
            y += w + rng.uniform(0, 0.6)
            n += 1


def frame(label, center, w, h, mats, wall_axis):
    """Gilt picture frame with a murky canvas, hung on a wall facing along wall_axis ('x' or 'y')."""
    t = 3
    size = (t, w, h) if wall_axis == "x" else (w, t, h)
    inner = (t + 0.6, w - 10, h - 10) if wall_axis == "x" else (w - 10, t + 0.6, h - 10)
    block(label, center, size, mats["frame"], "Props/Frames")
    block(label + "_Canvas", center, inner, mats["canvas"], "Props/Frames")


def build_exterior(mats, rng):
    """Foggy teal-green night beyond the east window (AW2's forest paths)."""
    f = "Exterior"
    block("Ground", (4400, 0, -12), (8000, 12000, 20), mats["ground"], f)
    block("Ground_Behind", (-2500, 0, -12), (4200, 12000, 20), mats["ground"], f)
    # A path leading away from the window so the eye has a way into the fog.
    for i in range(8):
        block("Path%d" % i, (800 + i * 600, -60 + i * i * 12, -1), (620, 260, 2),
              mats["path"], f, rot=(0, 0, rng.uniform(-6, 6)))

    def pine(label, x, y, h):
        trunk_w = h * 0.035
        block(label + "_Trunk", (x, y, h * 0.35), (trunk_w, trunk_w, h * 0.7), mats["bark"], f + "/Trees",
              mesh=CYLINDER)
        for k, (zf, wf) in enumerate(((0.35, 0.42), (0.58, 0.3), (0.8, 0.18))):
            block("%s_Crown%d" % (label, k), (x, y, h * zf), (h * wf, h * wf, h * 0.42), mats["foliage"],
                  f + "/Trees", mesh=CONE, rot=(0, 0, rng.uniform(0, 360)))

    def bare(label, x, y, h):
        w = rng.uniform(14, 28)
        lean = (rng.uniform(-6, 6), rng.uniform(-6, 6), 0)
        block(label, (x, y, h / 2), (w, w, h), mats["bark"], f + "/Trees", mesh=CYLINDER, rot=lean)

    n = 0
    while n < 70:
        x = rng.uniform(700, 6500)
        y = rng.uniform(-4500, 4500)
        if abs(y + 60) < 250 + x * 0.04:  # keep the path clear
            continue
        h = rng.uniform(900, 1800)
        if rng.random() < 0.7:
            pine("Pine%d" % n, x, y, h)
        else:
            bare("Bare%d" % n, x, y, h * 0.8)
        n += 1
    # A few trees behind the house so the room sits in a forest, not on a plane.
    for i in range(12):
        pine("PineBack%d" % i, rng.uniform(-3500, -600), rng.uniform(-4000, 4000), rng.uniform(900, 1600))

    # Low ground mist hugging the forest floor, thicker than the room haze.
    try:
        mist = light(unreal.LocalFogVolume, "GroundMist", (3500, 0, 0), folder=f)
        mist.set_actor_scale3d(unreal.Vector(40, 60, 6))
        mc = mist.get_component_by_class(unreal.LocalFogVolumeComponent)
        mc.set_editor_property("radial_fog_extinction", 0.0)
        mc.set_editor_property("height_fog_extinction", 1.2)
        mc.set_editor_property("height_fog_falloff", 1200.0)
        mc.set_editor_property("fog_albedo", unreal.LinearColor(0.75, 0.9, 0.85, 1))
        mc.set_editor_property("fog_emissive", unreal.LinearColor(0.0002, 0.0006, 0.0006, 1))
    except Exception as e:
        unreal.log_warning("[LookDev] ground mist skipped: %s" % e)

    # Moon disc, far out against the light direction; never casts shadow.
    fwd = unreal.Rotator(*MOON_ROT).get_forward_vector()
    d = 40000
    block("MoonDisc", (-fwd.x * d, -fwd.y * d, -fwd.z * d), (900, 900, 900), mats["moon"], f,
          shadow=False, mesh=SPHERE)


def build_level(mats):
    # Rebuild in place: the editor opens L_LookDev on startup, so it can't be deleted here.
    if eal.does_asset_exist(MAP_PATH):
        levels.load_level(MAP_PATH)
        actors.destroy_actors([a for a in actors.get_all_level_actors()
                               if not isinstance(a, unreal.WorldSettings) and type(a) is not unreal.Brush])
    elif not levels.new_level(MAP_PATH):
        raise RuntimeError("could not create " + MAP_PATH)

    rng = random.Random(1947)  # fixed seed: same clutter every rebuild
    build_exterior(mats, random.Random(1948))
    wall, wood, brass = mats["wall"], mats["wood"], mats["brass"]

    # Room shell: interior x in [-400, 400], y in [-300, 300], z in [0, 300].
    block("Floor", (0, 0, -10), (840, 640, 20), mats["floor"])
    block("Ceiling", (0, 0, 310), (840, 640, 20), mats["ceiling"])
    block("Wall_South", (0, -310, 150), (840, 20, 320), wall)
    block("Wall_North", (0, 310, 150), (840, 20, 320), wall)
    block("Wall_West", (-410, 0, 150), (20, 600, 320), wall)
    # East wall with a window opening: y in [-90, 90], z in [90, 250].
    block("Wall_East_L", (410, -195, 150), (20, 210, 320), wall)
    block("Wall_East_R", (410, 195, 150), (20, 210, 320), wall)
    block("Wall_East_Sill", (410, 0, 45), (20, 180, 90), wall)
    block("Wall_East_Head", (410, 0, 280), (20, 180, 60), wall)
    # Window mullions and frame so the opening reads as a window, not a panel.
    for i, y in enumerate((-90, 0, 90)):
        block("Window_Mullion%d" % i, (402, y, 170), (6, 5 if i == 1 else 8, 160), wood)
    for i, z in enumerate((90, 170, 250)):
        block("Window_Transom%d" % i, (402, 0, z), (6, 180, 5 if i == 1 else 8), wood)
    for i, z in enumerate((40, 140, 240)):  # dado and picture rails catch grazing light
        block("Rail_South%d" % i, (0, -298, z), (800, 4, 4), wood)

    # East wall: bookcases either side of the window (the Knives Out arch, simplified).
    bookcase("Bookcase_S", 400, -295, -95, mats, rng)
    bookcase("Bookcase_N", 400, 95, 295, mats, rng)

    # South wall: the case board between two sconces (AW2 Mind Place).
    bx, bw = 170, 240
    block("CaseBoard", (bx, -296, 160), (bw, 4, 110), wood, "Props/CaseBoard")
    # Two loose rows of notes and photos joined by string, like the AW2 board.
    n = 0
    for row_z in (180, 140):
        for i in range(10):
            x = bx - bw / 2 + 16 + i * 23 + rng.uniform(-3, 3)
            if rng.random() < 0.2:
                continue
            is_photo = rng.random() < 0.45
            size = (11, 0.6, 13) if is_photo else (14, 0.6, 16)
            block("Note%d" % n, (x, -293.5, row_z + rng.uniform(-4, 4)), size,
                  mats["photo" if is_photo else "paper"], "Props/CaseBoard")
            n += 1
        block("String_%d" % row_z, (bx, -293.2, row_z + 6), (bw - 20, 0.3, 0.3), mats["rug"], "Props/CaseBoard")
    for i, x in enumerate((bx - bw / 2 + 25, bx + bw / 2 - 25)):
        block("Sconce%d_Arm" % i, (x, -290, 228), (4, 20, 4), brass, "Props/CaseBoard")
        block("Sconce%d_Shade" % i, (x, -280, 220), (16, 16, 14), mats["shade"], "Props/CaseBoard", shadow=False)
        spot("Sconce%d_Down" % i, (x, -275, 214), (0, -75, -90), 7.0, 50, 20, 320)
        spot("Sconce%d_Up" % i, (x, -280, 228), (0, 90, 0), 3.0, 50, 20, 200)
        practical("Sconce%d_Glow" % i, (x, -270, 220), 0.6, 250, source=8)

    # Desk with a banker's lamp, west of the board.
    block("Desk", (-110, -250, 76), (150, 75, 5), wood, "Props/Desk")
    block("Desk_PedestalA", (-170, -250, 37), (35, 70, 74), wood, "Props/Desk")
    block("Desk_PedestalB", (-50, -250, 37), (35, 70, 74), wood, "Props/Desk")
    block("Desk_Papers", (-90, -245, 79), (30, 22, 1), mats["paper"], "Props/Desk")
    block("DeskLamp_Base", (-150, -265, 80), (14, 10, 3), brass, "Props/Desk")
    block("DeskLamp_Stem", (-150, -265, 92), (2, 2, 24), brass, "Props/Desk")
    block("DeskLamp_Shade", (-150, -258, 104), (30, 12, 6), mats["shade_green"], "Props/Desk", shadow=False)
    practical("Practical_DeskLamp", (-150, -255, 98), 4.0, 280, 2600)

    # Armchair and side table with a lamp, mid-room.
    ax, ay = 140, 60
    block("Armchair_Seat", (ax, ay, 25), (80, 80, 45), mats["leather"], "Props/Armchair")
    block("Armchair_Back", (ax + 35, ay, 70), (14, 80, 90), mats["leather"], "Props/Armchair")
    block("Armchair_ArmL", (ax, ay - 38, 55), (80, 12, 25), mats["leather"], "Props/Armchair")
    block("Armchair_ArmR", (ax, ay + 38, 55), (80, 12, 25), mats["leather"], "Props/Armchair")
    block("SideTable", (ax + 10, ay - 85, 30), (40, 40, 60), wood, "Props/Armchair")
    block("TableLamp_Stem", (ax + 10, ay - 85, 75), (3, 3, 30), brass, "Props/Armchair")
    block("TableLamp_Shade", (ax + 10, ay - 85, 98), (26, 26, 20), mats["shade"], "Props/Armchair", shadow=False)
    practical("Practical_TableLamp", (ax + 10, ay - 85, 96), 5.0, 320, 2500)
    block("Rug", (40, 0, 1), (360, 240, 1), mats["rug"], "Props")

    # Fireplace on the west wall (behind the hero camera; lights the back of the room).
    block("Fireplace", (-385, 80, 60), (40, 160, 120), brass, "Props")
    practical("Practical_Fireplace", (-350, 80, 40), 10.0, 450, 1900, source=30)

    # Gallery of frames on the north and south walls.
    frame("Frame0", (-300, -297, 190), 70, 90, mats, "y")
    frame("Frame1", (-30, -297, 200), 50, 60, mats, "y")
    for i, (x, w, h) in enumerate(((-250, 90, 70), (-120, 55, 75), (20, 80, 100))):
        frame("FrameN%d" % i, (x, 297, 175 + rng.uniform(-10, 10)), w, h, mats, "y")

    # Ceiling fixture: dim, so the ceiling stays mostly dark as in both references.
    block("Pendant_Shade", (60, 0, 260), (30, 30, 12), mats["shade"], "Props", shadow=False)
    block("Pendant_Rod", (60, 0, 282), (2, 2, 36), brass, "Props")
    practical("Practical_Pendant", (60, 0, 252), 3.0, 380, 2700, source=10)

    # Cold moonlight through the window: the one cool source, for god rays in the haze.
    mc = light(unreal.DirectionalLight, "Moonlight", (0, 0, 600), MOON_ROT).light_component
    mc.set_editor_property("intensity", 0.8)
    mc.set_editor_property("light_color", rgb(95, 175, 160))
    mc.set_editor_property("volumetric_scattering_intensity", 4.0)

    # Moonbeam cheat: a spot just outside the window, aligned with the moon, whose only real job
    # is strong volumetric shafts indoors (the forest stays dark because the moon itself is dim).
    fwd = unreal.Rotator(*MOON_ROT).get_forward_vector()
    # Kept close to the glass so its cone barely crosses the exterior fog (no glowing blob outside).
    beam_at = (410 - fwd.x * 250, 0 - fwd.y * 250, 170 - fwd.z * 250)
    bc = spot("Moonbeam", beam_at, MOON_ROT, 40.0, 26, 18, 1600)
    bc.set_editor_property("use_temperature", False)
    bc.set_editor_property("light_color", rgb(95, 175, 160))
    bc.set_editor_property("volumetric_scattering_intensity", 8.0)

    sc = light(unreal.SkyLight, "SkyLight", (0, 0, 500)).light_component
    sc.set_editor_property("real_time_capture", True)
    sc.set_editor_property("intensity", 0.08)
    sc.set_editor_property("lower_hemisphere_is_black", True)

    # Torch preview: a narrow, neutral-warm beam from the player's eye.
    spot("Torch_Preview", (-300, 180, 150), (0, -12, -25), 12.0, 16, 7, 1500, 4300)

    # Fog: the forest dissolves into teal-green; the same volumetric haze carries the god rays indoors.
    fgc = light(unreal.ExponentialHeightFog, "HeightFog", (0, 0, 0), folder="Atmosphere").component
    fgc.set_editor_property("fog_density", 0.05)
    fgc.set_editor_property("fog_height_falloff", 0.15)
    fgc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.0008, 0.0024, 0.0018, 1))
    fgc.set_editor_property("directional_inscattering_luminance", unreal.LinearColor(0.003, 0.008, 0.0065, 1))
    fgc.set_editor_property("directional_inscattering_exponent", 6.0)
    fgc.set_editor_property("enable_volumetric_fog", True)
    fgc.set_editor_property("volumetric_fog_scattering_distribution", 0.6)
    fgc.set_editor_property("volumetric_fog_extinction_scale", 2.5)
    fgc.set_editor_property("volumetric_fog_albedo", rgb(170, 210, 192))

    # Dust in the room's air, so the moonlight through the window shows as shafts.
    try:
        dust = light(unreal.LocalFogVolume, "RoomDust", (0, 0, 150), folder="Atmosphere")
        dust.set_actor_scale3d(unreal.Vector(4.2, 3.2, 1.6))  # local fog volume is 2 m across at scale 1
        dc = dust.get_component_by_class(unreal.LocalFogVolumeComponent)
        dc.set_editor_property("radial_fog_extinction", 4.0)
        dc.set_editor_property("height_fog_extinction", 0.0)
        dc.set_editor_property("fog_albedo", unreal.LinearColor(0.9, 0.9, 0.9, 1))
    except Exception as e:
        unreal.log_warning("[LookDev] room dust skipped: %s" % e)

    # Post-process: fixed exposure, cream highlights / teal-leaning shadows, grain, vignette.
    ppv = light(unreal.PostProcessVolume, "PostProcess_Global", (0, 0, 0), folder="Atmosphere")
    ppv.set_editor_property("unbound", True)
    s = ppv.get_editor_property("settings")
    set_pp(s,
           auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
           auto_exposure_bias=0.5,
           auto_exposure_apply_physical_camera_exposure=False,
           color_saturation=unreal.Vector4(0.8, 0.8, 0.8, 1),
           color_gain_shadows=unreal.Vector4(0.92, 1.0, 0.98, 1),
           color_saturation_shadows=unreal.Vector4(0.8, 0.8, 0.8, 1),
           color_gain_highlights=unreal.Vector4(1.03, 1.0, 0.94, 1),
           color_contrast=unreal.Vector4(1.08, 1.08, 1.08, 1),
           bloom_intensity=0.6,
           vignette_intensity=0.6,
           film_grain_intensity=0.25,
           scene_fringe_intensity=0.3,
           local_exposure_highlight_contrast_scale=0.8,
           local_exposure_shadow_contrast_scale=0.8)
    ppv.set_editor_property("settings", s)

    cam = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(-330, 220, 165),
                                        unreal.Rotator(0, -8, -30))
    cam.set_actor_label("Cam_Hero")
    cam.set_folder_path("Cameras")
    board_cam = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(170, -40, 160),
                                              unreal.Rotator(0, 0, -90))
    board_cam.set_actor_label("Cam_Board")
    board_cam.set_folder_path("Cameras")
    window_cam = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(250, 40, 140),
                                               unreal.Rotator(0, 8, -5))
    window_cam.set_actor_label("Cam_Window")
    window_cam.set_folder_path("Cameras")

    ps = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-330, 220, 100),
                                       unreal.Rotator(0, 0, -30))
    ps.set_folder_path("Gameplay")

    levels.save_current_level()


log("building materials")
mats = build_materials()
log("building level")
build_level(mats)
log("done")
unreal.SystemLibrary.quit_editor()
