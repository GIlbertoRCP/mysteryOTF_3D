import os
import math
import numpy as np
from PIL import Image

OUT_DIR = r"B:\mysteryOTF\Content\Art\Textures"
os.makedirs(OUT_DIR, exist_ok=True)

SIZE = 1024

def seamless_noise(size, scale=8.0, octaves=4, persistence=0.5, seed=42):
    """Generate seamless 2D Perlin-like noise using periodic wrapping."""
    np.random.seed(seed)
    grid = np.zeros((size, size), dtype=np.float32)
    for oct in range(octaves):
        freq = (2 ** oct) * scale
        amp = persistence ** oct
        # Periodic grid sampling
        fx = int(freq)
        fy = int(freq)
        if fx < 1: fx = 1
        if fy < 1: fy = 1
        r = np.random.rand(fx, fy).astype(np.float32)
        # Repeat r across size
        from PIL import Image as PImage
        img = PImage.fromarray((r * 255).astype(np.uint8)).resize((size, size), PImage.Resampling.BICUBIC)
        # Gaussian blur for smooth gradient
        grid += (np.array(img, dtype=np.float32) / 255.0) * amp
    # Normalize to 0-1
    grid = (grid - grid.min()) / (grid.max() - grid.min() + 1e-6)
    return grid

def height_to_normal(height, strength=2.0):
    """Compute seamless normal map from height field using Sobel filter with wrap padding."""
    h = np.pad(height, ((1, 1), (1, 1)), mode='wrap')
    
    # Sobel kernels
    dx = (h[0:-2, 2:] + 2*h[1:-1, 2:] + h[2:, 2:]) - (h[0:-2, 0:-2] + 2*h[1:-1, 0:-2] + h[2:, 0:-2])
    dy = (h[2:, 0:-2] + 2*h[2:, 1:-1] + h[2:, 2:]) - (h[0:-2, 0:-2] + 2*h[0:-2, 1:-1] + h[0:-2, 2:])
    
    dx = -dx * strength
    dy = -dy * strength
    dz = np.ones_like(dx)
    
    length = np.sqrt(dx**2 + dy**2 + dz**2) + 1e-6
    nx = (dx / length) * 0.5 + 0.5
    ny = (dy / length) * 0.5 + 0.5
    nz = (dz / length) * 0.5 + 0.5
    
    normal = np.stack([nx, ny, nz], axis=-1)
    return (np.clip(normal, 0, 1) * 255).astype(np.uint8)

def compute_ao(height, radius=4, strength=1.5):
    """Compute local ambient occlusion / cavity map from heightmap."""
    from PIL import Image as PImage, ImageFilter
    h_img = PImage.fromarray((height * 255).astype(np.uint8))
    blurred = np.array(h_img.filter(ImageFilter.GaussianBlur(radius)), dtype=np.float32) / 255.0
    diff = height - blurred
    ao = 1.0 - np.clip(-diff * strength, 0.0, 1.0)
    return (ao * 255).astype(np.uint8)

print("Starting PBR texture generation...")

# ==============================================================================
# 1. DARK WOOD FLOOR (Parquet / Planks)
# ==============================================================================
print("Generating Wood Floor PBR set...")
num_planks_x = 8
plank_w = SIZE // num_planks_x
wood_h = np.zeros((SIZE, SIZE), dtype=np.float32)
wood_bc = np.zeros((SIZE, SIZE, 3), dtype=np.float32)
wood_rough = np.zeros((SIZE, SIZE), dtype=np.float32)

grain = seamless_noise(SIZE, scale=2.0, octaves=5, seed=101)
micro_grain = seamless_noise(SIZE, scale=16.0, octaves=3, seed=102)

for i in range(num_planks_x):
    x0 = i * plank_w
    x1 = (i + 1) * plank_w
    # Plank wood tone variation
    tint = 0.85 + 0.3 * np.sin(i * 1.7)
    wood_bc[:, x0:x1, 0] = (0.16 * tint) * (0.8 + 0.4 * grain[:, x0:x1])
    wood_bc[:, x0:x1, 1] = (0.09 * tint) * (0.8 + 0.4 * grain[:, x0:x1])
    wood_bc[:, x0:x1, 2] = (0.05 * tint) * (0.8 + 0.4 * grain[:, x0:x1])
    
    # Grain height
    wood_h[:, x0:x1] = grain[:, x0:x1] * 0.35 + micro_grain[:, x0:x1] * 0.1
    # Plank bevel edges
    edge_dist = np.minimum(np.arange(plank_w), plank_w - 1 - np.arange(plank_w))
    bevel = np.clip(edge_dist / 3.0, 0.0, 1.0)
    wood_h[:, x0:x1] *= bevel[None, :]
    wood_bc[:, x0:x1] *= (0.5 + 0.5 * bevel[None, :, None])
    
    # Roughness: satin varnish (0.35 - 0.55), rougher in grooves
    wood_rough[:, x0:x1] = 0.38 + 0.15 * (1.0 - bevel[None, :]) + 0.1 * grain[:, x0:x1]

