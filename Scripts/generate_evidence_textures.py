import os
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFont

OUT_DIR = r"B:\mysteryOTF\Content\Art\Textures"
os.makedirs(OUT_DIR, exist_ok=True)
SIZE = 1024

def height_to_normal(height, strength=2.0):
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

print("Generating Evidence & Gameplay PBR Textures...")

# ==============================================================================
# 1. VINTAGE POCKET WATCH DIAL
# ==============================================================================
print("1. Pocket Watch Dial...")
watch_img = Image.new("RGBA", (SIZE, SIZE), (242, 238, 226, 255))
draw = ImageDraw.Draw(watch_img)
cx, cy, r_outer = SIZE // 2, SIZE // 2, SIZE // 2 - 40

# Outer brass / gold bevel ring
draw.ellipse((cx - r_outer, cy - r_outer, cx + r_outer, cy + r_outer), outline=(180, 140, 60, 255), width=16)
draw.ellipse((cx - r_outer + 12, cy - r_outer + 12, cx + r_outer - 12, cy + r_outer - 12), outline=(120, 90, 40, 255), width=4)

# 60 Minute railway track
r_track_out = r_outer - 30
r_track_in = r_outer - 44
draw.ellipse((cx - r_track_out, cy - r_track_out, cx + r_track_out, cy + r_track_out), outline=(40, 35, 30, 255), width=3)
draw.ellipse((cx - r_track_in, cy - r_track_in, cx + r_track_in, cy + r_track_in), outline=(40, 35, 30, 255), width=2)

for m in range(60):
    angle = math.radians(m * 6 - 90)
    w = 4 if m % 5 == 0 else 1
    r_in = r_track_in - (6 if m % 5 == 0 else 0)
    x0 = cx + math.cos(angle) * r_track_out
    y0 = cy + math.sin(angle) * r_track_out
    x1 = cx + math.cos(angle) * r_in
    y1 = cy + math.sin(angle) * r_in
    draw.line((x0, y0, x1, y1), fill=(40, 35, 30, 255), width=w)

