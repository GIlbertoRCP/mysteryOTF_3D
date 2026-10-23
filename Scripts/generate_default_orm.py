import os
import numpy as np
from PIL import Image

OUT_DIR = r"B:\mysteryOTF\Content\Art\Textures"
size = 64

# R = AO (1.0 = 255), G = Roughness (1.0 = 255), B = Metallic (0.0 = 0)
orm_arr = np.zeros((size, size, 3), dtype=np.uint8)
orm_arr[:, :, 0] = 255 # AO
orm_arr[:, :, 1] = 255 # Roughness
orm_arr[:, :, 2] = 0   # Metallic

Image.fromarray(orm_arr).save(os.path.join(OUT_DIR, "T_Default_ORM.png"))
print("Generated T_Default_ORM.png")
