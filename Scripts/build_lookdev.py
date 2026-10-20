"""Builds /Game/Maps/L_LookDev: a placeholder room lit for noir look development.

Run headless from the repo root:
  UnrealEditor-Cmd.exe NoirMystery.uproject -ExecutePythonScript=Scripts/build_lookdev.py -unattended -nosplash -nullrhi

Re-running overwrites the level and the M_Master instances, so tune values here
(or tweak in the editor and copy them back) rather than hand-editing twice.
"""
import unreal

MAP_PATH = "/Game/Maps/L_LookDev"
MAT_DIR = "/Game/Materials"
CUBE = "/Engine/BasicShapes/Cube.Cube"

assets = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(msg):
    unreal.log("[LookDev] " + msg)


# ---------------------------------------------------------------- materials

def build_master_material():
    path = MAT_DIR + "/M_Master"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
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
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def make_instance(parent, name, color, roughness, metallic=0.0):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = assets.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant,
                             unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(*color, 1))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    eal.save_loaded_asset(mi)
    return mi


# ---------------------------------------------------------------- level

def block(label, center, size_m, material, folder="Room"):
    """Spawns an engine cube sized in metres (cube is 1 m)."""
    a = actors.spawn_actor_from_object(unreal.load_asset(CUBE), unreal.Vector(*center))
    a.set_actor_scale3d(unreal.Vector(*size_m))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    a.static_mesh_component.set_material(0, material)
    return a


def light(cls, label, loc, rot=(0, 0, 0), folder="Lights"):
    a = actors.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    return a


def kelvin(comp, k):
    comp.set_editor_property("use_temperature", True)
    comp.set_editor_property("temperature", k)


def set_pp(settings, **values):
    """Sets PostProcessSettings fields together with their override_ flags."""
    for key, value in values.items():
        try:
            settings.set_editor_property("override_" + key, True)
            settings.set_editor_property(key, value)
        except Exception as e:  # property names drift between engine versions
            unreal.log_warning("[LookDev] post-process '%s' skipped: %s" % (key, e))


