"""Wall cling and wall jump off solid sprites (round 18): SMW's SolidSpriteBlock pushes the player out of a solid
sprite's side without a blocked flag, so MC Mario never saw those walls (no cling, no wall jump).
Each run: Yoshi's Island 1 (save state 9), flat ground at x 260, sprites cleared; a solid sprite is spawned ahead of
Mario at chest-to-head height, the script waits D frames (the flying blocks move, so each D meets them at another
spot), then MC Mario jumps toward it (B for 3 frames: a hop, or 10: a jump; the direction held) and keeps pushing;
from frame 16 on he taps jump every 6 frames (the wall jump needs a new press while clinging).
Sprites: `box` (three message boxes B9 stacked: a still wall), `fly83` / `fly84` (flying ? blocks, the one that flies
off and the winged one that flies to and fro), `used83` / `used84` (the same, already hit: $C2 = 1).
A run counts a cling when MC's wall pose (hero frame 17) shows in the air, and a wall jump when, after the cling, he
leaves it moving away (MC x speed <= -8, away from the sprite on his right).
Usage: python -I solid_sprite_wall_test.py <root>
"""
import csv, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = os.path.abspath(sys.argv[1])
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'solid_sprite_wall')
SPRITES = {
    'box': ['spawn B9 40 -16', 'spawn B9 40 -32', 'spawn B9 40 -48'],
    'fly83': ['spawn 83 44 -28'],
    'fly84': ['spawn 84 44 -28'],
    'used83': ['spawn 83 44 -28', 'poke C2 1'],
    'used84': ['spawn 84 44 -28', 'poke C2 1'],
}
JUMPS = {'hop': 3, 'jump': 10}
DELAYS = range(0, 40, 4)


def run(job):
    name, jump, d = job
    out = os.path.join(out_root, '%s_%s_%02d' % (name, jump, d))
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        os.remove(os.path.join(out, f))
    s = ['loadstate 9', 'wait 5', 'cheat 1 1', 'glide 260 240', 'wait 50', 'clearsprites'] + SPRITES[name]
    s += ['wait %d' % (d + 1), 'log a.csv']
    for f in range(80):
        b = f < JUMPS[jump] or (f >= 16 and f % 6 == 4)
        s += ['hold RIGHT' + (' B' if b else ''), 'wait 1']
        if f == 30:
            s.append('shot a')
    s += ['hold', 'wait 10', 'log off', 'quit']
    p = os.path.join(out, 'script.txt')
    open(p, 'w').write('\n'.join(s) + '\n')
    subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', out], cwd=game, capture_output=True,
                   text=True, timeout=300)
    rows = list(csv.DictReader(open(os.path.join(out, 'a.csv'))))
    cling = next((i for i, r in enumerate(rows) if r['mcframe'] == '17' and r['mcground'] == '0'), None)
    kicked = cling is not None and any(float(r['mcxs']) <= -8 and r['mcground'] == '0' for r in rows[cling:])
    return job, cling, kicked


jobs = [(n, j, d) for n in SPRITES for j in JUMPS for d in DELAYS]
with ThreadPoolExecutor(6) as ex:
    res = list(ex.map(run, jobs))
tot = {}
for (name, jump, d), cling, kicked in res:
    print('%-7s %-4s delay %2d  cling %-5s wall jump %s' % (name, jump, d, cling, kicked), flush=True)
    c = tot.setdefault(name + ' ' + jump, [0, 0, 0])
    c[0] += 1
    c[1] += cling is not None
    c[2] += kicked
for name, (n, c, k) in tot.items():
    print('%-12s runs %2d  clung %2d  wall jumps %2d' % (name, n, c, k))
