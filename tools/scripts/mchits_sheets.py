"""Contact sheets of the MC hit reactions sweep: one row per sprite id the cheat took over (cheat on), with the frames
before the first punch (p0), right after each of the first three combos (k1-k3) and 20 frames later (p1-p3).
Usage: python -I mchits_sheets.py <root> <out prefix> [ids...]
"""
import json, os, sys
from PIL import Image, ImageDraw

root, prefix = sys.argv[1], sys.argv[2]
sweep = os.path.join(root, 'work', 'pilot', 'mchits_sweep')
ids = sys.argv[3:]
if not ids:
    rows = {}
    for f in os.listdir(sweep):
        if f.startswith('results_') and f != 'results_0F_0F.json':
            rows.update(json.load(open(os.path.join(sweep, f))))
    ids = sorted(i for i, r in rows.items()
                 if any(s['life'] is not None for snap in r['on']['snaps'] for s in snap['sprites'] if s['id'] == i))
SHOTS = ['p0', 'k1', 'p1', 'k2', 'p2', 'k3']
W, H, PER = 256, 120, 6  # the lower part of the screen (y 104-224) at full size, where the enemies are
for page in range(0, len(ids), PER):
    chunk = ids[page:page + PER]
    sheet = Image.new('RGB', (40 + W * len(SHOTS), H * len(chunk)), (20, 20, 30))
    d = ImageDraw.Draw(sheet)
    for r, sid in enumerate(chunk):
        d.text((4, r * H + 4), sid, fill=(255, 255, 0))
        for c, s in enumerate(SHOTS):
            p = os.path.join(sweep, '%s_on' % sid, s + '.bmp')
            if os.path.exists(p):
                sheet.paste(Image.open(p).convert('RGB').crop((0, 224 - H, 256, 224)), (40 + c * W, r * H))
            d.text((40 + c * W + 2, r * H + H - 12), s, fill=(255, 255, 0))
    out = '%s_%02d.png' % (prefix, page // PER)
    sheet.save(out)
    print(out, ' '.join(chunk))
