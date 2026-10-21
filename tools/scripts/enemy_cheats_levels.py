"""Round 12 enemy cheats in real levels: Fast enemies (34), Chasing enemies (35) and Jumping enemies (36) together.
For each ordinary enemy id, a level it appears in (from the Triple-enemies audit), Mario flies there (pilot `seek`),
lands next to the enemy, backs off a little and stays there for 360 frames, jumping every 45 frames (so jumping
enemies jump), with
Infinite MC life and No damage on. Run with the three cheats on and off (MC Mario).
Checks: no crash, Mario alive at the end, and the enemy's slots over time (sprites lines). Shots s1..s4 (s1, s3 just
after a jump); enemy_cheats_sheets.py makes the contact sheets (on: s1..s4, off: s4).
Usage: python -I enemy_cheats_levels.py <root> [ids...] [--jobs N]
Writes work/pilot/enemy_cheats_levels/<id>_<on|off>/ and results.json.
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
out_root = os.path.join(root, 'work', 'pilot', 'enemy_cheats_levels')
os.makedirs(out_root, exist_ok=True)
audit = json.load(open(os.path.join(root, 'work', 'pilot', 'triple_audit', 'summary.json')))['rows']
LEVELS = {r['id']: r['levels'] for r in audit if r['levels'] and r['kind'] in ('walker', 'flier / swimmer', 'terrain')}
if not ids:
    ids = sorted(LEVELS)


def script(sid, level, on):
    s = ['loadstate 9', 'wait 5', 'cheat 0 1', 'cheat 1 1', 'cheat 3 1']
    s += ['cheat %d %d' % (c, on) for c in (34, 35, 36)]
    # after the seek Mario stands 40 px left of the enemy: he backs off further left, then jumps every 45 frames
    # (s1, s3: 14 frames after a jump, while jumping enemies are in the air; s2, s4: at the end of a 90-frame stretch)
    s += ['warp %s' % level, 'wait 120', 'seek %s' % sid, 'wait 10', 'hold LEFT', 'wait 16', 'hold', 'wait 20',
          'sprites']
    for n in range(1, 5):
        if n % 2:
            s += ['hold B', 'wait 8', 'hold', 'wait 6', 'shot s%d' % n, 'sprites', 'wait 31']
            s += ['hold B', 'wait 8', 'hold', 'wait 37']
        else:
            for j in range(2):
                s += ['hold B', 'wait 8', 'hold', 'wait 37']
            s += ['shot s%d' % n, 'sprites']
    s += ['status', 'quit']
    return '\n'.join(s) + '\n'


LINE = re.compile(r'sprite slot (\d+) id ([0-9A-F]{2}) status (\d+) x=(-?\d+) y=(-?\d+).*\(frame (\d+)\)')


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
        cur['sprites'].append(dict(id=m.group(2), status=int(m.group(3)), x=int(m.group(4)), y=int(m.group(5))))
    try:
        status = json.load(open(os.path.join(d, 'status.json')))
    except (OSError, ValueError):
        status = {}
    return dict(rc=rc, found=found, snaps=snaps, status=status)


def one(sid):
    level = LEVELS[sid][0]
    res = dict(level=level, on=run(sid, level, 1), off=run(sid, level, 0))
    count = [sum(s['id'] == sid for s in snap['sprites']) for snap in res['on']['snaps']]
    print(sid, level, 'found', res['on']['found'], 'rc', res['on']['rc'], res['off']['rc'], 'on count', count, flush=True)
    return sid, res


with ThreadPoolExecutor(jobs) as ex:
    results = dict(ex.map(one, [i for i in ids if i in LEVELS]))
json.dump(results, open(os.path.join(out_root, 'results.json'), 'w'), indent=1)
