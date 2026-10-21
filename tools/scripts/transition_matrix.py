"""Transition matrix: the level select started from many kinds of places, compared with the same level started from
the map.

FROM (where Mario is when the level select starts a level):
  map         the overworld (save state 9, `kill`, wait)
  level       inside Yoshi's Island 1 (save state 9)
  pipe_room   the room behind Yoshi's Island 3's pipe, entered through the pipe the game's own way
  secondary   a room entered through a secondary exit (`warp 1CB 1`: SMW's secondary-entrance flag set)
  iggy        Iggy's mode 7 room (tilting platform), after 200 frames of the fight
  reznor      a Reznor room (rotating wheel), after 200 frames
  bigboo      Big Boo's ghost house room
  bowser      Bowser's room, after 400 frames (the clown car up, the battle on)
  water       Soda Lake (011, under water)
  vertical    Vanilla Secret 1 (109, a vertical level)
  ghost       Donut Ghost House (004)
  layer2      Valley of Bowser 2 (115, moving layer-2 walls)
  wings       Yoshi's wings area (0C8)
  palace      Yellow Switch Palace (014)
  autoscroll  Butter Bridge 1 (00C, the screen scrolls by itself)
  castle      Iggy's Castle (101, after its door scene)
TO (chosen in the level select): 106 Yoshi's Island 2, 109 Vanilla Secret 1 (vertical), 011 Soda Lake (water),
  101 Iggy's Castle (castle door scene), 004 Donut Ghost House, 115 Valley of Bowser 2, 000 the 1-UP bonus game,
  0C8 Yoshi's wings area, 095 the unused Reznor room, 10D Bowser's Front Door, and 'rush' = BOSS RUSH > START (Iggy).
For every pair: after the start, Mario holds RIGHT for 150 frames (No damage). Recorded at the start (mode 14 + 30
frames) and after the walk: level, game mode, player state, Mario's x / y, the camera (layer 1 x / y), mode 7
rotation, SMW's scroll flags. A pair FAILS when it differs from the same target started from the map: another
level, another player state, Mario more than 24 px from the map start's place, a camera that does not follow
him as it does from the map (more than 32 px off), mode 7 rotation left over, or the run crashed.
With --noclear the level select's clean-up of the level RAM is switched off (pilot `lsclear 0`) for every FROM but
the map: the failures it then shows are what the clean-up fixes.
Usage: python -I transition_matrix.py <root> [--jobs N] [--from a,b] [--to x,y] [--noclear]
Writes work/pilot/transition_matrix/<from>__<to>/ (script, out.txt, shots) and transition_matrix.txt there.
"""
import os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = sys.argv[1]
jobs = int(sys.argv[sys.argv.index('--jobs') + 1]) if '--jobs' in sys.argv else 6
NOCLEAR = '--noclear' in sys.argv  # without the starts' clean-up (pilot `lsclear 0`): what it fixes
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'transition_matrix' + ('_noclear' if NOCLEAR else ''))

S9 = ['loadstate 9', 'wait 5', 'cheat 1 1', 'cheat 3 1']
FROM = {
    'map': S9 + ['kill', 'wait 400'],
    'level': S9 + ['wait 60'],
    'pipe_room': S9 + ['warp 03', 'wait 150', 'glide 1752 120', 'wait 40', 'hold DOWN', 'wait 40', 'hold',
                       'waitmode 14', 'wait 120'],
    'secondary': S9 + ['warp 1CB 1', 'waitmode 0F 60', 'waitmode 14', 'wait 120'],
    'iggy': S9 + ['warp 1F6', 'waitmode 0F 60', 'waitmode 14', 'wait 200'],
    'reznor': S9 + ['warp 0DF', 'waitmode 0F 60', 'waitmode 14', 'wait 200'],
    'bigboo': S9 + ['warp 0E4', 'waitmode 0F 60', 'waitmode 14', 'wait 200'],
    'bowser': S9 + ['warp 1C7', 'waitmode 0F 60', 'waitmode 14', 'wait 400'],
    'water': S9 + ['warp 011', 'waitmode 0F 60', 'waitmode 14', 'wait 120'],
    'vertical': S9 + ['warp 109', 'waitmode 0F 60', 'waitmode 14', 'wait 120'],
    'ghost': S9 + ['warp 004', 'waitmode 0F 60', 'waitmode 14', 'wait 120'],
    'layer2': S9 + ['warp 115', 'waitmode 0F 60', 'waitmode 14', 'wait 120'],
    'wings': S9 + ['warp 0C8', 'waitmode 0F 60', 'waitmode 14', 'wait 120'],
    'palace': S9 + ['warp 014', 'waitmode 0F 60', 'waitmode 14', 'wait 120'],
    'autoscroll': S9 + ['warp 00C', 'waitmode 0F 60', 'waitmode 14', 'wait 200'],
    'castle': S9 + ['warp 101', 'waitmode 0F 60', 'waitmode 14', 'wait 300'],
}
TO = ['106', '109', '011', '101', '004', '115', '000', '0C8', '095', '10D', 'rush']

