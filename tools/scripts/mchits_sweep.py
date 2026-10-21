"""MC hit reactions audit: every normal sprite (0x00-0xC8) punched again and again by Mario Combat Mario in Yoshi's
House, with the "MC hit reactions" cheat on and, as the reference, off.

Per sprite and run: the sprite is spawned 40 px to Mario's right, watched for 40 frames, then Mario walks up to it and
punches it (pilot `attack`), up to 8 times, 30 frames apart, then 150 frames more. After every punch the sprite table
is printed (life, knocked flag, position, status) and a screenshot is taken. Each run is logged (h.csv).
Usage: python -I mchits_sweep.py <root> [first_id last_id] [--jobs N]
Writes work/pilot/mchits_sweep/<id>_<on|off>/ and results_<lo>_<hi>.json; mchits_report.py summarises them.
"""
import csv, json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = sys.argv[1]
args = [a for a in sys.argv[2:] if not a.startswith('--')]
lo, hi = (int(args[0], 16), int(args[1], 16)) if len(args) >= 2 else (0, 0xC8)
jobs = int(sys.argv[sys.argv.index('--jobs') + 1]) if '--jobs' in sys.argv else 4
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'mchits_sweep')
os.makedirs(out_root, exist_ok=True)
PUNCHES = 8


def script(sid, on):
    s = ['loadstate 8', 'wait 5', 'cheat 0 0', 'cheat 25 %d' % on, 'log h.csv', 'spawn %02X 40 0' % sid, 'wait 40',
         'shot p0', 'sprites']
    for n in range(1, PUNCHES + 1):  # walk up and jab, then three more taps: the combo's stronger hits fling it
        s += ['attack %02X 30' % sid, 'wait 4']
        for t in range(3):
            s += ['hold Y', 'wait 2', 'hold', 'wait 5']
        s += ['shot k%d' % n, 'wait 20', 'shot p%d' % n, 'sprites']
    s += ['hold', 'wait 150', 'shot end', 'sprites', 'log off', 'quit']
    return '\n'.join(s) + '\n'


LINE = re.compile(r'sprite slot (\d+) id ([0-9A-F]{2}) status (\d+) x=(-?\d+) y=(-?\d+).*\(frame (\d+)\)')
HITS = re.compile(r' mc-hits life=([\d.-]+) knocked=(\d) kind=(\d)')


def parse(out, sid):
    """The sprite table after each `sprites` command: the slots holding the spawned sprite id."""
    snaps, cur = [], None
    attacks = 0
    for line in out.splitlines():
        if line.startswith('attack:'):
            attacks += 1
        m = LINE.match(line)
        if not m:
            continue
        frame = int(m.group(6))
        h = HITS.search(line)
        if cur is None or cur['frame'] != frame:
            cur = dict(frame=frame, sprites=[])
            snaps.append(cur)
        cur['sprites'].append(dict(slot=int(m.group(1)), id=m.group(2), status=int(m.group(3)), x=int(m.group(4)),
                                   y=int(m.group(5)), life=float(h.group(1)) if h else None,
                                   knocked=h.group(2) == '1' if h else None))
    return snaps, attacks


def run(sid, on):
    d = os.path.join(out_root, '%02X_%s' % (sid, 'on' if on else 'off'))
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write(script(sid, on))
    try:
        r = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                           text=True, timeout=300)
        rc, out = r.returncode, r.stdout
    except subprocess.TimeoutExpired:
        rc, out = 'timeout', ''
    snaps, attacks = parse(out, sid)
    try:
        rows = list(csv.DictReader(open(os.path.join(d, 'h.csv'))))
    except OSError:
        rows = []
    life = [float(r['mclife']) for r in rows]
    return dict(rc=rc, attacks=attacks, snaps=snaps, mc_life_min=min(life) if life else None,
                mc_died=any(r['state'] == '9' for r in rows), frames=len(rows))


def one(sid):
    res = {'on': run(sid, 1), 'off': run(sid, 0)}
    print('%02X' % sid, json.dumps({k: dict(rc=v['rc'], attacks=v['attacks'], mc_life_min=v['mc_life_min'])
                                    for k, v in res.items()}), flush=True)
    return '%02X' % sid, res


with ThreadPoolExecutor(jobs) as ex:
    results = dict(ex.map(one, range(lo, hi + 1)))
json.dump(results, open(os.path.join(out_root, 'results_%02X_%02X.json' % (lo, hi)), 'w'), indent=1)
