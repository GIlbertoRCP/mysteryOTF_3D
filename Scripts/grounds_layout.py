"""The manor grounds, as plain data and functions (no Unreal, no third-party modules).

Both halves of the build use it: gen_grounds.py (system Python) writes the terrain tiles and the
ground-material masks from it, and build_grounds.py (in the editor) places the house shell, the
props and every scattered tree, rock and fern from it. Units are centimetres, X east, Y north,
the house floor at Z = 0.

The house (Scripts/build_manor.py) runs east-west: a hall down the middle, rooms either side.
Its front door is at the west end of the hall and opens on a terrace and the drive, which winds
west through the woods to the iron gate. The garden door at the east end opens on a walled garden;
from the garden's south-east corner a path runs down to the lake.
"""
import math
import random

# ---------------------------------------------------------------- the house and its surroundings

HOUSE = (-1000.0, -770.0, 1000.0, 770.0)          # the outer walls (x0, y0, x1, y1)
SHELL = 40.0                                     # stone cladding outside them
EAVES = 320.0                                    # wall top; the roof sits on it
TERRACE = (-1750.0, -950.0, -1000.0, 950.0)      # cobbles at the front door
GARDEN = (1000.0, -2600.0, 4600.0, 2600.0)       # lawn inside a low wall
GARDEN_GATE = ((4600.0, -1700.0), (4600.0, -1200.0))   # gap in the east wall, toward the lake
STATUE = (2900.0, 0.0)

# Outbuildings: real case rooms out on the grounds (they play only their own places; see
# NoirRoom::bOnlySuits). The conservatory stands in the garden's north half, its door on the
# cross path; the chapel in a clearing north of the drive, its door at the end of its own path.
CONSERVATORY = (2350.0, 1475.0, 3450.0, 2225.0)
CHAPEL = (-4600.0, 2950.0, -3800.0, 4250.0)
CHAPEL_PATH = [(-3500.0, 120.0), (-3800.0, 1200.0), (-4100.0, 2250.0), (-4200.0, 2900.0)]
CHAPEL_PAD = (-4400.0, 2700.0, -4000.0, 2960.0)   # flagstones before its door

# The drive, from the terrace to the gate (a polyline, smoothed when drawn), and its width.
DRIVE = [(-1750.0, 0.0), (-3200.0, 150.0), (-5200.0, -600.0), (-7600.0, -900.0), (-10200.0, 100.0),
         (-12600.0, 1100.0), (-14400.0, 1300.0)]
DRIVE_W = 620.0
GATE = (-14400.0, 1300.0)
CAR = (-2700.0, 900.0, 200.0)                     # x, y, yaw: parked on the verge by the terrace

# The lake: an irregular ellipse south-east of the house; the water sits at WATER_Z.
LAKE_C = (8600.0, -6600.0)
LAKE_R = (3300.0, 2500.0)
WATER_Z = -90.0
PATH = [(4700.0, -1450.0), (5600.0, -2500.0), (6500.0, -3500.0), (7300.0, -4150.0)]
PATH_W = 260.0
PIER = (7400.0, -4400.0, -45.0)                   # where it leaves the shore, and its heading (deg)
FIRE_PIT = (6300.0, -4300.0)

# Where the detective can go, and the whole terrain (hills close the world in beyond PLAY).
PLAY = (-16000.0, -12500.0, 12500.0, 9000.0)
WORLD = (-21000.0, -17500.0, 17500.0, 14000.0)
TILES = (4, 4)
QUADS = 120                                       # per tile side

# Named areas, for the map and the room name on screen (uncast NoirRooms): label, Spanish, box.
AREAS = [
    ("Front Terrace", "Terraza", (TERRACE[0] - 400, TERRACE[1], TERRACE[2], TERRACE[3])),
    ("The Drive", "El camino", (-14600.0, -1600.0, -2150.0, 2000.0)),
    ("Garden", "Jardín", GARDEN),
    ("Lakeside", "La orilla", (5000.0, -9500.0, 12000.0, -3000.0)),
]


