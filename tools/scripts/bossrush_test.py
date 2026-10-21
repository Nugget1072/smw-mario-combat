"""Boss rush tests (LEVEL SELECT > BOSS RUSH, pilot `bossrush [all] [end]`).

Scenarios, each a pilot run from the save states (9: inside Yoshi's Island 1; `kill` + wait reaches the map):
  chain_castles  from the map: 8 fights; each is won by setting SMW's boss-end flag ($13C6, `poke 13C6 1`), as the
                 bosses' own code does; the rooms must come in order, then the map with progress and lives unchanged
  chain_all      the same with every boss (10 rooms: Reznor once)
  from_level     started inside a level: 8 fights, then the map as it was
  retry          a death (`kill`) in the 2nd fight: the same room again, lives unchanged, the clock running on
  end_on_death   with "end run": a death in the 2nd fight ends the run on the map, lives and progress unchanged
  abandon        a level chosen from the level select during a run ends the run; that level starts
  real_fights    every boss, fought by MC Mario's punches (pilot `attack boss`), no pokes: at least the first three
                 fights must be won and the rooms must follow in order
Usage: python -I bossrush_test.py <root> [scenario ...]
Writes work/pilot/bossrush/<scenario>/ (script, out.txt, screenshots) and prints PASS / FAIL per scenario.
"""
import os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = sys.argv[1]
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'bossrush')
CASTLES = ['1F6', '0E5', '1F2', '0D9', '0CC', '0D3', '1EB', '1C7']
ALL = ['1F6', '0E4', '0E5', '0DF', '1F2', '0D9', '0CC', '0D3', '1EB', '1C7']  # (one Reznor: the first fortress)
TO_MAP = ['loadstate 9', 'wait 5', 'kill', 'wait 400']


def fight_won():  # the boss's end flag, then the 150 frames before the next fight (and its load)
    return ['poke 13C6 1', 'wait 260']


def chain(rooms, start):
    s = start + ['progress', 'peek 1EA2 141', 'whereami', 'bossrush%s' % (' all' if len(rooms) > 8 else ''), 'wait 200']
    for i, r in enumerate(rooms):
        s += ['whereami', 'rushinfo', 'shot f%02d' % i] + fight_won()
    s += ['wait 200', 'whereami', 'progress', 'peek 1EA2 141', 'rushinfo', 'shot end']
    return s


def save_diff(out):
    """Save-buffer bytes ($1EA2-$1F2E) that differ between the run's start and end, as addresses."""
    pk = [l.split(':')[1].split() for l in out.splitlines() if l.startswith('peek 01EA2')]
    return [0x1EA2 + i for i in range(len(pk[0])) if pk[0][i] != pk[1][i]] if len(pk) == 2 else ['missing']


