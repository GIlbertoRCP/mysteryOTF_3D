"""Gives the imported Poly Haven models proper materials (run after import_polyhaven.py).

  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript="Scripts/materials_polyhaven.py" -unattended

The importer's own materials are FBX Phong stand-ins (roughness read as shininess, no alpha, and
for some models no textures at all). This imports each model's textures from SourceArt, builds two
parents in /Game/Environment/Materials (M_PH_Surface, opaque; M_PH_Foliage, masked and two-sided
with light through the leaves) and gives every slot of every mesh an instance with the textures of
its part (bark, twig, planks...). Poly Haven's normals are OpenGL-style: their green is flipped.
"""
import os
import re

import unreal

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "SourceArt", "PolyHaven", "models")
ENV = "/Game/Environment"
MATS = ENV + "/Materials"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MAPS = ("diff", "nor_gl", "rough", "metal", "alpha")


WEATHER_MPC = "/Game/Environment/Weather/MPC_Weather"


def wet(m, base, rough, x, y, base_out="", rough_out=""):
    """Darker and glossier with the weather's Wetness (Scripts/build_weather.py makes the
    collection); returns the (base colour, roughness) to plug in. Unchanged if there is none."""
    mpc = unreal.load_asset(WEATHER_MPC) if unreal.EditorAssetLibrary.does_asset_exist(WEATHER_MPC) else None
    if not mpc:
        return (base, base_out), (rough, rough_out)
    w = mel.create_material_expression(m, unreal.MaterialExpressionCollectionParameter, x, y)
    unreal.NoirScriptLibrary.bind_collection_parameter(w, mpc, "Wetness")  # name and id
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


def log(msg):
    unreal.log("[PHMat] " + msg)


def import_texture(path, dest):
    stem = os.path.splitext(os.path.basename(path))[0]
    asset = "%s/T_%s" % (dest, stem)
    if not eal.does_asset_exist(asset):
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", path)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("destination_name", "T_" + stem)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tools.import_asset_tasks([t])
    tex = unreal.load_asset(asset)
    if "_nor_gl" in stem:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)
    elif "_diff" not in stem:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
        tex.set_editor_property("srgb", False)
    eal.save_loaded_asset(tex)
    return tex


def textures_by_part(name):
    """{part: {map: texture}} for a model; the part is '' when the model has one set."""
    folder = os.path.join(ROOT, name, "textures")
    dest = "%s/Models/%s/Textures" % (ENV, name)
    parts = {}
    for f in sorted(os.listdir(folder)):
        m = re.match(r"^%s_?(.*?)_?(%s)_\d+k\.\w+$" % (re.escape(name), "|".join(MAPS)), f)
        if not m:
            continue
        parts.setdefault(m.group(1), {})[m.group(2)] = import_texture(os.path.join(folder, f), dest)
    return parts


def new_material(name):
    path = "%s/%s" % (MATS, name)
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return tools.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())


def tparam(m, name, x, y, default, kind):
    n = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("texture", unreal.load_asset(default))
    n.set_editor_property("sampler_type", {
        "color": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
        "normal": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
        "gray": unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE}[kind])
    return n


def sparam(m, name, x, y, value):
    n = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", value)
    return n


def vparam(m, name, x, y, value):
    n = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(*value))
    return n


def gray_default(name, value):
    """A 4x4 linear greyscale texture, for the greyscale slots' defaults (an sRGB colour texture
    there fails the sampler check)."""
    import sys as _sys
    _sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import gen_grounds
    path = "%s/%s" % (MATS, name)
    if not eal.does_asset_exist(path):
        png = os.path.join(os.path.dirname(ROOT), "..", "Generated", name + ".png")
        png = os.path.normpath(png)
        os.makedirs(os.path.dirname(png), exist_ok=True)
        gen_grounds.write_png(png, 4, 4, [[value] * 4 for _ in range(4)], 1)
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", png)
        t.set_editor_property("destination_path", MATS)
        t.set_editor_property("destination_name", name)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tools.import_asset_tasks([t])
    tex = unreal.load_asset(path)
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    eal.save_loaded_asset(tex)
    return tex.get_path_name()


