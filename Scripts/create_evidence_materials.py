import unreal

eal = unreal.EditorAssetLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary

MAT_DIR = "/Game/Materials"
master = unreal.load_asset(f"{MAT_DIR}/M_Master")

def make_pbr_mi(name, base_color_tex, normal_tex, orm_tex, uv_tiling=1.0, emissive=(0,0,0)):
    path = f"{MAT_DIR}/{name}"
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = assets.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    
    mel.set_material_instance_parent(mi, master)
    mel.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(1, 1, 1, 1))
    mel.set_material_instance_vector_parameter_value(mi, "Emissive", unreal.LinearColor(*emissive, 1))
    
    t_bc = unreal.load_asset(f"/Game/Art/Textures/{base_color_tex}")
    if t_bc: mel.set_material_instance_texture_parameter_value(mi, "T2D_BaseColor", t_bc)
    
    t_norm = unreal.load_asset(f"/Game/Art/Textures/{normal_tex}")
    if t_norm: mel.set_material_instance_texture_parameter_value(mi, "T2D_Normal", t_norm)
    
    t_orm = unreal.load_asset(f"/Game/Art/Textures/{orm_tex}")
    if t_orm: mel.set_material_instance_texture_parameter_value(mi, "T2D_ORM", t_orm)
    
    if uv_tiling != 1.0:
        mel.set_material_instance_scalar_parameter_value(mi, "UV_Tiling", uv_tiling)
    
    eal.save_loaded_asset(mi)
    print(f"Created/Updated PBR Material Instance: {path}")
    return mi

make_pbr_mi("MI_WatchDial", "T_WatchDial_BaseColor", "T_WatchDial_Normal", "T_WatchDial_ORM", 1.0)
make_pbr_mi("MI_Letter", "T_Letter_BaseColor", "T_Letter_Normal", "T_Letter_ORM", 1.0)
make_pbr_mi("MI_Yarn", "T_Yarn_BaseColor", "T_Yarn_Normal", "T_Yarn_ORM", 4.0)

print("Evidence PBR Material Instances created successfully!")
