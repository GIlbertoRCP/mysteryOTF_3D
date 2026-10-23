"""The manor's weather: fog in layers and rain (see NoirWeather.h). Run after build_grounds.py.

  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript="Scripts/build_weather.py" -unattended

Makes, in /Game/Environment/Weather:
  T_FogNoise      tileable cloud noise (generated here) for the wisps;
  MPC_Weather     Wetness and Rain, which NoirWeather drives and the ground, stone and prop
                  materials read (build_grounds.py, materials_polyhaven.py);
  M_Fog_Wisp      soft, lit fog cards: two octaves of drifting noise, soft edges, fading where they
                  meet geometry and close to the camera;
  M_Rain          thin lit streaks;
and places one NoirWeather in L_Manor (replacing the last), and marks the grounds' areas outdoors.
"""
import math
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_grounds  # noqa: E402  (its PNG writer)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = "/Game/Environment/Weather"
MAP_PATH = "/Game/Maps/L_Manor"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(msg):
    unreal.log("[Weather] " + msg)


# ---------------------------------------------------------------- noise

def fog_noise(size=256, seed=11):
    """Tileable fractal value noise, soft and cloudy, 0..255 grey."""
    rng = random.Random(seed)
    octaves = []
    for o in range(5):
        cells = 4 << o
        grid = [[rng.random() for _ in range(cells)] for _ in range(cells)]
        octaves.append((cells, grid, 0.5 ** o))

    def smooth(t):
        return t * t * (3 - 2 * t)

    rows = []
    lo, hi = 1e9, -1e9
    vals = []
    for j in range(size):
        row = []
        for i in range(size):
            v = 0.0
            for cells, grid, amp in octaves:
                x, y = i / size * cells, j / size * cells
                x0, y0 = int(x) % cells, int(y) % cells
                x1, y1 = (x0 + 1) % cells, (y0 + 1) % cells
                fx, fy = smooth(x - int(x)), smooth(y - int(y))
                a = grid[y0][x0] + (grid[y0][x1] - grid[y0][x0]) * fx
                b = grid[y1][x0] + (grid[y1][x1] - grid[y1][x0]) * fx
                v += (a + (b - a) * fy) * amp
            row.append(v)
            lo, hi = min(lo, v), max(hi, v)
        vals.append(row)
    for row in vals:
        rows.append([int(255 * ((v - lo) / (hi - lo)) ** 1.4 + 0.5) for v in row])
    return rows


def import_png(path, name):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tools.import_asset_tasks([t])
    tex = unreal.load_asset("%s/%s" % (DEST, name))
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    eal.save_loaded_asset(tex)
    return tex


# ---------------------------------------------------------------- assets

def parameter_collection():
    path = DEST + "/MPC_Weather"
    mpc = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        "MPC_Weather", DEST, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    params = []
    for name, value in (("Wetness", 0.75), ("Rain", 0.65)):
        p = unreal.CollectionScalarParameter()
        p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", value)
        params.append(p)
    mpc.set_editor_property("scalar_parameters", params)
    unreal.NoirScriptLibrary.fix_collection_ids(mpc)  # Python-made parameters come without ids
    eal.save_loaded_asset(mpc)
    return mpc


def new_material(name):
    path = "%s/%s" % (DEST, name)
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())


def node(m, cls, x, y, **props):
    n = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def link(a, out, b, inp):
    if not mel.connect_material_expressions(a, out, b, inp):
        log("could not connect %s.%s -> %s.%s" % (a.get_class().get_name(), out, b.get_class().get_name(), inp))


def const(m, v, x, y):
    return node(m, unreal.MaterialExpressionConstant, x, y, r=v)


def mul(m, a, b, x, y, a_out="", b_out=""):
    n = node(m, unreal.MaterialExpressionMultiply, x, y)
    link(a, a_out, n, "A")
    link(b, b_out, n, "B")
    return n


def translucent(m):
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    try:
        m.set_editor_property("apply_fogging", True)
    except Exception:
        pass


