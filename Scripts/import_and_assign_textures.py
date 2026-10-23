import os
import glob
import unreal

print("=== STARTING PBR TEXTURE IMPORT AND MATERIAL ASSIGNMENT ===")

TEXTURES_SRC = "B:/mysteryOTF/Content/Art/Textures"
DEST_PATH = "/Game/Art/Textures"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# Ensure directory exists in Unreal
if not eal.does_directory_exist(DEST_PATH):
    eal.make_directory(DEST_PATH)

png_files = glob.glob(os.path.join(TEXTURES_SRC, "*.png"))
print(f"Found {len(png_files)} PNG files to import...")

tasks = []
for file_path in png_files:
    file_path = file_path.replace("\\", "/")
    asset_name = os.path.splitext(os.path.basename(file_path))[0]
    
    task = unreal.AssetImportTask()
    task.filename = file_path
    task.destination_path = DEST_PATH
    task.destination_name = asset_name
    task.replace_existing = True
    task.automated = True
    task.save = True
    tasks.append(task)

asset_tools.import_asset_tasks(tasks)
print("Import tasks completed. Configuring texture settings...")

loaded_textures = {}
for task in tasks:
    asset_path = f"{DEST_PATH}/{task.destination_name}"
    tex = unreal.load_asset(asset_path)
    if not tex:
        print(f"Warning: Could not load imported texture {asset_path}")
        continue
    
    name = task.destination_name
    # Set correct compression and sRGB flags
    if "Normal" in name:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
    elif "ORM" in name:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    else: # BaseColor
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property("srgb", True)
    
    eal.save_loaded_asset(tex)
    loaded_textures[name] = tex
    print(f"Configured and saved: {asset_path}")

print(f"Successfully loaded and configured {len(loaded_textures)} textures!")

# ==============================================================================
# ASSIGN TEXTURES TO MATERIAL INSTANCES
# ==============================================================================

