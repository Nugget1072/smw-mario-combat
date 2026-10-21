"""Level select test, part 1: every entry of the list, started from the map and from inside a level.

Per entry and start place, one pilot run:
  map:   loadstate 9 (Yoshi's Island 1), `kill`, wait for the map; `progress` (hash of SMW's save buffer)
  level: loadstate 9, in Yoshi's Island 1
then `levelselect <level>`, 240 frames, `whereami` + shot (the level must be loading or loaded: game mode 14 and the
level number the entry names; the bonus game is level 000, the wings areas 0C8 / 1C8), 150 frames of running and
jumping right + shot, `kill`, 800 frames (a level without a timer shows TIME UP first), `whereami` + `progress` + shot. After the death Mario must be back on the
map, on his own submap and tile (from the map) / where YI1's map put him, with the save buffer exactly as before
(practice run: nothing beaten, no path, no midway), and no practice session left open.
Usage: python -I levelselect_test.py <root> [--jobs N] [levels...]
Writes work/pilot/levelselect_test/<level>_<map|level>/ and results.json; prints failures and a summary.
"""
import json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = sys.argv[1]
argv = sys.argv[2:]
jobs = 6
if '--jobs' in argv:
    i = argv.index('--jobs')
    jobs = int(argv[i + 1])
    del argv[i:i + 2]
only = [a.upper() for a in argv]
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'levelselect_test')
os.makedirs(out_root, exist_ok=True)


def pilot(d, lines):
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(lines) + '\n')
    try:
        r = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                           text=True, timeout=300)
        return r.returncode, r.stdout
    except subprocess.TimeoutExpired:
        return 'timeout', ''


# the list, from the game itself
rc, out = pilot(os.path.join(out_root, '_list'), ['wait 2', 'levellist', 'quit'])
ENTRIES = [m.groups() for m in re.finditer(r'levellist (\d+) group (\d+) (.*?) level (\w+) kind (\d) translevel (\w+) (.*)', out)]
W = re.compile(r'whereami: mode (\w+) level (\w+) sublevels (\d+) session (\d) map (\d+) pos (\d+),(\d+) ow_level (\w+) '
               r'state (\d+) lives (\d+) power (\d+) yoshi (\d+) time (\d+) x (-?\d+)')
P = re.compile(r'progress: (\w+)')


def expected_level(e):
    lv, kind = int(e[3], 16), int(e[4])
    return lv  # (the bonus game: 000; the wings areas: their own number)


def run(e, start):
    lv = e[3]
    d = os.path.join(out_root, '%s_%s' % (lv, start))
    pre = ['loadstate 9', 'wait 5', 'cheat 0 0', 'cheat 3 0']
    if start == 'map':
        pre += ['kill', 'wait 400']
    pre += ['whereami', 'progress']
    lines = pre + ['levelselect %s' % lv, 'wait 240', 'whereami', 'shot s1', 'hold RIGHT', 'wait 60', 'hold RIGHT B',
                   'wait 30', 'hold RIGHT', 'wait 60', 'hold', 'whereami', 'shot s2', 'kill', 'wait 800', 'whereami',
                   'progress', 'shot s3', 'quit']
    rc, out = pilot(d, lines)
    w = [m.groups() for m in W.finditer(out)]
    p = [m.group(1) for m in P.finditer(out)]
    fails = []
    if rc != 0:
        fails.append('rc %s' % rc)
    if len(w) < 4 or len(p) < 2:
        fails.append('missing output (%d whereami, %d progress)' % (len(w), len(p)))
        return dict(entry=e, start=start, fails=fails, w=w, p=p)
    w0, w1, w2, w3 = w[:4]
    if w1[0] != '14' or int(w1[1], 16) != expected_level(e):
        fails.append('after start: mode %s level %s (want 14 %s)' % (w1[0], w1[1], lv))
    if w1[2] != '0' and int(e[4]) in (0, 1):
        fails.append('started as a room (sublevels %s)' % w1[2])
    if w3[0] != '0E':
        fails.append('after the death: mode %s, not the map' % w3[0])
    if w3[3] != '0':
        fails.append('practice session still open')
    if w0[0] == '0E' and (w3[4], w3[5], w3[6]) != (w0[4], w0[5], w0[6]):
        fails.append('map position %s:%s,%s (was %s:%s,%s)' % (w3[4], w3[5], w3[6], w0[4], w0[5], w0[6]))
    if w0[0] == '0E' and p[1] != p[0]:
        fails.append('save buffer changed %s -> %s' % (p[0], p[1]))
    return dict(entry=e, start=start, fails=fails, w=w, p=p)


jobs_list = [(e, s) for e in ENTRIES for s in ('map', 'level') if not only or e[3] in only]
with ThreadPoolExecutor(jobs) as ex:
    results = list(ex.map(lambda a: run(*a), jobs_list))
json.dump(results, open(os.path.join(out_root, 'results.json'), 'w'), indent=1)
bad = [r for r in results if r['fails']]
for r in bad:
    print('FAIL', r['entry'][3], r['entry'][6], 'from', r['start'], '; '.join(r['fails']))
print('%d runs (%d entries x map / level), %d failed' % (len(results), len(results) // 2, len(bad)))
