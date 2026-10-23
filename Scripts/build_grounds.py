"""Builds the manor grounds around L_Manor's house: terrain, ground materials, the house's outside,
the garden, drive, gate, lake and pier, the scattered woods, moonlight and the night sky.

  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript="Scripts/build_grounds.py [materials]" -unattended

Run after build_manor.py (it adds to that level, replacing only what it built last time: everything
in the "Grounds" folders) and after import_polyhaven.py and gen_grounds.py. Not -nullrhi: the
materials must compile for real. Layout: grounds_layout.py.
"""
import math
import os
import random
import re
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_lookdev as L  # noqa: E402
import grounds_layout as G  # noqa: E402

MAP_PATH = "/Game/Maps/L_Manor"
ENV = "/Game/Environment"
GEN = ENV + "/Grounds"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WALL_H = 320.0
# Night light levels, against the manor's manual exposure (also set by Scripts/tune_night.py).
MOON_LUX = 1.6
SKY_LIGHT = 0.6
SKY_GLOW = 0.06
DOOR_W, DOOR_H = 160.0, 250.0

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
actors = L.actors


def log(msg):
    unreal.log("[Grounds] " + msg)


WEATHER_MPC = "/Game/Environment/Weather/MPC_Weather"


def wet(m, base, rough, x, y, base_out="", rough_out=""):
    """Darker and glossier with the weather's Wetness (Scripts/build_weather.py makes the
    collection); returns the (base colour, roughness) to plug in. Unchanged if there is none."""
    mpc = unreal.load_asset(WEATHER_MPC) if unreal.EditorAssetLibrary.does_asset_exist(WEATHER_MPC) else None
    if not mpc:
        return (base, base_out), (rough, rough_out)
    weather = mel.create_material_expression(m, unreal.MaterialExpressionCollectionParameter, x - 300, y)
    unreal.NoirScriptLibrary.bind_collection_parameter(weather, mpc, "Wetness")  # name and id
    # How much the weather reaches this surface: 0 for anything that is never out in it (the Mind Place).
    reach = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x - 300, y + 100)
    reach.set_editor_property("parameter_name", "WetAffect")
    reach.set_editor_property("default_value", 1.0)
    w = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, x - 150, y)
    mel.connect_material_expressions(weather, "", w, "A")
    mel.connect_material_expressions(reach, "", w, "B")
    out = []
    for src, src_out, k, dy in ((base, base_out, 0.55, 0), (rough, rough_out, 0.22, 200)):
        scaled = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, x + 200, y + dy)
        mel.connect_material_expressions(src, src_out, scaled, "A")
        kn = mel.create_material_expression(m, unreal.MaterialExpressionConstant, x, y + dy + 80)
        kn.set_editor_property("r", k)
        mel.connect_material_expressions(kn, "", scaled, "B")
        lerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, x + 350, y + dy)
        mel.connect_material_expressions(src, src_out, lerp, "A")
        mel.connect_material_expressions(scaled, "", lerp, "B")
        mel.connect_material_expressions(w, "", lerp, "Alpha")
        out.append((lerp, ""))
    return out[0], out[1]


def save(asset):
    eal.save_loaded_asset(asset)
    return asset


# ---------------------------------------------------------------- textures and materials

def tex(name, kind):
    """A ground texture imported by import_polyhaven.py: kind is Diffuse, nor_gl or arm."""
    t = unreal.load_asset("%s/Textures/%s/T_%s_%s" % (ENV, name, name, kind))
    if kind == "nor_gl" and not t.get_editor_property("flip_green_channel"):
        t.set_editor_property("flip_green_channel", True)  # Poly Haven's normals are OpenGL-style
        save(t)
    return t


def import_masks():
    out = {}
    for name in ("T_Grounds_MaskA", "T_Grounds_MaskB"):
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(ROOT, "SourceArt", "Generated", name + ".png"))
        t.set_editor_property("destination_path", GEN)
        t.set_editor_property("destination_name", name)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tools.import_asset_tasks([t])
        m = unreal.load_asset("%s/%s" % (GEN, name))
        m.set_editor_property("srgb", False)
        m.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        m.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        m.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        m.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
        out[name] = save(m)
    return out


def new_material(name, folder=GEN):
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def node(m, cls, x, y, **props):
    n = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def link(a, out, b, inp):
    if not mel.connect_material_expressions(a, out, b, inp):
        log("could not connect %s.%s -> %s.%s" % (a.get_class().get_name(), out, b.get_class().get_name(), inp))