# ---------------------------------------------------------------- shapes

def _smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def box_dist(x, y, box):
    """Distance from a point to a box (0 inside)."""
    dx = max(box[0] - x, 0.0, x - box[2])
    dy = max(box[1] - y, 0.0, y - box[3])
    return math.hypot(dx, dy)


def _seg_dist(x, y, a, b):
    ax, ay = a
    bx, by = b
    vx, vy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy)))
    return math.hypot(x - (ax + t * vx), y - (ay + t * vy))


def _chaikin(points, rounds=3):
    pts = list(points)
    for _ in range(rounds):
        out = [pts[0]]
        for a, b in zip(pts, pts[1:]):
            out.append((0.75 * a[0] + 0.25 * b[0], 0.75 * a[1] + 0.25 * b[1]))
            out.append((0.25 * a[0] + 0.75 * b[0], 0.25 * a[1] + 0.75 * b[1]))
        out.append(pts[-1])
        pts = out
    return pts


DRIVE_SMOOTH = _chaikin(DRIVE)
PATH_SMOOTH = _chaikin(PATH)
CHAPEL_PATH_SMOOTH = _chaikin(CHAPEL_PATH)


def line_dist(x, y, line):
    return min(_seg_dist(x, y, a, b) for a, b in zip(line, line[1:]))


def lake_edge(x, y):
    """Signed distance-ish to the lake's shore: negative in the water."""
    dx, dy = (x - LAKE_C[0]) / LAKE_R[0], (y - LAKE_C[1]) / LAKE_R[1]
    ang = math.atan2(dy, dx)
    wobble = 1.0 + 0.12 * math.sin(3 * ang + 0.7) + 0.07 * math.sin(7 * ang + 2.1)
    return (math.hypot(dx, dy) - wobble) * min(LAKE_R)


# ---------------------------------------------------------------- the ground

def _noise(x, y):
    """Smooth rolling ground from a few incommensurate waves (deterministic, no tables)."""
    h = 0.0
    for fx, fy, ph, amp in ((1 / 7300, 1 / 6100, 0.3, 140), (1 / 3100, -1 / 3900, 1.9, 70),
                            (-1 / 1700, 1 / 2300, 4.2, 30), (1 / 11000, 1 / 9000, 2.6, 220)):
        h += amp * math.sin(x * fx * 2 * math.pi + ph) * math.cos(y * fy * 2 * math.pi + ph * 0.7)
    return h


def height(x, y):
    h = _noise(x, y)
    # The house, terrace and garden stand on a level platform just under the floor.
    level = min(box_dist(x, y, HOUSE), box_dist(x, y, TERRACE), box_dist(x, y, GARDEN), box_dist(x, y, CHAPEL) + 200.0)
    h *= _smooth((level - 300.0) / 2500.0)
    h -= 6.0
    # The drive and the lake path are cut gently into the slope.
    d = line_dist(x, y, DRIVE_SMOOTH)
    h = h * (0.35 + 0.65 * _smooth((d - DRIVE_W * 0.5) / 900.0))
    # The lake basin.
    e = lake_edge(x, y)
    if e < 600.0:
        bowl = -380.0 * _smooth(-e / 1400.0) if e < 0 else 0.0
        shore = WATER_Z + 40.0 + e * 0.12
        t = _smooth((e + 200.0) / 800.0)
        h = (bowl + WATER_Z + 30.0 if e < 0 else shore) * (1 - t) + h * t
    # Hills beyond the play area close the world in.
    out = box_dist(x, y, PLAY)
    if out > 0:
        h += 2600.0 * _smooth(out / 3500.0) + 0.25 * out * _smooth(out / 6000.0)
    return h