def build_parent(name, foliage):
    m = new_material(name)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("used_with_nanite", True)
    if foliage:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property("two_sided", True)
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        m.set_editor_property("opacity_mask_clip_value", 0.4)
    white = "/Engine/EngineResources/WhiteSquareTexture"
    gray_white = gray_default("T_PH_GrayWhite", 255)
    black = gray_default("T_PH_GrayBlack", 0)
    diff = tparam(m, "Diffuse", -800, -300, white, "color")
    tint = vparam(m, "Tint", -800, -100, (1, 1, 1, 1))
    bc = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, -250)
    mel.connect_material_expressions(diff, "RGB", bc, "A")
    mel.connect_material_expressions(tint, "", bc, "B")
    if foliage:
        mel.connect_material_property(bc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    nor = tparam(m, "Normal", -800, 50, "/Engine/EngineMaterials/DefaultNormal", "normal")
    mel.connect_material_property(nor, "RGB", unreal.MaterialProperty.MP_NORMAL)
    rough = tparam(m, "Roughness", -800, 300, gray_white, "gray")
    rs = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, 300)
    mel.connect_material_expressions(rough, "R", rs, "A")
    mel.connect_material_expressions(sparam(m, "RoughnessScale", -800, 500, 1.0), "", rs, "B")
    if foliage:
        mel.connect_material_property(rs, "", unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        (wb, wbo), (wr, wro) = wet(m, bc, rs, -350, 400)
        mel.connect_material_property(wb, wbo, unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(wr, wro, unreal.MaterialProperty.MP_ROUGHNESS)
    if foliage:
        alpha = tparam(m, "Alpha", -800, 650, gray_white, "gray")
        mel.connect_material_property(alpha, "R", unreal.MaterialProperty.MP_OPACITY_MASK)
        # Light through the needles: the leaf colour, darkened.
        sss = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, 0)
        mel.connect_material_expressions(bc, "", sss, "A")
        mel.connect_material_expressions(sparam(m, "Translucency", -800, 850, 0.35), "", sss, "B")
        mel.connect_material_property(sss, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    else:
        metal = tparam(m, "Metallic", -800, 650, black, "gray")
        mel.connect_material_property(metal, "R", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def pick_part(slot_part, parts):
    if slot_part in parts:
        return slot_part
    leafy = any(w in slot_part for w in ("twig", "leaf", "needle", "foliage"))
    for p in parts:
        if leafy and "twig" in p:
            return p
    for p in parts:  # trunks, dead branches and the like wear the bark
        if any(w in p for w in ("bark", "branches", "poles")) and "twig" not in p:
            return p
    return sorted(parts)[0]


def main():
    surface = build_parent("M_PH_Surface", False)
    foliage = build_parent("M_PH_Foliage", True)
    for name in sorted(os.listdir(ROOT)):
        parts = textures_by_part(name)
        if not parts:
            log("%s: no textures" % name)
            continue
        dest = "%s/Models/%s" % (ENV, name)
        made = {}
        for p in eal.list_assets(dest, recursive=False):
            mesh = unreal.load_asset(p)
            if not isinstance(mesh, unreal.StaticMesh):
                continue
            mesh.modify()
            slots = mesh.get_editor_property("static_materials")
            for i, slot in enumerate(slots):
                slot_part = str(slot.material_slot_name)
                slot_part = slot_part[len(name):].lstrip("_") if slot_part.startswith(name) else slot_part
                part = pick_part(slot_part, parts)
                if part not in made:
                    maps = parts[part]
                    leaf = "alpha" in maps
                    mi_name = "MI_%s%s" % (name, "_" + part if part else "")
                    mi_path = "%s/%s" % (dest, mi_name)
                    mi = unreal.load_asset(mi_path) if eal.does_asset_exist(mi_path) else tools.create_asset(
                        mi_name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
                    mi.set_editor_property("parent", foliage if leaf else surface)
                    for key, param in (("diff", "Diffuse"), ("nor_gl", "Normal"), ("rough", "Roughness"),
                                       ("metal", "Metallic"), ("alpha", "Alpha")):
                        if key in maps:
                            mel.set_material_instance_texture_parameter_value(mi, param, maps[key])
                    eal.save_loaded_asset(mi)
                    made[part] = mi
                mesh.set_material(i, made[part])  # writing static_materials back does not stick
            eal.save_loaded_asset(mesh)
        log("%s: %s" % (name, ", ".join("%s%s" % (k or "(one set)", " [leaf]" if "alpha" in parts[k] else "") for k in made)))
    log("done")


main()
