"""Bakes the manor grounds' ground-material masks to PNG (system Python, no third-party modules).

  python Scripts/gen_grounds.py [size]

Writes SourceArt/Generated/T_Grounds_MaskA.png (R gravel, G lawn, B mud, A cobbles) and
T_Grounds_MaskB.png (R leaf litter), covering grounds_layout.WORLD with north at the top row.
The terrain material samples them by world position (see build_grounds.py).
"""
import os
import struct
import sys
import zlib

import grounds_layout as G

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "SourceArt", "Generated")


def write_png(path, w, h, rows, channels):
    color = {1: 0, 4: 6}[channels]
    raw = b"".join(b"\x00" + bytes(r) for r in rows)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, color, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 6)))
        f.write(chunk(b"IEND", b""))


def main():
    size = int(sys.argv[1]) if len(sys.argv) > 1 else 1024
    os.makedirs(OUT, exist_ok=True)
    x0, y0, x1, y1 = G.WORLD
    rows_a, rows_b = [], []
    for j in range(size):
        y = y1 - (j + 0.5) / size * (y1 - y0)   # north at the top
        ra, rb = [], []
        for i in range(size):
            x = x0 + (i + 0.5) / size * (x1 - x0)
            if G.box_dist(x, y, G.PLAY) > 4000.0:
                gravel = lawn = mud = cobble = 0.0
                leaves = 0.5
            else:
                gravel, lawn, mud, cobble, leaves = G.masks(x, y)
            ra += [int(255 * max(0.0, min(1.0, v)) + 0.5) for v in (gravel, lawn, mud, cobble)]
            rb.append(int(255 * max(0.0, min(1.0, leaves)) + 0.5))
        rows_a.append(ra)
        rows_b.append(rb)
        if j % 128 == 0:
            print("row %d/%d" % (j, size), flush=True)
    write_png(os.path.join(OUT, "T_Grounds_MaskA.png"), size, size, rows_a, 4)
    write_png(os.path.join(OUT, "T_Grounds_MaskB.png"), size, size, rows_b, 1)
    print("wrote masks to " + OUT)


if __name__ == "__main__":
    main()