def sample(m, texture, uv, x, y, kind="color"):
    st = {"color": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
          "normal": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
          "masks": unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
          "linear": unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR}[kind]
    n = node(m, unreal.MaterialExpressionTextureSample, x, y, texture=texture, sampler_type=st,
             sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    link(uv, "", n, "UVs")
    return n


def world_uv(m, scale_cm, x, y):
    """World XY divided by a tile size: ground textures laid flat, the same across terrain tiles."""
    wp = node(m, unreal.MaterialExpressionWorldPosition, x - 400, y)
    mask = node(m, unreal.MaterialExpressionComponentMask, x - 250, y, r=True, g=True, b=False, a=False)
    link(wp, "", mask, "")
    div = node(m, unreal.MaterialExpressionDivide, x - 100, y)
    link(mask, "", div, "A")
    k = node(m, unreal.MaterialExpressionConstant, x - 250, y + 80, r=scale_cm)
    link(k, "", div, "B")
    return div


def build_terrain_material(mask_a, mask_b):
    """Forest floor and leaf litter everywhere, painted over by the masks: gravel (drive, paths),
    lawn, mud (the lake shore) and cobbles (terrace, the strip around the house)."""
    m = new_material("M_Grounds_Terrain")
    m.set_editor_property("used_with_nanite", True)
    # Mask UVs: world XY onto grounds_layout.WORLD, north at the top.
    x0, y0, x1, y1 = G.WORLD
    wp = node(m, unreal.MaterialExpressionWorldPosition, -2600, -900)
    mx = node(m, unreal.MaterialExpressionComponentMask, -2450, -950, r=True, g=False, b=False, a=False)
    my = node(m, unreal.MaterialExpressionComponentMask, -2450, -850, r=False, g=True, b=False, a=False)
    link(wp, "", mx, "")
    link(wp, "", my, "")
    u = node(m, unreal.MaterialExpressionMultiply, -2150, -950)
    u_off = node(m, unreal.MaterialExpressionAdd, -2300, -950)
    link(mx, "", u_off, "A")
    link(node(m, unreal.MaterialExpressionConstant, -2450, -1050, r=-x0), "", u_off, "B")
    link(u_off, "", u, "A")
    link(node(m, unreal.MaterialExpressionConstant, -2300, -1050, r=1.0 / (x1 - x0)), "", u, "B")
    v = node(m, unreal.MaterialExpressionMultiply, -2150, -850)
    v_off = node(m, unreal.MaterialExpressionSubtract, -2300, -850)
    link(node(m, unreal.MaterialExpressionConstant, -2450, -750, r=y1), "", v_off, "A")
    link(my, "", v_off, "B")
    link(v_off, "", v, "A")
    link(node(m, unreal.MaterialExpressionConstant, -2300, -750, r=1.0 / (y1 - y0)), "", v, "B")
    uv = node(m, unreal.MaterialExpressionAppendVector, -2000, -900)
    link(u, "", uv, "A")
    link(v, "", uv, "B")
    ma = sample(m, mask_a, uv, -1800, -1000, "linear")
    mb = sample(m, mask_b, uv, -1800, -800, "linear")

    layers = [  # (texture, tile cm, mask output) painted in this order over the base
        ("forest_leaves_04", 260.0, None),
        ("mud_forest", 300.0, (ma, "B")),
        ("leafy_grass", 260.0, (ma, "G")),
        ("gravel_road", 380.0, (ma, "R")),
        ("mossy_cobblestone", 240.0, (ma, "A")),
    ]
    base_uv = world_uv(m, 300.0, -1600, -400)
    cur = [sample(m, tex("forest_ground_04", "Diffuse"), base_uv, -1400, -500),
           sample(m, tex("forest_ground_04", "nor_gl"), base_uv, -1400, -300, "normal"),
           sample(m, tex("forest_ground_04", "arm"), base_uv, -1400, -100, "masks")]
    y = 0
    for i, (name, tile, alpha) in enumerate(layers):
        uvn = world_uv(m, tile, -1600, y + 200)
        smp = [sample(m, tex(name, "Diffuse"), uvn, -1400, y + 100),
               sample(m, tex(name, "nor_gl"), uvn, -1400, y + 300, "normal"),
               sample(m, tex(name, "arm"), uvn, -1400, y + 500, "masks")]
        a_node, a_out = alpha if alpha else (mb, "R")
        nxt = []
        for k in range(3):
            lerp = node(m, unreal.MaterialExpressionLinearInterpolate, -1000 + 150 * i, y + 200 * k)
            link(cur[k], "", lerp, "A")
            link(smp[k], "", lerp, "B")
            link(a_node, a_out, lerp, "Alpha")
            nxt.append(lerp)
        cur = nxt
        y += 700
    (wb, wbo), (wr, wro) = wet(m, cur[0], cur[2], 200, 200, rough_out="G")
    mel.connect_material_property(wb, wbo, unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(cur[1], "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(wr, wro, unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(cur[2], "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.recompile_material(m)
    return save(m)


def build_world_pbr():
    """M_World_PBR: diffuse / normal / ARM box-projected from the world's axes (walls, roofs,
    garden walls), so stretched boxes keep the stone the right size. Each map is sampled along X,
    Y and Z and the three blended by how squarely the surface faces each axis."""
    m = new_material("M_World_PBR")
    m.set_editor_property("used_with_nanite", True)
    tile = node(m, unreal.MaterialExpressionScalarParameter, -2000, 0, parameter_name="TileCm", default_value=300.0)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -2000, -200)
    scaled = node(m, unreal.MaterialExpressionDivide, -1850, -150)
    link(wp, "", scaled, "A")
    link(tile, "", scaled, "B")
    planes = []
    for i, (a, b) in enumerate(((False, True), (True, False), (True, True))):
        # UVs for the planes facing X (YZ), Y (XZ) and Z (XY).
        r, g, bb = {0: (False, True, True), 1: (True, False, True), 2: (True, True, False)}[i]
        mask = node(m, unreal.MaterialExpressionComponentMask, -1650, -300 + 150 * i, r=r, g=g, b=bb, a=False)
        link(scaled, "", mask, "")
        planes.append(mask)
    # Blend weights: |normal| sharpened, normalised.
    nrm = node(m, unreal.MaterialExpressionVertexNormalWS, -2000, 400)
    absn = node(m, unreal.MaterialExpressionAbs, -1850, 400)
    link(nrm, "", absn, "")
    sharp = node(m, unreal.MaterialExpressionPower, -1700, 400)
    link(absn, "", sharp, "Base")
    link(node(m, unreal.MaterialExpressionConstant, -1850, 500, r=6.0), "", sharp, "Exp")
    dot = node(m, unreal.MaterialExpressionDotProduct, -1550, 500)
    link(sharp, "", dot, "A")
    link(node(m, unreal.MaterialExpressionConstant3Vector, -1700, 600, constant=unreal.LinearColor(1, 1, 1, 1)), "", dot, "B")
    weights = node(m, unreal.MaterialExpressionDivide, -1400, 450)
    link(sharp, "", weights, "A")
    link(dot, "", weights, "B")
    w = []
    for i, ch in enumerate(("R", "G", "B")):
        c = node(m, unreal.MaterialExpressionComponentMask, -1250, 350 + 100 * i, r=ch == "R", g=ch == "G", b=ch == "B", a=False)
        link(weights, "", c, "")
        w.append(c)

    def projected(pname, default, kind, y):
        tex_param = node(m, unreal.MaterialExpressionTextureObjectParameter, -1450, y,
                         parameter_name=pname, texture=unreal.load_asset(default))
        st = {"color": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
              "normal": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
              "masks": unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[kind]
        tex_param.set_editor_property("sampler_type", st)
        total = None
        for i in range(3):
            smp = node(m, unreal.MaterialExpressionTextureSample, -1100, y + 120 * i, sampler_type=st,
                       sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
            link(tex_param, "", smp, "Tex")
            link(planes[i], "", smp, "UVs")
            mul = node(m, unreal.MaterialExpressionMultiply, -900, y + 120 * i)
            link(smp, "RGB", mul, "A")
            link(w[i], "", mul, "B")
            if total is None:
                total = mul
            else:
                add = node(m, unreal.MaterialExpressionAdd, -750, y + 60 * i)
                link(total, "", add, "A")
                link(mul, "", add, "B")
                total = add
        return total

    diff = projected("Diffuse", "/Engine/EngineResources/WhiteSquareTexture", "color", -900)
    nor = projected("Normal", "/Engine/EngineMaterials/DefaultNormal", "normal", -400)
    # The default must be a masks texture itself, or the masks sampler refuses it.
    arm = projected("ARM", "%s/Textures/old_stone_wall/T_old_stone_wall_arm" % ENV, "masks", 100)
    tint = node(m, unreal.MaterialExpressionVectorParameter, -750, -1050, parameter_name="Tint",
                default_value=unreal.LinearColor(1, 1, 1, 1))
    bc = node(m, unreal.MaterialExpressionMultiply, -500, -950)
    link(diff, "", bc, "A")
    link(tint, "", bc, "B")
    nor_n = node(m, unreal.MaterialExpressionNormalize, -500, -400)
    link(nor, "", nor_n, "")
    mel.connect_material_property(nor_n, "", unreal.MaterialProperty.MP_NORMAL)
    ao = node(m, unreal.MaterialExpressionComponentMask, -500, 100, r=True, g=False, b=False, a=False)
    rough = node(m, unreal.MaterialExpressionComponentMask, -500, 200, r=False, g=True, b=False, a=False)
    link(arm, "", ao, "")
    link(arm, "", rough, "")
    mel.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    (wb, wbo), (wr, wro) = wet(m, bc, rough, -300, 500)
    mel.connect_material_property(wb, wbo, unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(wr, wro, unreal.MaterialProperty.MP_ROUGHNESS)
    emissive = node(m, unreal.MaterialExpressionVectorParameter, -500, 350, parameter_name="Emissive",
                    default_value=unreal.LinearColor(0, 0, 0, 1))
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    return save(m)


def world_instance(parent, name, texture, tile_cm, tint=(1, 1, 1)):
    path = "%s/%s" % (GEN, name)
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        name, GEN, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    mel.set_material_instance_texture_parameter_value(mi, "Diffuse", tex(texture, "Diffuse"))
    mel.set_material_instance_texture_parameter_value(mi, "Normal", tex(texture, "nor_gl"))
    mel.set_material_instance_texture_parameter_value(mi, "ARM", tex(texture, "arm"))
    mel.set_material_instance_scalar_parameter_value(mi, "TileCm", tile_cm)
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1))
    return save(mi)


def build_simple(name, color, roughness, emissive=(0, 0, 0), specular=0.5, unlit_cube=None):
    """A flat-coloured material (water, window glow, gate pillar caps)."""
    m = new_material(name)
    m.set_editor_property("used_with_nanite", True)
    c = node(m, unreal.MaterialExpressionConstant3Vector, -400, 0, constant=unreal.LinearColor(*color, 1))
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = node(m, unreal.MaterialExpressionConstant, -400, 100, r=roughness)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    s = node(m, unreal.MaterialExpressionConstant, -400, 200, r=specular)
    mel.connect_material_property(s, "", unreal.MaterialProperty.MP_SPECULAR)
    if any(emissive):
        e = node(m, unreal.MaterialExpressionConstant3Vector, -400, 300, constant=unreal.LinearColor(*emissive, 1))
        mel.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    return save(m)


def build_glass():
    """Clear, dark glass for the conservatory: mostly see-through, with a hard reflection."""
    m = new_material("M_Grounds_Glass")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    # Lit per pixel, so it carries a reflection (the names drift between engine versions).
    for mode in ("TLM_SURFACE_FORWARD_SHADING", "TLM_SURFACE_PER_PIXEL_LIGHTING", "TLM_SURFACE"):
        if hasattr(unreal.TranslucencyLightingMode, mode):
            m.set_editor_property("translucency_lighting_mode", getattr(unreal.TranslucencyLightingMode, mode))
            break
    m.set_editor_property("two_sided", True)
    c = node(m, unreal.MaterialExpressionConstant3Vector, -400, -100, constant=unreal.LinearColor(0.02, 0.03, 0.03, 1))
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(node(m, unreal.MaterialExpressionConstant, -400, 0, r=0.06), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(node(m, unreal.MaterialExpressionConstant, -400, 100, r=1.0), "", unreal.MaterialProperty.MP_SPECULAR)
    # More opaque at grazing angles, like real glass.
    fres = node(m, unreal.MaterialExpressionFresnel, -400, 200)
    lerp = node(m, unreal.MaterialExpressionLinearInterpolate, -200, 200)
    link(node(m, unreal.MaterialExpressionConstant, -400, 300, r=0.12), "", lerp, "A")
    link(node(m, unreal.MaterialExpressionConstant, -400, 380, r=0.6), "", lerp, "B")
    link(fres, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    return save(m)


def build_water():
    """Black, still water: a near-mirror with a slow double ripple."""
    m = new_material("M_Grounds_Water")
    c = node(m, unreal.MaterialExpressionConstant3Vector, -400, -200, constant=unreal.LinearColor(0.004, 0.006, 0.006, 1))
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(node(m, unreal.MaterialExpressionConstant, -400, -100, r=0.04), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(node(m, unreal.MaterialExpressionConstant, -400, 0, r=1.0), "", unreal.MaterialProperty.MP_SPECULAR)
    ripple = tex("mud_forest", "nor_gl")  # any soft normal will do, slowed and flattened
    out = None
    for i, (scale, speed) in enumerate(((900.0, (0.004, 0.002)), (1700.0, (-0.002, 0.003)))):
        uv = world_uv(m, scale, -1000, 200 + 300 * i)
        pan = node(m, unreal.MaterialExpressionPanner, -800, 200 + 300 * i, speed_x=speed[0], speed_y=speed[1])
        link(uv, "", pan, "Coordinate")
        s = sample(m, ripple, pan, -600, 200 + 300 * i, "normal")
        if out is None:
            out = s
        else:
            add = node(m, unreal.MaterialExpressionAdd, -400, 400)
            link(out, "", add, "A")
            link(s, "", add, "B")
            out = add
    flat = node(m, unreal.MaterialExpressionConstant3Vector, -400, 600, constant=unreal.LinearColor(0, 0, 1, 1))
    lerp = node(m, unreal.MaterialExpressionLinearInterpolate, -200, 500)
    link(flat, "", lerp, "A")
    link(out, "", lerp, "B")
    link(node(m, unreal.MaterialExpressionConstant, -400, 700, r=0.18), "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(m)
    return save(m)


def build_sky():
    """The overcast night sky (Poly Haven's kloppenheim_07), very dim, on a huge inside-out sphere."""
    cube = None
    latlong = None
    for p in eal.list_assets(ENV + "/Sky"):
        a = unreal.load_asset(p)
        if isinstance(a, unreal.TextureCube):
            cube = a
        elif isinstance(a, unreal.Texture2D):
            latlong = a
    m = new_material("M_Grounds_NightSky")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    try:
        m.set_editor_property("is_sky", True)
    except Exception:
        pass
    if cube:
        cam = node(m, unreal.MaterialExpressionCameraVectorWS, -800, 0)
        neg = node(m, unreal.MaterialExpressionMultiply, -650, 0)
        link(cam, "", neg, "A")
        link(node(m, unreal.MaterialExpressionConstant, -800, 100, r=-1.0), "", neg, "B")
        s = node(m, unreal.MaterialExpressionTextureSample, -450, 0, texture=cube)
        link(neg, "", s, "UVs")
        k = node(m, unreal.MaterialExpressionMultiply, -250, 0)
        link(s, "", k, "A")
        link(node(m, unreal.MaterialExpressionScalarParameter, -450, 200, parameter_name="Brightness", default_value=SKY_GLOW), "", k, "B")
        mel.connect_material_property(k, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    elif latlong:
        # Equirectangular: u from the heading, v from the height of the view direction.
        cam = node(m, unreal.MaterialExpressionCameraVectorWS, -1400, 0)
        neg = node(m, unreal.MaterialExpressionMultiply, -1250, 0)
        link(cam, "", neg, "A")
        link(node(m, unreal.MaterialExpressionConstant, -1400, 100, r=-1.0), "", neg, "B")
        dx = node(m, unreal.MaterialExpressionComponentMask, -1100, -100, r=True, g=False, b=False, a=False)
        dy = node(m, unreal.MaterialExpressionComponentMask, -1100, 0, r=False, g=True, b=False, a=False)
        dz = node(m, unreal.MaterialExpressionComponentMask, -1100, 100, r=False, g=False, b=True, a=False)
        for c in (dx, dy, dz):
            link(neg, "", c, "")
        atan = node(m, unreal.MaterialExpressionArctangent2, -950, -50)
        link(dy, "", atan, "Y")
        link(dx, "", atan, "X")
        u = node(m, unreal.MaterialExpressionMultiply, -800, -50)
        link(atan, "", u, "A")
        link(node(m, unreal.MaterialExpressionConstant, -950, 50, r=-0.5 / math.pi), "", u, "B")
        u1 = node(m, unreal.MaterialExpressionAdd, -650, -50)
        link(u, "", u1, "A")
        link(node(m, unreal.MaterialExpressionConstant, -800, 50, r=0.5), "", u1, "B")
        acos = node(m, unreal.MaterialExpressionArccosine, -950, 150)
        link(dz, "", acos, "")
        v = node(m, unreal.MaterialExpressionMultiply, -800, 150)
        link(acos, "", v, "A")
        link(node(m, unreal.MaterialExpressionConstant, -950, 250, r=1.0 / math.pi), "", v, "B")
        uv = node(m, unreal.MaterialExpressionAppendVector, -500, 50)
        link(u1, "", uv, "A")
        link(v, "", uv, "B")
        smp = node(m, unreal.MaterialExpressionTextureSample, -350, 0, texture=latlong,
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        link(uv, "", smp, "UVs")
        k = node(m, unreal.MaterialExpressionMultiply, -150, 0)
        link(smp, "RGB", k, "A")
        link(node(m, unreal.MaterialExpressionScalarParameter, -350, 200, parameter_name="Brightness", default_value=SKY_GLOW), "", k, "B")
        mel.connect_material_property(k, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        log("no sky imported; the sky stays black")
        c = node(m, unreal.MaterialExpressionConstant3Vector, -400, 0, constant=unreal.LinearColor(0.002, 0.003, 0.004, 1))
        mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    return save(m), cube


# ---------------------------------------------------------------- terrain

def _as_tuple(result):
    return result if isinstance(result, tuple) else (result,)


def build_terrain_tiles(material):
    """The ground as TILES Nanite meshes (Geometry Scripting): a subdivided rectangle per tile,
    every vertex lifted to grounds_layout.height, colliding as drawn."""
    x0, y0, x1, y1 = G.WORLD
    nx, ny = G.TILES
    tw, th = (x1 - x0) / nx, (y1 - y0) / ny
    n = G.QUADS
    meshes = []
    for ty in range(ny):
        for tx in range(nx):
            name = "SM_Grounds_Terrain_%d_%d" % (tx, ty)
            path = "%s/%s" % (GEN, name)
            if eal.does_asset_exist(path):
                eal.delete_asset(path)
            dm = unreal.DynamicMesh()
            cx, cy = x0 + (tx + 0.5) * tw, y0 + (ty + 0.5) * th
            unreal.GeometryScript_Primitives.append_rectangle_xy(
                dm, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(unreal.Vector(cx, cy, 0)),
                dimension_x=tw, dimension_y=th, steps_width=n, steps_height=n)
            plist = _as_tuple(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False))[1]
            pts = unreal.GeometryScript_List.convert_vector_list_to_array(plist)
            lifted = [unreal.Vector(v.x, v.y, G.height(v.x, v.y)) for v in pts]
            unreal.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(
                dm, unreal.GeometryScript_List.convert_array_to_vector_list(lifted))
            unreal.GeometryScript_Normals.recompute_normals(dm, unreal.GeometryScriptCalculateNormalsOptions())
            opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
            opts.set_editor_property("enable_nanite", True)
            opts.set_editor_property("enable_collision", True)
            opts.set_editor_property("enable_recompute_normals", False)
            opts.set_editor_property("enable_recompute_tangents", True)
            sm = _as_tuple(unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm, path, opts))[0]
            sm.set_editor_property("static_materials", [unreal.StaticMaterial(material_interface=material, material_slot_name="Ground")])
            body = sm.get_editor_property("body_setup")
            if body:
                body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            save(sm)
            meshes.append(sm)
        log("terrain row %d of %d" % (ty + 1, ny))
    return meshes


# ---------------------------------------------------------------- the level

def clear_previous():
    old = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()).startswith("Grounds")]
    old += [a for a in actors.get_all_level_actors() if a.get_actor_label() in ("HallEnd-1", "HallEnd1")]
    if old:
        actors.destroy_actors(old)
    log("cleared %d actors from the last build" % len(old))


def place(mesh, loc, rot=(0, 0, 0), scale=(1, 1, 1), folder="Grounds", label=None, material=None):
    a = actors.spawn_actor_from_object(mesh, unreal.Vector(*loc), unreal.Rotator(*rot))
    a.set_actor_scale3d(unreal.Vector(*scale))
    a.set_folder_path(folder)
    if label:
        a.set_actor_label(label)
    if material:
        a.static_mesh_component.set_material(0, material)
    return a


def box(label, center, size, material, folder="Grounds/House", rot=(0, 0, 0), shadow=True):
    return L.block(label, center, size, material, folder, shadow=shadow, rot=rot)


def models(name):
    """A Poly Haven model's meshes: its variants (a, b, c...), each once (LOD0 only)."""
    out = []
    for p in sorted(eal.list_assets("%s/Models/%s" % (ENV, name), recursive=False)):
        a = unreal.load_asset(p)
        if isinstance(a, unreal.StaticMesh) and not re.search(r"_LOD[1-9]$", a.get_name()):
            out.append(a)
    if not out:
        raise RuntimeError("model %s not imported" % name)
    return out


def model(name, part=None):
    for m in models(name):
        if part is None or m.get_name().endswith(part):
            return m
    raise RuntimeError("%s has no part %s" % (name, part))


def base_offset(mesh):
    """Where the mesh's base centre sits relative to its pivot (Poly Haven sets lay variants out
    in a row, so their pivots are rarely under them)."""
    b = mesh.get_bounds()
    return (b.origin.x, b.origin.y, b.origin.z - b.box_extent.z)


def stand(mesh, x, y, z, yaw=0.0, scale=1.0):
    """The transform that stands a mesh's base centre on (x, y, z), turned by yaw."""
    ox, oy, oz = base_offset(mesh)
    r = math.radians(yaw)
    dx = (ox * math.cos(r) - oy * math.sin(r)) * scale
    dy = (ox * math.sin(r) + oy * math.cos(r)) * scale
    return (x - dx, y - dy, z - oz * scale), (0, 0, yaw), (scale, scale, scale)


def place_on(mesh, x, y, z, yaw=0.0, scale=1.0, folder="Grounds", label=None):
    loc, rot, sc = stand(mesh, x, y, z, yaw, scale)
    return place(mesh, loc, rot, sc, folder, label)


def build_house_shell(stone, roof, glow, wood):
    """Stone outside walls, a slate gable roof, lit windows, and the two hall doors opened up."""
    # build_manor's outer faces: long walls at y = +/-760, wing ends at x = +/-910, hall sides +/-160.
    x0, y0, x1, y1 = -910.0, -760.0, 910.0, 760.0
    t = G.SHELL
    hz = WALL_H + 30
    # Long walls (north, south), the end walls of the two wings, the hall's ends with their doors.
    for side, y in (("N", y1 + t / 2), ("S", y0 - t / 2)):
        box("Shell_" + side, (0, y, hz / 2), (x1 - x0 + 2 * t, t, hz), stone)
    for sx in (-1, 1):
        x = sx * (x1 + t / 2)
        for side in (-1, 1):
            ya, yb = (160.0, y1 + t) if side > 0 else (y0 - t, -160.0)
            box("Shell_Wing%d_%d" % (sx, side), (x, (ya + yb) / 2, hz / 2), (t, yb - ya, hz), stone)
        # The hall runs out to x = +/-1000: its side walls outside, and its end wall with the door.
        for side in (-1, 1):
            box("Shell_HallSide%d_%d" % (sx, side), (sx * (x1 + (1000.0 + t - x1) / 2), side * (160.0 + t / 2), hz / 2),
                (1000.0 + t - x1, t, hz), stone)
        end = sx * (1000.0 + t / 2)
        for side in (-1, 1):
            box("Shell_HallEnd%d_%d" % (sx, side), (end, side * (DOOR_W / 2 + (150.0 + t - DOOR_W / 2) / 2), hz / 2),
                (t, 150.0 + t - DOOR_W / 2, hz), stone)
        box("Shell_DoorHead%d" % sx, (end, 0, (DOOR_H + hz) / 2), (t, DOOR_W, hz - DOOR_H), stone)
        # The inner face of the hall's end, with the same opening (replaces build_manor's solid wall).
        for side in (-1, 1):
            box("HallEndInner%d_%d" % (sx, side), (sx * 1000.0, side * (DOOR_W / 2 + (150.0 - DOOR_W / 2) / 2), WALL_H / 2),
                (20, 150.0 - DOOR_W / 2, WALL_H), stone)
        box("HallEndInnerHead%d" % sx, (sx * 1000.0, 0, (DOOR_H + WALL_H) / 2), (20, DOOR_W, WALL_H - DOOR_H), stone)
        box("DoorFrame%d" % sx, (end + sx * (t / 2 + 4), 0, DOOR_H + 6), (10, DOOR_W + 30, 12), wood)
        for side in (-1, 1):
            box("DoorPost%d_%d" % (sx, side), (end + sx * (t / 2 + 4), side * (DOOR_W / 2 + 9), DOOR_H / 2), (10, 14, DOOR_H), wood)
        # A step down from each door.
        box("DoorStep%d" % sx, (end + sx * (t + 60), 0, -4), (120, DOOR_W + 80, 14), stone)
    # The roof: two slate slopes meeting over the hall, gables of stone, a pair of chimneys.
    span = (y1 - y0) / 2 + t + 60
    pitch = 34.0
    rise = span * math.tan(math.radians(pitch))
    slope = span / math.cos(math.radians(pitch))
    for side in (-1, 1):
        box("Roof_%d" % side, (0, side * span / 2, hz + rise / 2), (x1 - x0 + 2 * t + 120, slope + 10, 22), roof,
            rot=(side * pitch, 0, 0))
    for sx in (-1, 1):
        for k in range(6):
            frac = (k + 0.5) / 6
            w = 2 * span * (1 - frac)
            box("Gable%d_%d" % (sx, k), (sx * (x1 + t / 2), 0, hz + rise * frac), (t, w, rise / 6 + 2), stone)
    for sx in (-1, 1):
        box("Chimney%d" % sx, (sx * 600.0, 420.0, hz + rise * 0.7 + 120), (90, 70, rise * 0.9 + 160), stone)
        box("ChimneyCap%d" % sx, (sx * 600.0, 420.0, hz + rise * 1.15 + 205), (110, 90, 14), stone)
    # Windows: warm panes in the long walls, two to a room, mullions over them.
    for side, y in ((1, y1 + t + 1), (-1, y0 - t - 1)):
        for rx in (-600.0, 0.0, 600.0):
            for wx in (rx - 130, rx + 130):
                box("Window_%d_%d" % (side, int(wx)), (wx, y, 175), (90, 4, 150), glow, shadow=False)
                box("WindowSill_%d_%d" % (side, int(wx)), (wx, y + side * 6, 96), (110, 14, 8), stone)
                box("WindowMullionV_%d_%d" % (side, int(wx)), (wx, y + side * 3, 175), (6, 4, 150), wood, shadow=False)
                box("WindowMullionH_%d_%d" % (side, int(wx)), (wx, y + side * 3, 195), (90, 4, 6), wood, shadow=False)
    # Lanterns at both doors.
    for sx in (-1, 1):
        for side in (-1, 1):
            loc = (sx * (1000.0 + t + 20), side * (DOOR_W / 2 + 60), 230)
            box("DoorLantern%d_%d" % (sx, side), loc, (16, 16, 24), glow, shadow=False)
            c = L.practical("DoorLight%d_%d" % (sx, side), (loc[0] + sx * 20, loc[1], loc[2] - 10), 9.0, 900, 2300, source=6)
            c.get_owner().set_folder_path("Grounds/Lights")


def garden_walls(wall):
    gx0, gy0, gx1, gy1 = G.GARDEN
    h, t = 110.0, 45.0
    (gate_x, gate_y0), (_, gate_y1) = G.GARDEN_GATE

    def run(label, a, b):
        (ax, ay), (bx, by) = a, b
        length = math.hypot(bx - ax, by - ay)
        yaw = math.degrees(math.atan2(by - ay, bx - ax))
        box(label, ((ax + bx) / 2, (ay + by) / 2, h / 2 - 10), (length + t, t, h), wall, "Grounds/Garden", rot=(0, 0, yaw))
        box(label + "_Cap", ((ax + bx) / 2, (ay + by) / 2, h - 4), (length + t + 8, t + 10, 10), wall, "Grounds/Garden", rot=(0, 0, yaw))

    # North and south from the house to the east corners, the east wall either side of its gate.
    run("GardenWallN", (gx0, gy1), (gx1, gy1))
    run("GardenWallS", (gx0, gy0), (gx1, gy0))
    run("GardenWallE1", (gx1, gy0), (gate_x, gate_y0))
    run("GardenWallE2", (gate_x, gate_y1), (gx1, gy1))
    for i, y in enumerate((gate_y0, gate_y1)):
        box("GardenGatePier%d" % i, (gate_x, y, 80), (60, 60, 170), wall, "Grounds/Garden")
    # And short walls from the house's corners out to the garden's, closing it off.
    run("GardenWallNW", (900.0, G.HOUSE[3]), (gx0, gy1))
    run("GardenWallSW", (900.0, G.HOUSE[1]), (gx0, gy0))


def build_props(wall, stone_mat):
    sx, sy = G.STATUE
    box("StatuePlinth", (sx, sy, 50), (140, 140, 110), wall, "Grounds/Garden")
    place_on(model("gothic_statue"), sx, sy, 105, 180, folder="Grounds/Garden", label="Statue")
    bench = model("painted_wooden_bench")
    for i, (bx, by, yaw) in enumerate(((sx - 900, 450, 90), (sx + 900, -450, -90), (sx, 950, 0), (sx, -950, 180))):
        place_on(bench, bx, by, G.height(bx, by), yaw, folder="Grounds/Garden", label="Bench%d" % i)
    # The car on the verge by the terrace, under its cover (its parts share one pivot).
    cx, cy, cyaw = G.CAR
    for part in models("covered_car"):
        place(part, (cx, cy, G.height(cx, cy)), (0, 0, cyaw), folder="Grounds/Drive", label="Car_" + part.get_name())
    # The gate between two stone piers, a wall running off into the trees either side.
    gx, gy = G.GATE
    for part in models("large_iron_gate"):
        place(part, (gx, gy, G.height(gx, gy)), (0, 0, 90), folder="Grounds/Drive", label="Gate_" + part.get_name())
    half = 150.0
    for side in (-1, 1):
        py = gy + side * (half + 40)
        box("GatePier%d" % side, (gx, py, 170), (90, 90, 360), stone_mat, "Grounds/Drive")
        box("GateWall%d" % side, (gx, py + side * 1500, 120), (60, 3000, 260), stone_mat, "Grounds/Drive")
    box("GateBlocker", (gx - 60, gy, 300), (40, 2 * half + 200, 800), stone_mat, "Grounds/Bounds").set_actor_hidden_in_game(True)
    # The pier out over the water, the old fire pit on the bank above it.
    # The pier's sections are laid out along its -Y, the deck 2.67 m above the pivot: aim -Y at the water.
    px, py, _ = G.PIER
    dx, dy = G.LAKE_C[0] - px, G.LAKE_C[1] - py
    yaw = math.degrees(math.atan2(dx, -dy))
    for part in models("modular_wooden_pier"):
        if "section" in part.get_name():
            place(part, (px, py, G.WATER_Z + 55 - 267), (0, 0, yaw), folder="Grounds/Lake", label="Pier_" + part.get_name())
    fx, fy = G.FIRE_PIT
    place_on(model("stone_fire_pit"), fx, fy, G.height(fx, fy), folder="Grounds/Lake", label="FirePit")
    ember = L.practical("FirePitEmbers", (fx, fy, G.height(fx, fy) + 25), 3.0, 500, 1700, source=30)
    ember.get_owner().set_folder_path("Grounds/Lights")


# ---------------------------------------------------------------- outbuildings

def interior_materials():
    """The look-dev materials the manor's furniture builders use, loaded as they are (the master
    material is not rebuilt)."""
    master = unreal.load_asset(L.MAT_DIR + "/M_Master")
    keep = L.build_master_material
    L.build_master_material = lambda: master
    try:
        m = L.build_materials()
    finally:
        L.build_master_material = keep
    m["night_glass"] = L.make_instance(master, "MI_Glass_Night", (0.01, 0.02, 0.02), 0.1, emissive=(0.004, 0.012, 0.012))
    m["candle"] = L.make_instance(master, "MI_Candle_Wax", (0.55, 0.5, 0.4), 0.6, emissive=(0.05, 0.03, 0.01))
    return m


def furnish(tag, frame, kind, lx, ly, yaw, name, m, rng):
    """One of the manor's furniture builders, then its searchable renamed for this building."""
    import build_manor as BM
    before = set(a.get_name() for a in actors.get_all_level_actors() if isinstance(a, unreal.NoirSearchable))
    BM.BUILDERS[kind](tag, frame.child(lx, ly, yaw), m, rng)
    for a in actors.get_all_level_actors():
        if isinstance(a, unreal.NoirSearchable) and a.get_name() not in before:
            a.set_editor_property("display_name", unreal.Text(name))


def case_room(room_id, box, suits, can_be_scene, body, stands, label):
    """The NoirRoom that lets a case cast this building, its stand points and where the body lies."""
    x0, y0, x1, y1 = box
    room = actors.spawn_actor_from_class(unreal.NoirRoom, unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, 160))
    room.set_actor_label("Room_" + room_id)
    room.set_folder_path("Grounds/Outbuildings")
    room.set_editor_property("room_id", unreal.Name(room_id))
    room.set_editor_property("can_be_scene", can_be_scene)
    room.set_editor_property("only_suits", True)
    room.set_editor_property("suits", suits)
    room.get_editor_property("bounds").set_box_extent(unreal.Vector((x1 - x0) / 2 - 10, (y1 - y0) / 2 - 10, 160))
    if body:
        room.set_editor_property("body_point", unreal.Transform(unreal.Vector(body[0], body[1], -160),
                                                               unreal.Rotator(0, 0, body[2]), unreal.Vector(1, 1, 1)))
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    for i, (lx, ly, yaw) in enumerate(stands):
        st = actors.spawn_actor_from_class(unreal.NoirStandPoint, unreal.Vector(cx + lx, cy + ly, 0), unreal.Rotator(0, 0, yaw))
        st.set_actor_label("Stand_%s_%d" % (room_id, i))
        st.set_folder_path("Grounds/Outbuildings")
    log("%s: a case room for %s" % (label, ", ".join(suits)))


def build_conservatory(stone, glass, iron, floor, m):
    """Glass on a low stone plinth, white iron ribs, a glass roof; the door on the garden path."""
    import build_manor as BM
    x0, y0, x1, y1 = G.CONSERVATORY
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    w, d = x1 - x0, y1 - y0
    f = "Grounds/Outbuildings/Conservatory"
    plinth, top = 60.0, 300.0
    box("Con_Floor", (cx, cy, -4), (w, d, 12), floor, f)
    door_w = 160.0
    # Plinth and glass along each side; the south side leaves the door.
    sides = [("N", (x0, y1), (x1, y1)), ("E", (x1, y0), (x1, y1)), ("W", (x0, y0), (x0, y1)),
             ("S1", (x0, y0), (cx - door_w / 2, y0)), ("S2", (cx + door_w / 2, y0), (x1, y0))]
    for tag, (ax, ay), (bx, by) in sides:
        length = math.hypot(bx - ax, by - ay)
        yaw = math.degrees(math.atan2(by - ay, bx - ax))
        mx, my = (ax + bx) / 2, (ay + by) / 2
        box("Con_Plinth" + tag, (mx, my, plinth / 2), (length + 30, 30, plinth), stone, f, rot=(0, 0, yaw))
        box("Con_Glass" + tag, (mx, my, (plinth + top) / 2), (length, 3, top - plinth), glass, f, rot=(0, 0, yaw), shadow=False)
        box("Con_Beam" + tag, (mx, my, top), (length + 12, 12, 10), iron, f, rot=(0, 0, yaw))
        n = max(1, int(length // 110))
        for k in range(n + 1):
            t = k / n
            box("Con_Post%s_%d" % (tag, k), (ax + (bx - ax) * t, ay + (by - ay) * t, (plinth + top) / 2), (7, 7, top - plinth), iron, f)
    box("Con_DoorHead", (cx, y0, top - 30), (door_w + 20, 12, 60), iron, f)
    # The roof: two glass slopes on iron ribs, ridge along the length.
    pitch = 28.0
    half = d / 2 + 20
    rise = half * math.tan(math.radians(pitch))
    slope = half / math.cos(math.radians(pitch))
    for side in (-1, 1):
        box("Con_Roof%d" % side, (cx, cy + side * half / 2, top + rise / 2), (w + 30, slope, 3), glass, f, rot=(side * pitch, 0, 0), shadow=False)
        for k in range(int(w // 110) + 1):
            x = x0 + k * w / int(w // 110)
            box("Con_Rib%d_%d" % (side, k), (x, cy + side * half / 2, top + rise / 2 + 3), (6, slope, 8), iron, f, rot=(side * pitch, 0, 0))
    box("Con_Ridge", (cx, cy, top + rise + 4), (w + 30, 10, 10), iron, f)
    # Inside: what a case can search, and the plants.
    rng = random.Random(41)
    frame = BM.Frame(cx, cy, 0)
    furnish("Con", frame, "desk", -300, 250, 0, "the potting bench", m, rng)
    furnish("Con2", frame, "cupboard", 330, 255, 0, "the seed cabinet", m, rng)
    furnish("Con3", frame, "sofa", -410, -120, 90, "the wicker settee", m, rng)
    furnish("Con4", frame, "wastebasket", 400, -230, 0, "the watering cans", m, rng)
    furnish("Con5", frame, "window", 20, 330, 0, "the back panes", m, rng)
    for i, (px, py, kind) in enumerate(((-420, 280, "fern_02"), (-180, 290, "shrub_02"), (150, 300, "fern_02"),
                                        (460, 120, "shrub_02"), (-460, 60, "fern_02"))):
        place_on(models(kind)[i % len(models(kind))], cx + px, cy + py, 0, rng.uniform(0, 360), 1.1, f, "Con_Plant%d" % i)
    lamp = L.practical("Con_Lantern", (cx, cy, 250), 5.0, 900, 2400, source=8)
    lamp.get_owner().set_folder_path("Grounds/Lights")
    box("Con_LanternBox", (cx, cy, 262), (20, 20, 26), m["shade"], f, shadow=False)
    case_room("Conservatory", G.CONSERVATORY, ["Conservatory"], True, (-40, -60, -20),
              [(-150, -200, 0), (200, -140, 180)], "the conservatory")


def build_chapel(stone, roof, glow, m):
    """A small stone chapel, steep slate roof, lancet windows faintly lit, candles on the altar."""
    import build_manor as BM
    x0, y0, x1, y1 = G.CHAPEL
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    w, d = x1 - x0, y1 - y0
    f = "Grounds/Outbuildings/Chapel"
    h, t = 460.0, 40.0
    door_w, door_h = 170.0, 280.0
    box("Chp_Floor", (cx, cy, -4), (w, d, 12), stone, f)
    box("Chp_WallE", (x1, cy, h / 2), (t, d + t, h), stone, f)
    box("Chp_WallW", (x0, cy, h / 2), (t, d + t, h), stone, f)
    box("Chp_WallN", (cx, y1, h / 2), (w, t, h), stone, f)
    for side in (-1, 1):
        seg = (w - door_w) / 2
        box("Chp_WallS%d" % side, (cx + side * (door_w / 2 + seg / 2), y0, h / 2), (seg, t, h), stone, f)
    box("Chp_DoorHead", (cx, y0, (door_h + h) / 2), (door_w, t, h - door_h), stone, f)
    # Steep roof, ridge north-south, and gable ends stepped in stone.
    pitch = 48.0
    half = w / 2 + 40
    rise = half * math.tan(math.radians(pitch))
    slope = half / math.cos(math.radians(pitch))
    for side in (-1, 1):
        box("Chp_Roof%d" % side, (cx + side * half / 2, cy, h + rise / 2), (slope, d + 80, 24), roof,
            f, rot=(0, -side * pitch, 0))
    for end, y in (("N", y1), ("S", y0)):
        for k in range(8):
            frac = (k + 0.5) / 8
            box("Chp_Gable%s%d" % (end, k), (cx, y, h + rise * frac), (2 * half * (1 - frac), t, rise / 8 + 2), stone, f)
    box("Chp_Bellcote", (cx, y0, h + rise + 60), (70, 50, 120), stone, f)
    # Lancet windows down both sides, a faint glow behind them.
    for side, x in ((1, x1 + t / 2 + 1), (-1, x0 - t / 2 - 1)):
        for k, wy in enumerate((cy - 350, cy, cy + 350)):
            box("Chp_Lancet%d_%d" % (side, k), (x, wy, 250), (3, 60, 200), glow, f, shadow=False)
    # Inside: pews either side of the aisle, the lectern, the vestry cupboard, the east window.
    rng = random.Random(77)
    frame = BM.Frame(cx, cy, 0)
    furnish("Chp", frame, "rug", 0, -150, 90, "the aisle runner", m, rng)
    furnish("Chp2", frame, "sofa", -220, -250, 0, "the pews", m, rng)
    for k, py in enumerate((-450, -50, 150)):
        for side in (-1, 1):
            if side < 0 and py == -250:
                continue
            box("Chp_Pew%d_%d" % (k, side), (cx + side * 220, cy + py, 45), (220, 50, 10), m["wood"], f)
            box("Chp_PewBack%d_%d" % (k, side), (cx + side * 220, cy + py + 25, 80), (220, 6, 70), m["wood"], f)
    furnish("Chp3", frame, "desk", 0, 380, 180, "the lectern", m, rng)
    furnish("Chp4", frame, "cupboard", 300, 560, 0, "the vestry cupboard", m, rng)
    furnish("Chp5", frame, "window", -150, 620, 0, "the stained-glass window", m, rng)
    box("Chp_Altar", (cx, cy + 560, 50), (200, 70, 100), stone, f)
    for k, ax in enumerate((-70, 70)):
        c = L.practical("Chp_Candle%d" % k, (cx + ax, cy + 560, 128), 2.5, 600, 1800, source=1.5)
        c.get_owner().set_folder_path("Grounds/Lights")
        box("Chp_CandleWax%d" % k, (cx + ax, cy + 560, 112), (4, 4, 22), m["candle"], f, shadow=False)
    case_room("Chapel", G.CHAPEL, ["Chapel"], False, None,
              [(-120, -300, 90), (150, 0, 180), (-200, 250, 0)], "the chapel")


def build_bounds(stone_mat):
    """Invisible walls: around the play area, and just inside the lake's shore."""
    x0, y0, x1, y1 = G.PLAY
    for label, c, s in (("BoundN", ((x0 + x1) / 2, y1, 1500), (x1 - x0, 60, 4000)),
                        ("BoundS", ((x0 + x1) / 2, y0, 1500), (x1 - x0, 60, 4000)),
                        ("BoundW", (x0, (y0 + y1) / 2, 1500), (60, y1 - y0, 4000)),
                        ("BoundE", (x1, (y0 + y1) / 2, 1500), (60, y1 - y0, 4000))):
        a = box(label, c, s, stone_mat, "Grounds/Bounds", shadow=False)
        a.set_actor_hidden_in_game(True)
    # The shore: a ring of blocks where the bank meets the water (the pier is walked out on).
    n = 64
    pts = []
    for k in range(n):
        ang = 2 * math.pi * k / n
        r = 1.0
        for _ in range(30):  # walk out from the centre to the shoreline
            x = G.LAKE_C[0] + math.cos(ang) * G.LAKE_R[0] * r
            y = G.LAKE_C[1] + math.sin(ang) * G.LAKE_R[1] * r
            if G.lake_edge(x, y) > -120.0:
                break
            r += 0.02
        pts.append((x, y))
    px, py, _ = G.PIER
    for k in range(n):
        (ax, ay), (bx, by) = pts[k], pts[(k + 1) % n]
        mx, my = (ax + bx) / 2, (ay + by) / 2
        if math.hypot(mx - px, my - py) < 260.0:
            continue
        yaw = math.degrees(math.atan2(by - ay, bx - ax))
        a = box("Shore%d" % k, (mx, my, G.WATER_Z + 150), (math.hypot(bx - ax, by - ay) + 20, 30, 400), stone_mat,
                "Grounds/Bounds", rot=(0, 0, yaw), shadow=False)
        a.set_actor_hidden_in_game(True)


def build_scatter():
    points = G.all_scatter()
    scatter = actors.spawn_actor_from_class(unreal.NoirScatter, unreal.Vector(0, 0, 0))
    scatter.set_actor_label("Woods")
    scatter.set_folder_path("Grounds")
    sets = []
    total = 0
    rng = random.Random(7)
    for kind, pts in points.items():
        variants = models(kind)
        if kind == "grass_medium_01":  # the tiny tufts vanish at this spacing
            variants = [v for v in variants if "_tiny_" not in v.get_name()]
        buckets = [[] for _ in variants]
        for x, y, z, yaw, k in pts:
            i = rng.randrange(len(variants))
            loc, rot, sc = stand(variants[i], x, y, z, yaw, k)
            buckets[i].append(unreal.Transform(unreal.Vector(*loc), unreal.Rotator(*rot), unreal.Vector(*sc)))
        for mesh, transforms in zip(variants, buckets):
            if not transforms:
                continue
            st = unreal.NoirScatterSet()
            st.set_editor_property("mesh", mesh)
            st.set_editor_property("instances", transforms)
            st.set_editor_property("collision", kind in G.BLOCKING)
            st.set_editor_property("cast_shadow", kind != "grass_medium_01")
            st.set_editor_property("cull_distance", G.CULL.get(kind, 0.0))
            st.set_editor_property("case_keep", G.CASE_KEEP.get(kind, 1.0))
            sets.append(st)
        total += len(pts)
    scatter.set_editor_property("sets", sets)
    scatter.rebuild()
    log("scattered %d things in %d sets" % (total, len(sets)))


def build_areas():
    for label, es, (x0, y0, x1, y1) in G.AREAS:
        room = actors.spawn_actor_from_class(unreal.NoirRoom, unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, 400))
        room.set_actor_label("Area_" + label.replace(" ", ""))
        room.set_folder_path("Grounds/Areas")
        room.set_editor_property("room_id", unreal.Name("Area_" + label.replace(" ", "")))
        room.set_editor_property("cast", False)
        room.set_editor_property("outdoors", True)
        room.set_editor_property("fixed_name", label)
        room.set_editor_property("fixed_name_es", es)
        room.get_editor_property("bounds").set_box_extent(unreal.Vector((x1 - x0) / 2, (y1 - y0) / 2, 1200))


def build_lighting(sky_mat, cube):
    """Moonlight through cloud, the dim sky, and fog that lies in the hollows."""
    moon = L.light(unreal.DirectionalLight, "Moon", (0, 0, 2000), rot=(0, -32, 140), folder="Grounds/Lights")
    mc = moon.light_component
    mc.set_editor_property("intensity", MOON_LUX)
    mc.set_editor_property("use_temperature", True)
    mc.set_editor_property("temperature", 9500)
    mc.set_editor_property("light_source_angle", 1.5)
    mc.set_editor_property("volumetric_scattering_intensity", 2.0)
    mc.set_editor_property("atmosphere_sun_light", False)
    sphere = place(unreal.load_asset("/Engine/BasicShapes/Sphere"), (0, 0, 0), scale=(2000, 2000, 2000),
                   folder="Grounds/Lights", label="NightSky", material=sky_mat)
    smc = sphere.static_mesh_component
    smc.set_editor_property("cast_shadow", False)
    # A solid 1 km sphere around the world would throw the detective out of it.
    smc.set_collision_profile_name("NoCollision")
    smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    sphere.set_actor_enable_collision(False)
    for a in actors.get_all_level_actors():
        if isinstance(a, unreal.SkyLight):
            sc = a.light_component
            if cube:
                sc.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
                sc.set_editor_property("cubemap", cube)
            sc.set_editor_property("intensity", SKY_LIGHT)
            sc.set_editor_property("light_color", unreal.Color(170, 190, 255, 255))
        if isinstance(a, unreal.ExponentialHeightFog):
            fog = a.component
            fog.set_editor_property("fog_density", 0.035)
            fog.set_editor_property("fog_height_falloff", 0.35)
            fog.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.0015, 0.002, 0.0028, 1))
            fog.set_editor_property("volumetric_fog_scattering_distribution", 0.6)
            fog.set_editor_property("volumetric_fog_extinction_scale", 1.2)


def main():
    stage = sys.argv[1] if len(sys.argv) > 1 else "all"
    L.levels.load_level(MAP_PATH)
    masks = import_masks()
    terrain_mat = build_terrain_material(masks["T_Grounds_MaskA"], masks["T_Grounds_MaskB"])
    world = build_world_pbr()
    stone = world_instance(world, "MI_Manor_Stone", "old_stone_wall", 220.0, (0.75, 0.72, 0.68))
    roof = world_instance(world, "MI_Manor_Slate", "grey_roof_tiles_02", 200.0, (0.55, 0.58, 0.62))
    wall = world_instance(world, "MI_Garden_Wall", "mossy_stone_wall", 200.0)
    glow = build_simple("M_Grounds_WindowGlow", (0.3, 0.18, 0.08), 0.4, emissive=(1.6, 0.75, 0.25))
    wood = unreal.load_asset(L.MAT_DIR + "/MI_Wood_Dark") or wall
    water = build_water()
    sky, cube = build_sky()
    if stage == "materials":  # just the materials: the level keeps what it has
        log("materials rebuilt")
        return
    if stage == "scatter":  # just the woods
        L.levels.load_level(MAP_PATH)
        actors.destroy_actors([a for a in actors.get_all_level_actors() if isinstance(a, unreal.NoirScatter)])
        build_scatter()
        L.levels.save_current_level()
        log("woods rebuilt")
        return
    tiles = build_terrain_tiles(terrain_mat)

    clear_previous()
    for i, sm in enumerate(tiles):
        place(sm, (0, 0, 0), folder="Grounds/Terrain", label=sm.get_name())
    plane = place(unreal.load_asset("/Engine/BasicShapes/Plane"), (G.LAKE_C[0], G.LAKE_C[1], G.WATER_Z),
                  scale=(G.LAKE_R[0] * 2.6 / 100, G.LAKE_R[1] * 2.6 / 100, 1), folder="Grounds/Lake", label="LakeWater", material=water)
    plane.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    build_house_shell(stone, roof, glow, wood)
    garden_walls(wall)
    build_props(wall, stone)
    interior = interior_materials()
    iron = build_simple("M_Grounds_PaintedIron", (0.55, 0.56, 0.54), 0.5)
    tiles = world_instance(world, "MI_Garden_Tiles", "mossy_cobblestone", 160.0, (0.8, 0.8, 0.8))
    build_conservatory(wall, build_glass(), iron, tiles, interior)
    build_chapel(stone, roof, glow, interior)
    build_bounds(stone)
    build_scatter()
    build_areas()
    build_lighting(sky, cube)
    L.levels.save_current_level()
    log("done")


if __name__ == "__main__":
    main()
    unreal.SystemLibrary.quit_editor()