def masks(x, y):
    """Ground material weights at a point: gravel, lawn, mud, cobbles, leaf litter (0..1 each)."""
    drive = 1 - _smooth((line_dist(x, y, DRIVE_SMOOTH) - DRIVE_W * 0.45) / 140.0)
    path = 1 - _smooth((line_dist(x, y, PATH_SMOOTH) - PATH_W * 0.45) / 90.0)
    garden_path = 0.0
    if box_dist(x, y, GARDEN) == 0:
        cross = min(abs(y), abs(x - STATUE[0])) - 110.0
        ring = abs(math.hypot(x - STATUE[0], y - STATUE[1]) - 650.0) - 110.0
        garden_path = 1 - _smooth(min(cross, ring) / 40.0)
    chapel_path = 1 - _smooth((line_dist(x, y, CHAPEL_PATH_SMOOTH) - PATH_W * 0.45) / 90.0)
    gravel = max(drive, path * 0.8, chapel_path * 0.8, garden_path)
    lawn = (1 - _smooth(box_dist(x, y, GARDEN) / 150.0)) * (1 - garden_path)
    e = lake_edge(x, y)
    mud = 1 - _smooth((e - 250.0) / 300.0)
    cobble = max(1 - _smooth(box_dist(x, y, TERRACE) / 60.0), 1 - _smooth(box_dist(x, y, HOUSE) / 200.0) if box_dist(x, y, HOUSE) > 0 else 0.0,
                 1 - _smooth(box_dist(x, y, CHAPEL_PAD) / 60.0))
    leaves = 0.5 + 0.5 * math.sin(x / 900.0 + math.cos(y / 1300.0) * 2.0) * math.cos(y / 1100.0 - x / 2400.0)
    return gravel, lawn, mud, cobble, leaves


# ---------------------------------------------------------------- what grows where

def clearing(x, y):
    """How far into the woods a point is: 0 on paths, lawns and water, 1 deep in the trees."""
    d = min(
        box_dist(x, y, HOUSE) - 900.0,
        box_dist(x, y, TERRACE) - 900.0,
        box_dist(x, y, GARDEN) - 500.0,
        line_dist(x, y, DRIVE_SMOOTH) - DRIVE_W * 0.5 - 250.0,
        line_dist(x, y, PATH_SMOOTH) - PATH_W * 0.5 - 150.0,
        line_dist(x, y, CHAPEL_PATH_SMOOTH) - PATH_W * 0.5 - 150.0,
        box_dist(x, y, CHAPEL) - 650.0,
        lake_edge(x, y) - 350.0,
    )
    return _smooth(d / 1400.0)


def scatter(kind, rng):
    """Points (x, y, z, yaw, scale) for one kind of dressing, jittered on a grid and thinned."""
    spec = SCATTER[kind]
    step = spec["step"]
    x0, y0, x1, y1 = spec.get("area", WORLD)
    out = []
    y = y0
    while y < y1:
        x = x0
        while x < x1:
            px, py = x + rng.uniform(0, step), y + rng.uniform(0, step)
            if rng.random() < spec["where"](px, py):
                s = rng.uniform(*spec["scale"])
                out.append((px, py, height(px, py) + spec.get("sink", 0.0) * s, rng.uniform(0, 360), s))
            x += step
        y += step
    return out


def _deep(lo, hi):
    return lambda x, y: _smooth((clearing(x, y) - lo) / (hi - lo))


def _edge(x, y):
    c = clearing(x, y)
    return 0.9 * _smooth(c / 0.25) * (1 - _smooth((c - 0.5) / 0.4))


def _near_play(margin):
    return lambda x, y: 1.0 if box_dist(x, y, PLAY) <= margin else 0.0


def _under_trees(x, y):
    return _deep(0.05, 0.5)(x, y) * _near_play(1500.0)(x, y)


def _lawn_edge(x, y):
    inside = box_dist(x, y, GARDEN) == 0
    near_wall = min(x - GARDEN[0], GARDEN[2] - x, y - GARDEN[1], GARDEN[3] - y) < 220.0
    return 0.85 if inside and near_wall and x > GARDEN[0] + 150 else 0.0