# Roman numerals
romans = ["XII", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI"]
r_numerals = r_outer - 110
for i, num in enumerate(romans):
    angle = math.radians(i * 30 - 90)
    nx = cx + math.cos(angle) * r_numerals
    ny = cy + math.sin(angle) * r_numerals
    # Draw simple sharp serif style text
    draw.text((nx, ny), num, fill=(30, 25, 20, 255), anchor="mm")

# Subdial at 6 o'clock (seconds track)
sub_cy = cy + 130
sub_r = 90
draw.ellipse((cx - sub_r, sub_cy - sub_r, cx + sub_r, sub_cy + sub_r), outline=(50, 45, 40, 255), width=2)
for s in range(12):
    angle = math.radians(s * 30 - 90)
    sx0 = cx + math.cos(angle) * sub_r
    sy0 = sub_cy + math.sin(angle) * sub_r
    sx1 = cx + math.cos(angle) * (sub_r - 10)
    sy1 = sub_cy + math.sin(angle) * (sub_r - 10)
    draw.line((sx0, sy0, sx1, sy1), fill=(50, 45, 40, 255), width=2)

# Central text: "CHRONOMÈTRE / ASHWORTH & SONS"
draw.text((cx, cy - 110), "A S H W O R T H", fill=(50, 45, 40, 255), anchor="mm")
draw.text((cx, cy - 85), "L O N D O N", fill=(80, 75, 70, 255), anchor="mm")

# Hairline crack running from 10 to 4 across the glass
draw.line((cx - 260, cy - 200, cx - 80, cy - 40), fill=(180, 160, 150, 220), width=2)
draw.line((cx - 80, cy - 40, cx + 110, cy + 90), fill=(180, 160, 150, 220), width=2)
draw.line((cx + 110, cy + 90, cx + 280, cy + 180), fill=(180, 160, 150, 220), width=2)

watch_bc = np.array(watch_img.convert("RGB"))
watch_h = np.ones((SIZE, SIZE), dtype=np.float32) * 0.5
# Inset bevel
Y, X = np.ogrid[:SIZE, :SIZE]
dist_c = np.sqrt((X - cx)**2 + (Y - cy)**2)
watch_h[dist_c > r_outer] = 0.2
watch_h[dist_c > r_outer - 15] = 0.8
# Crack groove
crack_mask = (watch_bc[:, :, 0] < 200) & (dist_c < r_outer - 20)
watch_h[crack_mask] -= 0.15

watch_normal = height_to_normal(watch_h, strength=4.0)
watch_rough = np.full((SIZE, SIZE), 0.18, dtype=np.float32) # smooth glossy enamel & glass
watch_rough[dist_c > r_outer - 15] = 0.35 # brass rim
watch_rough[crack_mask] = 0.6
watch_metal = np.zeros((SIZE, SIZE), dtype=np.float32)
watch_metal[dist_c > r_outer - 15] = 0.95 # brass metallic

watch_ao = np.full((SIZE, SIZE), 255, dtype=np.uint8)
watch_ao[dist_c > r_outer - 15] = 180
watch_orm = np.stack([watch_ao, (watch_rough * 255).astype(np.uint8), (watch_metal * 255).astype(np.uint8)], axis=-1)

Image.fromarray(watch_bc).save(os.path.join(OUT_DIR, "T_WatchDial_BaseColor.png"))
Image.fromarray(watch_normal).save(os.path.join(OUT_DIR, "T_WatchDial_Normal.png"))
Image.fromarray(watch_orm).save(os.path.join(OUT_DIR, "T_WatchDial_ORM.png"))

# ==============================================================================
# 2. VINTAGE HANDWRITTEN LETTER
# ==============================================================================
print("2. Vintage Letter...")
letter_img = Image.new("RGBA", (SIZE, SIZE), (230, 215, 185, 255))
ldraw = ImageDraw.Draw(letter_img)

# Crease folds
ldraw.line((SIZE // 2, 0, SIZE // 2, SIZE), fill=(200, 185, 155, 255), width=4)
ldraw.line((0, SIZE // 3, SIZE, SIZE // 3), fill=(200, 185, 155, 255), width=3)
ldraw.line((0, 2 * SIZE // 3, SIZE, 2 * SIZE // 3), fill=(200, 185, 155, 255), width=3)

# Cursive script lines
for line_y in range(120, SIZE - 120, 48):
    y_var = line_y + int(np.random.randn() * 3)
    lx = 120
    while lx < SIZE - 150:
        word_len = np.random.randint(25, 95)
        # Simulate cursive word loops
        pts = [(lx, y_var + np.random.randint(-3, 4))]
        for step in range(5):
            pts.append((lx + (step + 1) * (word_len // 5), y_var + np.random.randint(-6, 7)))
        ldraw.line(pts, fill=(35, 30, 45, 230), width=3)
        lx += word_len + np.random.randint(18, 35)

# Red wax seal stamp on back
seal_x, seal_y, seal_r = SIZE // 2, SIZE - 180, 90
ldraw.ellipse((seal_x - seal_r, seal_y - seal_r, seal_x + seal_r, seal_y + seal_r), fill=(160, 20, 20, 255))
ldraw.ellipse((seal_x - seal_r + 14, seal_y - seal_r + 14, seal_x + seal_r - 14, seal_y + seal_r - 14), outline=(120, 10, 10, 255), width=6)
ldraw.text((seal_x, seal_y), "M", fill=(120, 10, 10, 255), anchor="mm")

letter_bc = np.array(letter_img.convert("RGB"))
letter_h = np.ones((SIZE, SIZE), dtype=np.float32) * 0.5
letter_h[SIZE//2-2:SIZE//2+2, :] -= 0.15 # vertical fold
letter_h[:, SIZE//3-2:SIZE//3+2] -= 0.15
letter_h[:, 2*SIZE//3-2:2*SIZE//3+2] -= 0.15
# Raised wax seal
Y, X = np.ogrid[:SIZE, :SIZE]
seal_dist = np.sqrt((X - seal_x)**2 + (Y - seal_y)**2)
letter_h[seal_dist < seal_r] = 0.85
letter_h[seal_dist < seal_r - 14] = 0.75

letter_norm = height_to_normal(letter_h, strength=3.0)
letter_rough = np.full((SIZE, SIZE), 0.88, dtype=np.float32) # matte parchment
letter_rough[seal_dist < seal_r] = 0.32 # smooth glossy wax
letter_orm = np.stack([np.full((SIZE, SIZE), 255, dtype=np.uint8), (letter_rough * 255).astype(np.uint8), np.zeros((SIZE, SIZE), dtype=np.uint8)], axis=-1)

Image.fromarray(letter_bc).save(os.path.join(OUT_DIR, "T_Letter_BaseColor.png"))
Image.fromarray(letter_norm).save(os.path.join(OUT_DIR, "T_Letter_Normal.png"))
Image.fromarray(letter_orm).save(os.path.join(OUT_DIR, "T_Letter_ORM.png"))

# ==============================================================================
# 3. TEXTURED RED WOOL YARN (For Board Cable/Yarn Connections)
# ==============================================================================
print("3. Wool Yarn...")
yarn_bc = np.zeros((SIZE, SIZE, 3), dtype=np.uint8)
yarn_bc[:, :, 0] = 190  # Rich deep crimson red
yarn_bc[:, :, 1] = 18
yarn_bc[:, :, 2] = 22

# Spiral twisted yarn fiber normal
twist_y = np.linspace(0, 32 * np.pi, SIZE, endpoint=False)
twist_x = np.linspace(0, 4 * np.pi, SIZE, endpoint=False)
TY, TX = np.meshgrid(twist_y, twist_x)
yarn_h = np.sin(TY + TX) * 0.5 + 0.5

yarn_norm = height_to_normal(yarn_h, strength=4.0)
yarn_rough = np.full((SIZE, SIZE), 245, dtype=np.uint8) # 0.96 matte wool
yarn_orm = np.stack([np.full((SIZE, SIZE), 240, dtype=np.uint8), yarn_rough, np.zeros((SIZE, SIZE), dtype=np.uint8)], axis=-1)

Image.fromarray(yarn_bc).save(os.path.join(OUT_DIR, "T_Yarn_BaseColor.png"))
Image.fromarray(yarn_norm).save(os.path.join(OUT_DIR, "T_Yarn_Normal.png"))
Image.fromarray(yarn_orm).save(os.path.join(OUT_DIR, "T_Yarn_ORM.png"))

# ==============================================================================
# 4. UV LUMINOL EVIDENCE DECALS (Fingerprints & Blood Spatters)
# ==============================================================================
print("4. UV Luminol Evidence Decals...")
# Fingerprint ridge whorls
fp_img = Image.new("L", (SIZE, SIZE), 0)
fp_draw = ImageDraw.Draw(fp_img)
for r in range(20, 380, 16):
    fp_draw.ellipse((cx - r, cy - int(r * 1.3), cx + r, cy + int(r * 1.3)), outline=255, width=6)

# Blood spatter droplets
for _ in range(40):
    bx = cx + np.random.randint(-350, 350)
    by = cy + np.random.randint(-350, 350)
    br = np.random.randint(6, 28)
    fp_draw.ellipse((bx - br, by - br, bx + br, by + br), fill=255)

fp_arr = np.array(fp_img)
# Gaussian blur to soften ridges
from PIL import ImageFilter
fp_smooth = np.array(fp_img.filter(ImageFilter.GaussianBlur(2)))
# Luminol emissive cyan/blue glow
luminol_bc = np.zeros((SIZE, SIZE, 4), dtype=np.uint8)
luminol_bc[:, :, 0] = (fp_smooth * 0.1).astype(np.uint8) # faint red
luminol_bc[:, :, 1] = (fp_smooth * 0.85).astype(np.uint8) # strong cyan green
luminol_bc[:, :, 2] = fp_smooth # strong blue
luminol_bc[:, :, 3] = fp_smooth # alpha mask

Image.fromarray(luminol_bc).save(os.path.join(OUT_DIR, "T_Luminol_Emissive.png"))

print("All evidence & gameplay textures generated successfully!")