# Staggered end joints
for i in range(num_planks_x):
    y_joint = int((i * 0.37 * SIZE) % SIZE)
    wood_h[max(0, y_joint-2):min(SIZE, y_joint+2), i*plank_w:(i+1)*plank_w] *= 0.1
    wood_bc[max(0, y_joint-2):min(SIZE, y_joint+2), i*plank_w:(i+1)*plank_w] *= 0.3

wood_normal = height_to_normal(wood_h, strength=4.0)
wood_ao = compute_ao(wood_h, radius=3, strength=2.0)
wood_orm = np.stack([wood_ao, (np.clip(wood_rough, 0, 1) * 255).astype(np.uint8), np.zeros((SIZE, SIZE), dtype=np.uint8)], axis=-1)

Image.fromarray((np.clip(wood_bc, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT_DIR, "T_WoodFloor_BaseColor.png"))
Image.fromarray(wood_normal).save(os.path.join(OUT_DIR, "T_WoodFloor_Normal.png"))
Image.fromarray(wood_orm).save(os.path.join(OUT_DIR, "T_WoodFloor_ORM.png"))

# ==============================================================================
# 2. VICTORIAN WALL (Oxblood Damask / Panelling)
# ==============================================================================
print("Generating Victorian Wall PBR set...")
x = np.linspace(0, 4 * np.pi, SIZE, endpoint=False)
y = np.linspace(0, 4 * np.pi, SIZE, endpoint=False)
X, Y = np.meshgrid(x, y)
# Damask pattern
damask = np.sin(X) * np.cos(Y) + 0.5 * np.sin(2 * X + Y) * np.cos(X - 2 * Y)
damask = (damask - damask.min()) / (damask.max() - damask.min() + 1e-6)

fabric_noise = seamless_noise(SIZE, scale=32.0, octaves=2, seed=201)
wall_h = damask * 0.5 + fabric_noise * 0.1

wall_bc = np.zeros((SIZE, SIZE, 3), dtype=np.float32)
# Deep oxblood red with golden damask sheen highlights
wall_bc[:, :, 0] = 0.14 + 0.08 * damask
wall_bc[:, :, 1] = 0.02 + 0.03 * damask
wall_bc[:, :, 2] = 0.02 + 0.01 * damask

wall_normal = height_to_normal(wall_h, strength=2.5)
wall_ao = compute_ao(wall_h, radius=4, strength=1.5)
wall_rough = (0.75 - 0.25 * damask + 0.1 * fabric_noise)
wall_orm = np.stack([wall_ao, (np.clip(wall_rough, 0, 1) * 255).astype(np.uint8), np.zeros((SIZE, SIZE), dtype=np.uint8)], axis=-1)

Image.fromarray((np.clip(wall_bc, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT_DIR, "T_Wall_BaseColor.png"))
Image.fromarray(wall_normal).save(os.path.join(OUT_DIR, "T_Wall_Normal.png"))
Image.fromarray(wall_orm).save(os.path.join(OUT_DIR, "T_Wall_ORM.png"))

# ==============================================================================
# 3. WET GROUND / COBBLESTONE (Puddle Modulation)
# ==============================================================================
print("Generating Wet Ground PBR set...")
# Voronoi-like cobblestones
cobble_x = np.linspace(0, 12 * np.pi, SIZE, endpoint=False)
cobble_y = np.linspace(0, 12 * np.pi, SIZE, endpoint=False)
CX, CY = np.meshgrid(cobble_x, cobble_y)
stones = np.abs(np.sin(CX) * np.cos(CY))
stones = np.sqrt(stones)

mud_noise = seamless_noise(SIZE, scale=4.0, octaves=4, seed=301)
# Puddles in low areas of mud_noise
puddle_mask = np.clip((mud_noise - 0.5) * 4.0, 0.0, 1.0) # 1 = puddle, 0 = dry stone

ground_h = stones * (1.0 - puddle_mask * 0.8) + mud_noise * 0.2
ground_normal = height_to_normal(ground_h, strength=5.0)
# Flatten normal in puddle areas
ground_normal[:, :, 0] = (ground_normal[:, :, 0].astype(np.float32) * (1.0 - puddle_mask) + 128 * puddle_mask).astype(np.uint8)
ground_normal[:, :, 1] = (ground_normal[:, :, 1].astype(np.float32) * (1.0 - puddle_mask) + 128 * puddle_mask).astype(np.uint8)
ground_normal[:, :, 2] = (ground_normal[:, :, 2].astype(np.float32) * (1.0 - puddle_mask) + 255 * puddle_mask).astype(np.uint8)

ground_bc = np.zeros((SIZE, SIZE, 3), dtype=np.float32)
# Dark wet stone and muddy puddles
ground_bc[:, :, 0] = 0.06 * stones * (1.0 - puddle_mask * 0.4) + 0.02
ground_bc[:, :, 1] = 0.06 * stones * (1.0 - puddle_mask * 0.4) + 0.025
ground_bc[:, :, 2] = 0.05 * stones * (1.0 - puddle_mask * 0.4) + 0.02

# Roughness: stone ~0.65, puddle ~0.04 (mirror wet reflections!)
ground_rough = 0.65 * (1.0 - puddle_mask) + 0.04 * puddle_mask
ground_ao = compute_ao(ground_h, radius=5, strength=2.0)
ground_orm = np.stack([ground_ao, (np.clip(ground_rough, 0, 1) * 255).astype(np.uint8), np.zeros((SIZE, SIZE), dtype=np.uint8)], axis=-1)

Image.fromarray((np.clip(ground_bc, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT_DIR, "T_Ground_BaseColor.png"))
Image.fromarray(ground_normal).save(os.path.join(OUT_DIR, "T_Ground_Normal.png"))
Image.fromarray(ground_orm).save(os.path.join(OUT_DIR, "T_Ground_ORM.png"))

# ==============================================================================
# 4. VICTORIAN WOVEN RUG / CARPET
# ==============================================================================
print("Generating Rug PBR set...")
rug_x = np.linspace(-np.pi, np.pi, SIZE, endpoint=False)
rug_y = np.linspace(-np.pi, np.pi, SIZE, endpoint=False)
RX, RY = np.meshgrid(rug_x, rug_y)
R_dist = np.sqrt(RX**2 + RY**2)
rug_medallion = np.cos(4 * R_dist) * np.sin(8 * np.arctan2(RY, RX))
rug_medallion = (rug_medallion - rug_medallion.min()) / (rug_medallion.max() - rug_medallion.min() + 1e-6)

fiber = seamless_noise(SIZE, scale=64.0, octaves=2, seed=401)
rug_h = rug_medallion * 0.3 + fiber * 0.2

rug_bc = np.zeros((SIZE, SIZE, 3), dtype=np.float32)
# Oxblood, gold, and dark navy weave
rug_bc[:, :, 0] = 0.25 * rug_medallion + 0.08 * (1.0 - rug_medallion)
rug_bc[:, :, 1] = 0.06 * rug_medallion + 0.02 * (1.0 - rug_medallion)
rug_bc[:, :, 2] = 0.04 * rug_medallion + 0.03 * (1.0 - rug_medallion)

rug_normal = height_to_normal(rug_h, strength=2.0)
rug_ao = compute_ao(rug_h, radius=2, strength=1.2)
rug_rough = np.full((SIZE, SIZE), 0.94, dtype=np.float32) # matte wool pile
rug_orm = np.stack([rug_ao, (np.clip(rug_rough, 0, 1) * 255).astype(np.uint8), np.zeros((SIZE, SIZE), dtype=np.uint8)], axis=-1)

Image.fromarray((np.clip(rug_bc, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT_DIR, "T_Rug_BaseColor.png"))
Image.fromarray(rug_normal).save(os.path.join(OUT_DIR, "T_Rug_Normal.png"))
Image.fromarray(rug_orm).save(os.path.join(OUT_DIR, "T_Rug_ORM.png"))

# ==============================================================================
# 5. CEILING PLASTER
# ==============================================================================
print("Generating Ceiling Plaster PBR set...")
plaster_noise = seamless_noise(SIZE, scale=12.0, octaves=4, seed=501)
plaster_micro = seamless_noise(SIZE, scale=48.0, octaves=2, seed=502)
plaster_h = plaster_noise * 0.6 + plaster_micro * 0.2

plaster_bc = np.zeros((SIZE, SIZE, 3), dtype=np.float32)
plaster_bc[:, :, 0] = 0.35 + 0.08 * plaster_noise
plaster_bc[:, :, 1] = 0.33 + 0.07 * plaster_noise
plaster_bc[:, :, 2] = 0.30 + 0.06 * plaster_noise

plaster_normal = height_to_normal(plaster_h, strength=1.5)
plaster_ao = compute_ao(plaster_h, radius=3, strength=1.0)
plaster_rough = np.full((SIZE, SIZE), 0.90, dtype=np.float32)
plaster_orm = np.stack([plaster_ao, (np.clip(plaster_rough, 0, 1) * 255).astype(np.uint8), np.zeros((SIZE, SIZE), dtype=np.uint8)], axis=-1)

Image.fromarray((np.clip(plaster_bc, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT_DIR, "T_Ceiling_BaseColor.png"))
Image.fromarray(plaster_normal).save(os.path.join(OUT_DIR, "T_Ceiling_Normal.png"))
Image.fromarray(plaster_orm).save(os.path.join(OUT_DIR, "T_Ceiling_ORM.png"))

# ==============================================================================
# 6. CORKBOARD (For Mind Place Board)
# ==============================================================================
print("Generating Corkboard PBR set...")
cork_large = seamless_noise(SIZE, scale=8.0, octaves=3, seed=601)
cork_chips = seamless_noise(SIZE, scale=24.0, octaves=3, seed=602)
cork_pores = seamless_noise(SIZE, scale=64.0, octaves=2, seed=603)
cork_h = cork_large * 0.3 + cork_chips * 0.5 + cork_pores * 0.2

cork_bc = np.zeros((SIZE, SIZE, 3), dtype=np.float32)
cork_bc[:, :, 0] = 0.42 * (0.8 + 0.4 * cork_chips)
cork_bc[:, :, 1] = 0.28 * (0.8 + 0.4 * cork_chips)
cork_bc[:, :, 2] = 0.15 * (0.8 + 0.4 * cork_chips)

cork_normal = height_to_normal(cork_h, strength=3.0)
cork_ao = compute_ao(cork_h, radius=3, strength=1.8)
cork_rough = np.full((SIZE, SIZE), 0.88, dtype=np.float32)
cork_orm = np.stack([cork_ao, (np.clip(cork_rough, 0, 1) * 255).astype(np.uint8), np.zeros((SIZE, SIZE), dtype=np.uint8)], axis=-1)

Image.fromarray((np.clip(cork_bc, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT_DIR, "T_Corkboard_BaseColor.png"))
Image.fromarray(cork_normal).save(os.path.join(OUT_DIR, "T_Corkboard_Normal.png"))
Image.fromarray(cork_orm).save(os.path.join(OUT_DIR, "T_Corkboard_ORM.png"))

# ==============================================================================
# 7. RAIN DROPLETS & STREAKS (For MI_Glass_Night)
# ==============================================================================
print("Generating Rain Droplets Normal Map...")
np.random.seed(777)
rain_h = np.zeros((SIZE, SIZE), dtype=np.float32)

# Random droplets
for _ in range(800):
    rx = np.random.randint(0, SIZE)
    ry = np.random.randint(0, SIZE)
    radius = np.random.randint(2, 8)
    y_trail = np.random.randint(10, 40)
    
    # Droplet bead
    y_idx = (np.arange(ry - radius, ry + radius)) % SIZE
    x_idx = (np.arange(rx - radius, rx + radius)) % SIZE
    YY, XX = np.meshgrid(y_idx, x_idx)
    d = np.sqrt((XX - rx)**2 + (YY - ry)**2)
    mask = d < radius
    bead = np.maximum(0, 1.0 - d / radius)
    for iy in range(len(y_idx)):
        for ix in range(len(x_idx)):
            if mask[ix, iy]:
                rain_h[y_idx[iy], x_idx[ix]] = max(rain_h[y_idx[iy], x_idx[ix]], bead[ix, iy])
                
    # Vertical trail
    trail_len = min(y_trail, SIZE)
    for step in range(trail_len):
        ty = (ry + step) % SIZE
        tx = (rx + int(np.random.randn() * 0.5)) % SIZE
        rain_h[ty, tx] = max(rain_h[ty, tx], 0.25 * (1.0 - step / trail_len))

rain_normal = height_to_normal(rain_h, strength=6.0)
Image.fromarray(rain_normal).save(os.path.join(OUT_DIR, "T_RainDroplets_Normal.png"))

print(f"All PBR textures generated successfully in {OUT_DIR}!")
