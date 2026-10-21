"""Round 13 Respawning enemies (cheat 39) in real levels. For each ordinary enemy id, a level it appears in (from the
Triple-enemies audit): Mario flies there (pilot `seek`), defeats it with Super punch (pilot `attack`), then flies
400 px on and back (pilot `glideby`, high above the ground) so its spot scrolls out of view and in again. Cheat on and
off, Infinite MC life on. Sprite tables: S0 after the seek, S1 after the attack, S2 right after coming back.
Result per enemy: defeated (none of its id alive in S1 near the spot, or fewer than in S0) and back (alive again in S2).
Usage: python -I respawn_levels.py <root> [ids...] [--jobs N]
Writes work/pilot/respawn_levels/<id>_<on|off>/ (shots s1, s2) and results.json; prints a summary.
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
out_root = os.path.join(root, 'work', 'pilot', 'respawn_levels')
os.makedirs(out_root, exist_ok=True)
audit = json.load(open(os.path.join(root, 'work', 'pilot', 'triple_audit', 'summary.json')))['rows']
LEVELS = {r['id']: r['levels'] for r in audit if r['levels'] and r['kind'] in ('walker', 'flier / swimmer', 'terrain')}
if not ids:
    ids = sorted(LEVELS)


def script(sid, level, on):
    s = ['loadstate 9', 'wait 5', 'cheat 0 1', 'cheat 3 1', 'cheat 11 1', 'cheat 39 %d' % on, 'warp %s' % level,
         'wait 120', 'seek %s' % sid, 'wait 40', 'peek 13', 'sprites']
    for n in range(2):
        s += ['attack %s 30' % sid, 'wait 4']
        for t in range(3):
            s += ['hold Y', 'wait 2', 'hold', 'wait 5']
    s += ['wait 60', 'shot s1', 'peek 13', 'sprites', 'glideby 400 -100', 'glideby -400 -40', 'shot s2', 'peek 13', 'sprites',
          'status', 'quit']
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
                           text=True, timeout=600)
        rc, out = r.returncode, r.stdout
    except subprocess.TimeoutExpired:
        rc, out = 'timeout', ''
    snaps, cur, found = [], None, None
    for line in out.splitlines():
        if line.startswith('seek:'):
            found = 'found' in line
        if line.startswith('peek 00013'):  # each sprite table follows a peek (an empty table prints nothing)
            cur = dict(sprites=[])
            snaps.append(cur)
            continue
        m = LINE.match(line)
        if not m or cur is None:
            continue
        cur['sprites'].append(dict(id=m.group(2), status=int(m.group(3)), x=int(m.group(4)), y=int(m.group(5))))
    try:
        status = json.load(open(os.path.join(d, 'status.json')))
    except (OSError, ValueError):
        status = {}
    alive = [sum(s['id'] == sid and s['status'] >= 8 for s in sn['sprites']) for sn in snaps]
    return dict(rc=rc, found=found, alive=alive, status=status)


def one(sid):
    level = LEVELS[sid][0]
    res = dict(level=level, on=run(sid, level, 1), off=run(sid, level, 0))
    print(sid, level, 'found', res['on']['found'], 'rc', res['on']['rc'], res['off']['rc'], 'alive on', res['on']['alive'],
          'off', res['off']['alive'], flush=True)
    return sid, res


with ThreadPoolExecutor(jobs) as ex:
    results = dict(ex.map(one, [i for i in ids if i in LEVELS]))
json.dump(results, open(os.path.join(out_root, 'results.json'), 'w'), indent=1)
