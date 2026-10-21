"""Contact sheets for enemy_cheats_levels.py: one row per enemy (cheats on: s1..s4, cheats off: s4), six rows per sheet,
labelled with the enemy id, the level and the run's exit code / Mario's state. Prints a summary line per enemy.
Usage: python -I enemy_cheats_sheets.py <root>   (writes work/pilot/enemy_cheats_levels/sheet_NN.png)
"""
import json, os, sys
from PIL import Image, ImageDraw

root = sys.argv[1]
d = os.path.join(root, 'work', 'pilot', 'enemy_cheats_levels')
res = json.load(open(os.path.join(d, 'results.json')))
ids = sorted(res)
for sheet in range(0, len(ids), 6):
    part = ids[sheet:sheet + 6]
    img = Image.new('RGB', (256 * 5, 224 * len(part)), (30, 30, 40))
    dr = ImageDraw.Draw(img)
    for row, sid in enumerate(part):
        r = res[sid]
        shots = [os.path.join(d, sid + '_on', 's%d.bmp' % n) for n in range(1, 5)] + [os.path.join(d, sid + '_off', 's4.bmp')]
        for col, f in enumerate(shots):
            if os.path.exists(f):
                img.paste(Image.open(f).convert('RGB'), (col * 256, row * 224))
            label = '%s %s %s' % (sid, r['level'], 'on s%d' % (col + 1) if col < 4 else 'off s4')
            dr.text((col * 256 + 3, row * 224 + 212), label, fill=(255, 255, 0))
    img.save(os.path.join(d, 'sheet_%02d.png' % (sheet // 6)))
for sid in ids:
    r = res[sid]
    st_on, st_off = r['on'].get('status', {}), r['off'].get('status', {})
    print(sid, r['level'], 'rc', r['on']['rc'], r['off']['rc'], 'on', {k: st_on.get(k) for k in ('game_mode', 'player_state', 'mc_life')},
          'off', {k: st_off.get(k) for k in ('game_mode', 'player_state', 'mc_life')})
