"""Imports the Poly Haven source art (see fetch_polyhaven.py) into /Game/Environment.

  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript="Scripts/import_polyhaven.py" -nullrhi -unattended

Models go to /Game/Environment/Models/<name> (mesh plus the textures the importer brings along),
ground and wall textures to /Game/Environment/Textures/<name>, the night sky to /Game/Environment/Sky.
Every mesh is built as Nanite; foliage keeps its area as it simplifies. Already imported: skipped,
unless the script is run with "force".
"""
import os
import sys

import unreal

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "SourceArt", "PolyHaven")
DEST = "/Game/Environment"
FORCE = "force" in sys.argv
FOLIAGE = ("fir_", "pine_", "fern", "moss_01", "grass_", "shrub_")

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("[Import] " + msg)


def run(tasks):
    if tasks:
        tools.import_asset_tasks(tasks)


def task(filename, dest, name=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", filename)
    t.set_editor_property("destination_path", dest)
    if name:
        t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    return t


def nanite(mesh, foliage):
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", True)
    if foliage:
        try:
            settings.set_editor_property("shape_preservation", unreal.NaniteShapePreservation.PRESERVE_AREA)
        except Exception:
            pass
    mesh.set_editor_property("nanite_settings", settings)
    eal.save_loaded_asset(mesh)


def import_models():
    for name in sorted(os.listdir(os.path.join(ROOT, "models"))):
        folder = os.path.join(ROOT, "models", name)
        fbx = [f for f in os.listdir(folder) if f.endswith(".fbx")][0]
        dest = "%s/Models/%s" % (DEST, name)
        if eal.does_directory_exist(dest) and eal.list_assets(dest) and not FORCE:
            log("%s already imported" % name)
            continue
        run([task(os.path.join(folder, fbx), dest)])
        meshes = [eal.load_asset(p) for p in eal.list_assets(dest, recursive=True)]
        meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
        for m in meshes:
            nanite(m, name.startswith(FOLIAGE))
            b = m.get_bounds()
            log("%s: %s, %d material slots, size %s" % (name, m.get_name(), len(m.get_editor_property("static_materials")),
                                                       b.box_extent * 2))


def import_textures():
    for name in sorted(os.listdir(os.path.join(ROOT, "textures"))):
        folder = os.path.join(ROOT, "textures", name)
        dest = "%s/Textures/%s" % (DEST, name)
        tasks = []
        for f in sorted(os.listdir(folder)):
            asset = "T_" + os.path.splitext(f)[0]
            if eal.does_asset_exist("%s/%s" % (dest, asset)) and not FORCE:
                continue
            tasks.append(task(os.path.join(folder, f), dest, asset))
        run(tasks)
        for path in eal.list_assets(dest):
            tex = eal.load_asset(path)
            if not isinstance(tex, unreal.Texture2D):
                continue
            n = tex.get_name()
            if n.endswith("_nor_gl"):
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property("srgb", False)
                tex.set_editor_property("flip_green_channel", True)  # OpenGL-style normals
            elif n.endswith("_arm"):
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                tex.set_editor_property("srgb", False)
            eal.save_loaded_asset(tex)
        log("textures %s" % name)


def import_sky():
    folder = os.path.join(ROOT, "hdri")
    for f in os.listdir(folder):
        run([task(os.path.join(folder, f), DEST + "/Sky", "T_" + os.path.splitext(f)[0])])
    for path in eal.list_assets(DEST + "/Sky"):
        a = eal.load_asset(path)
        log("sky: %s (%s)" % (a.get_name(), a.get_class().get_name()))


import_models()
import_textures()
import_sky()
log("done")