SCENARIOS = {
    'chain_castles': chain(CASTLES, TO_MAP),
    'chain_all': chain(ALL, TO_MAP),
    'from_level': chain(CASTLES, ['loadstate 9', 'wait 5']),
    'retry': TO_MAP + ['progress', 'whereami', 'bossrush', 'wait 200', 'whereami'] + fight_won() +
             ['whereami', 'rushinfo', 'kill', 'wait 260', 'whereami', 'rushinfo', 'shot after_death', 'wait 60',
              'rushinfo'],
    'end_on_death': TO_MAP + ['progress', 'whereami', 'bossrush end', 'wait 200', 'whereami'] + fight_won() +
                    ['whereami', 'kill', 'wait 500', 'whereami', 'progress', 'rushinfo', 'shot end'],
    'abandon': TO_MAP + ['progress', 'bossrush', 'wait 200', 'whereami', 'levelselect 106', 'wait 200', 'whereami',
                         'rushinfo', 'kill', 'wait 500', 'whereami', 'progress'],
    # Every boss before Bowser (each fight played 150 frames, then won), then Bowser fought by jump kicks: the battle
    # must be upright (mode 7 rotation 0, angle 32) in his fighting phases and his hits must count.
    'bowser_after_all': TO_MAP + ['cheat 1 1', 'bossrush all', 'waitmode 13'] +
                        sum([['wait 150', 'poke 13C6 1', 'waitmode 0F 400', 'waitmode 13 600'] for _ in ALL[:-1]], []) +
                        ['waitmode 14', 'wait 600', 'whereami'] +
                        sum([['attack A0 40 air 8', 'peek 14B5', 'wait 10', 'peek 14B5', 'peek 1525', 'peek 36 4',
                              'bosspunches']
                             for _ in range(150)], []) + ['shot end'],
    # The Koopa Kids that climb walls (Morton, Roy) after the bosses before them: each must move about
    # (sprite 29 seen at 6+ different places in 12 samples, 20 frames apart) as in a fresh room. (Ludwig waits at his
    # place until Mario comes, in a fresh room too.)
    'kids_move': TO_MAP + ['cheat 1 1', 'bossrush all', 'waitmode 13'] +
                 sum([(['waitmode 14'] + sum([['wait 20', 'sprites'] for _ in range(12)], []) +
                       ['shot k_%s' % r] if r in ('0E5', '0CC') else ['waitmode 14', 'wait 150']) +
                      ['whereami', 'poke 13C6 1', 'waitmode 0F 400', 'waitmode 13 600'] for r in ALL[:8]], []),
    # (a bot: walk up to the nearest boss and punch, every third time a jump kick; the last music command sent to the
    # sound driver, $1DFF, is sampled every round: each fight must have its song on, not none or the fade-out 80)
    'real_fights': TO_MAP + ['cheat 1 1', 'progress', 'bossrush all', 'wait 200', 'whereami'] +
                   sum([['attack boss 40%s' % (' air' if n % 3 == 0 else ''), 'wait 10', 'whereami', 'rushinfo',
                         'peek 1DFF']
                        for n in range(260)], []) + ['shot end'],
}


