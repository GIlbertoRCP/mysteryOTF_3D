"""Builds /Game/Maps/L_LookDev: a placeholder study lit for noir look development.

Run headless from the repo root:
  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript=Scripts/build_lookdev.py -unattended -nosplash -nullrhi

Re-running rebuilds the level and the M_Master instances in place, so tune values here
(or tweak in the editor and copy them back) rather than hand-editing twice.

Targets (Reference/, not in git): the Knives Out study (many small warm practicals,
each with its own pool; dense, muted red/brown/green clutter) and the Alan Wake 2
Mind Place board (paper on dark wood between two glowing sconces, the rest near-black).
"""
import math
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
    """M_Master: PBR pipeline with BaseColor, Normal, ORM, and procedural UV tiling."""
    path = MAT_DIR + "/M_Master"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = assets.create_asset("M_Master", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    # Also drawn as instances (the train's falling snow).
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    t_white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    t_normal = unreal.load_asset("/Engine/EngineMaterials/DefaultNormal")
    t_flat_normal = unreal.load_asset("/Engine/EngineMaterials/FlatNormal")
    t_orm_default = unreal.load_asset("/Game/Art/Textures/T_Default_ORM")
    if not t_orm_default:
        png_path = "B:/mysteryOTF/Content/Art/Textures/T_Default_ORM.png"
        if os.path.exists(png_path):
            task = unreal.AssetImportTask()
            task.filename = png_path.replace("\\", "/")
            task.destination_path = "/Game/Art/Textures"
            task.destination_name = "T_Default_ORM"
            task.replace_existing = True
            task.automated = True
            task.save = True
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            t_orm_default = unreal.load_asset("/Game/Art/Textures/T_Default_ORM")
            if t_orm_default:
                t_orm_default.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                t_orm_default.set_editor_property("srgb", False)
                eal.save_loaded_asset(t_orm_default)

    def param(cls, name, x, y, default):
        node = mel.create_material_expression(mat, cls, x, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", default)
        return node

    def tex_param(name, x, y, default_tex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        node.set_editor_property("parameter_name", name)
        if default_tex:
            node.set_editor_property("texture", default_tex)
        node.set_editor_property("sampler_type", sampler_type)
        return node

    # UV Tiling
    tex_coord = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1200, -200)
    uv_tiling = param(unreal.MaterialExpressionScalarParameter, "UV_Tiling", -1200, -100, 1.0)
    uv_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1000, -150)
    mel.connect_material_expressions(tex_coord, "", uv_mul, "A")
    mel.connect_material_expressions(uv_tiling, "", uv_mul, "B")

    detail_uv_tiling = param(unreal.MaterialExpressionScalarParameter, "Detail_UV_Tiling", -1200, 400, 5.0)
    detail_uv_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1000, 350)
    mel.connect_material_expressions(tex_coord, "", detail_uv_mul, "A")
    mel.connect_material_expressions(detail_uv_tiling, "", detail_uv_mul, "B")

    # Base Color: Tinted Base * T2D_BaseColor * Noise
    t2d_base = tex_param("T2D_BaseColor", -800, -350, t_white, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    mel.connect_material_expressions(uv_mul, "", t2d_base, "Coordinates")
    base_tint = param(unreal.MaterialExpressionVectorParameter, "BaseColor", -800, -150, unreal.LinearColor(1.0, 1.0, 1.0, 1))
    tint_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, -250)
    mel.connect_material_expressions(t2d_base, "", tint_mul, "A")
    mel.connect_material_expressions(base_tint, "", tint_mul, "B")

    noise = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, -600, -450)
    noise.set_editor_property("scale", 0.05)
    noise.set_editor_property("output_min", 0.88)
    noise.set_editor_property("output_max", 1.0)

    final_base = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, -300)
    mel.connect_material_expressions(tint_mul, "", final_base, "A")
    mel.connect_material_expressions(noise, "", final_base, "B")
    mel.connect_material_property(final_base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # ORM
    t2d_orm = tex_param("T2D_ORM", -800, 50, t_orm_default, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    mel.connect_material_expressions(uv_mul, "", t2d_orm, "Coordinates")

    # AO (R)
    ao_scale = param(unreal.MaterialExpressionScalarParameter, "AmbientOcclusion", -600, 0, 1.0)
    ao_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 20)
    mel.connect_material_expressions(t2d_orm, "R", ao_mul, "A")
    mel.connect_material_expressions(ao_scale, "", ao_mul, "B")
    mel.connect_material_property(ao_mul, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    # Roughness (G) + MicroRoughness
    rough_scale = param(unreal.MaterialExpressionScalarParameter, "Roughness", -600, 120, 0.7)
    rough_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 140)
    mel.connect_material_expressions(t2d_orm, "G", rough_mul, "A")
    mel.connect_material_expressions(rough_scale, "", rough_mul, "B")

    micro_rough = param(unreal.MaterialExpressionScalarParameter, "MicroRoughness", -400, 240, 0.0)
    rough_add = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -250, 160)
    mel.connect_material_expressions(rough_mul, "", rough_add, "A")
    mel.connect_material_expressions(micro_rough, "", rough_add, "B")

    rough_clamp = mel.create_material_expression(mat, unreal.MaterialExpressionClamp, -100, 160)
    rough_clamp.set_editor_property("min_default", 0.0)
    rough_clamp.set_editor_property("max_default", 1.0)
    mel.connect_material_expressions(rough_add, "", rough_clamp, "")
    mel.connect_material_property(rough_clamp, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Metallic (B)
    metal_scale = param(unreal.MaterialExpressionScalarParameter, "Metallic", -600, 260, 0.0)
    metal_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 280)
    mel.connect_material_expressions(t2d_orm, "B", metal_mul, "A")
    mel.connect_material_expressions(metal_scale, "", metal_mul, "B")
    mel.connect_material_property(metal_mul, "", unreal.MaterialProperty.MP_METALLIC)

    # Specular & Emissive
    spec = param(unreal.MaterialExpressionScalarParameter, "Specular", -400, 380, 0.5)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    emissive = param(unreal.MaterialExpressionVectorParameter, "Emissive", -400, 480, unreal.LinearColor(0, 0, 0, 1))
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Normal & Detail Normal
    t2d_norm = tex_param("T2D_Normal", -800, 600, t_normal, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(uv_mul, "", t2d_norm, "Coordinates")
    t2d_detail = tex_param("T2D_DetailNormal", -800, 800, t_flat_normal, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(detail_uv_mul, "", t2d_detail, "Coordinates")

    # NormalStrength blends from flat (0) to the map as painted (1).
    flat = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -600, 540)
    flat.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 1))
    norm_strength = param(unreal.MaterialExpressionScalarParameter, "NormalStrength", -600, 660, 1.0)
    norm_scaled = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -400, 620)
    mel.connect_material_expressions(flat, "", norm_scaled, "A")
    mel.connect_material_expressions(t2d_norm, "", norm_scaled, "B")
    mel.connect_material_expressions(norm_strength, "", norm_scaled, "Alpha")

    norm_add = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -300, 700)
    mel.connect_material_expressions(norm_scaled, "", norm_add, "A")
    mel.connect_material_expressions(t2d_detail, "", norm_add, "B")
    mel.connect_material_property(norm_add, "", unreal.MaterialProperty.MP_NORMAL)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat



def make_instance(parent, name, color, roughness, metallic=0.0, emissive=(0, 0, 0),
                  tex_bc=None, tex_norm=None, tex_orm=None, uv_tiling=1.0, normal_strength=1.0):
    # With an ORM texture the texture's G channel is the roughness; the scalar only scales it,
    # so leave it at 1 (a leftover 0.6 would turn 0.85 wool into a 0.5 semi-gloss that mirrors lamps).
    if tex_orm:
        roughness = 1.0
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
    if tex_bc:
        t = unreal.load_asset(f"/Game/Art/Textures/{tex_bc}")
        if t: mel.set_material_instance_texture_parameter_value(mi, "T2D_BaseColor", t)
    if tex_norm:
        t = unreal.load_asset(f"/Game/Art/Textures/{tex_norm}")
        if t: mel.set_material_instance_texture_parameter_value(mi, "T2D_Normal", t)
    if tex_orm:
        t = unreal.load_asset(f"/Game/Art/Textures/{tex_orm}")
        if t: mel.set_material_instance_texture_parameter_value(mi, "T2D_ORM", t)
    if uv_tiling != 1.0:
        mel.set_material_instance_scalar_parameter_value(mi, "UV_Tiling", uv_tiling)
    mel.set_material_instance_scalar_parameter_value(mi, "NormalStrength", normal_strength)
    eal.save_loaded_asset(mi)
    return mi


