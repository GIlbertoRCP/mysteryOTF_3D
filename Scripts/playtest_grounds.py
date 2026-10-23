"""Tours the manor grounds: stands the detective at a few viewpoints and screenshots each, with
frame stats on screen. Logs what the woods hold and the frame time at each stop.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_grounds.py" -unattended -nosplash
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grounds_layout as G  # noqa: E402

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eactors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level("/Game/Maps/L_Manor")
for a in eactors.get_all_level_actors():
    if isinstance(a, unreal.NoirCaseDirector):
        a.set_editor_property("seed", "TEST-gothic-Detective-0")
levels.editor_request_begin_play()
state = {"t": 0.0, "step": 0, "steps": None, "frames": []}

# (name, x, y, yaw, pitch); the detective stands on the ground there.
# Options (-ExecCmds does not pass arguments through, so NOIR_TOUR works too): diag, gpu, profile,
# and view names to visit only those.
ARGS = sys.argv[1:] + os.environ.get("NOIR_TOUR", "").split()
DIAG = "diag" in ARGS
VIEWS = [
    ("hall_to_front_door", -700.0, 0.0, 180.0, 0.0),
    ("room_interior", -600.0, 300.0, 70.0, -5.0),
    ("terrace_to_house", -2900.0, -700.0, 12.0, 4.0),
    ("drive", -6200.0, -800.0, 175.0, 0.0),
    ("garden", 1450.0, -2100.0, 40.0, 2.0),
    ("lake_pier", 6700.0, -3700.0, -55.0, -4.0),
    ("woods", -4200.0, 4800.0, 20.0, 0.0),
]


def log(msg):
    unreal.log("[GroundsTour] " + msg)


def script(world, pawn):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    for s in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NoirScatter):
        log("scatter %s: %d instances" % (s.get_actor_label() if hasattr(s, "get_actor_label") else s.get_name(), s.count_instances()))
    cmd = lambda c: (lambda: unreal.SystemLibrary.execute_console_command(world, c))
    if "notorch" in ARGS:
        pawn.set_torch_on(False)
    steps = [(0.2, cmd("stat unit")), (0.2, cmd("stat gpu" if "gpu" in ARGS else "stat fps"))]
    only = [a for a in ARGS if a in [v[0] for v in VIEWS]]
    for name, x, y, yaw, pitch in [v for v in VIEWS if not only or v[0] in only]:
        z = (0.0 if abs(x) < 1000 and abs(y) < 760 else G.height(x, y)) + 100.0

        def go(x=x, y=y, z=z, yaw=yaw, pitch=pitch, name=name):
            pawn.teleport(unreal.Vector(x, y, z), unreal.Rotator(0, 0, yaw))
            pc.set_control_rotation(unreal.Rotator(0, pitch, yaw))
            state["frames"] = []
            state["view"] = name

        def report(name=name):
            f = state["frames"]
            if f:
                f = sorted(f)
                log("%s: median frame %.1f ms, worst %.1f ms over %d frames" % (name, 1000 * f[len(f) // 2], 1000 * f[-1], len(f)))

        where = lambda name=name: log("%s: detective at %s" % (name, pawn.get_actor_location()))
        steps += [(0.5, go), (4.0, report), (0.1, where), (0.1, cmd("Shot showui"))]
        if "tune" in ARGS:
            for g in [float(v) for v in os.environ.get("NOIR_GLOW", "0 0.0012 0.003 0.006").split()]:
                def glow(g=g, name=name):
                    for w in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NoirWeather):
                        w.set_editor_property("fog_glow", unreal.LinearColor(g, g * 1.08, g * 1.33, 1))
                    log("%s: fog glow %.4f" % (name, g))
                steps += [(0.1, glow), (1.5, cmd("Shot showui"))]
        if os.environ.get("NOIR_EXP"):
            # Console settings applied one after another (cumulative), the frame time after each.
            for c in ["(baseline)"] + [c.strip() for c in os.environ["NOIR_EXP"].split(";") if c.strip()]:
                def apply(c=c):
                    if c != "(baseline)":
                        unreal.SystemLibrary.execute_console_command(world, c)
                    state["frames"] = []

                def measure(c=c, name=name):
                    f = sorted(state["frames"])
                    if f:
                        log("%s | %-45s median %.1f ms" % (name, c, 1000 * f[len(f) // 2]))
                steps += [(0.1, apply), (3.0, measure)]
        if "cpu" in ARGS:
            steps += [(0.2, cmd("stat dumpframe -ms=0.3 -root=gamethread")), (1.0, lambda: None)]
        if "profile" in ARGS:
            steps += [(0.2, cmd("ProfileGPU")), (1.5, lambda: None)]
        if DIAG:
            steps += [(0.1, cmd("r.VolumetricFog 0")), (0.8, cmd("Shot showui")), (0.1, cmd("showflag.fog 0")),
                      (0.8, cmd("Shot showui")), (0.1, cmd("r.VolumetricFog 1")), (0.1, cmd("showflag.fog 1"))]
    steps += [(1.0, None)]
    return steps


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
    if state["steps"] is None:
        if not pawn or state["t"] < 25:
            return
        try:
            state["steps"] = script(world, pawn)
        except Exception as e:  # never leave the editor hanging
            log("script failed: %s" % e)
            state["steps"] = [(0.1, None)]
        state["t"] = 0.0
    state["frames"].append(dt)
    steps = state["steps"]
    if state["step"] >= len(steps):
        return
    delay, action = steps[state["step"]]
    if state["t"] < delay:
        return
    state["t"] = 0.0
    state["step"] += 1
    if action:
        action()
    else:
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