assignments = {
    "/Game/Materials/MI_Floor_DarkWood": {
        "BaseColor": "T_WoodFloor_BaseColor",
        "Normal": "T_WoodFloor_Normal",
        "ORM": "T_WoodFloor_ORM",
        "UV_Tiling": 3.0,
    },
    "/Game/Materials/MI_Wood_Dark": {
        "BaseColor": "T_WoodFloor_BaseColor",
        "Normal": "T_WoodFloor_Normal",
        "ORM": "T_WoodFloor_ORM",
        "UV_Tiling": 2.0,
    },
    "/Game/Materials/MI_Wall_Oxblood": {
        "BaseColor": "T_Wall_BaseColor",
        "Normal": "T_Wall_Normal",
        "ORM": "T_Wall_ORM",
        "UV_Tiling": 2.5,
    },
    "/Game/Materials/MI_Wall_Teak": {
        "BaseColor": "T_WoodFloor_BaseColor",
        "Normal": "T_WoodFloor_Normal",
        "ORM": "T_WoodFloor_ORM",
        "UV_Tiling": 2.0,
    },
    "/Game/Materials/MI_Ground_Wet": {
        "BaseColor": "T_Ground_BaseColor",
        "Normal": "T_Ground_Normal",
        "ORM": "T_Ground_ORM",
        "UV_Tiling": 3.0,
    },
    "/Game/Materials/MI_Ground_Path": {
        "BaseColor": "T_Ground_BaseColor",
        "Normal": "T_Ground_Normal",
        "ORM": "T_Ground_ORM",
        "UV_Tiling": 2.5,
    },
    "/Game/Materials/MI_Rug_Oxblood": {
        "BaseColor": "T_Rug_BaseColor",
        "Normal": "T_Rug_Normal",
        "ORM": "T_Rug_ORM",
        "UV_Tiling": 1.0,
    },
    "/Game/Materials/MI_Ceiling_Plaster": {
        "BaseColor": "T_Ceiling_BaseColor",
        "Normal": "T_Ceiling_Normal",
        "ORM": "T_Ceiling_ORM",
        "UV_Tiling": 4.0,
    },
    "/Game/Materials/MI_Corkboard": {
        "BaseColor": "T_Corkboard_BaseColor",
        "Normal": "T_Corkboard_Normal",
        "ORM": "T_Corkboard_ORM",
        "UV_Tiling": 2.0,
    },
    "/Game/Materials/MI_Leather_Green": {
        "BaseColor": "T_Leather_BaseColor",
        "Normal": "T_Leather_Normal",
        "ORM": "T_Leather_ORM",
        "UV_Tiling": 2.0,
    },
    "/Game/Materials/MI_Book_Oxblood": {
        "BaseColor": "T_Leather_BaseColor",
        "Normal": "T_Leather_Normal",
        "ORM": "T_Leather_ORM",
        "UV_Tiling": 3.0,
    },
    "/Game/Materials/MI_Book_Green": {
        "BaseColor": "T_Leather_BaseColor",
        "Normal": "T_Leather_Normal",
        "ORM": "T_Leather_ORM",
        "UV_Tiling": 3.0,
    },
    "/Game/Materials/MI_Book_Brown": {
        "BaseColor": "T_Leather_BaseColor",
        "Normal": "T_Leather_Normal",
        "ORM": "T_Leather_ORM",
        "UV_Tiling": 3.0,
    },
    "/Game/Materials/MI_Book_Navy": {
        "BaseColor": "T_Leather_BaseColor",
        "Normal": "T_Leather_Normal",
        "ORM": "T_Leather_ORM",
        "UV_Tiling": 3.0,
    },
    "/Game/Materials/MI_Book_Cream": {
        "BaseColor": "T_Leather_BaseColor",
        "Normal": "T_Leather_Normal",
        "ORM": "T_Leather_ORM",
        "UV_Tiling": 3.0,
    },
    "/Game/Materials/MI_Suspect_Margaret": {
        "BaseColor": "T_Fabric_Wool_BaseColor",
        "Normal": "T_Fabric_Wool_Normal",
        "ORM": "T_Fabric_Wool_ORM",
        "UV_Tiling": 2.5,
    },
    "/Game/Materials/MI_Suspect_Edmund": {
        "BaseColor": "T_Fabric_Wool_BaseColor",
        "Normal": "T_Fabric_Wool_Normal",
        "ORM": "T_Fabric_Wool_ORM",
        "UV_Tiling": 2.5,
    },
    "/Game/Materials/MI_Wall_Baize": {
        "BaseColor": "T_Fabric_Wool_BaseColor",
        "Normal": "T_Fabric_Wool_Normal",
        "ORM": "T_Fabric_Wool_ORM",
        "UV_Tiling": 3.0,
    },
    "/Game/Materials/MI_Skin_Placeholder": {
        "BaseColor": "T_Skin_BaseColor",
        "Normal": "T_Skin_Normal",
        "ORM": "T_Skin_ORM",
        "UV_Tiling": 1.0,
    },
    "/Game/Materials/MI_Canvas_Murky": {
        "BaseColor": "T_Canvas_BaseColor",
        "Normal": "T_Canvas_Normal",
        "ORM": "T_Canvas_ORM",
        "UV_Tiling": 1.0,
    },
    "/Game/Materials/MI_Brass": {
        "BaseColor": "T_Brass_BaseColor",
        "Normal": "T_Brass_Normal",
        "ORM": "T_Brass_ORM",
        "UV_Tiling": 1.5,
    },
    "/Game/Materials/MI_Frame_Gilt": {
        "BaseColor": "T_Brass_BaseColor",
        "Normal": "T_Brass_Normal",
        "ORM": "T_Brass_ORM",
        "UV_Tiling": 2.0,
    },
}

for mi_path, params in assignments.items():
    mi = unreal.load_asset(mi_path)
    if not mi:
        print(f"Warning: Material instance not found: {mi_path}")
        continue
    
    if "BaseColor" in params and params["BaseColor"] in loaded_textures:
        mel.set_material_instance_texture_parameter_value(mi, "T2D_BaseColor", loaded_textures[params["BaseColor"]])
    if "Normal" in params and params["Normal"] in loaded_textures:
        mel.set_material_instance_texture_parameter_value(mi, "T2D_Normal", loaded_textures[params["Normal"]])
    if "ORM" in params and params["ORM"] in loaded_textures:
        mel.set_material_instance_texture_parameter_value(mi, "T2D_ORM", loaded_textures[params["ORM"]])
    if "UV_Tiling" in params:
        mel.set_material_instance_scalar_parameter_value(mi, "UV_Tiling", params["UV_Tiling"])
    
    eal.save_loaded_asset(mi)
    print(f"Assigned PBR textures to: {mi_path}")

# ==============================================================================
# GLASS RAIN DROPLETS
# ==============================================================================
glass_mi = unreal.load_asset("/Game/Materials/MI_Glass_Night")
if glass_mi and "T_RainDroplets_Normal" in loaded_textures:
    mel.set_material_instance_texture_parameter_value(glass_mi, "T2D_Normal", loaded_textures["T_RainDroplets_Normal"])
    mel.set_material_instance_scalar_parameter_value(glass_mi, "UV_Tiling", 1.5)
    eal.save_loaded_asset(glass_mi)
    print("Assigned Rain Droplets Normal to MI_Glass_Night!")

print("=== PBR TEXTURE IMPORT AND ASSIGNMENT FINISHED SUCCESSFULLY! ===")