def build_level(mats):
    # Rebuild in place: the editor opens L_LookDev on startup, so it can't be deleted here.
    if eal.does_asset_exist(MAP_PATH):
        levels.load_level(MAP_PATH)
        actors.destroy_actors([a for a in actors.get_all_level_actors()
                               if not isinstance(a, unreal.WorldSettings) and type(a) is not unreal.Brush])
    elif not levels.new_level(MAP_PATH):
        raise RuntimeError("could not create " + MAP_PATH)

    wall, floor, ceiling, wood, brass = (mats[k] for k in ("wall", "floor", "ceiling", "wood", "brass"))

    # Room shell: 8 m x 6 m x 3.3 m, interior x in [-400, 400], y in [-300, 300], z in [0, 330].
    block("Floor", (0, 0, -10), (8.4, 6.4, 0.2), floor)
    block("Ceiling", (0, 0, 340), (8.4, 6.4, 0.2), ceiling)
    block("Wall_South", (0, -310, 165), (8.4, 0.2, 3.5), wall)
    block("Wall_North", (0, 310, 165), (8.4, 0.2, 3.5), wall)
    block("Wall_West", (-410, 0, 165), (0.2, 6.0, 3.5), wall)
    # East wall with a window opening: y in [-100, 100], z in [90, 240].
    block("Wall_East_L", (410, -205, 165), (0.2, 2.1, 3.5), wall)
    block("Wall_East_R", (410, 205, 165), (0.2, 2.1, 3.5), wall)
    block("Wall_East_Sill", (410, 0, 40), (0.2, 2.0, 1.0), wall)
    block("Wall_East_Head", (410, 0, 290), (0.2, 2.0, 1.0), wall)

    # Placeholder clutter so light has something to fall across.
    block("Desk", (-80, -170, 75), (1.6, 0.8, 0.06), wood, "Props")
    for i, (x, y) in enumerate(((-150, -200), (-10, -200), (-150, -140), (-10, -140))):
        block("Desk_Leg%d" % i, (x, y, 36), (0.06, 0.06, 0.72), wood, "Props")
    block("Bookcase", (-150, 285, 110), (2.0, 0.35, 2.2), wood, "Props")
    block("Fireplace", (-385, 80, 60), (0.5, 1.6, 1.2), brass, "Props")
    block("Rug", (0, 30, 1), (3.2, 2.2, 0.01), mats["rug"], "Props")
    block("CaseBoard", (395, -230, 160), (0.04, 1.2, 0.9), wood, "Props")

    # Cold moonlight through the window (teal), for god rays in the fog.
    moon = light(unreal.DirectionalLight, "Moonlight", (0, 0, 600), (0, -28, 190))
    mc = moon.light_component
    mc.set_editor_property("intensity", 0.8)
    mc.set_editor_property("light_color", unreal.Color(170, 205, 200, 255))
    mc.set_editor_property("volumetric_scattering_intensity", 4.0)

    sky = light(unreal.SkyLight, "SkyLight", (0, 0, 500))
    sc = sky.light_component
    sc.set_editor_property("real_time_capture", True)
    sc.set_editor_property("intensity", 0.25)
    sc.set_editor_property("lower_hemisphere_is_black", True)

    # Warm practicals.
    lamp = light(unreal.PointLight, "Practical_DeskLamp", (-40, -170, 125))
    lc = lamp.point_light_component
    lc.set_editor_property("intensity", 12.0)
    lc.set_editor_property("attenuation_radius", 700.0)
    lc.set_editor_property("source_radius", 6.0)
    kelvin(lc, 2400)
    block("DeskLamp_Shade", (-40, -170, 125), (0.25, 0.25, 0.2), brass, "Props")
    block("DeskLamp_Stem", (-40, -170, 100), (0.03, 0.03, 0.45), brass, "Props")

    fire = light(unreal.PointLight, "Practical_Fireplace", (-340, 80, 45))
    fc = fire.point_light_component
    fc.set_editor_property("intensity", 20.0)
    fc.set_editor_property("attenuation_radius", 900.0)
    fc.set_editor_property("source_radius", 30.0)
    fc.set_editor_property("light_color", unreal.Color(255, 120, 45, 255))

    board = light(unreal.SpotLight, "Practical_CaseBoardLamp", (330, -230, 290), (0, -55, 0))
    bc = board.spot_light_component
    bc.set_editor_property("intensity", 15.0)
    bc.set_editor_property("outer_cone_angle", 35.0)
    bc.set_editor_property("inner_cone_angle", 18.0)
    kelvin(bc, 2700)

    # Torch preview: a narrow, slightly cool beam where the player camera will be.
    torch = light(unreal.SpotLight, "Torch_Preview", (-300, 180, 150), (0, -12, -25))
    tc = torch.spot_light_component
    tc.set_editor_property("intensity", 20.0)
    tc.set_editor_property("outer_cone_angle", 18.0)
    tc.set_editor_property("inner_cone_angle", 8.0)
    tc.set_editor_property("attenuation_radius", 1500.0)
    kelvin(tc, 4200)

    # Fog: thin volumetric haze indoors, teal tint.
    fog = light(unreal.ExponentialHeightFog, "HeightFog", (0, 0, 0), folder="Atmosphere")
    fgc = fog.component
    fgc.set_editor_property("fog_density", 0.03)
    fgc.set_editor_property("fog_height_falloff", 0.2)
    fgc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.02, 0.035, 0.035, 1))
    fgc.set_editor_property("enable_volumetric_fog", True)
    fgc.set_editor_property("volumetric_fog_extinction_scale", 2.0)
    fgc.set_editor_property("volumetric_fog_albedo", unreal.Color(200, 215, 210, 255))

    # Post-process: fixed exposure, warm highlights / teal shadows, grain, vignette.
    ppv = light(unreal.PostProcessVolume, "PostProcess_Global", (0, 0, 0), folder="Atmosphere")
    ppv.set_editor_property("unbound", True)
    s = ppv.get_editor_property("settings")
    set_pp(s,
           auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
           auto_exposure_bias=-0.5,
           auto_exposure_apply_physical_camera_exposure=False,
           color_saturation=unreal.Vector4(0.85, 0.85, 0.85, 1),
           color_gain_shadows=unreal.Vector4(0.9, 1.0, 0.97, 1),
           color_saturation_shadows=unreal.Vector4(0.9, 1.0, 1.0, 1),
           color_gain_highlights=unreal.Vector4(1.08, 1.0, 0.88, 1),
           color_contrast=unreal.Vector4(1.1, 1.1, 1.1, 1),
           bloom_intensity=0.4,
           vignette_intensity=0.55,
           film_grain_intensity=0.25,
           scene_fringe_intensity=0.3,
           local_exposure_highlight_contrast_scale=0.8,
           local_exposure_shadow_contrast_scale=0.8)
    ppv.set_editor_property("settings", s)

    cam = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(-330, 220, 165),
                                        unreal.Rotator(0, -8, -30))
    cam.set_actor_label("Cam_Hero")
    cam.set_folder_path("Cameras")

    ps = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-330, 220, 100),
                                       unreal.Rotator(0, 0, -30))
    ps.set_folder_path("Gameplay")

    levels.save_current_level()


log("building materials")
master = build_master_material()
mats = {
    "wall": make_instance(master, "MI_Wall_Oxblood", (0.16, 0.035, 0.03), 0.8),
    "floor": make_instance(master, "MI_Floor_DarkWood", (0.08, 0.045, 0.025), 0.55),
    "ceiling": make_instance(master, "MI_Ceiling_Plaster", (0.22, 0.2, 0.17), 0.9),
    "wood": make_instance(master, "MI_Wood_Dark", (0.06, 0.035, 0.02), 0.5),
    "brass": make_instance(master, "MI_Brass", (0.55, 0.4, 0.18), 0.35, 1.0),
    "rug": make_instance(master, "MI_Rug_Oxblood", (0.12, 0.02, 0.02), 0.95),
}
log("building level")
build_level(mats)
log("done")
unreal.SystemLibrary.quit_editor()