def run(name):
    d = os.path.join(out_root, name)
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(SCENARIOS[name] + ['quit']) + '\n')
    try:
        r = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                           text=True, timeout=1200)
        rc, out = r.returncode, r.stdout
    except subprocess.TimeoutExpired:
        rc, out = 'timeout', ''
    open(os.path.join(d, 'out.txt'), 'w').write(out)
    w = re.findall(r'whereami: mode (\w+) level (\w+) .* lives (\d+) power (\d+) ', out)
    prog = re.findall(r'progress: (\w+)', out)
    info = re.findall(r'rushinfo: (.*) \(frame', out)
    problems = []
    if rc != 0:
        problems.append('rc %s' % rc)
    if name.startswith('chain') or name == 'from_level':
        rooms = CASTLES if name != 'chain_all' else ALL
        seen = [lv for mode, lv, _, _ in w[1:1 + len(rooms)]]
        if seen != rooms:
            problems.append('rooms %s' % seen)
        end = w[-1]
        if end[0] != '0E':
            problems.append('not back on the map: mode %s' % end[0])
        # Started inside a level, the save buffer holds Mario's map pose ($1F13-$1F16) as the level left it; the map
        # sets it again when it loads. Everything else must be as it was.
        diff = [a for a in save_diff(out) if not (name == 'from_level' and 0x1F13 <= a <= 0x1F16)]
        if diff:
            problems.append('save bytes changed %s' % [hex(a) for a in diff])
        if end[2:] != w[0][2:]:
            problems.append('lives, power %s -> %s' % (w[0][2:], end[2:]))
        if info[-1] != 'none':
            problems.append('run still on: %s' % info[-1])
        counts = [int(m) for m in re.findall(r'BOSS (\d+)/', ' '.join(info))]
        if counts != list(range(1, len(rooms) + 1)):
            problems.append('hud %s' % counts)
    elif name == 'retry':
        # whereami: map, fight 1, fight 2, fight 2 after the death
        if len(w) < 4 or w[2][1] != CASTLES[1] or w[3][1] != CASTLES[1] or w[3][0] != '14':
            problems.append('after death %s' % (w,))
        if len(w) >= 4 and w[3][2] != w[0][2]:
            problems.append('lives %s -> %s' % (w[0][2], w[3][2]))
        if len(info) < 3 or not info[1].startswith('BOSS 2/8') or not info[2].startswith('BOSS 2/8'):
            problems.append('hud %s' % info)
        cs = [int(m.group(1)) * 6000 + int(m.group(2)) * 100 + int(m.group(3))
              for m in (re.search(r'(\d+):(\d+)\.(\d+)', i) for i in info) if m]
        if len(cs) < 3 or not cs[0] < cs[1] < cs[2]:
            problems.append('clock %s' % info)
    elif name == 'end_on_death':
        end = w[-1]
        if end[0] != '0E' or end[2:] != w[0][2:] or len(prog) != 2 or prog[0] != prog[1] or info[-1] != 'none':
            problems.append('end %s progress %s info %s' % (end, prog, info))
    elif name == 'abandon':
        if len(w) < 2 or w[1][1] != '106' or info[0] != 'none':
            problems.append('abandon %s %s' % (w, info))
        if len(prog) != 2 or prog[0] != prog[1]:
            problems.append('progress %s' % prog)
    elif name == 'bowser_after_all':
        lines = out.splitlines()
        hurt = sum(1 for l in lines if l.startswith('peek 014B5') and not l.endswith(' 00'))
        samples, upright = 0, 0
        for i, l in enumerate(lines):
            if l.startswith('peek 01525') and i + 1 < len(lines) and i >= 1:
                phase = int(l.split()[-1], 16)
                hit = not lines[i - 1].endswith(' 00')
                b = [int(x, 16) for x in lines[i + 1].split(':')[1].split()]
                if 7 <= phase <= 9 and not hit:
                    samples += 1
                    upright += b[0] == 0 and b[1] == 0 and b[2] == 0x20
        if not w or w[0][1] != '1C7':
            problems.append('not in Bowser\'s room: %s' % (w[:1],))
        # (his hurt timer $14B5 runs 76 frames, which a round can outlast; his fighting phase moving on is a hit too)
        phases = [int(l.split()[-1], 16) for l in lines if l.startswith('peek 01525')]
        fight = [p for p in phases if p >= 7]
        if hurt == 0 and not (fight and max(fight) > fight[0]):
            problems.append('Bowser never hurt')
        if samples < 10 or upright < 0.8 * samples:
            problems.append('upright in %d of %d fighting samples' % (upright, samples))
    elif name == 'kids_move':
        block, seen = [], {}
        for l in out.splitlines():
            m = re.match(r'sprite slot \d+ id 29 status 8 x=(-?\d+) y=(-?\d+)', l)
            if m:
                block.append((int(m.group(1)) // 4, int(m.group(2)) // 4))
            m = re.match(r'whereami: mode \w+ level (\w+)', l)
            if m:
                if m.group(1) in ('0E5', '0CC'):
                    seen[m.group(1)] = len(set(block))
                block = []
        for r in ('0E5', '0CC'):
            if seen.get(r, 0) < 6:
                problems.append('%s: Koopa Kid at %d places' % (r, seen.get(r, 0)))
    elif name == 'real_fights':
        rooms = []
        for _, lv, _, _ in w:
            if not rooms or rooms[-1] != lv:
                rooms.append(lv)
        if rooms[:4] != ['1F6', '0E4', '0E5', '0DF'][:len(rooms[:4])] or len(rooms) < 3:
            problems.append('rooms %s' % rooms)
        # music: the samples taken in each room while fighting (mode 14), most of them with a song on
        music, room = {}, None
        for l in out.splitlines():
            m = re.match(r'whereami: mode (\w+) level (\w+)', l)
            if m:
                room = m.group(2) if m.group(1) == '14' else None
            elif l.startswith('peek 01DFF') and room:
                music.setdefault(room, []).append(l.split()[-1] not in ('00', '80'))
        quiet = ['%s %d/%d' % (r, sum(v), len(v)) for r, v in music.items() if sum(v) < 0.6 * len(v)]
        if quiet:
            problems.append('fights without music: %s' % ', '.join(quiet))
        print('real_fights music per room: %s' % ', '.join('%s %d/%d' % (r, sum(v), len(v)) for r, v in music.items()))
    return name, problems, len(w)


names = [a for a in sys.argv[2:] if a in SCENARIOS] or list(SCENARIOS)
with ThreadPoolExecutor(4) as ex:
    for name, problems, n in ex.map(run, names):
        print('%-14s %s %s' % (name, 'PASS' if not problems else 'FAIL', '; '.join(problems)), flush=True)