def camera_fade(m, near0, near1, x, y):
    """0 at near0 from the camera, 1 from near1 on."""
    cam = node(m, unreal.MaterialExpressionCameraPositionWS, x - 400, y)
    wp = node(m, unreal.MaterialExpressionWorldPosition, x - 400, y + 100)
    dist = node(m, unreal.MaterialExpressionDistance, x - 250, y + 50)
    link(cam, "", dist, "A")
    link(wp, "", dist, "B")
    sub = node(m, unreal.MaterialExpressionSubtract, x - 120, y + 50)
    link(dist, "", sub, "A")
    link(const(m, near0, x - 250, y + 150), "", sub, "B")
    div = node(m, unreal.MaterialExpressionDivide, x, y + 50)
    link(sub, "", div, "A")
    link(const(m, near1 - near0, x - 120, y + 150), "", div, "B")
    sat = node(m, unreal.MaterialExpressionSaturate, x + 120, y + 50)
    link(div, "", sat, "")
    return sat


def build_wisp(noise):
    m = new_material("M_Fog_Wisp")
    translucent(m)
    # Two octaves of noise drifting at different speeds, offset per card so no two match.
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1600, -300)
    rnd = node(m, unreal.MaterialExpressionPerInstanceRandom, -1600, -150)
    octaves = []
    for i, (scale, speed) in enumerate(((0.6, (0.012, 0.004)), (1.7, (-0.02, 0.009)))):
        s = mul(m, uv, const(m, scale, -1600, -50 + 120 * i), -1450, -250 + 120 * i)
        off = node(m, unreal.MaterialExpressionAdd, -1300, -250 + 120 * i)
        link(s, "", off, "A")
        link(mul(m, rnd, const(m, 7.31 + 3 * i, -1600, 60 + 120 * i), -1450, -150 + 120 * i), "", off, "B")
        pan = node(m, unreal.MaterialExpressionPanner, -1150, -250 + 120 * i, speed_x=speed[0], speed_y=speed[1])
        link(off, "", pan, "Coordinate")
        smp = node(m, unreal.MaterialExpressionTextureSample, -1000, -300 + 160 * i, texture=noise,
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
        link(pan, "", smp, "UVs")
        octaves.append(smp)
    cloud = mul(m, octaves[0], octaves[1], -800, -250, "R", "R")
    cloud2 = mul(m, cloud, const(m, 2.2, -950, -100), -650, -250)
    # Soft round edges: 1 at the card's middle, 0 at its rim.
    centre = node(m, unreal.MaterialExpressionConstant2Vector, -1300, 200, r=0.5, g=0.5)
    d = node(m, unreal.MaterialExpressionDistance, -1150, 150)
    link(uv, "", d, "A")
    link(centre, "", d, "B")
    edge = node(m, unreal.MaterialExpressionOneMinus, -1000, 150)
    link(mul(m, d, const(m, 2.0, -1150, 250), -1050, 250), "", edge, "")
    edge_s = node(m, unreal.MaterialExpressionSaturate, -900, 150)
    link(edge, "", edge_s, "")
    edge_p = node(m, unreal.MaterialExpressionPower, -780, 150)
    link(edge_s, "", edge_p, "Base")
    link(const(m, 1.6, -900, 250), "", edge_p, "Exp")
    # Fade where the card meets the ground or a wall, and near the camera.
    depth = node(m, unreal.MaterialExpressionDepthFade, -780, 350)
    link(const(m, 1.0, -950, 350), "", depth, "Opacity")
    link(const(m, 250.0, -950, 430), "", depth, "FadeDistance")
    near = camera_fade(m, 120.0, 650.0, -780, 500)
    opacity = node(m, unreal.MaterialExpressionScalarParameter, -650, 0, parameter_name="Opacity", default_value=0.32)
    a = mul(m, cloud2, edge_p, -500, -150)
    a = mul(m, a, depth, -380, -50)
    a = mul(m, a, near, -260, 50)
    a = mul(m, a, opacity, -140, 100)
    clamp = node(m, unreal.MaterialExpressionSaturate, -20, 100)
    link(a, "", clamp, "")
    mel.connect_material_property(clamp, "", unreal.MaterialProperty.MP_OPACITY)
    col = node(m, unreal.MaterialExpressionVectorParameter, -300, -350, parameter_name="Color",
               default_value=unreal.LinearColor(0.62, 0.64, 0.68, 1))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = node(m, unreal.MaterialExpressionVectorParameter, -300, -200, parameter_name="Glow",
                default_value=unreal.LinearColor(0.0012, 0.0014, 0.0018, 1))
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def build_rain(mpc):
    m = new_material("M_Rain")
    translucent(m)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    v = node(m, unreal.MaterialExpressionComponentMask, -1050, 0, r=False, g=True, b=False, a=False)
    link(uv, "", v, "")
    # Brightest mid-streak, fading to nothing at either end.
    centred = node(m, unreal.MaterialExpressionSubtract, -900, 0)
    link(v, "", centred, "A")
    link(const(m, 0.5, -1050, 100), "", centred, "B")
    a = node(m, unreal.MaterialExpressionAbs, -780, 0)
    link(centred, "", a, "")
    ends = node(m, unreal.MaterialExpressionOneMinus, -660, 0)
    link(mul(m, a, const(m, 2.0, -780, 100), -700, 80), "", ends, "")
    near = camera_fade(m, 40.0, 220.0, -800, 300)
    rain = node(m, unreal.MaterialExpressionCollectionParameter, -800, 600)
    unreal.NoirScriptLibrary.bind_collection_parameter(rain, mpc, "Rain")  # name and id
    opacity = node(m, unreal.MaterialExpressionScalarParameter, -650, 200, parameter_name="Opacity", default_value=0.22)
    # Light rain is fainter as well as sparser.
    heavy = node(m, unreal.MaterialExpressionSaturate, -560, 600)
    link(mul(m, rain, const(m, 1.6, -800, 700), -680, 650), "", heavy, "")
    o = mul(m, ends, near, -500, 50)
    o = mul(m, o, opacity, -380, 100)
    o = mul(m, o, heavy, -260, 150)
    mel.connect_material_property(o, "", unreal.MaterialProperty.MP_OPACITY)
    col = node(m, unreal.MaterialExpressionConstant3Vector, -300, -200, constant=unreal.LinearColor(0.7, 0.74, 0.8, 1))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = node(m, unreal.MaterialExpressionConstant3Vector, -300, -80, constant=unreal.LinearColor(0.004, 0.0045, 0.005, 1))
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


# ---------------------------------------------------------------- the level

def place_weather(wisp, rain, mpc):
    levels.load_level(MAP_PATH)
    old = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.NoirWeather)]
    if old:
        actors.destroy_actors(old)
    w = actors.spawn_actor_from_class(unreal.NoirWeather, unreal.Vector(0, 0, 500))
    w.set_actor_label("Weather")
    w.set_folder_path("Grounds")
    w.set_editor_property("wisp_material", wisp)
    w.set_editor_property("rain_material", rain)
    w.set_editor_property("parameters", mpc)
    n = 0
    for a in actors.get_all_level_actors():
        if isinstance(a, unreal.NoirRoom) and a.get_actor_label().startswith("Area_"):
            a.set_editor_property("outdoors", True)
            n += 1
    levels.save_current_level()
    log("placed the weather; %d areas marked outdoors" % n)


def main():
    path = os.path.join(ROOT, "SourceArt", "Generated", "T_FogNoise.png")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if not os.path.exists(path):
        rows = fog_noise()
        gen_grounds.write_png(path, len(rows[0]), len(rows), rows, 1)
    noise = import_png(path, "T_FogNoise")
    mpc = parameter_collection()
    wisp = build_wisp(noise)
    rain = build_rain(mpc)
    place_weather(wisp, rain, mpc)
    log("done")


if __name__ == "__main__":
    main()
    unreal.SystemLibrary.quit_editor()