def build_materials():
    master = build_master_material()
    m = lambda *a, **k: make_instance(master, *a, **k)
    return {
        # Dark stained panelling: brown with a little red, never orange.
        "wall": m("MI_Wall_Oxblood", (1.0, 1.0, 1.0), 0.7, tex_bc="T_Wall_BaseColor", tex_norm="T_Wall_Normal", tex_orm="T_Wall_ORM", uv_tiling=2.5),
        "floor": m("MI_Floor_DarkWood", (1.0, 1.0, 1.0), 0.5, tex_bc="T_WoodFloor_BaseColor", tex_norm="T_WoodFloor_Normal", tex_orm="T_WoodFloor_ORM", uv_tiling=3.0),
        "ceiling": m("MI_Ceiling_Plaster", (1.0, 1.0, 1.0), 0.9, tex_bc="T_Ceiling_BaseColor", tex_norm="T_Ceiling_Normal", tex_orm="T_Ceiling_ORM", uv_tiling=4.0),
        "wood": m("MI_Wood_Dark", (1.0, 1.0, 1.0), 0.45, tex_bc="T_WoodFloor_BaseColor", tex_norm="T_WoodFloor_Normal", tex_orm="T_WoodFloor_ORM", uv_tiling=2.0),
        "brass": m("MI_Brass", (0.8, 0.65, 0.35), 0.35, 1.0, tex_bc="T_Brass_BaseColor", tex_norm="T_Brass_Normal", tex_orm="T_Brass_ORM", uv_tiling=1.5),
        "rug": m("MI_Rug_Oxblood", (1.0, 1.0, 1.0), 0.95, tex_bc="T_Rug_BaseColor", tex_norm="T_Rug_Normal", tex_orm="T_Rug_ORM", uv_tiling=1.0),
        "corkboard": m("MI_Corkboard", (1.0, 1.0, 1.0), 0.88, tex_bc="T_Corkboard_BaseColor", tex_norm="T_Corkboard_Normal", tex_orm="T_Corkboard_ORM", uv_tiling=7.0, normal_strength=0.45),
        "watch_dial": m("MI_WatchDial", (1.0, 1.0, 1.0), 0.3, tex_bc="T_WatchDial_BaseColor", tex_norm="T_WatchDial_Normal", tex_orm="T_WatchDial_ORM", uv_tiling=1.0),
        "letter": m("MI_Letter", (1.0, 1.0, 1.0), 0.8, tex_bc="T_Letter_BaseColor", tex_norm="T_Letter_Normal", tex_orm="T_Letter_ORM", uv_tiling=1.0),
        "yarn": m("MI_Yarn", (1.0, 1.0, 1.0), 0.9, tex_bc="T_Yarn_BaseColor", tex_norm="T_Yarn_Normal", tex_orm="T_Yarn_ORM", uv_tiling=4.0),
        # The one saturated accent allowed by the palette.
        "wax": m("MI_Wax_Red", (0.25, 0.012, 0.01), 0.35),
        "ink": m("MI_Ink", (0.012, 0.012, 0.015), 0.6),
        "steel": m("MI_Steel_Dark", (0.15, 0.15, 0.16), 0.3, 1.0),
        # Suspects: one signature colour each.
        "skin": m("MI_Skin_Placeholder", (0.75, 0.55, 0.45), 0.5, tex_bc="T_Skin_BaseColor", tex_norm="T_Skin_Normal", tex_orm="T_Skin_ORM", uv_tiling=1.0, normal_strength=0.3),
        "margaret": m("MI_Suspect_Margaret", (0.03, 0.18, 0.10), 0.5, tex_bc="T_Fabric_Wool_BaseColor", tex_norm="T_Fabric_Wool_Normal", tex_orm="T_Fabric_Wool_ORM", uv_tiling=2.5, normal_strength=0.35),
        "edmund": m("MI_Suspect_Edmund", (0.35, 0.22, 0.08), 0.6, tex_bc="T_Fabric_Wool_BaseColor", tex_norm="T_Fabric_Wool_Normal", tex_orm="T_Fabric_Wool_ORM", uv_tiling=2.5, normal_strength=0.35),
        "leather": m("MI_Leather_Green", (0.05, 0.15, 0.08), 0.45, tex_bc="T_Leather_BaseColor", tex_norm="T_Leather_Normal", tex_orm="T_Leather_ORM", uv_tiling=2.0, normal_strength=0.6),
        "paper": m("MI_Paper_Aged", (0.42, 0.37, 0.28), 0.85),
        "photo": m("MI_Photo_Dark", (0.06, 0.06, 0.055), 0.3),
        "frame": m("MI_Frame_Gilt", (0.7, 0.55, 0.25), 0.5, 1.0, tex_bc="T_Brass_BaseColor", tex_norm="T_Brass_Normal", tex_orm="T_Brass_ORM", uv_tiling=2.0),
        "canvas": m("MI_Canvas_Murky", (0.15, 0.13, 0.10), 0.8, tex_bc="T_Canvas_BaseColor", tex_norm="T_Canvas_Normal", tex_orm="T_Canvas_ORM", uv_tiling=1.0, normal_strength=0.6),
        # Lamp shades read as glowing cream, like the sconces on the AW2 board.
        "shade": m("MI_LampShade_Glow", (0.5, 0.42, 0.3), 0.9, emissive=(2.6, 1.9, 1.05)),
        "shade_green": m("MI_LampShade_Banker", (0.02, 0.06, 0.03), 0.3, emissive=(0.06, 0.22, 0.09)),
        "books": [
            m("MI_Book_Oxblood", (0.25, 0.04, 0.03), 0.6, tex_bc="T_Leather_BaseColor", tex_norm="T_Leather_Normal", tex_orm="T_Leather_ORM", uv_tiling=3.0, normal_strength=0.6),
            m("MI_Book_Green", (0.05, 0.16, 0.08), 0.6, tex_bc="T_Leather_BaseColor", tex_norm="T_Leather_Normal", tex_orm="T_Leather_ORM", uv_tiling=3.0, normal_strength=0.6),
            m("MI_Book_Brown", (0.20, 0.11, 0.05), 0.6, tex_bc="T_Leather_BaseColor", tex_norm="T_Leather_Normal", tex_orm="T_Leather_ORM", uv_tiling=3.0, normal_strength=0.6),
            m("MI_Book_Navy", (0.04, 0.07, 0.18), 0.6, tex_bc="T_Leather_BaseColor", tex_norm="T_Leather_Normal", tex_orm="T_Leather_ORM", uv_tiling=3.0, normal_strength=0.6),
            m("MI_Book_Cream", (0.80, 0.72, 0.55), 0.7, tex_bc="T_Leather_BaseColor", tex_norm="T_Leather_Normal", tex_orm="T_Leather_ORM", uv_tiling=3.0, normal_strength=0.6),
        ],
        # Exterior: everything outside is near-black and read through fog.
        "ground": m("MI_Ground_Wet", (1.0, 1.0, 1.0), 0.25, tex_bc="T_Ground_BaseColor", tex_norm="T_Ground_Normal", tex_orm="T_Ground_ORM", uv_tiling=3.0),
        "path": m("MI_Ground_Path", (1.0, 1.0, 1.0), 0.4, tex_bc="T_Ground_BaseColor", tex_norm="T_Ground_Normal", tex_orm="T_Ground_ORM", uv_tiling=2.5),
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
    smc.set_editor_property("override_materials", [material])
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


# ---------------------------------------------------------------- evidence models
#
# Placeholder clue models as lists of engine shapes (ANoirEvidence builds the components).
# Each is modelled around the origin with its "front" facing +X: inspection starts with +X
# toward the viewer. Real meshes replace these later via the actor's Mesh.

def _part(mesh, mat, center, size_cm, rot=(0, 0, 0)):
    p = unreal.NoirEvidencePart()
    p.set_editor_property("mesh", unreal.load_asset(mesh))
    p.set_editor_property("material", mat)
    p.set_editor_property("transform", unreal.Transform(
        unreal.Vector(*center), unreal.Rotator(*rot), unreal.Vector(*(c / 100.0 for c in size_cm))))
    return p


def _box(mat, center, size_cm, rot=(0, 0, 0)):
    return _part(CUBE, mat, center, size_cm, rot)


def _disc(mat, x, diameter, thickness, y=0.0, z=0.0):
    """Cylinder lying along X (a face pointing at the viewer)."""
    return _part(CYLINDER, mat, (x, y, z), (diameter, diameter, thickness), (0, 90, 0))


def _hand(mat, clock_deg, length, width, x):
    """Clock hand on a +X-facing dial; 0 deg is twelve o'clock, clockwise as seen from +X."""
    t = math.radians(clock_deg)
    y, z = -math.sin(t), math.cos(t)  # a viewer at +X looking toward -X has -Y on its right
    return _box(mat, (x, y * length / 2, z * length / 2), (0.1, width, length), (-clock_deg, 0, 0))


def evidence_models(mats):
    brass, ink = mats["brass"], mats["ink"]
    watch_face = mats.get("watch_dial", mats["paper"])
    letter_mat = mats.get("letter", mats["paper"])
    # Pocket watch, stopped at 10:42; engraving on the case back.
    watch = [
        _disc(brass, 0, 5.0, 1.2),
        _disc(watch_face, 0.62, 4.4, 0.2),
        _hand(ink, (10 + 42 / 60) * 30, 1.2, 0.2, 0.75),
        _hand(ink, 42 * 6, 1.8, 0.12, 0.78),
        _disc(brass, 0.8, 0.25, 0.25),
        _part(CYLINDER, brass, (0, 0, 2.8), (0.7, 0.7, 0.7)),
        _disc(brass, 0, 1.6, 0.25, z=3.5),
        _disc(mats["frame"], -0.62, 3.2, 0.1),
    ]
    # Sealed letter: address on the front, unbroken wax seal on the back.
    letter = [_box(letter_mat, (0, 0, 0), (0.5, 16, 10))]
    for w, z in ((7, 1.2), (8, 0), (5, -1.2)):
        letter.append(_box(ink, (0.28, 1.5, z), (0.1, w, 0.35)))
    letter.append(_box(mats["books"][0], (0.28, -6, 3), (0.1, 2, 2.4)))
    letter.append(_disc(mats["wax"], -0.45, 2.6, 0.5))
    # Cufflink: "H.T." monogram on the face, jeweller's stamp on the toggle bar.
    link = [_disc(brass, 0, 1.8, 0.3)]
    for y, z, w, h in ((-0.45, 0, 0.08, 0.7), (-0.15, 0, 0.08, 0.7), (-0.3, 0, 0.3, 0.07),  # H
                       (0.3, 0.3, 0.45, 0.08), (0.3, 0, 0.08, 0.7)):                       # T
        link.append(_box(ink, (0.17, y, z), (0.06, w, h)))
    link.append(_part(CYLINDER, brass, (-0.6, 0, 0), (0.25, 0.25, 0.9), (0, 90, 0)))
    link.append(_box(mats["steel"], (-1.1, 0, 0), (0.25, 1.4, 0.3)))
    return {"watch": watch, "letter": letter, "cufflink": link}


def detail(direction, text, threshold=0.85):
    d = unreal.NoirInspectDetail()
    d.set_editor_property("local_direction", unreal.Vector(*direction))
    d.set_editor_property("text", unreal.Text(text))
    d.set_editor_property("facing_threshold", threshold)
    return d


def evidence(label, parts, loc, rot, name, description, details):
    a = actors.spawn_actor_from_class(unreal.NoirEvidence, unreal.Vector(*loc), unreal.Rotator(*rot))
    a.set_actor_label(label)
    a.set_folder_path("Gameplay/Evidence")
    a.set_editor_property("display_name", unreal.Text(name))
    a.set_editor_property("description", unreal.Text(description))
    a.set_editor_property("details", details)
    a.set_editor_property("hidden_until_found", True)
    a.set_editor_property("parts", parts)
    return a


def topic(question, answer, requires_cracked=False):
    t = unreal.NoirTopic()
    t.set_editor_property("question", unreal.Text(question))
    t.set_editor_property("answer", unreal.Text(answer))
    t.set_editor_property("requires_cracked", requires_cracked)
    return t


def reaction(evidence_actor, response, stress):
    r = unreal.NoirEvidenceReaction()
    r.set_editor_property("evidence", evidence_actor)
    r.set_editor_property("response", unreal.Text(response))
    r.set_editor_property("stress", stress)
    return r


def suspect(label, loc, yaw, mats, colour, name, occupation, greeting, topics, reactions, cracked, clue):
    a = actors.spawn_actor_from_class(unreal.NoirSuspect, unreal.Vector(*loc), unreal.Rotator(0, 0, yaw))
    a.set_actor_label(label)
    a.set_folder_path("Gameplay/Suspects")
    body = a.get_editor_property("body")
    body.set_material(0, mats[colour])
    body.set_editor_property("override_materials", [mats[colour]])
    head = a.get_editor_property("head")
    head.set_material(0, mats["skin"])
    head.set_editor_property("override_materials", [mats["skin"]])
    for key, value in (("display_name", name), ("occupation", occupation), ("greeting", greeting),
                       ("cracked_line", cracked), ("cracked_clue", clue)):
        a.set_editor_property(key, unreal.Text(value))
    a.set_editor_property("topics", topics)
    a.set_editor_property("reactions", reactions)
    return a


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
    # The cards on it are pinned in play by NoirCase, from what the detective knows.
    block("CaseBoard", (bx, -296, 160), (bw, 4, 110), mats.get("corkboard", wood), "Props/CaseBoard")
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
    # Lights the ground and floor; it scatters little in the fog so the forest stays dark.
    # The Moonbeam below supplies the visible shafts.
    mc.set_editor_property("intensity", 4.0)
    mc.set_editor_property("light_color", rgb(95, 175, 160))
    mc.set_editor_property("volumetric_scattering_intensity", 0.6)  # forest fog stays dark
    mc.set_editor_property("cast_volumetric_shadow", True)

    # Moonbeam: a narrow spot far out along the moon direction, aimed through the window. Being far
    # away its beam is even along its length (no hot spot outside the glass), and its volumetric
    # shadow lets the mullions cut the haze into shafts. The directional moon alone is too dim
    # indoors for shafts without making the forest glow.
    fwd = unreal.Rotator(*MOON_ROT).get_forward_vector()
    beam_at = (410 - fwd.x * 3000, 0 - fwd.y * 3000, 170 - fwd.z * 3000)
    bc = spot("Moonbeam", beam_at, MOON_ROT, 6000.0, 2.6, 2.0, 4000)
    bc.set_editor_property("use_temperature", False)
    bc.set_editor_property("light_color", rgb(95, 175, 160))
    bc.set_editor_property("volumetric_scattering_intensity", 25.0)
    bc.set_editor_property("cast_volumetric_shadow", True)
    bc.set_editor_property("source_radius", 0.0)

    sc = light(unreal.SkyLight, "SkyLight", (0, 0, 500)).light_component
    sc.set_editor_property("real_time_capture", False)  # no sky to capture; real-time would warn and go black
    sc.set_editor_property("intensity", 0.08)
    sc.set_editor_property("lower_hemisphere_is_black", True)

    # Torch preview: a narrow, neutral-warm beam from the player's eye.
    torch = spot("Torch_Preview", (-300, 180, 150), (0, -12, -25), 12.0, 16, 7, 1500, 4300)
    torch.get_owner().set_editor_property("is_editor_only_actor", True)  # the player carries the real one

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

    # Dust in the room's air, much denser than the forest fog, so the moonlight shows as shafts
    # indoors without brightening the exterior. Needs r.LocalFogVolume.RenderIntoVolumetricFog
    # (DefaultEngine.ini) so the dust is shadowed by the window frame.
    try:
        dust = light(unreal.LocalFogVolume, "RoomDust", (0, 0, 150), folder="Atmosphere")
        dust.set_actor_scale3d(unreal.Vector(4.2, 3.2, 1.6))  # local fog volume is 2 m across at scale 1
        dc = dust.get_component_by_class(unreal.LocalFogVolumeComponent)
        dc.set_editor_property("radial_fog_extinction", 12.0)
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
    side_cam = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(80, -260, 150),
                                             unreal.Rotator(0, 0, 75))
    side_cam.set_actor_label("Cam_Shafts")  # looks across the moonbeam to judge the shafts
    side_cam.set_folder_path("Cameras")

    ps = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-330, 220, 100),
                                       unreal.Rotator(0, 0, -30))
    ps.set_folder_path("Gameplay")

    # Evidence, hidden until its furniture is searched. Placed where it rests once found,
    # lying face up (front +X pointing at the ceiling).
    ev = evidence_models(mats)
    face_up = (0, 90, 0)
    watch = evidence("Evidence_Watch", ev["watch"], (-60, -240, 79.2), face_up, "the pocket watch",
                     "A gold pocket watch from the desk drawer. The glass is cracked.",
                     [detail((1, 0, 0), "The hands have stopped at 10:42."),
                      detail((-1, 0, 0), "Engraved inside the case back: 'To H., for every hour. M.'")])
    letter = evidence("Evidence_Letter", ev["letter"], (150, -25, 60.3), (0, 90, 20), "the sealed letter",
                      "A letter hidden inside a book of poems.",
                      [detail((1, 0, 0), "Addressed in a careful hand to Mr. H. Thorne. No postmark."),
                       detail((-1, 0, 0), "The wax seal is unbroken, pressed with the Ashworth crest.")])
    cufflink = evidence("Evidence_Cufflink", ev["cufflink"], (140, 60, 47.8), face_up, "the cufflink",
                        "A brass cufflink, wedged down the side of the cushion.",
                        [detail((1, 0, 0), "A worn monogram: H.T."),
                         detail((-1, 0, 0), "Stamped on the bar: 'Ashworth & Sons, Jewellers'.")])

    # Searchable furniture (milestone 2 will take these from the generated case).
    for label, center, extent, name, clue, found in (
        ("Search_Desk", (-110, -250, 60), (82, 44, 26), "the desk",
         "The desk diary, open at Tuesday: 'Cook's night off.'", watch),
        ("Search_Fireplace", (-385, 80, 60), (30, 90, 70), "the fireplace",
         "The poker is missing from its stand, and the hook has been wiped clean.", None),
        ("Search_Armchair", (140, 60, 45), (46, 46, 46), "the armchair", "", cufflink),
        ("Search_BookcaseN", (380, 195, 150), (24, 100, 150), "the bookcase", "", letter),
        ("Search_BookcaseS", (380, -195, 150), (24, 100, 150), "the bookcase",
         "A household ledger. The pantry accounts for the last three months have been torn out.", None),
    ):
        a = actors.spawn_actor_from_class(unreal.NoirSearchable, unreal.Vector(*center))
        a.set_actor_label(label)
        a.set_folder_path("Gameplay/Searchables")
        a.set_editor_property("display_name", unreal.Text(name))
        a.set_editor_property("clue_text", unreal.Text(clue))
        a.get_editor_property("bounds").set_box_extent(unreal.Vector(*extent))
        if found:
            a.set_editor_property("items", [found])

    # Suspects, standing where their alibis put them. Placeholder case: Harold Thorne is dead
    # in his study; the watch, letter and cufflink point at these two.
    margaret = suspect("Suspect_Margaret", (300, 120, 88), 200, mats, "margaret",
            "Margaret Ashworth", "the late partner's widow",
            "Detective. I suppose you'll want to know where I was. Everyone does.",
            [topic("Where were you at a quarter to eleven?",
                   "In the library, reading. I didn't hear a thing."),
             topic("How well did you know Mr. Thorne?",
                   "He was my husband's business partner. We were cordial. Nothing more."),
             topic("Who was Harold arguing with?",
                   "A man. Low and careful. The sort of voice that says 'sir' even when it's angry.",
                   requires_cracked=True)],
            [reaction(watch, "Where did you find that? It's only a watch. Plenty of people engrave watches.", 0.6),
             reaction(letter, "Anyone in this house could get at the family seal.", 0.5),
             reaction(cufflink, "Harold's, I expect. Ask Graves. He fusses over them.", 0.0)],
            "Fine. I gave Harold that watch. We were going to leave together on the night train. "
            "At 10:42 I heard him arguing in the study, and then nothing.",
            "Margaret Ashworth admits an affair with Thorne; they meant to leave on the night train. "
            "She heard an argument in the study at 10:42.")
    edmund = suspect("Suspect_Edmund", (-300, -40, 88), 10, mats, "edmund",
            "Edmund Graves", "the butler",
            "Sir. A terrible business. Anything I can do.",
            [topic("Where were you at a quarter to eleven?",
                   "In the pantry, sir, polishing the silver. Cook will tell you."),
             topic("Did Mr. Thorne have enemies?",
                   "Mr. Thorne kept his own counsel, sir. Though he'd been settling his accounts all week."),
             topic("Why did you hide the cufflink?",
                   "Because the family would be ruined, sir, if it came out what he'd been doing with Mrs. Ashworth.",
                   requires_cracked=True)],
            [reaction(cufflink, "Mr. Thorne's. I polish them every Sunday. I couldn't say how it got into the chair.", 0.7),
             reaction(watch, "A fine piece, sir. Not one I've seen him wear.", 0.4),
             reaction(letter, "The Ashworth seal. From the mistress's writing desk, I'd say.", 0.2)],
            "All right. I heard the shouting and went in after. Mr. Thorne was on the floor and the cufflink "
            "was on the rug. I put it out of sight. I didn't kill him, sir. I swear it.",
            "Graves found Thorne's body after the argument and hid the cufflink. Swears he didn't kill him.")

    # The case, on the case board: the solution, the accusation's options, and the true story.
    # Faces into the room (+Y), its front on the board's face.
    case = actors.spawn_actor_from_class(unreal.NoirCase, unreal.Vector(170, -300, 160), unreal.Rotator(0, 0, 90))
    case.set_actor_label("Case_ThorneMurder")
    case.set_editor_property("board_size", unreal.Vector2D(230, 100))
    case.set_folder_path("Gameplay")
    case.get_editor_property("bounds").set_box_extent(unreal.Vector(6, 120, 55))
    T = unreal.Text
    case.set_editor_property("title", T("The death of Harold Thorne"))
    case.set_editor_property("victim_name", T("Harold Thorne"))
    case.set_editor_property("suspects", [margaret, edmund])
    case.set_editor_property("motives", [
        T("Jealousy over his affair with Margaret"),
        T("He had found money missing from the household accounts"),
        T("An inheritance from the Ashworth estate"),
        T("To protect the family's good name")])
    case.set_editor_property("weapons", [
        T("The fireplace poker"), T("A letter opener"), T("Poison in his brandy"), T("The brass desk lamp")])
    case.set_editor_property("alibis", [
        T("Margaret Ashworth: reading alone in the library"),
        T("Edmund Graves: polishing silver in the pantry, with Cook")])
    for key, value in (("killer", 1), ("motive", 1), ("weapon", 0), ("broken_alibi", 1)):
        case.set_editor_property(key, value)
    case.set_editor_property("reveal_lines", [
        T("Graves. You were never in the pantry. Tuesday was Cook's night off."),
        T("Thorne had found the gaps in the household accounts. He meant to settle them before he left on the night train."),
        T("At 10:42 he confronted you here, in the study. You took the poker from the hearth."),
        T("Then you hid his cufflink, wiped the stand clean, and went back to polishing silver no one would ever check.")])
    entries = []
    for time, event in (
            ("9:30 pm", "Cook leaves for her night off. The pantry stands empty."),
            ("10:15 pm", "Thorne tells Graves he knows about the missing money and will settle the accounts before he leaves."),
            ("10:30 pm", "Margaret Ashworth waits in the library. She and Thorne mean to take the night train together."),
            ("10:42 pm", "Thorne and Graves argue in the study. Graves strikes him with the fireplace poker. "
                         "Thorne's watch, knocked from the desk, stops; Graves sweeps it into the drawer."),
            ("10:50 pm", "Graves pushes the torn cufflink down the side of the armchair, wipes the poker stand, "
                         "and carries the poker out into the fog."),
            ("11:05 pm", "Graves 'finds' the body and raises the alarm.")):
        e = unreal.NoirTimelineEntry()
        e.set_editor_property("time", T(time))
        e.set_editor_property("event", T(event))
        entries.append(e)
    case.set_editor_property("timeline", entries)

    levels.save_current_level()


if __name__ == "__main__":  # importable by Scripts/build_manor.py for its helpers
    log("building materials")
    mats = build_materials()
    log("building level")
    build_level(mats)
    log("done")
    unreal.SystemLibrary.quit_editor()