PEEKS = ['peek 1A 4', 'peek 94 4', 'peek 36 2', 'peek 1411 2', 'peek 1B93 1', 'whereami']


def script(frm, to):
    start = 'bossrush' if to == 'rush' else 'levelselect %s' % to
    off = ['lsclear 0'] if NOCLEAR and frm != 'map' else []  # (the map itself cleans up: its reference stays as is)
    s = FROM[frm] + off + ['whereami', 'shot f0_from', start, 'waitmode 0F 60', 'waitmode 14 1200', 'wait 30']
    s += PEEKS + ['shot f1_start', 'hold RIGHT', 'wait 150', 'hold'] + PEEKS + ['shot f2_walk', 'quit']
    return s


def parse(out):
    """the two records (start, after the walk): dict of fields"""
    recs, cur = [], {}
    for l in out.splitlines():
        m = re.match(r'peek 0*([0-9A-F]+): (.*)', l)
        if m:
            b = [int(x, 16) for x in m.group(2).split()]
            a = int(m.group(1), 16)
            if a == 0x1A:
                cur['camx'], cur['camy'] = b[0] | b[1] << 8, b[2] | b[3] << 8
            elif a == 0x94:
                cur['x'], cur['y'] = b[0] | b[1] << 8, b[2] | b[3] << 8
            elif a == 0x36:
                cur['rot'] = b[0] | b[1] << 8
            elif a == 0x1411:
                cur['hscroll'], cur['vscroll'] = b
            elif a == 0x1B93:
                cur['secondary'] = b[0]
        m = re.match(r'whereami: mode (\w+) level (\w+) .* state (\d+) ', l)
        if m and 'camx' in cur:
            cur.update(mode=m.group(1), level=m.group(2), state=int(m.group(3)))
            recs.append(cur)
            cur = {}
    return recs


def run(pair):
    frm, to = pair
    d = os.path.join(out_root, '%s__%s' % (frm, to))
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(script(frm, to)) + '\n')
    try:
        r = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                           text=True, timeout=600)
        rc, out = r.returncode, r.stdout
    except subprocess.TimeoutExpired:
        rc, out = 'timeout', ''
    open(os.path.join(d, 'out.txt'), 'w').write(out)
    return pair, rc, parse(out)


def as_int(v):
    return v - 0x10000 if v >= 0x8000 else v


froms = sys.argv[sys.argv.index('--from') + 1].split(',') if '--from' in sys.argv else list(FROM)
tos = sys.argv[sys.argv.index('--to') + 1].split(',') if '--to' in sys.argv else TO
if 'map' not in froms:
    froms = ['map'] + froms
pairs = [(f, t) for f in froms for t in tos]
with ThreadPoolExecutor(jobs) as ex:
    res = {pair: (rc, recs) for pair, rc, recs in ex.map(run, pairs)}

lines, fails = [], 0
for (frm, to), (rc, recs) in sorted(res.items()):
    ref = res[('map', to)][1]
    problems = []
    if rc != 0:
        problems.append('rc %s' % rc)
    if len(recs) < 2 or len(ref) < 2:
        problems.append('records %d' % len(recs))
    else:
        for i, tag in ((0, 'start'), (1, 'walk')):
            a, b = recs[i], ref[i]
            if a['level'] != b['level'] or a['mode'] != b['mode']:
                problems.append('%s level %s/%s mode %s/%s' % (tag, a['level'], b['level'], a['mode'], b['mode']))
            if a['state'] != b['state']:  # (castles and ghost houses start with the walk through the door: 0A)
                problems.append('%s state %d vs %d' % (tag, a['state'], b['state']))
            if abs(as_int(a['x']) - as_int(b['x'])) > 24 or abs(as_int(a['y']) - as_int(b['y'])) > 24:
                problems.append('%s mario %d,%d vs %d,%d' % (tag, as_int(a['x']), as_int(a['y']), as_int(b['x']),
                                                             as_int(b['y'])))
            if abs(as_int(a['camx']) - as_int(b['camx'])) > 32 or abs(as_int(a['camy']) - as_int(b['camy'])) > 32:
                problems.append('%s camera %d,%d vs %d,%d' % (tag, as_int(a['camx']), as_int(a['camy']),
                                                              as_int(b['camx']), as_int(b['camy'])))
            if a['rot'] != b['rot'] and to != 'rush':
                problems.append('%s m7 rotation %04X vs %04X' % (tag, a['rot'], b['rot']))
            if (a['hscroll'], a['vscroll'], a['secondary']) != (b['hscroll'], b['vscroll'], b['secondary']):
                problems.append('%s scroll/secondary %s vs %s' % (tag, (a['hscroll'], a['vscroll'], a['secondary']),
                                                                  (b['hscroll'], b['vscroll'], b['secondary'])))
    fails += bool(problems)
    lines.append('%-10s -> %-5s %s %s' % (frm, to, 'FAIL' if problems else 'ok  ', '; '.join(problems)))
lines.append('pairs %d, failed %d' % (len(res), fails))
open(os.path.join(out_root, 'transition_matrix.txt'), 'w').write('\n'.join(lines) + '\n')
print('\n'.join(lines))