SCATTER = {
    # Trees everywhere but the clearings; thicker away from the paths, on the hills too.
    "fir_tree_01": {"step": 650.0, "where": _deep(0.15, 0.6), "scale": (0.75, 1.25)},
    "fir_sapling_medium": {"step": 700.0, "where": lambda x, y: _edge(x, y) * _near_play(2000.0)(x, y), "scale": (0.8, 1.3)},
    "fir_sapling": {"step": 520.0, "where": lambda x, y: _edge(x, y) * _near_play(1000.0)(x, y), "scale": (0.7, 1.3)},
    "pine_sapling_small": {"step": 600.0, "where": lambda x, y: 0.6 * _edge(x, y) * _near_play(1000.0)(x, y), "scale": (0.8, 1.4)},
    "dead_tree_trunk": {"step": 2600.0, "where": _under_trees, "scale": (0.9, 1.2), "sink": -5.0},
    "dead_tree_trunk_02": {"step": 2600.0, "where": _under_trees, "scale": (0.9, 1.2), "sink": -5.0},
    "tree_stump_01": {"step": 2000.0, "where": _under_trees, "scale": (0.9, 1.3), "sink": -5.0},
    "tree_stump_02": {"step": 2000.0, "where": _under_trees, "scale": (0.9, 1.3), "sink": -5.0},
    "rock_moss_set_01": {"step": 1900.0, "where": lambda x, y: 0.7 * _under_trees(x, y), "scale": (0.8, 1.6), "sink": -10.0},
    "rock_moss_set_02": {"step": 1900.0, "where": lambda x, y: 0.7 * _under_trees(x, y), "scale": (0.8, 1.6), "sink": -10.0},
    "boulder_01": {"step": 3200.0, "where": _under_trees, "scale": (0.6, 1.2), "sink": -25.0},
    # Undergrowth, only where the detective can see it.
    "fern_02": {"step": 300.0, "where": lambda x, y: 0.55 * _deep(0.0, 0.35)(x, y) * _near_play(0.0)(x, y), "scale": (0.8, 1.5)},
    "shrub_02": {"step": 650.0, "where": lambda x, y: 0.5 * _edge(x, y) * _near_play(0.0)(x, y), "scale": (0.8, 1.4)},
    "shrub_04": {"step": 650.0, "where": lambda x, y: 0.5 * _edge(x, y) * _near_play(0.0)(x, y), "scale": (0.8, 1.4)},
    "grass_medium_01": {"step": 220.0, "where": lambda x, y: max(0.45 * _edge(x, y), _lawn_edge(x, y)) * _near_play(0.0)(x, y), "scale": (0.8, 1.3)},
    # Debris that lies differently each case.
    "dry_branches_medium_01": {"step": 800.0, "where": lambda x, y: 0.7 * _deep(0.0, 0.3)(x, y) * _near_play(0.0)(x, y), "scale": (0.8, 1.3)},
}

# Sets that block the detective, sets each case thins (share kept), and how far undergrowth is drawn.
BLOCKING = {"fir_tree_01", "fir_sapling_medium", "dead_tree_trunk", "dead_tree_trunk_02",
            "tree_stump_01", "tree_stump_02", "rock_moss_set_01", "rock_moss_set_02", "boulder_01"}
CASE_KEEP = {"dry_branches_medium_01": 0.55, "fern_02": 0.85}
CULL = {"fir_tree_01": 11000.0, "fir_sapling_medium": 8000.0, "fir_sapling": 5000.0, "pine_sapling_small": 5000.0,
        "dead_tree_trunk": 6000.0, "dead_tree_trunk_02": 6000.0, "tree_stump_01": 5000.0, "tree_stump_02": 5000.0,
        "rock_moss_set_01": 7000.0, "rock_moss_set_02": 7000.0, "boulder_01": 8000.0,
        "fern_02": 5500.0, "grass_medium_01": 4500.0, "shrub_02": 7000.0,
        "shrub_04": 7000.0, "dry_branches_medium_01": 4500.0}


def all_scatter(seed=1888):
    rng = random.Random(seed)
    return {kind: scatter(kind, rng) for kind in SCATTER}
