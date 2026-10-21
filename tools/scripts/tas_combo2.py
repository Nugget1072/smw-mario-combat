"""The combo video's combo, closed loop (round 19): on a tas.py session near Mario Combat's Bowser (King Koopas and
Instant hits cheats), every frame read Bowser (the pilot's `sprites` line: life, root, sub) and Mario (status), and:
keep within punch reach (walk toward him), face him, and jab on the frame his hurt animation (root 22) ends, so he
goes straight back into it; with --mid S also jab once while he is at hurt sub S. A hit does not restart his hurt
animation (MC's gotoAndStop on the frame it is already on), so this keeps him from ever acting.
A combo is a run of hits with him out of root 22 for at most IDLE frames in a row. Stops at --target hits.
Usage: python -I tas_combo2.py <session> <checkpoint> [--target 20] [--mid S] [--reach R] [--frames N]
"""
import contextlib, io, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tas  # noqa: E402

name, cp = sys.argv[1], sys.argv[2]
arg = lambda k, d: int(sys.argv[sys.argv.index(k) + 1]) if k in sys.argv else d
target, mid, reach, frames = arg('--target', 20), arg('--mid', -1), arg('--reach', 20), arg('--frames', 900)
close = arg('--close', 8)
stopsub = arg('--stopsub', 8)  # no walking after this hurt sub: Mario's speed adds to the jab's knockback  # closer than this while he is hurt: step back (each jab lunges Mario forward)
IDLE = 2
KOOPAS = {'00', '01', '02', '03', '04', '05', '06', '07'}


def rewind_to(c):
    s = tas.load(name)
    n = s['checkpoints'][c]
    tas.kill(s)
    s['moves'] = s['moves'][:n]
    s['checkpoints'] = {k: v for k, v in s['checkpoints'].items() if v <= n}
    tas.launch(name, s)
    with contextlib.redirect_stdout(io.StringIO()):
        tas.send(name, s, [])


def step(keys):
    s = tas.load(name)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        tas.send(name, s, ['hold %s' % ' '.join(keys), 'wait 1', 'sprites'], shot=False)  # one frame a step
    st = json.loads(re.search(r'STATUS (\{.*\})', buf.getvalue()).group(1))
    with open(os.path.join(tas.sdir(name), 'game_stdout.txt')) as f:
        f.seek(max(0, os.path.getsize(f.name) - 6000))
        tail = f.read()
    b = re.findall(r'bowser life=([\d.]+) root=(\d+) sub=(\d+)', tail)
    return st, (float(b[-1][0]), int(b[-1][1]), int(b[-1][2])) if b else None


rewind_to(cp)
st, bw = step([])
combo = best = idle = 0
prev_life = bw[0] if bw else 50.0
last_root, mid_done, prev_y = None, False, False
log = []
for n in range(frames):
    keys = []
    kp = [p for p in st.get('sprites_near', []) if p['id'] in KOOPAS and p['st'] == 8]
    k = min(kp, key=lambda p: abs(p['dx'])) if kp else None
    if k is None or bw is None:
        keys = ['RIGHT']
    else:
        life, root, sub = bw
        toward = 'RIGHT' if k['dx'] > 0 else 'LEFT'
        facing_ok = st.get('facing_right') == (toward == 'RIGHT')
        if root == 22 and last_root != 22:
            mid_done = False
        away = 'LEFT' if toward == 'RIGHT' else 'RIGHT'
        if root == 22 and sub <= 9 and abs(k['dx']) < close:
            keys = [away]
        elif not facing_ok or (abs(k['dx']) > reach and root == 22 and sub <= stopsub and n % 2 == 0) or \
                (abs(k['dx']) > reach + 10 and root != 22):
            keys = [toward]  # (also through his knockback: he must be in reach the frame his hurt ends)
        if facing_ok and abs(k['dx']) <= reach + 12:
            if root != 22 and (last_root == 22 or combo == 0):
                keys.append('Y')  # he just left his hurt animation (or the combo's first hit)
            elif root == 22 and sub >= mid >= 0 and not mid_done:
                keys.append('Y')
                mid_done = True
        last_root = root
    if 'Y' in keys and prev_y:
        keys.remove('Y')  # (a press needs Y let go of first)
    prev_y = 'Y' in keys
    st, bw = step(keys)
    if bw:
        life, root, sub = bw
        hit = life < prev_life - 0.01
        if hit:
            combo += 1
            best = max(best, combo)
            idle = 0
        elif root != 22:
            idle += 1
            if idle > IDLE:
                combo = 0
        else:
            idle = 0
        prev_life = life
        log.append('%d:%d/%d%s%s@%s%s' % (st.get('frame', 0), root, sub, '*' if hit else '', 'Y' if 'Y' in keys else '',
                                          k['dx'] if k else '-', '>' if 'RIGHT' in keys else '<' if 'LEFT' in keys else ''))
        if combo >= target:
            break
print(' '.join(log[-300:]))
print('best combo %d (now %d), Bowser life %s, Mario life %s' % (best, combo, bw and bw[0], st.get('mc_life')))
if combo >= target:
    s = tas.load(name)
    s['checkpoints']['combo_done'] = len(s['moves'])
    tas.save(name, s)
    print('COMBO %d (checkpoint combo_done)' % combo)
