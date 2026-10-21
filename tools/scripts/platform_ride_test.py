"""Moving and solid platforms (round 18): Mario Combat Mario against SMW Mario on every kind of platform sprite.
Each run: Yoshi's Island 1 (save state 9), flat ground at x 700, sprites cleared; one platform sprite is spawned
120 px above Mario, in open sky, and Mario is put 1 px above the platform (pilot `ontop 0 0 -7`: where the sprite's solid
code stands the player, so on the swinging end of a chained platform). He stands for 150 frames, walks right for 3
(MC Mario walks faster and slides on: a longer walk takes him off the smaller platforms), stands again, and jumps
straight up twice.
For each sprite the run is made with SMW Mario (`setting mc 0`, the reference) and MC Mario. Counted over the standing
frames (10-150): frames on the platform (SMW's blocked-below flag, which it sets on a platform too, with Mario well
above the floor) and drops into the air while standing (a ride that loses the platform for a frame or two, the
flicker). MC Mario must ride as long as SMW Mario (6 frames' slack), drop no more often, and still be on it at the
end of the jumps when SMW Mario is.
Usage: python -I platform_ride_test.py <root> [ids...]
Writes work/pilot/platform_ride/<id>_<mc|smw>/ and prints one line per sprite.
"""
import csv, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = os.path.abspath(sys.argv[1])
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'platform_ride')
FLOOR_PY = 352  # Mario's y standing on the ground at x 700
# checkerboard / rock platforms (horizontal, vertical), turn block bridges, falling checkerboard, the endless orange
# platform, chained platforms (brown, grey), moving castle block, sinking lava platform, flying grey turn blocks,
# falling grey platform, timed lift, flying ? blocks, message box (the water platforms 5B / 5D need water)
IDS = sys.argv[2:] or ['55', '56', '57', '58', '59', '5A', '5C', '5E', '5F', 'A3', 'BB', 'C0', 'C1', 'C4', 'BA', '83',
                       '84', 'B9']


def run(job):
    sid, mc = job
    d = os.path.join(out_root, '%s_%s' % (sid, 'mc' if mc else 'smw'))
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    s = ['loadstate 9', 'wait 5', 'cheat 1 1', 'setting mc %d' % mc, 'glide 700 240', 'wait 50', 'clearsprites',
         'spawn %s 0 -120' % sid, 'wait 2', 'ontop 0 0 -7', 'log a.csv', 'wait 75', 'shot a', 'wait 75', 'hold RIGHT',
         'wait 3', 'hold', 'wait 52', 'shot b', 'hold B', 'wait 10', 'hold', 'wait 40', 'hold B', 'wait 10', 'hold',
         'wait 40', 'shot c', 'log off', 'quit']
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(s) + '\n')
    subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                   text=True, timeout=300)
    return job, list(csv.DictReader(open(os.path.join(d, 'a.csv'))))


def on_platform(r):
    return (int(r['blocked']) & 4) != 0 and int(r['py']) < FLOOR_PY - 8


def stats(rows):
    stand = rows[10:150]
    on = sum(on_platform(r) for r in stand)
    drops = sum(1 for a, b in zip(stand, stand[1:]) if on_platform(a) and not on_platform(b))
    return on, drops, bool(rows) and on_platform(rows[-1])


jobs = [(i, mc) for i in IDS for mc in (0, 1)]
with ThreadPoolExecutor(6) as ex:
    res = dict(ex.map(run, jobs))
bad = 0
for sid in IDS:
    (on_s, drops_s, end_s), (on_m, drops_m, end_m) = stats(res[(sid, 0)]), stats(res[(sid, 1)])
    ok = on_m >= on_s - 6 and drops_m <= drops_s + 1 and (end_m or not end_s)
    bad += not ok
    print('%s  on it SMW %3d/140 MC %3d/140  drops SMW %2d MC %2d  on it at the end SMW %-5s MC %-5s %s' % (
        sid, on_s, on_m, drops_s, drops_m, end_s, end_m, 'ok' if ok else 'FAIL'), flush=True)
print('sprites %d, failed %d' % (len(IDS), bad))
