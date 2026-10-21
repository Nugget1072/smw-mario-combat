"""Every kind of platform and moving block where the game itself puts it (round 19): Mario Combat Mario against SMW
Mario on each, in a level that has it.
The rooms and positions come from the pilot `spritefind <id>` (work/pilot/platform_find.txt, made by this script when
missing). Each run: from save state 9, No damage and Infinite lives on, `warp` to the room, fly right at a height above
the sprite's place until one is loaded (`seek`; kinds marked `glide` are flown to directly), then Mario is put on the
nearest sprite of that kind, 1 px above its top (`ontopid <id> -7`), stands on it for 150 frames, walks right for 3
frames, waits, and hops straight up twice (B for 3 frames: MC Mario's full jump goes higher and lasts longer than SMW
Mario's, and a moving platform is gone when he comes down). The log follows the platform (`wx`/`wy`) and `onspr`
(misc_player_on_solid_sprite: SMW's "standing on a sprite", set by every platform routine).
Counted over the standing frames (10-150): frames on the platform and drops off it; and whether Mario is on it at the
end. MC Mario must ride as long as SMW Mario (10 frames' slack), drop no more often (+1), be on it at the end when SMW
Mario is, and not die where SMW Mario doesn't. A run where the sprite is not found counts as "n/a" (both Marios).
Usage: python -I platform_levels_test.py <root> [ids...]
Writes work/pilot/platform_levels/<id>_<mc|smw>/ and prints one line per kind.
"""
import csv, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = os.path.abspath(sys.argv[1])
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'platform_levels')
find_path = os.path.join(root, 'work', 'pilot', 'platform_find.txt')
# kind: (room, name); the sprite's first place in that room is used
KINDS = {
    '2F': ('013', 'springboard'), '41': ('002', 'dolphin (long jump)'), '42': ('002', 'dolphin (jumps)'),
    '43': ('002', 'dolphin (vertical)'), '49': ('006', 'growing / shrinking pipe end'),
    '55': ('005', 'checkerboard platform, horizontal'), '57': ('0BF', 'checkerboard platform, vertical'),
    '59': ('01C', 'turn block bridge, both ways'), '5A': ('020', 'turn block bridge, horizontal'),
    '5B': ('11C', 'brown platform floating in water'), '5D': ('102', 'orange platform floating in water'),
    '5F': ('103', 'brown platform on a chain'), '61': ('10A', 'skull raft on lava'),
    '62': ('005', 'brown platform, line-guided'), '63': ('005', 'checker / brown platform, line-guided'),
    '6B': ('119', 'wall springboard, left'), '6C': ('119', 'wall springboard, right'),
    '6D': ('0C9', 'invisible solid block'), '83': ('009', 'flying ? block, flies off'),
    '84': ('125', 'flying ? block, to and fro'), '8F': ('00C', 'scale platforms'),
    '9C': ('006', 'flying grey turn blocks'), 'A3': ('023', 'grey platform on a chain'),
    'B1': ('020', 'creating / eating block'), 'B7': ('01D', 'carrot top lift, upper right'),
    'B8': ('01D', 'carrot top lift, upper left'), 'B9': ('004', 'message box'), 'BA': ('113', 'timed lift'),
    'BB': ('0D4', 'moving castle block'), 'BC': ('020', 'Bowser statue'), 'BF': ('01D', 'Mega Mole'),
    'C0': ('10F', 'sinking lava platform'), 'C1': ('122', 'flying grey platform with wings'),
    'C4': ('00C', 'falling grey platform'), 'C8': ('10E', 'light switch block'),
}
GLIDE = set()  # kinds flown to directly instead of found by `seek`
IDS = [a.upper() for a in sys.argv[2:]] or list(KINDS)


def run_pilot(d, lines):
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(lines + ['quit']) + '\n')
    try:
        return subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game,
                              capture_output=True, text=True, timeout=600).stdout
    except subprocess.TimeoutExpired:
        return 'TIMEOUT'


if not os.path.exists(find_path):
    out = run_pilot(os.path.join(out_root, '_find'), ['wait 2'] + ['spritefind %s' % i for i in KINDS])
    open(find_path, 'w').write(out)
places = {}
for l in open(find_path):
    m = re.match(r'spritefind (\w+) level (\w+) screen \d+ x (\d+) y (\d+)', l)
    if m and m.group(1) in KINDS and KINDS[m.group(1)][0] == m.group(2) and m.group(1) not in places:
        places[m.group(1)] = (int(m.group(3)), int(m.group(4)))


def run(job):
    sid, mc = job
    room = KINDS[sid][0]
    x, y = places.get(sid, (128, 300))
    d = os.path.join(out_root, '%s_%s' % (sid, 'mc' if mc else 'smw'))
    go = ('glide %d %d' % (max(24, x - 8), max(32, y - 96)) if sid in GLIDE
          else 'seek %s %d' % (sid, max(32, y - 64)))
    s = ['loadstate 9', 'wait 5', 'cheat 1 1', 'cheat 3 1', 'setting mc %d' % mc, 'warp %s' % room,
         'waitmode 14 900', 'wait 1', go, 'wait 2',
         'ontopid %s -7' % sid, 'log a.csv', 'wait 75', 'shot a', 'wait 75', 'hold RIGHT', 'wait 3', 'hold', 'wait 52',
         'shot b', 'hold B', 'wait 3', 'hold', 'wait 47', 'hold B', 'wait 3', 'hold', 'wait 90', 'shot c',
         'log off', 'whereami']
    out = run_pilot(d, s)
    found = re.search(r'ontopid: \w+ slot (-?\d+)', out)
    slot = int(found.group(1)) if found else -1
    rows = list(csv.DictReader(open(os.path.join(d, 'a.csv')))) if os.path.exists(os.path.join(d, 'a.csv')) else []
    return job, slot, rows, out


def stats(rows):
    on = lambda r: r.get('onspr', '0') not in ('0', '')
    stand = rows[10:150]
    n = sum(on(r) for r in stand)
    drops = sum(1 for a, b in zip(stand, stand[1:]) if on(a) and not on(b))
    died = any(r['state'] == '9' for r in rows)
    return n, drops, bool(rows) and on(rows[-1]), died


jobs = [(i, mc) for i in IDS for mc in (0, 1)]
with ThreadPoolExecutor(6) as ex:
    res = {j: r for j, *r in ex.map(run, jobs)}
bad = 0
for sid in IDS:
    (slot_s, rows_s, _), (slot_m, rows_m, _) = res[(sid, 0)], res[(sid, 1)]
    if slot_s < 0 or slot_m < 0 or not rows_s or not rows_m:
        print('%s  %-36s n/a (sprite slot SMW %d MC %d)' % (sid, KINDS[sid][1], slot_s, slot_m), flush=True)
        continue
    (n_s, d_s, e_s, x_s), (n_m, d_m, e_m, x_m) = stats(rows_s), stats(rows_m)
    ok = n_m >= n_s - 10 and d_m <= d_s + 1 and (e_m or not e_s) and (x_s or not x_m)
    bad += not ok
    print('%s  %-36s on it SMW %3d/140 MC %3d/140  drops SMW %d MC %d  end SMW %-5s MC %-5s died SMW %-5s MC %-5s %s' % (
        sid, KINDS[sid][1], n_s, n_m, d_s, d_m, e_s, e_m, x_s, x_m, 'ok' if ok else 'FAIL'), flush=True)
print('kinds %d, failed %d' % (len(IDS), bad))
