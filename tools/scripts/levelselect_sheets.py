"""Contact sheets for levelselect_test.py: one row per entry (from the map: s1 started, s2 after running; from a
level: s1 started; from the map: s3 back on the map after the death), 8 rows per sheet, labelled.
Usage: python -I levelselect_sheets.py <root>   (writes work/pilot/levelselect_test/sheet_NN.png)
"""
import json, os, sys
from PIL import Image, ImageDraw

root = sys.argv[1]
d = os.path.join(root, 'work', 'pilot', 'levelselect_test')
res = json.load(open(os.path.join(d, 'results.json')))
entries = []
for r in res:
    if r['entry'] not in entries:
        entries.append(r['entry'])
cols = [('map', 's1', 'start (map)'), ('map', 's2', 'after running'), ('level', 's1', 'start (level)'),
        ('map', 's3', 'map after death')]
for sheet in range(0, len(entries), 8):
    part = entries[sheet:sheet + 8]
    img = Image.new('RGB', (256 * 4, 224 * len(part)), (30, 30, 40))
    dr = ImageDraw.Draw(img)
    for row, e in enumerate(part):
        for c, (start, shot, label) in enumerate(cols):
            f = os.path.join(d, '%s_%s' % (e[3], start), shot + '.bmp')
            if os.path.exists(f):
                img.paste(Image.open(f).convert('RGB'), (c * 256, row * 224))
            dr.text((c * 256 + 3, row * 224 + 212), '%s %s: %s' % (e[3], e[6], label), fill=(255, 255, 0))
    img.save(os.path.join(d, 'sheet_%02d.png' % (sheet // 8)))
print(len(entries), 'entries,', (len(entries) + 7) // 8, 'sheets')
