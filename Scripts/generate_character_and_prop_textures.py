import os
import math
import numpy as np
from PIL import Image, ImageFilter

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
        fx = max(1, int(freq))
        fy = max(1, int(freq))
        r = np.random.rand(fx, fy).astype(np.float32)
        img = Image.fromarray((r * 255).astype(np.uint8)).resize((size, size), Image.Resampling.BICUBIC)
        grid += (np.array(img, dtype=np.float32) / 255.0) * amp
    grid = (grid - grid.min()) / (grid.max() - grid.min() + 1e-6)
    return grid

def height_to_normal(height, strength=2.0):
    """Compute seamless normal map from height field using Sobel filter with wrap padding."""
    h = np.pad(height, ((1, 1), (1, 1)), mode='wrap')
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
    h_img = Image.fromarray((height * 255).astype(np.uint8))
    blurred = np.array(h_img.filter(ImageFilter.GaussianBlur(radius)), dtype=np.float32) / 255.0
    diff = height - blurred
    ao = 1.0 - np.clip(-diff * strength, 0.0, 1.0)
    return (ao * 255).astype(np.uint8)

def save_pbr_set(base_name, bc, normal, ao, rough, metal):
    Image.fromarray((np.clip(bc, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT_DIR, f"{base_name}_BaseColor.png"))
    Image.fromarray(normal).save(os.path.join(OUT_DIR, f"{base_name}_Normal.png"))
    # Channel-packed ORM (R = AO, G = Roughness, B = Metallic)
    orm = np.stack([ao, (np.clip(rough, 0, 1) * 255).astype(np.uint8), (np.clip(metal, 0, 1) * 255).astype(np.uint8)], axis=-1)
    Image.fromarray(orm).save(os.path.join(OUT_DIR, f"{base_name}_ORM.png"))
    print(f"Saved PBR set: {base_name}")

print("=== Generating Character & Prop PBR Textures ===")

# ==============================================================================
# 1. LEATHER (Chesterfield armchair, book covers, shoes)
# ==============================================================================
print("Generating Leather...")
cell_noise = seamless_noise(SIZE, scale=32.0, octaves=4, persistence=0.6, seed=201)
wrinkles = seamless_noise(SIZE, scale=4.0, octaves=5, persistence=0.4, seed=202)
leather_h = cell_noise * 0.7 + wrinkles * 0.3
leather_normal = height_to_normal(leather_h, strength=3.5)
leather_ao = compute_ao(leather_h, radius=3, strength=2.0)
leather_rough = 0.40 + 0.30 * cell_noise + 0.15 * wrinkles
leather_bc = np.stack([0.85 + 0.15 * cell_noise]*3, axis=-1) # neutral tint multiplier
save_pbr_set("T_Leather", leather_bc, leather_normal, leather_ao, leather_rough, np.zeros((SIZE, SIZE), dtype=np.float32))

# ==============================================================================
# 2. WOOL FABRIC / TWEED (Suspect coats, dresses, baize)
# ==============================================================================
print("Generating Fabric Wool / Tweed...")
# Twill weave pattern
x_coords = np.arange(SIZE)
y_coords = np.arange(SIZE)
xx, yy = np.meshgrid(x_coords, y_coords)
twill = (np.sin((xx + yy) * (2 * math.pi / 8.0)) + np.sin((xx - yy) * (2 * math.pi / 8.0))) * 0.5 + 0.5
wool_fuzz = seamless_noise(SIZE, scale=48.0, octaves=3, seed=203)
fabric_h = twill * 0.6 + wool_fuzz * 0.4
fabric_normal = height_to_normal(fabric_h, strength=2.2)
fabric_ao = compute_ao(fabric_h, radius=2, strength=1.8)
fabric_rough = 0.75 + 0.20 * wool_fuzz
fabric_bc = np.stack([0.90 + 0.10 * twill]*3, axis=-1)
save_pbr_set("T_Fabric_Wool", fabric_bc, fabric_normal, fabric_ao, fabric_rough, np.zeros((SIZE, SIZE), dtype=np.float32))

# ==============================================================================
# 3. SKIN (Suspect faces, hands, pores, subtle epidermal relief)
# ==============================================================================
print("Generating Skin...")
pore_noise = seamless_noise(SIZE, scale=64.0, octaves=3, seed=204)
skin_folds = seamless_noise(SIZE, scale=8.0, octaves=4, seed=205)
skin_h = pore_noise * 0.5 + skin_folds * 0.5
skin_normal = height_to_normal(skin_h, strength=1.4)
skin_ao = compute_ao(skin_h, radius=2, strength=1.2)
skin_rough = 0.45 + 0.25 * pore_noise # oily T-zone vs drier cheeks
skin_bc = np.stack([0.95 + 0.05 * skin_folds]*3, axis=-1)
save_pbr_set("T_Skin", skin_bc, skin_normal, skin_ao, skin_rough, np.zeros((SIZE, SIZE), dtype=np.float32))

# ==============================================================================
# 4. CANVAS (Murky oil paintings, picture frames)
# ==============================================================================
print("Generating Canvas...")
canvas_grid = (np.sin(xx * (2 * math.pi / 4.0)) * np.sin(yy * (2 * math.pi / 4.0))) * 0.5 + 0.5
brush_strokes = seamless_noise(SIZE, scale=3.0, octaves=4, persistence=0.6, seed=206)
canvas_h = canvas_grid * 0.4 + brush_strokes * 0.6
canvas_normal = height_to_normal(canvas_h, strength=2.8)
canvas_ao = compute_ao(canvas_h, radius=3, strength=2.0)
canvas_rough = 0.65 + 0.25 * brush_strokes
canvas_bc = np.stack([0.88 + 0.12 * canvas_grid]*3, axis=-1)
save_pbr_set("T_Canvas", canvas_bc, canvas_normal, canvas_ao, canvas_rough, np.zeros((SIZE, SIZE), dtype=np.float32))

# ==============================================================================
# 5. ANTIQUE BRASS / METAL (Sconces, banker's lamp, fireplace, frames)
# ==============================================================================
print("Generating Antique Brass...")
brushed_lines = seamless_noise(SIZE, scale=1.0, octaves=4, persistence=0.5, seed=207)
# Horizontal brushing
brush_y = (np.sin(yy * (2 * math.pi / 3.0))) * 0.5 + 0.5
tarnish_spots = seamless_noise(SIZE, scale=12.0, octaves=4, persistence=0.7, seed=208)
metal_h = brush_y * 0.3 + brushed_lines * 0.4 + tarnish_spots * 0.3
metal_normal = height_to_normal(metal_h, strength=2.0)
metal_ao = compute_ao(tarnish_spots, radius=4, strength=2.5)
metal_rough = 0.25 + 0.45 * tarnish_spots
metal_metallic = 0.95 - 0.25 * tarnish_spots # tarnish reduces pure metallicity
brass_bc = np.stack([0.90 + 0.10 * (1.0 - tarnish_spots)]*3, axis=-1)
save_pbr_set("T_Brass", brass_bc, metal_normal, metal_ao, metal_rough, metal_metallic)

print("=== ALL CHARACTER & PROP TEXTURES GENERATED SUCCESSFULLY ===")
