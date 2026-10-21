"""Replay a play.py session (an agent's moves) on the current build, to check a reported issue.

Usage: python -I replay_session.py <session> <checkpoint|end|N moves> [--then "<move>; <move>" ...] [--every N]
                                   [--pilot extra pilot lines, e.g. "hitboxes 1"] [--name <out name>]
Runs games/SuperMarioWorld/smw.exe with the session's base commands and moves (up to the checkpoint), then the extra
moves, logging every frame (work/pilot/<name>/rp.csv) and taking a shot every N frames of the extra moves; a contact
sheet lands in work/pilot/<name>_sheet.png. The sprite table is printed at the end (`sprites`).
"""
import json, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
args = sys.argv[1:]
sess, cp = args[0], args[1]
then, every, extra, name = [], 10, [], None
i = 2
while i < len(args):
    if args[i] == '--then':
        then += [m.strip() for m in args[i + 1].split(';') if m.strip()]
        i += 2
    elif args[i] == '--every':
        every = int(args[i + 1]); i += 2
    elif args[i] == '--pilot':
        extra.append(args[i + 1]); i += 2
    elif args[i] == '--name':
        name = args[i + 1]; i += 2
    else:
        sys.exit('unknown argument ' + args[i])
s = json.load(open(os.path.join(ROOT, 'work', 'play', 'sessions', sess, 'session.json')))
moves = s['moves']
if cp == 'end':
    n = len(moves)
elif cp.isdigit():
    n = int(cp)
else:
    n = s['checkpoints'][cp]
name = name or 'rp_%s_%s' % (sess, cp)
out = os.path.join(ROOT, 'work', 'pilot', name)
os.makedirs(out, exist_ok=True)
for f in os.listdir(out):
    os.remove(os.path.join(out, f))
lines = s['base'] + extra + moves[:n] + ['shot r000_checkpoint', 'log rp.csv']
k, since = 1, 0
for m in then:
    if m.startswith('wait'):
        left = int(m.split()[1])
        while left > 0:  # a shot runs one frame itself: it takes the last frame of its stretch (timing unchanged)
            step = min(left, every - since)
            left -= step
            since += step
            if since >= every:
                if step > 1:
                    lines.append('wait %d' % (step - 1))
                lines.append('shot r%03d' % k); k += 1; since = 0
            else:
                lines.append('wait %d' % step)
    else:
        lines.append(m)
lines += ['shot r%03d_end' % k, 'sprites', 'status', 'quit']
script = os.path.join(out, 'script.txt')
open(script, 'w').write('\n'.join(lines) + '\n')
game = os.path.join(ROOT, 'games', 'SuperMarioWorld')
r = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', script, '--out', out], cwd=game, capture_output=True,
                   text=True, timeout=600)
print('\n'.join(l for l in r.stdout.splitlines() if l.startswith('sprite slot')))
st = os.path.join(out, 'status.json')
if os.path.exists(st):
    print('STATUS ' + open(st).read().strip())
try:
    from PIL import Image, ImageDraw
    shots = sorted(f for f in os.listdir(out) if f.endswith('.bmp'))
    cols = 4
    rows = (len(shots) + cols - 1) // cols
    sheet = Image.new('RGB', (256 * cols, 224 * rows), (30, 30, 40))
    d = ImageDraw.Draw(sheet)
    for j, f in enumerate(shots):
        x, y = (j % cols) * 256, (j // cols) * 224
        sheet.paste(Image.open(os.path.join(out, f)).convert('RGB'), (x, y))
        d.text((x + 3, y + 212), f[:-4], fill=(255, 255, 0))
    sheet.save(os.path.join(ROOT, 'work', 'pilot', name + '_sheet.png'))
    print('SHEET work/pilot/%s_sheet.png' % name)
except Exception as e:
    print('no sheet:', e)
