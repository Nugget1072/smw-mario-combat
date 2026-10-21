"""MC hit reactions in real levels: for each enemy id, a level it appears in (from the Triple-enemies audit), Mario
flies there (pilot `seek`), lands next to the enemy and punches it with combos (pilot `attack` + taps), 6 times, then
waits. Cheat on and off, Infinite MC life on (so Mario survives to keep punching). Enemies tied to terrain (pipes,
nets, lava, lines, clouds, platforms) and walkers on real slopes, ledges and pits are covered here, which the
Yoshi's House sweep (mchits_sweep.py) can't do.
Usage: python -I mchits_levels.py <root> [ids...] [--jobs N]
Writes work/pilot/mchits_levels/<id>_<on|off>/ (shots, sprite tables) and results.json.
"""
import json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = sys.argv[1]
argv = sys.argv[2:]
jobs = 4
if '--jobs' in argv:
    i = argv.index('--jobs')
    jobs = int(argv[i + 1])
    del argv[i:i + 2]
ids = [a.upper() for a in argv]
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'mchits_levels')
os.makedirs(out_root, exist_ok=True)
audit = json.load(open(os.path.join(root, 'work', 'pilot', 'triple_audit', 'summary.json')))['rows']
LEVELS = {r['id']: r['levels'] for r in audit}
# enemies the Triple audit doesn't count (they are never tripled): Lakitu, Magikoopa, Hammer Bro
LEVELS.setdefault('1E', ['107', '009']); LEVELS.setdefault('1F', ['01F', '10D']); LEVELS.setdefault('9B', ['012', '109'])


def script(sid, level, on):
    s = ['loadstate 9', 'wait 5', 'cheat 0 1', 'cheat 3 1', 'cheat 25 %d' % on, 'warp %s' % level, 'wait 120',
         'seek %s' % sid, 'wait 40', 'shot p0', 'sprites']
    for n in range(1, 7):
        s += ['attack %s 30' % sid, 'wait 4']
        for t in range(3):
            s += ['hold Y', 'wait 2', 'hold', 'wait 5']
        s += ['shot k%d' % n, 'wait 20', 'shot p%d' % n, 'sprites']
    s += ['hold', 'wait 120', 'shot end', 'sprites', 'status', 'quit']
    return '\n'.join(s) + '\n'


LINE = re.compile(r'sprite slot (\d+) id ([0-9A-F]{2}) status (\d+) x=(-?\d+) y=(-?\d+).*\(frame (\d+)\)')
HITS = re.compile(r' mc-hits life=([\d.-]+) knocked=(\d) kind=(\d)')


def run(sid, level, on):
    d = os.path.join(out_root, '%s_%s' % (sid, 'on' if on else 'off'))
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write(script(sid, level, on))
    try:
        r = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                           text=True, timeout=400)
        rc, out = r.returncode, r.stdout
    except subprocess.TimeoutExpired:
        rc, out = 'timeout', ''
    snaps, cur, found = [], None, None
    for line in out.splitlines():
        if line.startswith('seek:'):
            found = 'found' in line
        m = LINE.match(line)
        if not m:
            continue
        if cur is None or cur['frame'] != int(m.group(6)):
            cur = dict(frame=int(m.group(6)), sprites=[])
            snaps.append(cur)
        h = HITS.search(line)
        if m.group(2) == sid:
            cur['sprites'].append(dict(status=int(m.group(3)), x=int(m.group(4)), y=int(m.group(5)),
                                       life=float(h.group(1)) if h else None, knocked=h.group(2) == '1' if h else None))
    try:
        status = json.load(open(os.path.join(d, 'status.json')))
    except (OSError, ValueError):
        status = {}
    return dict(rc=rc, found=found, snaps=snaps, status=status)


def one(sid):
    level = LEVELS[sid][0]
    res = dict(level=level, on=run(sid, level, 1), off=run(sid, level, 0))
    lives = [s['life'] for snap in res['on']['snaps'] for s in snap['sprites'] if s['life'] is not None]
    print(sid, level, 'found', res['on']['found'], 'rc', res['on']['rc'], res['off']['rc'], 'lives', lives, flush=True)
    return sid, res


with ThreadPoolExecutor(jobs) as ex:
    results = dict(ex.map(one, [i for i in ids if i in LEVELS]))
json.dump(results, open(os.path.join(out_root, 'results.json'), 'w'), indent=1)
