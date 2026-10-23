"""Downloads the outdoor source art from Poly Haven (all CC0) into SourceArt/PolyHaven.

  python Scripts/fetch_polyhaven.py

Models come as FBX with their 2k textures, textures as 2k diffuse / normal (GL) / ARM, the sky as a
2k EXR. Files already downloaded are skipped, so it can be re-run. SourceArt/ is not in git; the
imported assets under Content/Environment are (see import_polyhaven.py).
"""
import json
import os
import sys
import urllib.request

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "SourceArt", "PolyHaven")
HEADERS = {"User-Agent": "NoirMystery-asset-import/1.0"}

MODELS = [
    # The forest (Poly Haven's pine_forest collection).
    "fir_tree_01", "fir_sapling_medium", "fir_sapling", "pine_sapling_small",
    "dead_tree_trunk", "dead_tree_trunk_02", "tree_stump_01", "tree_stump_02",
    "rock_moss_set_01", "rock_moss_set_02", "boulder_01",
    "fern_02", "moss_01", "dry_branches_medium_01", "grass_medium_01", "shrub_02", "shrub_04",
    # The grounds.
    "large_iron_gate", "modular_wooden_pier", "covered_car", "gothic_statue", "painted_wooden_bench", "stone_fire_pit",
]
TEXTURES = [
    "forest_ground_04", "forest_leaves_04", "mud_forest", "leafy_grass", "gravel_road",
    "mossy_cobblestone", "mossy_stone_wall", "old_stone_wall", "grey_roof_tiles_02",
    # The Mind Place: one dark study, in walnut, leather and plaster.
    "dark_paneled_wood", "herringbone_parquet", "plastered_wall_04", "brown_leather",
    "black_walnut_veneer_01", "quatrefoil_jacquard_fabric",
]
# Poly Haven has no cork: the case board's comes from ambientCG (also CC0), unpacked into the same
# layout as the Poly Haven textures (its roughness stands in for the ARM map).
AMBIENTCG = {"cork001": "Cork001"}
HDRIS = ["kloppenheim_07_puresky"]
RES = "2k"


def get_json(url):
    return json.load(urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS)))


def fetch(url, path):
    if os.path.exists(path) and os.path.getsize(path) > 0:
        return 0
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".part"
    with urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS)) as r, open(tmp, "wb") as f:
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)
    os.replace(tmp, path)
    return os.path.getsize(path)


def main():
    total = 0
    for asset in MODELS:
        files = get_json("https://api.polyhaven.com/files/" + asset)
        fbx = (files["fbx"].get(RES) or files["fbx"]["1k"])["fbx"]
        folder = os.path.join(ROOT, "models", asset)
        total += fetch(fbx["url"], os.path.join(folder, os.path.basename(fbx["url"])))
        for rel, inc in fbx.get("include", {}).items():
            total += fetch(inc["url"], os.path.join(folder, *rel.split("/")))
        print("model   %-24s %6.0f MB so far" % (asset, total / 1e6), flush=True)
    for asset in TEXTURES:
        files = get_json("https://api.polyhaven.com/files/" + asset)
        folder = os.path.join(ROOT, "textures", asset)
        for kind in ("Diffuse", "nor_gl", "arm"):
            if kind not in files:
                print("  (%s has no %s)" % (asset, kind))
                continue
            entry = files[kind][RES]
            fmt = "jpg" if "jpg" in entry else "png"
            total += fetch(entry[fmt]["url"], os.path.join(folder, "%s_%s.%s" % (asset, kind, fmt)))
        print("texture %-24s %6.0f MB so far" % (asset, total / 1e6), flush=True)
    import io
    import zipfile
    for name, asset in AMBIENTCG.items():
        folder = os.path.join(ROOT, "textures", name)
        if os.path.isdir(folder) and len(os.listdir(folder)) >= 3:
            continue
        os.makedirs(folder, exist_ok=True)
        data = urllib.request.urlopen(urllib.request.Request(
            "https://ambientcg.com/get?file=%s_2K-JPG.zip" % asset, headers=HEADERS)).read()
        total += len(data)
        z = zipfile.ZipFile(io.BytesIO(data))
        for member, kind in (("_Color.jpg", "Diffuse"), ("_NormalGL.jpg", "nor_gl"), ("_Roughness.jpg", "arm")):
            src = [n for n in z.namelist() if n.endswith(member)]
            if src:
                with open(os.path.join(folder, "%s_%s.jpg" % (name, kind)), "wb") as f:
                    f.write(z.read(src[0]))
        print("texture %-24s %6.0f MB so far (ambientCG)" % (name, total / 1e6), flush=True)
    for asset in HDRIS:
        files = get_json("https://api.polyhaven.com/files/" + asset)
        entry = files["hdri"][RES]["exr"]
        total += fetch(entry["url"], os.path.join(ROOT, "hdri", "%s_%s.exr" % (asset, RES)))
        print("hdri    %-24s %6.0f MB so far" % (asset, total / 1e6), flush=True)
    print("done: %.0f MB downloaded" % (total / 1e6))


if __name__ == "__main__":
    sys.exit(main())
