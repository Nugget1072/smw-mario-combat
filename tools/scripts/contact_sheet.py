"""Make a labelled contact sheet of PNG/JPG images (nearest-neighbour scaled).

Usage: python -I contact_sheet.py <out.png> <scale> <max_width> <img> [img ...]
"""
import os, sys
from PIL import Image, ImageDraw

out, scale, maxw = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
ims = [(os.path.basename(p), Image.open(p).convert("RGBA")) for p in sys.argv[4:]]
cells = []
for name, im in ims:
    im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
    cells.append((name, im))
pad, lab = 6, 12
rows, row, x, rh = [], [], 0, 0
for name, im in cells:
    w = im.width + pad
    if row and x + w > maxw:
        rows.append((row, rh)); row, x, rh = [], 0, 0
    row.append((x, name, im)); x += w; rh = max(rh, im.height + lab + pad)
if row:
    rows.append((row, rh))
H = sum(r[1] for r in rows)
W = min(maxw, max(sum(c[2].width + pad for c in r[0]) for r in rows))
sheet = Image.new("RGBA", (W, H), (60, 60, 90, 255))
dr = ImageDraw.Draw(sheet)
y = 0
for r, rh in rows:
    for x, name, im in r:
        dr.text((x + 2, y), f"{name} {im.width // scale}x{im.height // scale}", fill=(255, 255, 0, 255))
        sheet.alpha_composite(im, (x, y + lab))
    y += rh
sheet.save(out)
print(out, sheet.size)
