"""Side-by-side sheet: real Mario Combat screenshots (left) vs the SMW port (right) for the same moves.

Usage: python -I side_by_side.py <mc_shots_dir> <port_shots_dir> <out.png> name1 name2 ...
Mario is located in both images by his cap colours; the port crop is scaled to the original's on-screen size
(MC: 3 stage px per art pixel x window DPI scale; port: 1 art pixel = 1 SNES pixel).
"""
import os, sys
import numpy as np
from PIL import Image, ImageDraw

mc_dir, port_dir, out = sys.argv[1:4]
names = sys.argv[4:]
CAPS = [(255, 66, 115), (181, 41, 99), (173, 41, 99)]


def find(img, tol):
    a = np.asarray(img.convert("RGB")).astype(int)
    m = np.zeros(a.shape[:2], bool)
    for c in CAPS:
        m |= (np.abs(a - c).sum(axis=2) <= tol)
    ys, xs = np.nonzero(m)
    if not len(xs):
        return None
    return int(np.median(xs)), int(np.median(ys))


cells = []
for n in names:
    mc = Image.open(os.path.join(mc_dir, n + ".png")).convert("RGB")
    port = Image.open(os.path.join(port_dir, n + ".bmp")).convert("RGB")
    scale = mc.width / 700 * 3  # window DPI scale x 3 stage px per art pixel
    pc = find(port, 30)
    mcc = find(mc, 24)
    if pc is None or mcc is None:
        print("Mario not found in", n, "port" if pc is None else "mc")
        continue
    pw, ph = 56, 56
    pcrop = port.crop((pc[0] - pw // 2, pc[1] - ph // 2 - 4, pc[0] + pw // 2, pc[1] + ph // 2 - 4))
    pcrop = pcrop.resize((int(pw * scale), int(ph * scale)), Image.NEAREST)
    mcrop = mc.crop((mcc[0] - int(pw * scale) // 2, mcc[1] - int((ph // 2 + 4) * scale),
                     mcc[0] + int(pw * scale) // 2, mcc[1] + int((ph // 2 - 4) * scale)))
    cells.append((n, mcrop, pcrop))
W = cells[0][1].width
H = cells[0][1].height
cols = 2
sheet = Image.new("RGB", (cols * (2 * W + 30), ((len(cells) + cols - 1) // cols) * (H + 24)), (30, 30, 40))
d = ImageDraw.Draw(sheet)
for i, (n, a, b) in enumerate(cells):
    x = (i % cols) * (2 * W + 30)
    y = (i // cols) * (H + 24)
    d.text((x + 4, y + 4), f"{n}:  Mario Combat (Flash)   |   SMW port", fill=(255, 255, 0))
    sheet.paste(a, (x, y + 20))
    sheet.paste(b, (x + W + 4, y + 20))
sheet.save(out)
print(out, sheet.size)
